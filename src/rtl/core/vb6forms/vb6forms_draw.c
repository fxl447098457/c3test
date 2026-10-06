// vb6forms_draw.c - Form / Printer 的绘图方法家族：PSet / Line / Circle / Point
// 2026-10-06 新增。
//
// 为什么单开一个文件：Form/Printer 的绘图面此前**完全缺失** —— 只有 Shape/Line 控件
// （vb6forms_widget.c 那条自绘路径）。用户代码在 `Form_Paint` 里写
// `Me.PSet / Me.Line / Me.Circle / Me.Point` 时，cgen 把它们当未知 Form 成员落进
// COM dispatch 桩（cgen_expr_member_form_builtin.inc 末尾那段注释写明"运行时不可靠"），
// 于是方法既不画也不报错。
//
// HDC 来源（关键）：**不新建 DC，沿用窗体自己的**。口径与
// vb6_ControlDrawDC（vb6forms_ctrl.c:709）完全一致：
//     1. 先问窗口属性 VB6_PaintDC —— WM_PAINT 派发期宿主已经 BeginPaint 过了
//        （cgen_form_wndproc_subclass.inc:384 与 create.inc:18 两处都 SetPropW 了），
//        此时必须用**那一张**，另 GetDC 会画到别处、且 WM_PAINT 里 GetDC 是错用法；
//     2. 否则 GetDC(hwnd)（Print/Cls 等在 WM_PAINT 之外调用的场景）。
// 非 WM_PAINT 期拿的 DC 由本文件负责 ReleaseDC，fromPaint 那张不释放。
//
// 状态存哪：绘图状态（ForeColor / DrawWidth / DrawStyle / FillStyle / CurrentX/Y）
// 一律按 HWND 存窗口属性（"VB6_FgColor" / "VB6_DrawWidth" ...），与
// vb6_ControlPrint 用 "VB6_PrintX/Y" 同一套手法。这样:
//   * Form 与控件天然隔离（各自 HWND）；
//   * 不需要给宿主模型加字段、不需要动 vb6_UserControlDesc 那三张按名桥表
//     （MEMORY 记着"加槽一律追加末尾"，能不碰就不碰）；
//   * 缺省值 = 属性不存在 = 0，各方法自己兜底成 VB6 的初始值。
//
// 单位：与 Print / TextWidth 同一口径 —— 绘图坐标按**缇(twips)** 收，落到 DC 前用
// MM_TEXT + 缇→像素换算（1 px = 15 缇 @96dpi）。ScaleMode 系列属另一条线
// （ScaleX/ScaleY/ScaleWidth/ScaleHeight），本文件只在 ScaleMode==vbPixels(3) 时
// 直通像素、其余按缇换算，与 vb6_ScaleUserToPx 同款判据。
#include "vb6forms.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <wchar.h>

#ifdef _WIN32
#include <windows.h>
#include <oleauto.h>
#endif

// ============================================================
// HDC 获取（Form 与 Printer 共用的那一张）
// ============================================================
typedef struct {
    HDC dc;
    BOOL fromPaint;   // TRUE = 借的是 WM_PAINT 派发期那张, 不可 Release
} vb6_draw_dc_t;

static vb6_draw_dc_t vb6_DrawAcquire(void* hwndOrNull) {
    vb6_draw_dc_t out;
    HWND hw = (HWND)hwndOrNull;
    out.dc = NULL;
    out.fromPaint = FALSE;
    if (hw) {
        HDC painting = (HDC)GetPropW(hw, L"VB6_PaintDC");
        if (painting) {           // 优先用派发期那张 (见文件头口径 1)
            out.dc = painting;
            out.fromPaint = TRUE;
            return out;
        }
    }
    // Printer: 传进来的 hwndOrNull 是 NULL 之外的哨兵时另走 g_printerDC,
    // 由 vb6_DrawAcquirePrinter 单独处理。
    out.dc = hw ? GetDC(hw) : NULL;
    return out;
}

