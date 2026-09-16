// vb6forms_widget.c - vb6forms 模块拆分: 控件子类化 + 窗体属性 + Label/Button/Menu/Shape 属性 + 绘图属性
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
// P18-F: 控件子类化基础设施
// ============================================================

// 通用控件子类化安装 (复用VB6_OrigProc属性模式)
void vb6_InstallControlSubclass(void* hwnd, void* subclassProc) {
    if (!hwnd) return;
    HWND hw = (HWND)hwnd;
    /* Only install once */
    if (GetPropW(hw, L"VB6_OrigProc")) return;
    WNDPROC origProc = (WNDPROC)SetWindowLongPtrW(hw, GWLP_WNDPROC, (LONG_PTR)subclassProc);
    if (origProc) SetPropW(hw, L"VB6_OrigProc", (HANDLE)origProc);
}

// 获取原始窗口过程 (子类化Proc内调用CallWindowProc用)
void* vb6_GetOriginalWndProc(void* hwnd) {
    if (!hwnd) return NULL;
    return (void*)GetPropW((HWND)hwnd, L"VB6_OrigProc");
}

// 移除控件子类化 (WM_DESTROY时调用)
void vb6_RemoveControlSubclass(void* hwnd) {
    if (!hwnd) return;
    HWND hw = (HWND)hwnd;
    WNDPROC origProc = (WNDPROC)GetPropW(hw, L"VB6_OrigProc");
    if (origProc) {
        SetWindowLongPtrW(hw, GWLP_WNDPROC, (LONG_PTR)origProc);
        RemovePropW(hw, L"VB6_OrigProc");
    }
    RemovePropW(hw, L"VB6_MouseTracked");
}

// 启动鼠标跟踪 (TrackMouseEvent封装, 用于MouseEnter/MouseLeave)
void vb6_StartMouseTracking(void* hwnd) {
    if (!hwnd) return;
    TRACKMOUSEEVENT tme;
    tme.cbSize = sizeof(tme);
    tme.dwFlags = TME_LEAVE | TME_HOVER;
    tme.hwndTrack = (HWND)hwnd;
    tme.dwHoverTime = HOVER_DEFAULT;
    TrackMouseEvent(&tme);
}

// ============================================================
// P20-40: Form属性 (KeyPreview/WindowState/ControlBox/MaxButton/MinButton)
// ============================================================

int32_t vb6_GetKeyPreview(void* hwnd) {
    if (!hwnd) return 0;
    return GetPropW((HWND)hwnd, L"VB6_KeyPreview") ? -1 : 0;
}

void vb6_SetKeyPreview(void* hwnd, int32_t val) {
    if (!hwnd) return;
    if (val) SetPropW((HWND)hwnd, L"VB6_KeyPreview", (HANDLE)1);
    else RemovePropW((HWND)hwnd, L"VB6_KeyPreview");
}

int32_t vb6_GetWindowState(void* hwnd) {
    if (!hwnd) return 0;
    WINDOWPLACEMENT wp;
    wp.length = sizeof(wp);
    if (GetWindowPlacement((HWND)hwnd, &wp)) {
        switch (wp.showCmd) {
            case SW_SHOWMINIMIZED: return 1;
            case SW_SHOWMAXIMIZED: return 2;
            default: return 0;
        }
    }
    return 0;
}

void vb6_SetWindowState(void* hwnd, int32_t val) {
    if (!hwnd) return;
    switch (val) {
        case 0: ShowWindow((HWND)hwnd, SW_SHOWNORMAL); break;
        case 1: ShowWindow((HWND)hwnd, SW_MINIMIZE); break;
        case 2: ShowWindow((HWND)hwnd, SW_SHOWMAXIMIZED); break;
    }
}

int32_t vb6_GetScaleWidth(void* hwnd) {
    if (!hwnd) return 0;
    RECT rc;
    if (GetClientRect((HWND)hwnd, &rc)) {
        /* VB6 ScaleWidth: client width in twips (1 twip = 1/1440 inch) */
        /* TwipsPerPixelX is typically 15 at 96 DPI */
        int32_t twipsPerPixel = (int32_t)(1440.0 / GetDeviceCaps(GetDC(NULL), LOGPIXELSX));
        return (int32_t)(rc.right - rc.left) * twipsPerPixel;
    }
    return 0;
}

