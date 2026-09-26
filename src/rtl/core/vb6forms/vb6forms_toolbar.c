// vb6forms_toolbar.c — ai/029 C29-5a: VB6 Toolbar 控件的窗口 + 标量属性 + 设计期按钮
//
// 复刻口径 (ai/内置控件/Toolbar 与 StatusBar 控件.md + D6): **不加载 MSCOMCTL.OCX**。
// 改之前这枚控件连窗口都没有 —— `controlTypeToWin32Class` 无格，且被
// `cgen_form_create_controls.inc` 里"ImageList || Toolbar 走 CoCreateInstance"那一组扣住
// (直接 continue)，于是 `vb6_hwnd_tb1` 压根不声明：实测读一个 `tb1.Visible` 就是
// **C2065: vb6_hwnd_tb1 未声明的标识符**，整件工程编不过 (x64 里那条 OCX 路本来就静默空转)。
// 本批把它从那组里**摘出来**换成原生 `ToolbarWindow32` (comctl32 注册类；
// vb6_ComCtl_Init 里的 ICC_BAR_CLASSES 早就覆盖了它，不需要新登记)。
//
// 本批范围 = 不吃 Buttons 集合语法的那三面:
//   ShowTips         Boolean, VB6 默认 True     → TBSTYLE_TOOLTIPS   (0x100)
//   TextStyle        0=文字在图标下方(默认) 1=在右侧 → TBSTYLE_LIST     (0x1000)
//   AllowCustomize   Boolean                    → CCS_ADJUSTABLE     (0x20)
//   Align            0..4 (VB6 默认 1 靠上)      → 属性袋 (停靠引擎还没接, 见下)
//   设计期 Buttons   逐条 TB_ADDBUTTONSW 真建进控件
//   留 5b (等成员对象机制, 与 TreeView 8b / ListView 同一套): `Buttons` 的 VB 侧读写
//   (Count / (i).Key / .Caption / .ToolTipText / Add / Remove)、`ButtonClick(Button As Button)`、
//   `ImageList` 关联 (没有它时按钮一律 I_IMAGENONE = 纯文字, 与 VB6 不配图标的观感一致)、
//   `Align` 的真停靠。
//
// 真值放哪: 前三条就是**窗口样式位** (getter 读 GWL_STYLE，读数与观感同源，与 C29-8a 同一口径)。
//   Align 是 VB6 侧概念、原生没有对应位 ⇒ 存窗口属性 (SetProp +1 偏移那套手法：
//   GetPropW 对"未设置"与"存了 0"都返回 NULL，不偏移就把 `Align = 0 不停靠` 当成没写)。
//   按钮自己那张表 (key/caption/style/image/width) 存在本文件的固定槽表里 —— 5b 的
//   Button 对象、将来的 ButtonClick 分发与 TB_GETBUTTONTEXT 那一路都要用它。

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#endif

#include "vb6forms.h"
#include "vb6forms_internal.h"

#ifdef _WIN32