static void vb6_DrawRelease(vb6_draw_dc_t* d, void* hwndOrNull) {
    if (!d->dc) return;
    if (d->fromPaint) return;     // 派发期那张由宿主 EndPaint 收尾, 不能 Release
    HWND hw = (HWND)hwndOrNull;
    if (hw) ReleaseDC(hw, d->dc);
    d->dc = NULL;
}

// ============================================================
// 绘图状态 (按 HWND 存窗口属性; 缺省 = 0 = 未设)
// ============================================================
static int32_t vb6_DrawGetI(void* hwnd, const wchar_t* name, int32_t dflt) {
    HWND hw = (HWND)hwnd;
    if (!hw) return dflt;
    intptr_t v = (intptr_t)GetPropW(hw, name);
    return (v == 0) ? dflt : (int32_t)v;
}

static void vb6_DrawSetI(void* hwnd, const wchar_t* name, int32_t v) {
    HWND hw = (HWND)hwnd;
    if (!hw) return;
    // **存裸值**: 读侧 vb6_DrawGetI 也按裸值答, 两边同一套编码。
    // "0 与从没设过不可分辨"不在编码层解, 由各属性自己的缺省档兜 (账 #233 订正:
    // 这里原先存 v+1 而读侧不减 => 写 3 读回 4, 且 Step 那一路每画一次多带 1):
    // DrawWidth 的 setter 先钳到 >=1, VB6_BackColor 的写者本来就存裸值。
    SetPropW(hw, name, (HANDLE)(INT_PTR)v);
}

// 笔位 (CurrentX / CurrentY) **只有一份存储** —— vb6forms_widget_prop.c 那对 float
// 出口。此前本文件在同一个窗口属性名上另开了一套 int32 编码: `vb6_Form_Print` 读 float、
// PSet/Line/Circle 读 int32, 两边互读必错 (账 #233)。单位口径还不齐 (Print 按像素推进、
// 绘图按用户单位), 那一问记在账 #224, 本刀不动。
static int32_t vb6_DrawCurX(HWND hw) { return (int32_t)vb6_GetCurrentX((void*)hw); }
static int32_t vb6_DrawCurY(HWND hw) { return (int32_t)vb6_GetCurrentY((void*)hw); }
static void vb6_DrawSetCurX(HWND hw, int32_t v) { vb6_SetCurrentX((void*)hw, (float)v); }
static void vb6_DrawSetCurY(HWND hw, int32_t v) { vb6_SetCurrentY((void*)hw, (float)v); }

// 前景色: 0 (黑) 是合法值, 但不能表示"没设过" => 用 VB6 语义兜底
// (BackColor/BackStyle 决定首色; 这里取 vbBlack = 0, 与 Print 同一档)。
static COLORREF vb6_DrawForeColor(void* hwnd) {
    HWND hw = (HWND)hwnd;
    if (!hw) return RGB(0, 0, 0);
    intptr_t v = (intptr_t)GetPropW(hw, L"VB6_DrawForeColor");
    if (v == 0) return RGB(0, 0, 0);
    return (COLORREF)(int32_t)(v - 1);
}

static void vb6_DrawSetForeColor(void* hwnd, int32_t c) {
    HWND hw = (HWND)hwnd;
    if (!hw) return;
    SetPropW(hw, L"VB6_DrawForeColor", (HANDLE)(INT_PTR)(c + 1));
}

// ============================================================
// 单位换算: 缇 <-> 像素
//   VB6 窗体默认 ScaleMode = vbTwips(1), 绘图坐标单位是**缇**。
//   MM_TEXT 下 1 像素 = 15 缇 @96dpi; 换算要按**该 DC 自己的** dpi 走, 否则
//   高 dpi 屏上整幅图缩掉一截 (与 Print 的 TextWidth 同款约束)。
// ============================================================
static int vb6_DrawScaleMode(void* hwnd) {
    // 账 #233: `VB6_ScaleMode` 的唯一写者是 vb6_SetScaleMode (存裸值), 这里以前按
    // "带 +1 的编码"读 => 同一个属性名两种编码。改问账 #197 那道权威: 读法与几何
    // 换算必须同一处 (vb6_WindowScaleModeSelf)。
    return (int)vb6_WindowScaleModeSelf(hwnd);
}