int32_t vb6_GetScaleHeight(void* hwnd) {
    if (!hwnd) return 0;
    RECT rc;
    if (GetClientRect((HWND)hwnd, &rc)) {
        /* VB6 ScaleHeight: client height in twips */
        int32_t twipsPerPixel = (int32_t)(1440.0 / GetDeviceCaps(GetDC(NULL), LOGPIXELSY));
        return (int32_t)(rc.bottom - rc.top) * twipsPerPixel;
    }
    return 0;
}

int32_t vb6_GetControlBox(void* hwnd) {
    if (!hwnd) return -1;
    LONG style = GetWindowLongW((HWND)hwnd, GWL_STYLE);
    return (style & WS_SYSMENU) ? -1 : 0;
}

void vb6_SetControlBox(void* hwnd, int32_t val) {
    if (!hwnd) return;
    LONG style = GetWindowLongW((HWND)hwnd, GWL_STYLE);
    if (val) style |= WS_SYSMENU;
    else style &= ~WS_SYSMENU;
    SetWindowLongW((HWND)hwnd, GWL_STYLE, style);
    SetWindowPos((HWND)hwnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
}

int32_t vb6_GetMaxButton(void* hwnd) {
    if (!hwnd) return -1;
    LONG style = GetWindowLongW((HWND)hwnd, GWL_STYLE);
    return (style & WS_MAXIMIZEBOX) ? -1 : 0;
}

void vb6_SetMaxButton(void* hwnd, int32_t val) {
    if (!hwnd) return;
    LONG style = GetWindowLongW((HWND)hwnd, GWL_STYLE);
    if (val) style |= WS_MAXIMIZEBOX;
    else style &= ~WS_MAXIMIZEBOX;
    SetWindowLongW((HWND)hwnd, GWL_STYLE, style);
    SetWindowPos((HWND)hwnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
}

int32_t vb6_GetMinButton(void* hwnd) {
    if (!hwnd) return -1;
    LONG style = GetWindowLongW((HWND)hwnd, GWL_STYLE);
    return (style & WS_MINIMIZEBOX) ? -1 : 0;
}

void vb6_SetMinButton(void* hwnd, int32_t val) {
    if (!hwnd) return;
    LONG style = GetWindowLongW((HWND)hwnd, GWL_STYLE);
    if (val) style |= WS_MINIMIZEBOX;
    else style &= ~WS_MINIMIZEBOX;
    SetWindowLongW((HWND)hwnd, GWL_STYLE, style);
    SetWindowPos((HWND)hwnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
}

// ============================================================
// P20-39: Label属性 (AutoSize/WordWrap/BackStyle)
// ============================================================

int32_t vb6_GetLabelAutoSize(void* hwnd) {
    if (!hwnd) return 0;
    return GetPropW((HWND)hwnd, L"VB6_AutoSize") ? -1 : 0;
}

void vb6_SetLabelAutoSize(void* hwnd, int32_t val) {
    if (!hwnd) return;
    if (val) {
        SetPropW((HWND)hwnd, L"VB6_AutoSize", (HANDLE)1);
        HDC hdc = GetDC((HWND)hwnd);
        char text[1024] = {0};
        GetWindowTextA((HWND)hwnd, text, sizeof(text));
        HFONT hFont = (HFONT)SendMessageW((HWND)hwnd, WM_GETFONT, 0, 0);
        HFONT hOld = (HFONT)SelectObject(hdc, hFont);
        SIZE sz;
        GetTextExtentPoint32A(hdc, text, (int)strlen(text), &sz);
        SelectObject(hdc, hOld);
        ReleaseDC((HWND)hwnd, hdc);
        SetWindowPos((HWND)hwnd, NULL, 0, 0, sz.cx + 4, sz.cy + 2, SWP_NOMOVE | SWP_NOZORDER);
    } else {
        RemovePropW((HWND)hwnd, L"VB6_AutoSize");
    }
}

int32_t vb6_GetLabelWordWrap(void* hwnd) {
    if (!hwnd) return 0;
    return GetPropW((HWND)hwnd, L"VB6_WordWrap") ? -1 : 0;
}

void vb6_SetLabelWordWrap(void* hwnd, int32_t val) {
    if (!hwnd) return;
    if (val) SetPropW((HWND)hwnd, L"VB6_WordWrap", (HANDLE)1);
    else RemovePropW((HWND)hwnd, L"VB6_WordWrap");
}

int32_t vb6_GetLabelBackStyle(void* hwnd) {
    if (!hwnd) return 1;
    return GetPropW((HWND)hwnd, L"VB6_BackStyle0") ? 0 : 1;
}

void vb6_SetLabelBackStyle(void* hwnd, int32_t val) {
    if (!hwnd) return;
    if (val == 0) {
        SetPropW((HWND)hwnd, L"VB6_BackStyle0", (HANDLE)1);
        LONG exStyle = GetWindowLongW((HWND)hwnd, GWL_EXSTYLE);
        exStyle |= WS_EX_TRANSPARENT;
        SetWindowLongW((HWND)hwnd, GWL_EXSTYLE, exStyle);
        InvalidateRect((HWND)hwnd, NULL, TRUE);
    } else {
        RemovePropW((HWND)hwnd, L"VB6_BackStyle0");
        LONG exStyle = GetWindowLongW((HWND)hwnd, GWL_EXSTYLE);
        exStyle &= ~WS_EX_TRANSPARENT;
        SetWindowLongW((HWND)hwnd, GWL_EXSTYLE, exStyle);
        InvalidateRect((HWND)hwnd, NULL, TRUE);
    }
}

// ============================================================
// P20-38: Button Style属性 (0=Standard, 1=Graphical)
// ============================================================

int32_t vb6_GetButtonStyle(void* hwnd) {
    if (!hwnd) return 0;
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_ButtonStyle");
    if (hProp) return (int32_t)(INT_PTR)hProp;
    return 0;  // Default: Standard
}

void vb6_SetButtonStyle(void* hwnd, int32_t val) {
    if (!hwnd) return;
    SetPropW((HWND)hwnd, L"VB6_ButtonStyle", (HANDLE)(INT_PTR)val);
    if (val == 1) {
        // Graphical style: make button flat/owner-draw visual
        LONG style = GetWindowLongW((HWND)hwnd, GWL_STYLE);
        style |= BS_BITMAP;
        SetWindowLongW((HWND)hwnd, GWL_STYLE, style);
        InvalidateRect((HWND)hwnd, NULL, TRUE);
    } else {
        // Standard style: remove bitmap style
        LONG style = GetWindowLongW((HWND)hwnd, GWL_STYLE);
        style &= ~BS_BITMAP;
        SetWindowLongW((HWND)hwnd, GWL_STYLE, style);
        InvalidateRect((HWND)hwnd, NULL, TRUE);
    }
}

// ============================================================
// P20-36: Menu属性 (Caption/Checked/Enabled/Visible)
// ============================================================

void* vb6_GetMenuCaption(void* menuHandle, int menuId) {
    if (!menuHandle) return SysAllocString(L"");
    MENUITEMINFOW mii;
    memset(&mii, 0, sizeof(mii));
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_STRING;
    mii.dwTypeData = NULL;
    if (!GetMenuItemInfoW((HMENU)menuHandle, (UINT)menuId, FALSE, &mii)) {
        return SysAllocString(L"");
    }
    if (mii.cch == 0) return SysAllocString(L"");
    WCHAR* buf = (WCHAR*)malloc((mii.cch + 1) * sizeof(WCHAR));
    if (!buf) return SysAllocString(L"");
    mii.dwTypeData = buf;
    mii.cch++;
    GetMenuItemInfoW((HMENU)menuHandle, (UINT)menuId, FALSE, &mii);
    BSTR result = SysAllocString(buf);
    free(buf);
    return result;
}

void vb6_SetMenuCaption(void* menuHandle, int menuId, void* bstrCaption) {
    if (!menuHandle || !bstrCaption) return;
    MENUITEMINFOW mii;
    memset(&mii, 0, sizeof(mii));
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_STRING;
    mii.dwTypeData = (BSTR)bstrCaption;
    SetMenuItemInfoW((HMENU)menuHandle, (UINT)menuId, FALSE, &mii);
    // Redraw menu bar
    HWND hwndOwner = NULL;
    if (menuHandle) {
        MENUBARINFO mbi;
        mbi.cbSize = sizeof(mbi);
        // Try to find the owner window - not easily available from HMENU
        // Just let the next paint refresh it
    }
}

int32_t vb6_GetMenuChecked(void* menuHandle, int menuId) {
    if (!menuHandle) return 0;
    MENUITEMINFOW mii;
    memset(&mii, 0, sizeof(mii));
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_STATE;
    if (!GetMenuItemInfoW((HMENU)menuHandle, (UINT)menuId, FALSE, &mii)) return 0;
    return (mii.fState & MFS_CHECKED) ? -1 : 0;
}

void vb6_SetMenuChecked(void* menuHandle, int menuId, int32_t val) {
    if (!menuHandle) return;
    CheckMenuItem((HMENU)menuHandle, (UINT)menuId, val ? MF_CHECKED : MF_UNCHECKED);
}

int32_t vb6_GetMenuEnabled(void* menuHandle, int menuId) {
    if (!menuHandle) return -1;
    MENUITEMINFOW mii;
    memset(&mii, 0, sizeof(mii));
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_STATE;
    if (!GetMenuItemInfoW((HMENU)menuHandle, (UINT)menuId, FALSE, &mii)) return -1;
    return (mii.fState & MFS_DISABLED) ? 0 : -1;
}

void vb6_SetMenuEnabled(void* menuHandle, int menuId, int32_t val) {
    if (!menuHandle) return;
    EnableMenuItem((HMENU)menuHandle, (UINT)menuId, val ? MF_ENABLED : (MF_DISABLED | MF_GRAYED));
}

int32_t vb6_GetMenuVisible(void* menuHandle, int menuId) {
    if (!menuHandle) return -1;
    MENUITEMINFOW mii;
    memset(&mii, 0, sizeof(mii));
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_STATE;
    if (!GetMenuItemInfoW((HMENU)menuHandle, (UINT)menuId, FALSE, &mii)) return -1;
    return (mii.fState & MFS_DEFAULT) ? 0 : -1;  // Use MS_HIDEMENU concept
}

void vb6_SetMenuVisible(void* menuHandle, int menuId, int32_t val) {
    if (!menuHandle) return;
    // Win32 doesn't have a native "hide menu item" - we remove/insert
    // For simplicity, use MF_BYCOMMAND with DeleteMenu/InsertMenu
    // But that's too destructive. Store visibility as property.
    // Use RemoveMenu/DeleteMenu only in offline scenario
    // For now: just toggle the state flag
    if (!val) {
        DeleteMenu((HMENU)menuHandle, (UINT)menuId, MF_BYCOMMAND);
    }
    // Note: making a menu item reappear after deletion is not possible
    // without recreating it. This is a known limitation.
}

// ============================================================
// P20-34: Shape属性 (Shape/BorderWidth/BorderStyle/FillStyle)
// ============================================================

int32_t vb6_GetShapeType(void* hwnd) {
    if (!hwnd) return 0;
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_ShapeType");
    if (hProp) return (int32_t)(INT_PTR)hProp;
    return 0;  // Default: Rectangle
}

void vb6_SetShapeType(void* hwnd, int32_t val) {
    if (!hwnd) return;
    SetPropW((HWND)hwnd, L"VB6_ShapeType", (HANDLE)(INT_PTR)val);
    InvalidateRect((HWND)hwnd, NULL, TRUE);
}

int32_t vb6_GetShapeBorderWidth(void* hwnd) {
    if (!hwnd) return 1;
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_ShapeBorderWidth");
    if (hProp) return (int32_t)(INT_PTR)hProp;
    return 1;  // Default: 1
}

void vb6_SetShapeBorderWidth(void* hwnd, int32_t val) {
    if (!hwnd) return;
    if (val < 1) val = 1;
    if (val > 8192) val = 8192;
    SetPropW((HWND)hwnd, L"VB6_ShapeBorderWidth", (HANDLE)(INT_PTR)val);
    InvalidateRect((HWND)hwnd, NULL, TRUE);
}

int32_t vb6_GetShapeBorderStyle(void* hwnd) {
    if (!hwnd) return 1;
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_ShapeBorderStyle");
    if (hProp) return (int32_t)(INT_PTR)hProp;
    return 1;  // Default: Solid
}

void vb6_SetShapeBorderStyle(void* hwnd, int32_t val) {
    if (!hwnd) return;
    SetPropW((HWND)hwnd, L"VB6_ShapeBorderStyle", (HANDLE)(INT_PTR)val);
    InvalidateRect((HWND)hwnd, NULL, TRUE);
}

int32_t vb6_GetShapeFillStyle(void* hwnd) {
    if (!hwnd) return 1;
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_ShapeFillStyle");
    if (hProp) return (int32_t)(INT_PTR)hProp;
    return 1;  // Default: Transparent
}

void vb6_SetShapeFillStyle(void* hwnd, int32_t val) {
    if (!hwnd) return;
    SetPropW((HWND)hwnd, L"VB6_ShapeFillStyle", (HANDLE)(INT_PTR)val);
    InvalidateRect((HWND)hwnd, NULL, TRUE);
}

// ============================================================
// P20-42: PictureBox图形属性 (AutoRedraw/ScaleMode/CurrentX/Y)
// ============================================================

int32_t vb6_GetAutoRedraw(void* hwnd) {
    if (!hwnd) return 0;
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_AutoRedraw");
    return hProp ? -1 : 0;
}

void vb6_SetAutoRedraw(void* hwnd, int32_t val) {
    if (!hwnd) return;
    if (val) {
        SetPropW((HWND)hwnd, L"VB6_AutoRedraw", (HANDLE)1);
        // AutoRedraw: 在内存中创建持久位图
        // 获取客户区大小
        RECT rc;
        GetClientRect((HWND)hwnd, &rc);
        HDC hdc = GetDC((HWND)hwnd);
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);
        // 用背景色填充
        HBRUSH bgBrush = (HBRUSH)(COLOR_BTNFACE + 1);
        FillRect(memDC, &rc, bgBrush);
        // 存储
        SetPropW((HWND)hwnd, L"VB6_AutoRedrawDC", (HANDLE)memDC);
        SetPropW((HWND)hwnd, L"VB6_AutoRedrawBmp", (HANDLE)memBmp);
        ReleaseDC((HWND)hwnd, hdc);
    } else {
        RemovePropW((HWND)hwnd, L"VB6_AutoRedraw");
        // 清理内存位图
        HANDLE hMemDC = GetPropW((HWND)hwnd, L"VB6_AutoRedrawDC");
        HANDLE hMemBmp = GetPropW((HWND)hwnd, L"VB6_AutoRedrawBmp");
        if (hMemDC) { DeleteDC((HDC)hMemDC); RemovePropW((HWND)hwnd, L"VB6_AutoRedrawDC"); }
        if (hMemBmp) { DeleteObject((HBITMAP)hMemBmp); RemovePropW((HWND)hwnd, L"VB6_AutoRedrawBmp"); }
    }
}

int32_t vb6_GetScaleMode(void* hwnd) {
    if (!hwnd) return 1;  // Default: Twips
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_ScaleMode");
    if (hProp) return (int32_t)(INT_PTR)hProp;
    return 1;  // Twips
}

void vb6_SetScaleMode(void* hwnd, int32_t val) {
    if (!hwnd) return;
    SetPropW((HWND)hwnd, L"VB6_ScaleMode", (HANDLE)(INT_PTR)val);
}

float vb6_GetCurrentX(void* hwnd) {
    if (!hwnd) return 0.0f;
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_CurrentX");
    if (hProp) {
        float val;
        memcpy(&val, &hProp, sizeof(float));
        return val;
    }
    return 0.0f;
}

void vb6_SetCurrentX(void* hwnd, float val) {
    if (!hwnd) return;
    HANDLE hProp;
    memcpy(&hProp, &val, sizeof(float));
    SetPropW((HWND)hwnd, L"VB6_CurrentX", hProp);
}

float vb6_GetCurrentY(void* hwnd) {
    if (!hwnd) return 0.0f;
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_CurrentY");
    if (hProp) {
        float val;
        memcpy(&val, &hProp, sizeof(float));
        return val;
    }
    return 0.0f;
}

void vb6_SetCurrentY(void* hwnd, float val) {
    if (!hwnd) return;
    HANDLE hProp;
    memcpy(&hProp, &val, sizeof(float));
    SetPropW((HWND)hwnd, L"VB6_CurrentY", hProp);
}