// cgen 那侧按字面量发，这里也钉死数值 (低 _WIN32_IE 下 commctrl.h 不给全)。
#ifndef TBSTYLE_TOOLTIPS
#define TBSTYLE_TOOLTIPS   0x0100
#endif
#ifndef TBSTYLE_FLAT
#define TBSTYLE_FLAT       0x0800
#endif
#ifndef TBSTYLE_LIST
#define TBSTYLE_LIST       0x1000
#endif
#ifndef TBSTYLE_DROPDOWN
#define TBSTYLE_DROPDOWN   0x0080
#endif
#ifndef CCS_ADJUSTABLE
#define CCS_ADJUSTABLE     0x0020
#endif
#ifndef BTNS_BUTTON
#define BTNS_BUTTON        0x0000
#endif
#ifndef BTNS_SEP
#define BTNS_SEP           0x0001
#endif
#ifndef BTNS_CHECK
#define BTNS_CHECK         0x0002
#endif
#ifndef BTNS_GROUP
#define BTNS_GROUP         0x0004
#endif
#ifndef BTNS_DROPDOWN
#define BTNS_DROPDOWN      0x0008
#endif
#ifndef TBSTATE_ENABLED
#define TBSTATE_ENABLED    0x0004
#endif
#ifndef I_IMAGENONE
#define I_IMAGENONE        (-2)
#endif
// 工具栏的消息与结构体在低 _WIN32_IE 下**整组缺席** (实测 10.0.19041 SDK + 本仓的
// 编译档就是这样)：TB_RESET/TB_ADDSTRINGW/TB_ADDBUTTONSW/TBBUTTONW 全要自带。
// TBBUTTONW 的字段序照 SDK commctrl.h —— 布局错了就是"消息发出去了、按钮全是乱的"。
#ifndef TB_RESET
#define TB_RESET            (WM_USER + 37)
#endif
#ifndef TB_BUTTONCOUNT
#define TB_BUTTONCOUNT      (WM_USER + 24)
#endif
#ifndef TB_BUTTONSTRUCTSIZE
#define TB_BUTTONSTRUCTSIZE (WM_USER + 30)
#endif
#ifndef TB_ADDSTRINGW
#define TB_ADDSTRINGW       (WM_USER + 137)
#endif
#ifndef TB_ADDBUTTONSW
#define TB_ADDBUTTONSW      (WM_USER + 68)
#endif
#ifndef TB_INSERTBUTTONW
#define TB_INSERTBUTTONW    (WM_USER + 67)
#endif
#ifndef TB_AUTOSIZE
#define TB_AUTOSIZE         (WM_USER + 27)
#endif
#ifndef _TBBUTTON_DEFINED
#define _TBBUTTON_DEFINED
typedef struct _TBBUTTONW {
    INT_PTR  iBitmap;
    int      idCommand;
    BYTE     fsState;
    BYTE     fsStyle;
    BYTE     bReserved[2];
    ULONG_PTR dwData;
    INT_PTR  iString;
} TBBUTTONW, *PTBBUTTONW;
#endif

static const wchar_t kTbAlign[] = L"VB6_TB_Align";

#define VB6_TB_MAX        16      // 一窗体里的 Toolbar 枚数
#define VB6_TB_BTNS_MAX  128      // 一枚 Toolbar 的按钮数

typedef struct {
    wchar_t* key;
    wchar_t* caption;
    wchar_t* tooltip;
    int      style;      // VB6 那一套 0..5 (不是 BTNS_*)
    int      image;      // ImageList 索引, -1 = 无
    int      width;      // VB6 请求宽度 (缇), 分隔符直接用它的像素值
} Vb6TbBtn;

typedef struct {
    HWND    hwnd;
    int     btnCount;     // 表里的按钮数
    int     nNative;      // **已经发进控件**的按钮数 (与 btnCount 分开算, 见 AddButton 那段)
    Vb6TbBtn btn[VB6_TB_BTNS_MAX];
} Vb6Toolbar;

static Vb6Toolbar g_tbs[VB6_TB_MAX];
static int g_tbCount = 0;

static Vb6Toolbar* vb6_TbFind(HWND h) {
    for (int i = 0; i < g_tbCount; i++) if (g_tbs[i].hwnd == h) return &g_tbs[i];
    return NULL;
}

static Vb6Toolbar* vb6_TbEnsure(HWND h) {
    Vb6Toolbar* t = vb6_TbFind(h);
    if (t) return t;
    if (g_tbCount >= VB6_TB_MAX) return NULL;
    t = &g_tbs[g_tbCount++];
    ZeroMemory(t, sizeof(*t));
    t->hwnd = h;
    return t;
}

static wchar_t* vb6_TbDup(const wchar_t* s) {
    if (!s || !*s) return NULL;
    int n = lstrlenW(s);
    wchar_t* p = (wchar_t*)HeapAlloc(GetProcessHeap(), 0, (size_t)(n + 1) * sizeof(wchar_t));
    if (p) { CopyMemory(p, s, (size_t)n * sizeof(wchar_t)); p[n] = 0; }
    return p;
}

static DWORD vb6_TbStyle(void* hwnd) {
    return (DWORD)(UINT_PTR)GetWindowLongPtrW((HWND)hwnd, GWL_STYLE);
}