static double vb6_DrawUserToPx(HDC dc, void* hwnd, double v) {
    if (vb6_DrawScaleMode(hwnd) == 3 /* vbPixels */) return v;   // 已是像素
    int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    if (dpi <= 0) dpi = 96;
    return v * dpi / 1440.0;
}

// ============================================================
// PSET —— 画一个点。VB6 签名: PSet [Step] (x, y) [color]
//   省略 color 时用 ForeColor; Step 为真时坐标相对 (CurrentX, CurrentY)。
//   PSet (不带坐标) = 用 ForeColor 画 CurrentX/CurrentY 一点。
// ============================================================
void vb6_Form_PSet(void* hwnd, int32_t step, int32_t hasXY,
                   double x, double y, int32_t hasColor, int32_t color) {
    HWND hw = (HWND)hwnd;
    int cx = vb6_DrawCurX(hw);
    int cy = vb6_DrawCurY(hw);
    int px = hasXY ? (int)x : cx;
    int py = hasXY ? (int)y : cy;
    if (step) { px += cx; py += cy; }        // Step: 相对当前笔位
    // PSet 总会移动笔位 (VB6 语义)
    int nx = hasXY ? px : cx;
    int ny = hasXY ? py : cy;
    vb6_DrawSetCurX(hw, nx);
    vb6_DrawSetCurY(hw, ny);

    vb6_draw_dc_t d = vb6_DrawAcquire(hwnd);
    if (!d.dc) return;
    double dx = vb6_DrawUserToPx(d.dc, hwnd, px);
    double dy = vb6_DrawUserToPx(d.dc, hwnd, py);
    COLORREF c = hasColor ? (COLORREF)color : vb6_DrawForeColor(hwnd);
    SetPixelV(d.dc, (int)dx, (int)dy, c);
    vb6_DrawRelease(&d, hwnd);
}

// ============================================================
// POINT —— 读回某点颜色。VB6: Point(x, y) -> Long (RGB 或 -1)
// ============================================================
int32_t vb6_Form_Point(void* hwnd, double x, double y) {
    vb6_draw_dc_t d = vb6_DrawAcquire(hwnd);
    if (!d.dc) return -1;
    double dx = vb6_DrawUserToPx(d.dc, hwnd, x);
    double dy = vb6_DrawUserToPx(d.dc, hwnd, y);
    COLORREF c = GetPixel(d.dc, (int)dx, (int)dy);
    vb6_DrawRelease(&d, hwnd);
    return (int32_t)c;   // 已经是 0x00BBGGRR, 与 VB6 的 Point 返回同序
}

