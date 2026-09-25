// vb6forms_ctrl.c - vb6forms 模块拆分: 控件通用属性 + 字体 + 颜色
// 由 vb6forms.c 按控件/窗体功能家族拆分而来 (纯搬移, 零行为改动)

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#endif

#include "vb6forms.h"
#include "vb6forms_internal.h"
#include <stdio.h>
#include <stdarg.h>

#include <stdlib.h>   /* malloc, free */
#include <oleauto.h>  /* SysAllocString, BSTR */
#include <olectl.h>   /* IPicture, OleLoadPicture, OLE_HANDLE */


// ============================================================
// 控件属性读写 (P7.5)
// ============================================================

wchar_t* vb6_GetControlText(void* hwnd) {
    if (!hwnd) return NULL;
    HWND h = (HWND)hwnd;
    int len = GetWindowTextLengthW(h);
    if (len <= 0) {
        // 返回空BSTR
        WCHAR empty[] = {0};
        return SysAllocString(empty);
    }
    WCHAR* buf = (WCHAR*)malloc((len + 1) * sizeof(WCHAR));
    if (!buf) return NULL;
    GetWindowTextW(h, buf, len + 1);
    BSTR bstr = SysAllocString(buf);
    free(buf);
    return (void*)bstr;
}

void vb6_SetControlText(void* hwnd, void* bstr) {
    if (!hwnd) return;
    // 支持BSTR和char*两种输入
    if (bstr) {
        // 尝试作为BSTR处理 (VB6字符串)
        BSTR bs = (BSTR)bstr;
        SetWindowTextW((HWND)hwnd, bs);
    }
}

int vb6_GetCheckValue(void* hwnd) {
    if (!hwnd) return 0;
    LRESULT state = SendMessageW((HWND)hwnd, BM_GETCHECK, 0, 0);
    return (int)state;  // BST_UNCHECKED=0, BST_CHECKED=1, BST_INDETERMINATE=2
}

void vb6_SetCheckValue(void* hwnd, int value) {
    if (!hwnd) return;
    SendMessageW((HWND)hwnd, BM_SETCHECK, (WPARAM)value, 0);
}

int vb6_GetControlVisible(void* hwnd) {
    if (!hwnd) return 0;
    return IsWindowVisible((HWND)hwnd) ? -1 : 0;  // VB6: True=-1
}

void vb6_SetControlVisible(void* hwnd, int visible) {
    if (!hwnd) return;
    ShowWindow((HWND)hwnd, visible ? SW_SHOW : SW_HIDE);
}

int vb6_GetControlEnabled(void* hwnd) {
    if (!hwnd) return 0;
    return IsWindowEnabled((HWND)hwnd) ? -1 : 0;  // VB6: True=-1
}

void vb6_SetControlEnabled(void* hwnd, int enabled) {
    if (!hwnd) return;
    EnableWindow((HWND)hwnd, enabled ? TRUE : FALSE);
}
// Position/size properties use twips on both reads and writes.
// Codegen calls these getters directly without pixel-to-twip conversion.
// Match the existing vb6_TwipToX/Y setters (15 twips per logical pixel).
// Returning pixels here makes Form_Resize mix ScaleWidth/Height twips with
// pixel margins, pushing stretched Image controls outside the parent client.
int vb6_GetControlLeft(void* hwnd) {
    if (!hwnd) return 0;
    RECT rc;
    GetWindowRect((HWND)hwnd, &rc);
    POINT pt = { rc.left, rc.top };
    ScreenToClient(GetParent((HWND)hwnd), &pt);
    return vb6_XToTwipX(pt.x);
}