static void vb6_TbSetBit(void* hwnd, DWORD mask, int on) {
    if (!hwnd) return;
    DWORD cur = vb6_TbStyle(hwnd);
    DWORD next = on ? (cur | mask) : (cur & ~mask);
    if (next == cur) return;
    SetWindowLongPtrW((HWND)hwnd, GWL_STYLE, (LONG_PTR)next);
    // TBSTYLE_LIST 改的是"文字与图标的排布", 要让控件重算一遍按钮尺寸才见效。
    SendMessageW((HWND)hwnd, TB_AUTOSIZE, 0, 0);
    InvalidateRect((HWND)hwnd, NULL, TRUE);
}

// ---------------- ShowTips ----------------
void vb6_Toolbar_SetShowTips(void* hwnd, int32_t val) {
    vb6_TbSetBit(hwnd, TBSTYLE_TOOLTIPS, val != 0);
}
int32_t vb6_Toolbar_GetShowTips(void* hwnd) {
    if (!hwnd) return -1;                 // VB6 默认 True
    return (vb6_TbStyle(hwnd) & TBSTYLE_TOOLTIPS) ? -1 : 0;
}

// ---------------- TextStyle ----------------
// 0 = tbrTextBelow(默认): 文字在图标下方; 1 = tbrTextRight: 文字在右侧 (TBSTYLE_LIST)。
void vb6_Toolbar_SetTextStyle(void* hwnd, int32_t val) {
    vb6_TbSetBit(hwnd, TBSTYLE_LIST, val != 0);
}
int32_t vb6_Toolbar_GetTextStyle(void* hwnd) {
    if (!hwnd) return 0;
    return (vb6_TbStyle(hwnd) & TBSTYLE_LIST) ? 1 : 0;
}

// ---------------- AllowCustomize ----------------
// 原生 CCS_ADJUSTABLE 只在**创建时**起作用 (运行期拖拽把手的行为是控件在 WM_NCHITTEST
// 里定的)，这里改位是为了让读数与"用户写了什么"一致 —— 真自定义行为等 5b 一起看。
void vb6_Toolbar_SetAllowCustomize(void* hwnd, int32_t val) {
    vb6_TbSetBit(hwnd, CCS_ADJUSTABLE, val != 0);
}
int32_t vb6_Toolbar_GetAllowCustomize(void* hwnd) {
    if (!hwnd) return 0;
    return (vb6_TbStyle(hwnd) & CCS_ADJUSTABLE) ? -1 : 0;
}

// ---------------- Align ----------------
void vb6_Toolbar_SetAlign(void* hwnd, int32_t val) {
    if (!hwnd) return;
    SetPropW((HWND)hwnd, kTbAlign, (HANDLE)(INT_PTR)(val + 1));
}
int32_t vb6_Toolbar_GetAlign(void* hwnd) {
    if (!hwnd) return 0;
    HANDLE h = GetPropW((HWND)hwnd, kTbAlign);
    return h ? (int32_t)(INT_PTR)h - 1 : 1;    // VB6 默认 1 = 靠上
}

// ---------------- 设计期/运行期按钮 ----------------
// VB6 的 Button.Style → 原生 BTNS_*:
//   0 普通 / 1 复选 / 2 按钮组(单选) / 3 分隔符 / 4 占位符 / 5 下拉
static int vb6_TbNativeStyle(int vbStyle) {
    switch (vbStyle) {
        case 1:  return BTNS_CHECK;
        case 2:  return BTNS_GROUP;
        case 3:  return BTNS_SEP;
        case 4:  return BTNS_SEP;          // 占位符在原生里就是"带宽度的分隔符"
        case 5:  return BTNS_DROPDOWN;
        default: return BTNS_BUTTON;
    }
}