// ============================================================
// LINE —— 画线。VB6 签名:
//   Line -(x1,y1)-(x2,y2) [, color] [, B][F]
//   Line -Step(x,y)- [, color] [, B][F]
//   Line [Step] (x1,y1) [Step] -(x2,y2) [, color] [, B][F]
// B/C/F 是**语法旗标**, parser (账 #220) 已折成位值: B=1 矩形、C=2 椭圆、F=4 填充,
// 按字母逐个置位 ⇒ BF=5、CF=6。位口径与 vb6_ControlLine 是同一张表 (账 #221)。
// ⚠ RTL 这边**不按名字认** —— 认名字会误伤用户自己的变量 `B`。
// 不带终点时 = 画到 CurrentX/CurrentY, 并把笔位移到起点 (VB6 语义)。
// ============================================================
void vb6_Form_Line(void* hwnd,
                   int32_t step1, int32_t has1, double x1, double y1,
                   int32_t step2, int32_t has2, double x2, double y2,
                   int32_t hasColor, int32_t color,
                   int32_t style) {
    HWND hw = (HWND)hwnd;
    int cx = vb6_DrawCurX(hw);
    int cy = vb6_DrawCurY(hw);

    int ax, ay, bx, by;
    if (has1) { ax = (int)x1; ay = (int)y1; if (step1) { ax += cx; ay += cy; } }
    else      { ax = cx; ay = cy; }
    if (has2) { bx = (int)x2; by = (int)y2; if (step2) { bx += cx; by += cy; } }
    else      { bx = cx; by = cy; }

    // 终点省略 (To) ⇒ 画完后笔位回到起点; 终点显式 ⇒ 笔位落在终点。
    int endX = has2 ? bx : ax;
    int endY = has2 ? by : ay;

    vb6_draw_dc_t d = vb6_DrawAcquire(hwnd);
    if (!d.dc) return;
    double pax = vb6_DrawUserToPx(d.dc, hwnd, ax);
    double pay = vb6_DrawUserToPx(d.dc, hwnd, ay);
    double pbx = vb6_DrawUserToPx(d.dc, hwnd, bx);
    double pby = vb6_DrawUserToPx(d.dc, hwnd, by);

    COLORREF c = hasColor ? (COLORREF)color : vb6_DrawForeColor(hwnd);
    int w = vb6_DrawGetI(hw, L"VB6_DrawWidth", 1);
    if (w < 1) w = 1;
    HPEN pen = CreatePen(PS_SOLID | (w > 1 ? PS_ENDCAP_ROUND : 0), w, c);
    HGDIOBJ oldPen = pen ? SelectObject(d.dc, pen) : NULL;

    // style 位值: B=1 矩形 / C=2 椭圆 / F=4 填充。任一位置位就不是"线段"。
    if (style & 0x7) {
        int wantEllipse = (style & 0x2) != 0;
        int wantFill    = (style & 0x4) != 0;
        HBRUSH br = NULL;
        HGDIOBJ oldBr = NULL;
        if (wantFill) {
            // F: 用当前前景色实心填充。B 与之同时给 = 填色 + 同色边框。
            br = CreateSolidBrush(c);
            if (br) oldBr = SelectObject(d.dc, br);
        } else {
            oldBr = SelectObject(d.dc, GetStockObject(NULL_BRUSH));
        }
        if (wantEllipse) {
            // Rectangle 的右/下边是**不画**的 (exclusive), 椭圆要 +1 才闭合。
            Ellipse(d.dc, (int)pax, (int)pay, (int)pbx + 1, (int)pby + 1);
        } else {
            Rectangle(d.dc, (int)pax, (int)pay, (int)pbx, (int)pby);
        }
        if (oldBr) SelectObject(d.dc, oldBr);
        if (br) DeleteObject(br);
    } else {
        MoveToEx(d.dc, (int)pax, (int)pay, NULL);
        LineTo(d.dc, (int)pbx, (int)pby);
    }

    if (oldPen) SelectObject(d.dc, oldPen);
    if (pen) DeleteObject(pen);
    vb6_DrawRelease(&d, hwnd);

    vb6_DrawSetCurX(hw, endX);
    vb6_DrawSetCurY(hw, endY);
}

