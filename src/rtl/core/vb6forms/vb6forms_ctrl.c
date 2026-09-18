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

void* vb6_GetControlText(void* hwnd) {
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
    LRESULT state = SendMessageA((HWND)hwnd, BM_GETCHECK, 0, 0);
    return (int)state;  // BST_UNCHECKED=0, BST_CHECKED=1, BST_INDETERMINATE=2
}

void vb6_SetCheckValue(void* hwnd, int value) {
    if (!hwnd) return;
    SendMessageA((HWND)hwnd, BM_SETCHECK, (WPARAM)value, 0);
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
    return pt.x * 15;
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
    return pt.y * 15;
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
    return (rc.right - rc.left) * 15;
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
    return (rc.bottom - rc.top) * 15;
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