void vb6_SetControlLeft(void* hwnd, int left) {
    if (!hwnd) return;
    RECT rc;
    GetWindowRect((HWND)hwnd, &rc);
    POINT pt = { rc.left, rc.top };
    ScreenToClient(GetParent((HWND)hwnd), &pt);
    SetWindowPos((HWND)hwnd, NULL, vb6_TwipToX(left), pt.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

int vb6_GetControlTop(void* hwnd) {
    if (!hwnd) return 0;
    RECT rc;
    GetWindowRect((HWND)hwnd, &rc);
    POINT pt = { rc.left, rc.top };
    ScreenToClient(GetParent((HWND)hwnd), &pt);
    return vb6_YToTwipY(pt.y);
}

void vb6_SetControlTop(void* hwnd, int top) {
    if (!hwnd) return;
    RECT rc;
    GetWindowRect((HWND)hwnd, &rc);
    POINT pt = { rc.left, rc.top };
    ScreenToClient(GetParent((HWND)hwnd), &pt);
    SetWindowPos((HWND)hwnd, NULL, pt.x, vb6_TwipToY(top), 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

int vb6_GetControlWidth(void* hwnd) {
    if (!hwnd) return 0;
    RECT rc;
    GetWindowRect((HWND)hwnd, &rc);
    return vb6_XToTwipX(rc.right - rc.left);
}

void vb6_SetControlWidth(void* hwnd, int width) {
    if (!hwnd) return;
    RECT rc;
    GetWindowRect((HWND)hwnd, &rc);
    SetWindowPos((HWND)hwnd, NULL, 0, 0, vb6_TwipToX(width), rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);
}

int vb6_GetControlHeight(void* hwnd) {
    if (!hwnd) return 0;
    RECT rc;
    GetWindowRect((HWND)hwnd, &rc);
    return vb6_YToTwipY(rc.bottom - rc.top);
}

void vb6_SetControlHeight(void* hwnd, int height) {
    if (!hwnd) return;
    RECT rc;
    GetWindowRect((HWND)hwnd, &rc);
    SetWindowPos((HWND)hwnd, NULL, 0, 0, rc.right - rc.left, vb6_TwipToY(height), SWP_NOMOVE | SWP_NOZORDER);
}

// P11.8: hWnd attribute (read-only)
void* vb6_GetControlHwnd(void* hwnd) {
    return hwnd;  // Already the HWND
}

// ============================================================
// P13.1: Font properties
// ============================================================

// Helper: get LOGFONT from control's current font
static int vb6_GetControlLogFont(void* hwnd, LOGFONTW* plf) {
    if (!hwnd || !plf) return 0;
    HFONT hFont = (HFONT)SendMessageW((HWND)hwnd, WM_GETFONT, 0, 0);
    if (!hFont) return 0;
    return GetObjectW(hFont, sizeof(LOGFONTW), plf) > 0;
}

// Helper: create new font from modified LOGFONT and set it on control
// Also deletes the old font if it was created by us (we track via prop)
static void vb6_SetControlFontFromLogFont(void* hwnd, const LOGFONTW* plf) {
    if (!hwnd || !plf) return;
    HFONT hNewFont = CreateFontIndirectW(plf);
    if (!hNewFont) return;
    HFONT hOldFont = (HFONT)SendMessageW((HWND)hwnd, WM_GETFONT, 0, 0);
    SendMessageW((HWND)hwnd, WM_SETFONT, (WPARAM)hNewFont, (LPARAM)TRUE);
    // Force redraw
    InvalidateRect((HWND)hwnd, NULL, TRUE);
    // Delete old font only if it's not a stock font
    if (hOldFont && GetObjectType(hOldFont) == OBJ_FONT) {
        // Safe to delete non-stock fonts; stock fonts have OBJ_FONT but
        // DeleteObject on stock fonts is a no-op, so it's safe
        DeleteObject(hOldFont);
    }
}

void* vb6_GetControlFontName(void* hwnd) {
    LOGFONTW lf;
    if (!vb6_GetControlLogFont(hwnd, &lf)) {
        WCHAR empty[] = {0};
        return SysAllocString(empty);
    }
    return SysAllocString(lf.lfFaceName);
}

void vb6_SetControlFontName(void* hwnd, void* bstrName) {
    if (!hwnd || !bstrName) return;
    LOGFONTW lf;
    if (!vb6_GetControlLogFont(hwnd, &lf)) {
        // No existing font, create a default LOGFONT
        memset(&lf, 0, sizeof(lf));
        lf.lfHeight = -13;  // Default ~10pt
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
        lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
        lf.lfQuality = DEFAULT_QUALITY;
        lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    }
    BSTR bs = (BSTR)bstrName;
    int len = SysStringLen(bs);
    if (len > LF_FACESIZE - 1) len = LF_FACESIZE - 1;
    memcpy(lf.lfFaceName, bs, len * sizeof(WCHAR));
    lf.lfFaceName[len] = L'\0';
    vb6_SetControlFontFromLogFont(hwnd, &lf);
}

float vb6_GetControlFontSize(void* hwnd) {
    LOGFONTW lf;
    if (!vb6_GetControlLogFont(hwnd, &lf)) return 0.0f;
    HDC hdc = GetDC(NULL);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(NULL, hdc);
    if (dpi <= 0) dpi = 96;
    int heightPx = lf.lfHeight < 0 ? -lf.lfHeight : lf.lfHeight;
    return (float)heightPx * 72.0f / (float)dpi;
}

void vb6_SetControlFontSize(void* hwnd, float sizePt) {
    if (!hwnd) return;
    LOGFONTW lf;
    if (!vb6_GetControlLogFont(hwnd, &lf)) {
        memset(&lf, 0, sizeof(lf));
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
        lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
        lf.lfQuality = DEFAULT_QUALITY;
        lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    }
    HDC hdc = GetDC(NULL);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(NULL, hdc);
    if (dpi <= 0) dpi = 96;
    // Convert points to pixel height (negative for character height)
    lf.lfHeight = -(int)(sizePt * (float)dpi / 72.0f + 0.5f);
    vb6_SetControlFontFromLogFont(hwnd, &lf);
}

int vb6_GetControlFontBold(void* hwnd) {
    LOGFONTW lf;
    if (!vb6_GetControlLogFont(hwnd, &lf)) return 0;
    return (lf.lfWeight >= FW_BOLD) ? -1 : 0;  // VB6: True=-1
}

void vb6_SetControlFontBold(void* hwnd, int bold) {
    if (!hwnd) return;
    LOGFONTW lf;
    if (!vb6_GetControlLogFont(hwnd, &lf)) {
        memset(&lf, 0, sizeof(lf));
        lf.lfHeight = -13;
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
        lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
        lf.lfQuality = DEFAULT_QUALITY;
        lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    }
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    vb6_SetControlFontFromLogFont(hwnd, &lf);
}

int vb6_GetControlFontItalic(void* hwnd) {
    LOGFONTW lf;
    if (!vb6_GetControlLogFont(hwnd, &lf)) return 0;
    return lf.lfItalic ? -1 : 0;  // VB6: True=-1
}

void vb6_SetControlFontItalic(void* hwnd, int italic) {
    if (!hwnd) return;
    LOGFONTW lf;
    if (!vb6_GetControlLogFont(hwnd, &lf)) {
        memset(&lf, 0, sizeof(lf));
        lf.lfHeight = -13;
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
        lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
        lf.lfQuality = DEFAULT_QUALITY;
        lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    }
    lf.lfItalic = italic ? TRUE : FALSE;
    vb6_SetControlFontFromLogFont(hwnd, &lf);
}

int vb6_GetControlFontUnderline(void* hwnd) {
    LOGFONTW lf;
    if (!vb6_GetControlLogFont(hwnd, &lf)) return 0;
    return lf.lfUnderline ? -1 : 0;  // VB6: True=-1
}

void vb6_SetControlFontUnderline(void* hwnd, int underline) {
    if (!hwnd) return;
    LOGFONTW lf;
    if (!vb6_GetControlLogFont(hwnd, &lf)) {
        memset(&lf, 0, sizeof(lf));
        lf.lfHeight = -13;
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
        lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
        lf.lfQuality = DEFAULT_QUALITY;
        lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    }
    lf.lfUnderline = underline ? TRUE : FALSE;
    vb6_SetControlFontFromLogFont(hwnd, &lf);
}

int vb6_GetControlFontStrikethrough(void* hwnd) {
    LOGFONTW lf;
    if (!vb6_GetControlLogFont(hwnd, &lf)) return 0;
    return lf.lfStrikeOut ? -1 : 0;  // VB6: True=-1
}

void vb6_SetControlFontStrikethrough(void* hwnd, int strike) {
    if (!hwnd) return;
    LOGFONTW lf;
    if (!vb6_GetControlLogFont(hwnd, &lf)) {
        memset(&lf, 0, sizeof(lf));
        lf.lfHeight = -13;
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
        lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
        lf.lfQuality = DEFAULT_QUALITY;
        lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    }
    lf.lfStrikeOut = strike ? TRUE : FALSE;
    vb6_SetControlFontFromLogFont(hwnd, &lf);
}

// ============================================================
// P13.2: ForeColor/BackColor
// ============================================================

int vb6_GetControlForeColor(void* hwnd) {
    if (!hwnd) return 0;
    // For most controls, text color is set via WM_CTLCOLOR* parent handler
    // We store foreground color as a window property
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_ForeColor");
    if (hProp) return (int)(INT_PTR)hProp;
    return 0;  // Default black
}

void vb6_SetControlForeColor(void* hwnd, int color) {
    if (!hwnd) return;
    SetPropW((HWND)hwnd, L"VB6_ForeColor", (HANDLE)(INT_PTR)color);
    InvalidateRect((HWND)hwnd, NULL, TRUE);
}

int vb6_GetControlBackColor(void* hwnd) {
    if (!hwnd) return 0;
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_BackColor");
    if (hProp) return (int)(INT_PTR)hProp;
    return (int)(INT_PTR)GetSysColor(COLOR_BTNFACE);  // Default
}

void vb6_SetControlBackColor(void* hwnd, int color) {
    if (!hwnd) return;
    SetPropW((HWND)hwnd, L"VB6_BackColor", (HANDLE)(INT_PTR)color);
    InvalidateRect((HWND)hwnd, NULL, TRUE);
}

// ============================================================
// Fix 185: 控件级绘制入口 PictureBox.Print / PictureBox.Cls
// ============================================================
//
// DC 来源有两档：_Paint 派发时挂上的 VB6_PaintDC（BeginPaint/EndPaint 之间才有
// 效，绝不能 ReleaseDC），否则回落 GetDC。VB6 允许在非 _Paint 时机 Print，效果就
// 是画在屏幕上、下次重绘即消失，这里保持同样的宽松度。
// 绘制光标 (PrintX/PrintY) 存窗口属性，Cls 归零 —— 等价于 VB6 的当前绘制位置。

static HDC vb6_ControlPrintDC(HWND hw, BOOL* pFromPaint) {
    HDC hdc = (HDC)GetPropW(hw, L"VB6_PaintDC");
    *pFromPaint = (hdc != NULL) ? TRUE : FALSE;
    if (hdc) return hdc;
    return GetDC(hw);
}

void vb6_ControlCls(void* hwnd) {
    if (!hwnd) return;
    HWND hw = (HWND)hwnd;
    BOOL fromPaint = FALSE;
    HDC hdc = vb6_ControlPrintDC(hw, &fromPaint);
    if (!hdc) return;
    RECT rc;
    GetClientRect(hw, &rc);
    HBRUSH br = CreateSolidBrush((COLORREF)vb6_GetControlBackColor(hwnd));
    if (br) {
        FillRect(hdc, &rc, br);
        DeleteObject(br);
    }
    if (!fromPaint) ReleaseDC(hw, hdc);
    RemovePropW(hw, L"VB6_PrintX");
    RemovePropW(hw, L"VB6_PrintY");
}

void vb6_ControlPrint(void* hwnd, void* bstrText) {
    if (!hwnd) return;
    HWND hw = (HWND)hwnd;
    BSTR text = (BSTR)bstrText;
    // 未赋值的 As String 是 NULL BSTR，对 VB6 而言等价于 ""（空行 = 只推进光标）
    int len = text ? (int)SysStringLen(text) : 0;
    BOOL fromPaint = FALSE;
    HDC hdc = vb6_ControlPrintDC(hw, &fromPaint);
    if (!hdc) return;
    HFONT hFont = (HFONT)SendMessageW(hw, WM_GETFONT, 0, 0);
    HFONT hOld = hFont ? (HFONT)SelectObject(hdc, hFont) : NULL;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, (COLORREF)vb6_GetControlForeColor(hwnd));
    RECT rc;
    GetClientRect(hw, &rc);
    int x = (int)(INT_PTR)GetPropW(hw, L"VB6_PrintX");
    int y = (int)(INT_PTR)GetPropW(hw, L"VB6_PrintY");
    if (len > 0) {
        ExtTextOutW(hdc, x, y, ETO_CLIPPED, &rc, text, len, NULL);
    }
    TEXTMETRICW tm;
    int advance = 0;
    if (GetTextMetricsW(hdc, &tm)) advance = tm.tmHeight + tm.tmExternalLeading;
    // VB6 的 Print 行末换行：光标回到最左并下移一行
    SetPropW(hw, L"VB6_PrintX", (HANDLE)(INT_PTR)0);
    SetPropW(hw, L"VB6_PrintY", (HANDLE)(INT_PTR)(y + advance));
    if (hOld) SelectObject(hdc, hOld);
    if (!fromPaint) ReleaseDC(hw, hdc);
}


// ============================================================
// C29-9 / D6: CommonDialog —— 原生 comdlg32，**不再走 MSComDlg.OCX**
// ============================================================
// 为什么必须换：那个 OCX 只有 32 位，x64 进程里 CoCreateInstance 直接失败，于是
// 今天这枚控件在 64 位下是静默空转（029 §九 给了实测读数：属性读回全空、ShowOpen
// 不出现、退出码照旧 0）。
//
// 实现口径：
//   * 属性宿主 = 一枚自注册的不可见子窗口 `VB6_COMMONDIALOG`（0x0、不带 WS_VISIBLE）。
//     有了句柄，字符串/整数属性就照 DirListBox 那一族同一套 SetPropW 存法走，cgen 的
//     "readFn(hwnd)" / "writeFn(hwnd, v)" 形状完全不用特判。
//     整数一律存 val+1：SetPropW(hwnd, name, (HANDLE)0) 与"从没设过"不可分辨，
//     而 Flags / Color / Min 的合法取值域含 0（C29-1a 那条教训）。
//   * API 只经 LoadLibrary + GetProcAddress 取（照 vb6_di_com_stubs.c 里 GDI+ 那条例子）
//     => 不给工具链加新的 import lib 依赖。
//   * VB6 的 Filter 用竖线串，原生 OPENFILENAME 要 `\0` 分隔的成对表 —— 折叠集中在
//     vb6_CdFoldFilter 一处做；读回时原样返回竖线串（VB6 的读数口径）。
//   * 取消：CancelError=True 时报 32755（VB6 的 cdlCancel），且**不改**已有读数。

#include <commdlg.h>

// 本模块的头链没引 vb6rtl_array.h，而取消路径要报 VB6 的 32755 —— 不声明就会踩
// "隐式原型"那一刀（C29-1b 刚被咬过：返回指针的函数按 int 取）。照 vb6com_internal.h 同法补一句。
extern void vb6_RaiseError(int32_t errNum, void* description);

static HMODULE vb6_ComDlgModule(void) {
    static HMODULE hMod = NULL;
    if (!hMod) hMod = LoadLibraryW(L"comdlg32.dll");
    return hMod;
}

static const wchar_t* vb6_CdDupSrc(const wchar_t* s) { return s ? s : (const wchar_t*)L""; }

static wchar_t* vb6_CdDupStr(const wchar_t* s) {
    s = vb6_CdDupSrc(s);
    size_t n = wcslen(s) + 1;
    wchar_t* b = (wchar_t*)HeapAlloc(GetProcessHeap(), 0, n * sizeof(wchar_t));
    if (b) wcscpy_s(b, n, s);
    return b;
}

static wchar_t* vb6_CdGetStr(void* hwnd, const wchar_t* key) {
    if (!hwnd) return SysAllocString(L"");
    HANDLE h = GetPropW((HWND)hwnd, key);
    return SysAllocString(h ? (const wchar_t*)h : (const wchar_t*)L"");
}

static void vb6_CdSetStr(void* hwnd, const wchar_t* key, const wchar_t* v) {
    if (!hwnd) return;
    HANDLE old = GetPropW((HWND)hwnd, key);
    if (old) { RemovePropW((HWND)hwnd, key); HeapFree(GetProcessHeap(), 0, old); }
    SetPropW((HWND)hwnd, key, vb6_CdDupStr(v));
}

static int vb6_CdGetInt(void* hwnd, const wchar_t* key, int dflt) {
    if (!hwnd) return dflt;
    HANDLE h = GetPropW((HWND)hwnd, key);
    return h ? (int)(INT_PTR)h - 1 : dflt;
}

static void vb6_CdSetInt(void* hwnd, const wchar_t* key, int v) {
    if (!hwnd) return;
    SetPropW((HWND)hwnd, key, (HANDLE)(INT_PTR)(v + 1));
}

#define VB6_CD_STR(Name, Key)                                                \
    wchar_t* vb6_CdGet##Name(void* hwnd) { return vb6_CdGetStr(hwnd, Key); }  \
    void vb6_CdSet##Name(void* hwnd, wchar_t* v) { vb6_CdSetStr(hwnd, Key, v); }

#define VB6_CD_INT(Name, Key, Dflt)                                          \
    int vb6_CdGet##Name(void* hwnd) { return vb6_CdGetInt(hwnd, Key, Dflt); } \
    void vb6_CdSet##Name(void* hwnd, int v) { vb6_CdSetInt(hwnd, Key, v); }

VB6_CD_STR(Filter,      L"VB6_Cd_Filter")
VB6_CD_STR(FileName,    L"VB6_Cd_FileName")
VB6_CD_STR(FileTitle,   L"VB6_Cd_FileTitle")
VB6_CD_STR(DialogTitle, L"VB6_Cd_DialogTitle")
VB6_CD_STR(InitDir,     L"VB6_Cd_InitDir")
VB6_CD_STR(DefaultExt,  L"VB6_Cd_DefaultExt")
VB6_CD_STR(FontName,    L"VB6_Cd_FontName")
VB6_CD_INT(Flags,       L"VB6_Cd_Flags",       0)
// CancelError 是布尔：VB6 的 True 是 **-1**，而整数袋存的是 val+1（避开 SetPropW 存 0
// 与"从没设过"不可分辨那一坑）⇒ -1 会被存成 0、读回来变成 False。这里单独 normalize：
// 存 0/1 再 +1，读回按 VB6 口径给 0 / -1。
int vb6_CdGetCancelError(void* hwnd) {
    return vb6_CdGetInt(hwnd, L"VB6_Cd_CancelError", 0) ? -1 : 0;
}
void vb6_CdSetCancelError(void* hwnd, int v) {
    vb6_CdSetInt(hwnd, L"VB6_Cd_CancelError", v ? 1 : 0);
}

VB6_CD_INT(Color,       L"VB6_Cd_Color",       0)
VB6_CD_INT(Min,         L"VB6_Cd_Min",         0)
VB6_CD_INT(Max,         L"VB6_Cd_Max",         0)
VB6_CD_INT(Copies,      L"VB6_Cd_Copies",      1)
VB6_CD_INT(FontSize,    L"VB6_Cd_FontSize",    0)

#undef VB6_CD_STR
#undef VB6_CD_INT

// 自注册的不可见类。WndProc 什么都不做 —— 它只是属性袋，外加用 GetParent 拿模态父窗。
static LRESULT CALLBACK vb6_CdWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    return DefWindowProcW(h, m, w, l);
}

// C29-T: Timer 的身份类（同样不可见、同样只当句柄用 —— 计时器真正的状态在
// vb6forms.c 的 g_timerTable 里，按这个句柄找回那一格）。
static LRESULT CALLBACK vb6_TimerWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    return DefWindowProcW(h, m, w, l);
}

void vb6_RegisterTimerClass(void* hInstance) {
    static BOOL done = FALSE;
    if (done) return;
    WNDCLASSW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc   = vb6_TimerWndProc;
    wc.hInstance     = (HINSTANCE)hInstance;
    wc.lpszClassName = L"VB6_TIMER";
    if (RegisterClassW(&wc)) done = TRUE;
}

void vb6_RegisterCommDialogClass(void* hInstance) {
    static BOOL done = FALSE;
    if (done) return;
    WNDCLASSW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc   = vb6_CdWndProc;
    wc.hInstance     = (HINSTANCE)hInstance;
    wc.lpszClassName = L"VB6_COMMONDIALOG";
    if (RegisterClassW(&wc)) done = TRUE;
}

// VB6 "文本 (*.txt)|*.txt|所有文件 (*.*)|*.*" -> 原生成对表（每段以 \0 结束、整体再补一个 \0）
static void vb6_CdFoldFilter(const wchar_t* src, wchar_t* dst, size_t dstChars) {
    size_t o = 0;
    if (!src || !*src) { dst[0] = 0; dst[1] = 0; return; }
    const wchar_t* p = src;
    while (*p && o + 2 < dstChars) {
        const wchar_t* bar = wcschr(p, L'|');
        size_t len = bar ? (size_t)(bar - p) : wcslen(p);
        if (len > dstChars - o - 2) len = dstChars - o - 2;
        for (size_t i = 0; i < len; i++) dst[o++] = p[i];
        dst[o++] = 0;                       // 段结束
        if (!bar) break;                    // 落单的一段（VB6 要求描述与模式成对）
        p = bar + 1;
    }
    dst[o] = 0;                             // 表结束
}

static HWND vb6_CdOwner(void* hwnd) {
    HWND h = (HWND)hwnd;
    HWND p = h ? GetParent(h) : NULL;
    return p ? p : h;
}

static void vb6_CdCancel(void* hwnd) {
    if (vb6_CdGetCancelError(hwnd))
        vb6_RaiseError(32755, SysAllocString(L"Dialog was canceled by the user"));
}

static const wchar_t* vb6_CdBaseName(const wchar_t* full) {
    const wchar_t* s = wcsrchr(full, L'\\');
    if (!s) s = wcsrchr(full, L'/');
    return s ? s + 1 : full;
}

int vb6_CdShowFile(void* hwnd, int saveAs) {
    HMODULE m = vb6_ComDlgModule();
    if (!hwnd || !m) return 0;
    typedef BOOL (WINAPI *FnOFN)(LPOPENFILENAMEW);
    FnOFN pFn = (FnOFN)(void*)GetProcAddress(m, saveAs ? "GetSaveFileNameW" : "GetOpenFileNameW");
    if (!pFn) return 0;

    wchar_t filter[2048];
    wchar_t* filtVB = vb6_CdGetFilter(hwnd);
    vb6_CdFoldFilter(filtVB, filter, 2048);
    SysFreeString(filtVB);

    wchar_t file[1024] = {0};
    wchar_t title[1024] = {0};
    wchar_t initDir[1024] = {0};
    wchar_t defExt[64] = {0};
    wchar_t* s;
    s = vb6_CdGetFileName(hwnd);    wcsncpy_s(file, 1024, s, _TRUNCATE);  SysFreeString(s);
    s = vb6_CdGetDialogTitle(hwnd); wcsncpy_s(title, 1024, s, _TRUNCATE); SysFreeString(s);
    s = vb6_CdGetInitDir(hwnd);     wcsncpy_s(initDir, 1024, s, _TRUNCATE); SysFreeString(s);
    s = vb6_CdGetDefaultExt(hwnd);  wcsncpy_s(defExt, 64, s, _TRUNCATE);  SysFreeString(s);

    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = vb6_CdOwner(hwnd);
    ofn.lpstrFilter = filter[0] ? filter : NULL;
    ofn.lpstrFile   = file;
    ofn.nMaxFile    = 1024;
    ofn.lpstrInitialDir = initDir[0] ? initDir : NULL;
    ofn.lpstrDefExt     = defExt[0] ? defExt : NULL;
    ofn.lpstrTitle      = title[0] ? title : (saveAs ? L"Save As" : L"Open");
    ofn.Flags = (DWORD)vb6_CdGetFlags(hwnd) | OFN_EXPLORER | OFN_HIDEREADONLY;
    if (saveAs) ofn.Flags |= OFN_OVERWRITEPROMPT;

    if (!pFn(&ofn)) { vb6_CdCancel(hwnd); return 0; }
    vb6_CdSetStr(hwnd, L"VB6_Cd_FileName", file);
    vb6_CdSetStr(hwnd, L"VB6_Cd_FileTitle", vb6_CdBaseName(file));
    return 1;
}

int vb6_CdShowOpen(void* hwnd) { return vb6_CdShowFile(hwnd, 0); }
int vb6_CdShowSave(void* hwnd) { return vb6_CdShowFile(hwnd, 1); }

int vb6_CdShowColor(void* hwnd) {
    HMODULE m = vb6_ComDlgModule();
    if (!hwnd || !m) return 0;
    typedef BOOL (WINAPI *FnCC)(LPCHOOSECOLORW);
    FnCC pFn = (FnCC)(void*)GetProcAddress(m, "ChooseColorW");
    if (!pFn) return 0;
    static COLORREF g_cCustom[16] = {0};
    CHOOSECOLORW cc;
    ZeroMemory(&cc, sizeof(cc));
    cc.lStructSize  = sizeof(cc);
    cc.hwndOwner    = vb6_CdOwner(hwnd);
    cc.rgbResult    = (COLORREF)vb6_CdGetColor(hwnd);
    cc.lpCustColors = g_cCustom;
    cc.Flags        = (DWORD)vb6_CdGetFlags(hwnd) | CC_ANYCOLOR | CC_RGBINIT;
    if (!pFn(&cc)) { vb6_CdCancel(hwnd); return 0; }
    vb6_CdSetColor(hwnd, (int)cc.rgbResult);
    return 1;
}

int vb6_CdShowFont(void* hwnd) {
    HMODULE m = vb6_ComDlgModule();
    if (!hwnd || !m) return 0;
    typedef BOOL (WINAPI *FnCF)(LPCHOOSEFONTW);
    FnCF pFn = (FnCF)(void*)GetProcAddress(m, "ChooseFontW");
    if (!pFn) return 0;
    LOGFONTW lf;
    ZeroMemory(&lf, sizeof(lf));
    wchar_t face[64] = {0};
    wchar_t* s = vb6_CdGetFontName(hwnd);
    wcsncpy_s(face, 64, s, _TRUNCATE);
    SysFreeString(s);
    wcscpy_s(lf.lfFaceName, (size_t)_countof(lf.lfFaceName), face);

    CHOOSEFONTW cf;
    ZeroMemory(&cf, sizeof(cf));
    cf.lStructSize = sizeof(cf);
    cf.hwndOwner   = vb6_CdOwner(hwnd);
    cf.lpLogFont   = &lf;
    cf.iPointSize  = vb6_CdGetFontSize(hwnd) * 10;
    cf.Flags       = (DWORD)vb6_CdGetFlags(hwnd) | CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT;
    if (!pFn(&cf)) { vb6_CdCancel(hwnd); return 0; }
    vb6_CdSetStr(hwnd, L"VB6_Cd_FontName", lf.lfFaceName);
    vb6_CdSetFontSize(hwnd, (int)(cf.iPointSize / 10));
    return 1;
}

int vb6_CdShowPrinter(void* hwnd) {
    HMODULE m = vb6_ComDlgModule();
    if (!hwnd || !m) return 0;
    typedef BOOL (WINAPI *FnPD)(LPPRINTDLGW);
    FnPD pFn = (FnPD)(void*)GetProcAddress(m, "PrintDlgW");
    if (!pFn) return 0;
    PRINTDLGW pd;
    ZeroMemory(&pd, sizeof(pd));
    pd.lStructSize = sizeof(pd);
    pd.hwndOwner   = vb6_CdOwner(hwnd);
    pd.Flags       = (DWORD)vb6_CdGetFlags(hwnd) | PD_RETURNDC;
    pd.nCopies     = (WORD)vb6_CdGetCopies(hwnd);
    pd.nFromPage   = (WORD)vb6_CdGetMin(hwnd);
    pd.nToPage     = (WORD)vb6_CdGetMax(hwnd);
    if (!pFn(&pd)) { vb6_CdCancel(hwnd); return 0; }
    if (pd.hDC) DeleteDC(pd.hDC);           // v1 不把 DC 交给用户（Printer 对象另立批次）
    vb6_CdSetCopies(hwnd, (int)pd.nCopies);
    return 1;
}

int vb6_CdShowAbout(void* hwnd) {
    HMODULE m = vb6_ComDlgModule();
    if (!hwnd || !m) return 0;
    typedef BOOL (WINAPI *FnSA)(HWND, LPCWSTR, LPCWSTR, HICON);
    FnSA pFn = (FnSA)(void*)GetProcAddress(m, "ShellAboutW");
    if (!pFn) return 0;
    wchar_t* t = vb6_CdGetDialogTitle(hwnd);
    BOOL ok = pFn(vb6_CdOwner(hwnd), (t && *t) ? t : (const wchar_t*)L"About",
                  (const wchar_t*)L"", NULL);
    SysFreeString(t);
    if (!ok) vb6_CdCancel(hwnd);
    return ok ? 1 : 0;
}