// ============================================================
// CIRCLE —— 画圆/椭圆。VB6 签名:
//   Circle [Step] (x, y), radius [, color] [, start] [, end] [, aspect]
//   Circle 用**画笔的当前位置**当圆心 (不传 x,y 时)。radius 必给。
//   aspect = 纵/横比 (1.0 = 正圆), 不给则由 DrawWidth/DrawHeight 比例算。
//   start/end 是弧度 (VB6 用弧度!), 给了就画弧。
// ============================================================
void vb6_Form_Circle(void* hwnd,
                     int32_t step, int32_t hasXY, double x, double y,
                     double radius,
                     int32_t hasColor, int32_t color,
                     int32_t hasStart, double startAngle,
                     int32_t hasEnd, double endAngle,
                     int32_t hasAspect, double aspect) {
    HWND hw = (HWND)hwnd;
    int cx = vb6_DrawCurX(hw);
    int cy = vb6_DrawCurY(hw);
    int ccx = hasXY ? (int)x : cx;
    int ccy = hasXY ? (int)y : cy;
    if (step) { ccx += cx; ccy += cy; }

    if (radius < 0) radius = -radius;

    // 画完笔位落在圆心 (VB6 语义)
    vb6_DrawSetCurX(hw, ccx);
    vb6_DrawSetCurY(hw, ccy);

    vb6_draw_dc_t d = vb6_DrawAcquire(hwnd);
    if (!d.dc) return;
    double dcx = vb6_DrawUserToPx(d.dc, hwnd, ccx);
    double dcy = vb6_DrawUserToPx(d.dc, hwnd, ccy);
    double r = vb6_DrawUserToPx(d.dc, hwnd, radius);
    // 半径要按**纵横**两个方向各算一次: VB6 的 Circle 传的是缇单位下的半径,
    // 落在 DC 上是椭圆 (除非 aspect=1 且 hwnd 方正)。
    double ry = r;
    if (hasAspect && aspect > 0) ry = r / aspect;
    else {
        // 未给 aspect: 按本窗体的 ScaleWidth/Height 比例 ⇒ 视觉上是"正圆"
        RECT rc; GetClientRect(hw, &rc);
        if (rc.right > 0 && rc.bottom > 0) {
            double sw = (double)rc.right, sh = (double)rc.bottom;
            if (sw > 0 && sh > 0) { /* 像素正圆: rx == ry 即可, 这里保持 r */ }
        }
    }
    int left   = (int)(dcx - r);
    int top    = (int)(dcy - ry);
    int right  = (int)(dcx + r);
    int bottom = (int)(dcy + ry);

    COLORREF c = hasColor ? (COLORREF)color : vb6_DrawForeColor(hwnd);
    int w = vb6_DrawGetI(hw, L"VB6_DrawWidth", 1);
    if (w < 1) w = 1;
    HPEN pen = CreatePen(PS_SOLID, w, c);
    HGDIOBJ oldPen = pen ? SelectObject(d.dc, pen) : NULL;
    HGDIOBJ oldBr  = SelectObject(d.dc, GetStockObject(NULL_BRUSH));

    if (hasStart || hasEnd) {
        // 弧: VB6 给的是弧度, 且 0 在**正右**、逆时针为正 (VB6 的坐标系 y 向下,
        // 所以这里 y 取负 ⇒ 视觉上逆时针)。
        double a0 = hasStart ? startAngle : 0.0;
        double a1 = hasEnd ? endAngle : 6.283185307179586;
        int nseg = 64;
        int px = (int)(dcx + r * cos(a0));
        int py = (int)(dcy - ry * sin(a0));
        MoveToEx(d.dc, px, py, NULL);
        for (int i = 1; i <= nseg; i++) {
            double t = a0 + (a1 - a0) * ((double)i / nseg);
            px = (int)(dcx + r * cos(t));
            py = (int)(dcy - ry * sin(t));
            LineTo(d.dc, px, py);
        }
    } else {
        Ellipse(d.dc, left, top, right, bottom);
    }

    SelectObject(d.dc, oldBr);
    if (oldPen) SelectObject(d.dc, oldPen);
    if (pen) DeleteObject(pen);
    vb6_DrawRelease(&d, hwnd);
}

// ============================================================
// CLS —— 清掉绘图表面。Form 上等价于用 BackColor 填满客户区。
// ============================================================
void vb6_Form_Cls(void* hwnd) {
    HWND hw = (HWND)hwnd;
    if (!hw) return;
    vb6_draw_dc_t d = vb6_DrawAcquire(hwnd);
    if (!d.dc) return;
    RECT rc;
    GetClientRect(hw, &rc);
    int32_t bg = vb6_DrawGetI(hw, L"VB6_BackColor", -1);
    COLORREF c = (bg == -1) ? RGB(240, 240, 240)/*vbFormBackColor*/ : (COLORREF)bg;
    HBRUSH br = CreateSolidBrush(c);
    if (br) { FillRect(d.dc, &rc, br); DeleteObject(br); }
    vb6_DrawRelease(&d, hwnd);
    vb6_DrawSetCurX(hw, 0);
    vb6_DrawSetCurY(hw, 0);
}