int vb6_Toolbar_AddButton(void* hwnd, int32_t index, const wchar_t* key, const wchar_t* caption,
                          int32_t style, int32_t image, const wchar_t* tooltip, int32_t width) {
    if (!hwnd) return 0;
    Vb6Toolbar* t = vb6_TbEnsure((HWND)hwnd);
    if (!t || t->btnCount >= VB6_TB_BTNS_MAX) return 0;

    // index <= 0 = 追加; VB6 的 Buttons.Add 第一形参就是"插到谁前面"的序号。
    int slot = (index > 0 && index - 1 < t->btnCount) ? (index - 1) : t->btnCount;
    if (slot < t->btnCount) {
        MoveMemory(&t->btn[slot + 1], &t->btn[slot],
                   (size_t)(t->btnCount - slot) * sizeof(Vb6TbBtn));
    }
    Vb6TbBtn* b = &t->btn[slot];
    b->key = vb6_TbDup(key); b->caption = vb6_TbDup(caption); b->tooltip = vb6_TbDup(tooltip);
    b->style = (int)style; b->image = (int)image; b->width = (int)width;
    t->btnCount++;

    int sep = ((int)style == 3 || (int)style == 4);
    // **v6 主题下的前置**: C3 产出的 exe 都嵌了 Common-Controls 6.0 的 manifest
    // (driver_link.cpp:780)，而 v6 的工具栏在**没被告知结构体尺寸**之前会收下
    // TB_ADDBUTTONSW 的 TRUE 返回值却一个按钮都不加 (实测: 表里 3 条、TB_BUTTONCOUNT=0，
    // 而不嵌 manifest 的独立 C 探针 (v5) 同一串消息是 1/2/3)。这条幂等、每次一发而已。
    SendMessageW((HWND)hwnd, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTONW), 0);
    TBBUTTONW bt;
    ZeroMemory(&bt, sizeof(bt));
    bt.iBitmap   = sep ? (INT_PTR)width                       // 分隔符: iBitmap 就是宽度
                       : ((image >= 0) ? (INT_PTR)image : (INT_PTR)I_IMAGENONE);
    bt.idCommand = (int)(slot + 1);                            // 1 基: 5b 的 ButtonClick 靠它回推
    bt.fsStyle   = (BYTE)vb6_TbNativeStyle((int)style);
    bt.fsState   = sep ? (BYTE)0 : (BYTE)TBSTATE_ENABLED;
    // 文字进控件自己的字符串表, iString 存它给的偏移 (分隔符不配图)。
    bt.iString   = sep ? 0
        : (INT_PTR)SendMessageW((HWND)hwnd, TB_ADDSTRINGW, 0,
                                (LPARAM)(caption ? caption : L""));

    // 只往控件上发这一条 —— **不能先 TB_RESET 再整表重发**: 实测那样 `TB_ADDBUTTONSW`
    // 返回 TRUE 却一个按钮都不进 (`.build/tbprobe` 那件 C 探针: add=1 而 count=0)，
    // 因为 reset 连字符串表一起清了、随后重发的 iString 全是旧偏移。
    LRESULT ok = (slot >= t->nNative)
        ? SendMessageW((HWND)hwnd, TB_ADDBUTTONSW, 1, (LPARAM)&bt)
        : SendMessageW((HWND)hwnd, TB_INSERTBUTTONW, (WPARAM)slot, (LPARAM)&bt);
    if (slot >= t->nNative) t->nNative = slot + 1;
    InvalidateRect((HWND)hwnd, NULL, TRUE);
    return ok ? -1 : 0;
}

// 原生侧的按钮数 —— 判据用它证"设计期那几条真进了控件"（VB 侧的 Buttons.Count 属于 5b）。
int32_t vb6_Toolbar_GetButtonCount(void* hwnd) {
    if (!hwnd) return 0;
    return (int32_t)(INT_PTR)SendMessageW((HWND)hwnd, TB_BUTTONCOUNT, 0, 0);
}

// ---------------- 设计期初值 ----------------
// 哨兵 -999 = .frm 里没写 (与 C29-8a 同一口径：不能用 -1，VB6 的 True 就是 -1)。
void vb6_Toolbar_Init(void* hwnd, int32_t showTips, int32_t textStyle,
                      int32_t allowCustomize, int32_t align) {
    if (!hwnd) return;
    if (showTips       != -999) vb6_Toolbar_SetShowTips(hwnd, showTips);
    if (textStyle      != -999) vb6_Toolbar_SetTextStyle(hwnd, textStyle);
    if (allowCustomize != -999) vb6_Toolbar_SetAllowCustomize(hwnd, allowCustomize);
    if (align          != -999) vb6_Toolbar_SetAlign(hwnd, align);
}

#endif // _WIN32