// ============================================================
// 绘图状态读侧 (cgen 的 `Me.ForeColor` / `Me.DrawWidth` 打这里；笔位不在这一族 ——
// 账 #233: 读写两侧都登记在 cgen_util_ctrl.cpp 的那两张表里, 且**成对**)。
// 与上面各方法同源: 状态按 HWND 存窗口属性, 缺省兜底成 VB6 初始值。
// ============================================================
int32_t vb6_Form_DrawGetForeColor(void* hwnd) { return (int32_t)vb6_DrawForeColor(hwnd); }
int32_t vb6_Form_DrawGetWidth(void* hwnd)     { return vb6_DrawGetI(hwnd, L"VB6_DrawWidth", 1); }

// 写侧。**登记进 cgen 的写侧表才是这条通路生效的唯一办法** —— 赋值语句发 C 时先问
// getControlPropWriteFn(Form, 名), 没登记就退化成"把读函数当左值" (C2106, 实测)。
// 编码口径与读侧同一套: 存裸值, "没设过"由各自的缺省档 / 钳位负责 (见 vb6_DrawSetI)。
void vb6_Form_DrawSetForeColor(void* hwnd, int32_t c) { vb6_DrawSetForeColor(hwnd, c); }
void vb6_Form_DrawSetWidth(void* hwnd, int32_t w) {
    if (w < 1) w = 1;
    vb6_DrawSetI(hwnd, L"VB6_DrawWidth", w);
}

// ============================================================
// Printer 版: 同一批方法, 走 g_printerDC。
// 为什么不复用上面的 HWND 路径: Printer 的 DC 是全局的, 没有 HWND 可挂窗口属性,
// 状态改存静态变量 (与 vb6_Printer_CurrentX/Y 同一套全局风格)。
// ============================================================
void* vb6_Printer_hDC(void);   /* vb6rtl_system.c */

static int32_t g_prnDrawX = 0, g_prnDrawY = 0;
static int32_t g_prnDrawFg = -1;   /* -1 = 未设 ⇒ 黑 */
static int32_t g_prnDrawWidth = 1;

void vb6_Printer_PSet(int32_t step, int32_t hasXY, double x, double y,
                      int32_t hasColor, int32_t color) {
    HDC dc = (HDC)vb6_Printer_hDC();
    if (!dc) return;
    int px = hasXY ? (int)x : g_prnDrawX;
    int py = hasXY ? (int)y : g_prnDrawY;
    if (step) { px += g_prnDrawX; py += g_prnDrawY; }
    g_prnDrawX = hasXY ? px : g_prnDrawX;
    g_prnDrawY = hasXY ? py : g_prnDrawY;
    COLORREF c = hasColor ? (COLORREF)color
                          : (g_prnDrawFg >= 0 ? (COLORREF)g_prnDrawFg : RGB(0, 0, 0));
    SetPixelV(dc, px, py, c);
}

int32_t vb6_Printer_Point(double x, double y) {
    HDC dc = (HDC)vb6_Printer_hDC();
    if (!dc) return -1;
    return (int32_t)GetPixel(dc, (int)x, (int)y);
}

void vb6_Printer_Line(int32_t step1, int32_t has1, double x1, double y1,
                      int32_t step2, int32_t has2, double x2, double y2,
                      int32_t hasColor, int32_t color,
                      int32_t style) {
    HDC dc = (HDC)vb6_Printer_hDC();
    if (!dc) return;
    int ax = has1 ? (int)x1 : g_prnDrawX;
    int ay = has1 ? (int)y1 : g_prnDrawY;
    int bx = has2 ? (int)x2 : g_prnDrawX;
    int by = has2 ? (int)y2 : g_prnDrawY;
    if (step1) { ax += g_prnDrawX; ay += g_prnDrawY; }
    if (step2) { bx += g_prnDrawX; by += g_prnDrawY; }
    g_prnDrawX = has2 ? bx : ax;
    g_prnDrawY = has2 ? by : ay;
    COLORREF c = hasColor ? (COLORREF)color
                          : (g_prnDrawFg >= 0 ? (COLORREF)g_prnDrawFg : RGB(0, 0, 0));
    int w = g_prnDrawWidth < 1 ? 1 : g_prnDrawWidth;
    HPEN pen = CreatePen(PS_SOLID, w, c);
    HGDIOBJ oldPen = pen ? SelectObject(dc, pen) : NULL;
    // style 位值与 Form 版同一张表: B=1 矩形 / C=2 椭圆 / F=4 填充。
    if (style & 0x7) {
        HBRUSH br = (style & 0x4) ? CreateSolidBrush(c) : NULL;
        HGDIOBJ ob = br ? SelectObject(dc, br) : SelectObject(dc, GetStockObject(NULL_BRUSH));
        if (style & 0x2) Ellipse(dc, ax, ay, bx + 1, by + 1);
        else            Rectangle(dc, ax, ay, bx, by);
        if (br) { SelectObject(dc, ob); DeleteObject(br); }
    } else {
        MoveToEx(dc, ax, ay, NULL);
        LineTo(dc, bx, by);
    }
    if (oldPen) SelectObject(dc, oldPen);
    if (pen) DeleteObject(pen);
}

void vb6_Printer_Circle(int32_t step, int32_t hasXY, double x, double y,
                        double radius, int32_t hasColor, int32_t color,
                        int32_t hasStart, double startAngle,
                        int32_t hasEnd, double endAngle,
                        int32_t hasAspect, double aspect) {
    HDC dc = (HDC)vb6_Printer_hDC();
    if (!dc) return;
    int ccx = hasXY ? (int)x : g_prnDrawX;
    int ccy = hasXY ? (int)y : g_prnDrawY;
    if (step) { ccx += g_prnDrawX; ccy += g_prnDrawY; }
    g_prnDrawX = ccx; g_prnDrawY = ccy;
    if (radius < 0) radius = -radius;
    COLORREF c = hasColor ? (COLORREF)color
                          : (g_prnDrawFg >= 0 ? (COLORREF)g_prnDrawFg : RGB(0, 0, 0));
    int w = g_prnDrawWidth < 1 ? 1 : g_prnDrawWidth;
    double ry = (hasAspect && aspect > 0) ? radius / aspect : radius;
    HPEN pen = CreatePen(PS_SOLID, w, c);
    HGDIOBJ oldPen = pen ? SelectObject(dc, pen) : NULL;
    HGDIOBJ oldBr = SelectObject(dc, GetStockObject(NULL_BRUSH));
    if (hasStart || hasEnd) {
        double a0 = hasStart ? startAngle : 0.0;
        double a1 = hasEnd ? endAngle : 6.283185307179586;
        for (int i = 0; i <= 64; i++) {
            double t = a0 + (a1 - a0) * ((double)i / 64.0);
            int px = (int)(ccx + radius * cos(t));
            int py = (int)(ccy - ry * sin(t));
            if (i == 0) MoveToEx(dc, px, py, NULL); else LineTo(dc, px, py);
        }
    } else {
        Ellipse(dc, (int)(ccx - radius), (int)(ccy - ry),
                (int)(ccx + radius), (int)(ccy + ry));
    }
    SelectObject(dc, oldBr);
    if (oldPen) SelectObject(dc, oldPen);
    if (pen) DeleteObject(pen);
}

// Printer 绘图状态读写 (cgen 的 Printer.ForeColor / .DrawWidth / .CurrentX 会打这里)
int32_t vb6_Printer_DrawGetCurrentX(void) { return g_prnDrawX; }
int32_t vb6_Printer_DrawGetCurrentY(void) { return g_prnDrawY; }
void vb6_Printer_DrawSetCurrentX(int32_t x) { g_prnDrawX = x; }
void vb6_Printer_DrawSetCurrentY(int32_t y) { g_prnDrawY = y; }
int32_t vb6_Printer_DrawGetForeColor(void) { return g_prnDrawFg; }
void vb6_Printer_DrawSetForeColor(int32_t c) { g_prnDrawFg = c; }
int32_t vb6_Printer_DrawGetWidth(void) { return g_prnDrawWidth; }
void vb6_Printer_DrawSetWidth(int32_t w) { g_prnDrawWidth = w < 1 ? 1 : w; }
