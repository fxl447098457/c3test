// VB6 Win32窓体运行时实现 (P7)
// 提供Win32窗口注册、创建、消息循环、控件管理等基础功能

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#endif

#include "vb6forms.h"
#include <stdio.h>
#include <stdlib.h>   /* malloc, free */
#include <oleauto.h>  /* SysAllocString, BSTR */

// 全局变量
static HINSTANCE g_hInstance = NULL;
static int g_nextControlId = 100;  // 控件ID从100开始 (1-99保留给菜单)

// 模态窗体状态
static HWND g_modalOwner = NULL;   // 被禁用的父窗口 (模态时)
static int g_modalResult = 0;      // 模态返回值

// Form_Unload回调类型:
// 返回0=允许关闭, 返回1=取消关闭 (对应VB6 vbCancel)
typedef int (*vb6_FormUnloadCallback)(void);

// 当前窗体的Unload回调 (每个窗体单独设置)
static vb6_FormUnloadCallback g_formUnloadCb = NULL;

// Timer回调类型
typedef void (*vb6_TimerCallback)(void);

// Timer回调表 (控件ID → 回调函数)
#define VB6_MAX_TIMERS 32
static struct {
    int timerId;
    vb6_TimerCallback callback;
} g_timerTable[VB6_MAX_TIMERS];
static int g_timerCount = 0;

// ============================================================
// 缇(Twip)转换
// ============================================================

int vb6_TwipToX(int twips) {
    // 1缇 = 1/15像素 (96 DPI标准)
    // Screen.TwipsPerPixelX 通常=15
    return twips / 15;
}

int vb6_TwipToY(int twips) {
    return twips / 15;
}

// ============================================================
// 窗体框架
// ============================================================

int vb6_RegisterFormClass(const char* className, void* wndProc, void* hInstance, int iconResId) {
    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;  // 支持双击
    wc.lpfnWndProc = (WNDPROC)wndProc;
    wc.hInstance = (HINSTANCE)hInstance;
    wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);  // VB6默认灰色背景
    wc.lpszClassName = className;

    if (iconResId > 0) {
        wc.hIcon = LoadIconA((HINSTANCE)hInstance, (LPCSTR)MAKEINTRESOURCEA(iconResId));
    } else {
        wc.hIcon = LoadIconA(NULL, (LPCSTR)IDI_APPLICATION);
    }
    wc.hIconSm = wc.hIcon;

    if (!RegisterClassExA(&wc)) {
        return -1;
    }
    return 0;
}

void* vb6_CreateFormWindow(const char* className, const char* formName,
    int x, int y, int width, int height, void* hInstance, void* userData) {
    // VB6坐标是缇, 转为像素
    int px = (x == CW_USEDEFAULT) ? CW_USEDEFAULT : vb6_TwipToX(x);
    int py = (y == CW_USEDEFAULT) ? CW_USEDEFAULT : vb6_TwipToY(y);
    int pw = vb6_TwipToX(width);
    int ph = vb6_TwipToY(height);

    // 创建窗口, 使用WS_OVERLAPPEDWINDOW样式 (VB6标准窗口)
    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD exStyle = 0;

    // 调整窗口大小使客户区匹配指定大小
    RECT rc = {0, 0, pw, ph};
    AdjustWindowRectEx(&rc, style, FALSE, exStyle);

    HWND hwnd = CreateWindowExA(
        exStyle,
        className,
        formName,
        style,
        px, py,
        rc.right - rc.left,
        rc.bottom - rc.top,
        NULL,   // 无父窗口
        NULL,   // 无菜单
        (HINSTANCE)hInstance,
        userData  // 传递给WM_CREATE
    );

    return (void*)hwnd;
}

// ============================================================
// 控件创建
// ============================================================

void* vb6_CreateControl(const char* win32Class, const char* controlName,
    long style, long exStyle,
    int x, int y, int width, int height,
    int id, void* hParent, void* hInstance) {
    int px = vb6_TwipToX(x);
    int py = vb6_TwipToY(y);
    int pw = vb6_TwipToX(width);
    int ph = vb6_TwipToY(height);

    HWND hwnd = CreateWindowExA(
        (DWORD)exStyle,
        win32Class,
        controlName,
        (DWORD)style,
        px, py, pw, ph,
        (HWND)hParent,
        (HMENU)(intptr_t)id,
        (HINSTANCE)hInstance,
        NULL
    );

    // 设置默认字体 (VB6使用MS Sans Serif 8.25pt)
    if (hwnd) {
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        if (!hFont) {
            hFont = CreateFontA(
                -11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, FF_DONTCARE, "MS Shell Dlg"
            );
        }
        SendMessage(hwnd, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(FALSE, 0));
    }

    return (void*)hwnd;
}

int vb6_NextControlId(void) {
    return g_nextControlId++;
}

void vb6_ResetControlId(void) {
    g_nextControlId = 100;
}

// ============================================================
// Timer管理
// ============================================================

int vb6_SetTimer(int interval, void* callback) {
    if (g_timerCount >= VB6_MAX_TIMERS) return -1;
    int id = g_nextControlId++;
    g_timerTable[g_timerCount].timerId = id;
    g_timerTable[g_timerCount].callback = (vb6_TimerCallback)callback;
    g_timerCount++;
    // 使用窗体句柄NULL + 定时器ID, 由消息循环分发
    SetTimer(NULL, id, interval, NULL);
    return id;
}

void vb6_KillTimer(int timerId) {
    KillTimer(NULL, timerId);
    // 从回调表移除
    for (int i = 0; i < g_timerCount; i++) {
        if (g_timerTable[i].timerId == timerId) {
            g_timerTable[i] = g_timerTable[g_timerCount - 1];
            g_timerCount--;
            break;
        }
    }
}

// ============================================================
// 消息循环
// ============================================================

int vb6_MessageLoop(void) {
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        // WM_TIMER: 查找回调表并调用
        if (msg.message == WM_TIMER) {
            int tid = (int)msg.wParam;
            for (int i = 0; i < g_timerCount; i++) {
                if (g_timerTable[i].timerId == tid) {
                    if (g_timerTable[i].callback) {
                        g_timerTable[i].callback();
                    }
                    break;
                }
            }
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

int vb6_DoEvents(void) {
    MSG msg;
    int count = 0;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        // WM_TIMER: 查找回调表并调用
        if (msg.message == WM_TIMER) {
            int tid = (int)msg.wParam;
            for (int i = 0; i < g_timerCount; i++) {
                if (g_timerTable[i].timerId == tid) {
                    if (g_timerTable[i].callback) {
                        g_timerTable[i].callback();
                    }
                    break;
                }
            }
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        count++;
        // 安全限制: 防止无限循环 (VB6 DoEvents行为: 处理完就返回)
        if (count > 1000) break;
    }
    return count;
}

// ============================================================
// 窗体事件桥接
// ============================================================

void vb6_SetFormUserData(void* hwnd, void* userData) {
    SetWindowLongPtrA((HWND)hwnd, GWLP_USERDATA, (LONG_PTR)userData);
}

void* vb6_GetFormUserData(void* hwnd) {
    return (void*)GetWindowLongPtrA((HWND)hwnd, GWLP_USERDATA);
}

// ============================================================
// 窗体工具函数
// ============================================================

void* vb6_GetAppInstance(void) {
    return (void*)g_hInstance;
}

void vb6_SetAppInstance(void* hInstance) {
    g_hInstance = (HINSTANCE)hInstance;
}

void vb6_ShowForm(void* hwnd, int modal) {
    if (!hwnd) return;
    ShowWindow((HWND)hwnd, SW_SHOWDEFAULT);
    UpdateWindow((HWND)hwnd);

    if (modal) {
        // 模态窗体: 禁用所有者, 进入本地消息循环
        HWND owner = GetWindow((HWND)hwnd, GW_OWNER);
        if (owner) {
            g_modalOwner = owner;
            EnableWindow(owner, FALSE);
        }

        // 本地消息循环 (直到窗体被销毁)
        MSG msg;
        while (IsWindow((HWND)hwnd) && GetMessage(&msg, NULL, 0, 0)) {
            // WM_TIMER分发
            if (msg.message == WM_TIMER) {
                int tid = (int)msg.wParam;
                for (int i = 0; i < g_timerCount; i++) {
                    if (g_timerTable[i].timerId == tid) {
                        if (g_timerTable[i].callback) {
                            g_timerTable[i].callback();
                        }
                        break;
                    }
                }
            }
            // 模态Tab键导航 (IsDialogMessage处理对话框键盘导航)
            if (!IsDialogMessageA((HWND)hwnd, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        // 恢复所有者窗口
        if (g_modalOwner) {
            EnableWindow(g_modalOwner, TRUE);
            SetActiveWindow(g_modalOwner);
            g_modalOwner = NULL;
        }
    }
}

void vb6_UnloadForm(void* hwnd) {
    if (!hwnd) return;
    DestroyWindow((HWND)hwnd);
}

// ============================================================
// Form_Unload回调
// ============================================================

void vb6_SetFormUnloadCallback(void* callback) {
    g_formUnloadCb = (vb6_FormUnloadCallback)callback;
}

int vb6_QueryFormUnload(void) {
    if (g_formUnloadCb) {
        return g_formUnloadCb();
    }
    return 0;  // 无回调=允许关闭
}

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
// P11.8: Position/Size attributes (pixels, all visible controls)
int vb6_GetControlLeft(void* hwnd) {
    if (!hwnd) return 0;
    RECT rc;
    GetWindowRect((HWND)hwnd, &rc);
    POINT pt = { rc.left, rc.top };
    ScreenToClient(GetParent((HWND)hwnd), &pt);
    return pt.x;
}

void vb6_SetControlLeft(void* hwnd, int left) {
    if (!hwnd) return;
    RECT rc;
    GetWindowRect((HWND)hwnd, &rc);
    POINT pt = { rc.left, rc.top };
    ScreenToClient(GetParent((HWND)hwnd), &pt);
    SetWindowPos((HWND)hwnd, NULL, left, pt.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

int vb6_GetControlTop(void* hwnd) {
    if (!hwnd) return 0;
    RECT rc;
    GetWindowRect((HWND)hwnd, &rc);
    POINT pt = { rc.left, rc.top };
    ScreenToClient(GetParent((HWND)hwnd), &pt);
    return pt.y;
}

void vb6_SetControlTop(void* hwnd, int top) {
    if (!hwnd) return;
    RECT rc;
    GetWindowRect((HWND)hwnd, &rc);
    POINT pt = { rc.left, rc.top };
    ScreenToClient(GetParent((HWND)hwnd), &pt);
    SetWindowPos((HWND)hwnd, NULL, pt.x, top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

int vb6_GetControlWidth(void* hwnd) {
    if (!hwnd) return 0;
    RECT rc;
    GetWindowRect((HWND)hwnd, &rc);
    return rc.right - rc.left;
}

void vb6_SetControlWidth(void* hwnd, int width) {
    if (!hwnd) return;
    RECT rc;
    GetWindowRect((HWND)hwnd, &rc);
    SetWindowPos((HWND)hwnd, NULL, 0, 0, width, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);
}

int vb6_GetControlHeight(void* hwnd) {
    if (!hwnd) return 0;
    RECT rc;
    GetWindowRect((HWND)hwnd, &rc);
    return rc.bottom - rc.top;
}

void vb6_SetControlHeight(void* hwnd, int height) {
    if (!hwnd) return;
    RECT rc;
    GetWindowRect((HWND)hwnd, &rc);
    SetWindowPos((HWND)hwnd, NULL, 0, 0, rc.right - rc.left, height, SWP_NOMOVE | SWP_NOZORDER);
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
// ============================================================
// P13.3: ListBox/ComboBox properties
// ============================================================

// Helper: check if HWND is a ListBox
static int vb6_IsListBox(void* hwnd) {
    if (!hwnd) return 0;
    wchar_t cls[64];
    GetClassNameW((HWND)hwnd, cls, 64);
    // VB6 ListBox creates "ListBox" class, our RTL uses "LISTBOX"
    return (wcsicmp(cls, L"LISTBOX") == 0 || wcsicmp(cls, L"ListBox") == 0);
}

int vb6_GetListCount(void* hwnd) {
    if (!hwnd) return 0;
    if (vb6_IsListBox(hwnd)) {
        return (int)SendMessageW((HWND)hwnd, LB_GETCOUNT, 0, 0);
    }
    return (int)SendMessageW((HWND)hwnd, CB_GETCOUNT, 0, 0);
}

int vb6_GetListIndex(void* hwnd) {
    if (!hwnd) return -1;
    if (vb6_IsListBox(hwnd)) {
        return (int)SendMessageW((HWND)hwnd, LB_GETCURSEL, 0, 0);
    }
    return (int)SendMessageW((HWND)hwnd, CB_GETCURSEL, 0, 0);
}

void vb6_SetListIndex(void* hwnd, int index) {
    if (!hwnd) return;
    if (vb6_IsListBox(hwnd)) {
        SendMessageW((HWND)hwnd, LB_SETCURSEL, (WPARAM)index, 0);
    } else {
        SendMessageW((HWND)hwnd, CB_SETCURSEL, (WPARAM)index, 0);
    }
}

void* vb6_GetListItem(void* hwnd, int index) {
    if (!hwnd) return SysAllocString(L"");
    int len;
    WCHAR* buf;
    BSTR bstr;
    if (vb6_IsListBox(hwnd)) {
        len = (int)SendMessageW((HWND)hwnd, LB_GETTEXTLEN, (WPARAM)index, 0);
        if (len == LB_ERR) return SysAllocString(L"");
        buf = (WCHAR*)malloc((len + 1) * sizeof(WCHAR));
        if (!buf) return SysAllocString(L"");
        SendMessageW((HWND)hwnd, LB_GETTEXT, (WPARAM)index, (LPARAM)buf);
        bstr = SysAllocString(buf);
        free(buf);
    } else {
        len = (int)SendMessageW((HWND)hwnd, CB_GETLBTEXTLEN, (WPARAM)index, 0);
        if (len == CB_ERR) return SysAllocString(L"");
        buf = (WCHAR*)malloc((len + 1) * sizeof(WCHAR));
        if (!buf) return SysAllocString(L"");
        SendMessageW((HWND)hwnd, CB_GETLBTEXT, (WPARAM)index, (LPARAM)buf);
        bstr = SysAllocString(buf);
        free(buf);
    }
    return (void*)bstr;
}

void vb6_AddItem(void* hwnd, void* bstrItem) {
    if (!hwnd) return;
    BSTR bs = (BSTR)bstrItem;
    if (!bs) {
        bs = SysAllocString(L"");
    }
    if (vb6_IsListBox(hwnd)) {
        SendMessageW((HWND)hwnd, LB_ADDSTRING, 0, (LPARAM)bs);
    } else {
        SendMessageW((HWND)hwnd, CB_ADDSTRING, 0, (LPARAM)bs);
    }
    if (bstrItem == NULL) {
        SysFreeString(bs);
    }
}

void vb6_RemoveItem(void* hwnd, int index) {
    if (!hwnd) return;
    if (vb6_IsListBox(hwnd)) {
        SendMessageW((HWND)hwnd, LB_DELETESTRING, (WPARAM)index, 0);
    } else {
        SendMessageW((HWND)hwnd, CB_DELETESTRING, (WPARAM)index, 0);
    }
}

void vb6_ClearList(void* hwnd) {
    if (!hwnd) return;
    if (vb6_IsListBox(hwnd)) {
        SendMessageW((HWND)hwnd, LB_RESETCONTENT, 0, 0);
    } else {
        SendMessageW((HWND)hwnd, CB_RESETCONTENT, 0, 0);
    }
}

// P14.4.3: ListBox/ComboBox extended properties
void vb6_SetListItem(void* hwnd, int index, void* bstrItem) {
    if (!hwnd) return;
    if (index < 0) return;
    const wchar_t* wstr = bstrItem ? (const wchar_t*)bstrItem : L"";
    if (vb6_IsListBox(hwnd)) {
        SendMessageW((HWND)hwnd, LB_DELETESTRING, (WPARAM)index, 0);
        SendMessageW((HWND)hwnd, LB_INSERTSTRING, (WPARAM)index, (LPARAM)wstr);
    } else {
        SendMessageW((HWND)hwnd, CB_DELETESTRING, (WPARAM)index, 0);
        SendMessageW((HWND)hwnd, CB_INSERTSTRING, (WPARAM)index, (LPARAM)wstr);
    }
}

int vb6_GetSelected(void* hwnd, int index) {
    if (!hwnd || index < 0) return 0;
    if (vb6_IsListBox(hwnd)) {
        return (int)SendMessageW((HWND)hwnd, LB_GETSEL, (WPARAM)index, 0);
    }
    return 0;  /* ComboBox: use ListIndex instead */
}

void vb6_SetSelected(void* hwnd, int index, int selected) {
    if (!hwnd || index < 0) return;
    if (vb6_IsListBox(hwnd)) {
        SendMessageW((HWND)hwnd, LB_SETSEL, (WPARAM)(selected ? 1 : 0), (LPARAM)index);
    }
}

int32_t vb6_GetItemData(void* hwnd, int index) {
    if (!hwnd || index < 0) return 0;
    if (vb6_IsListBox(hwnd)) {
        return (int32_t)SendMessageW((HWND)hwnd, LB_GETITEMDATA, (WPARAM)index, 0);
    } else {
        return (int32_t)SendMessageW((HWND)hwnd, CB_GETITEMDATA, (WPARAM)index, 0);
    }
}

void vb6_SetItemData(void* hwnd, int index, int32_t data) {
    if (!hwnd || index < 0) return;
    if (vb6_IsListBox(hwnd)) {
        SendMessageW((HWND)hwnd, LB_SETITEMDATA, (WPARAM)index, (LPARAM)data);
    } else {
        SendMessageW((HWND)hwnd, CB_SETITEMDATA, (WPARAM)index, (LPARAM)data);
    }
}

int vb6_GetNewIndex(void* hwnd) {
    if (!hwnd) return -1;
    if (vb6_IsListBox(hwnd)) {
        return (int)SendMessageW((HWND)hwnd, LB_GETCOUNT, 0, 0) - 1;
    } else {
        return (int)SendMessageW((HWND)hwnd, CB_GETCOUNT, 0, 0) - 1;
    }
}

// ============================================================
// P13.4: TextBox-specific properties
// ============================================================

int vb6_GetMultiLine(void* hwnd) {
    if (!hwnd) return 0;
    LONG style = GetWindowLongW((HWND)hwnd, GWL_STYLE);
    return (style & ES_MULTILINE) ? -1 : 0;  // VB6: True=-1
}

void vb6_SetMultiLine(void* hwnd, int multiline) {
    // Note: ES_MULTILINE cannot be changed after creation in Win32
    // This is a VB6 design-time only property; we store it but it has no effect
    if (!hwnd) return;
    // Store in window property for consistency
    SetPropW((HWND)hwnd, L"VB6_MultiLine", (HANDLE)(INT_PTR)(multiline ? -1 : 0));
}

int vb6_GetScrollBars(void* hwnd) {
    if (!hwnd) return 0;
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_ScrollBars");
    if (hProp) return (int)(INT_PTR)hProp;
    // Infer from style
    LONG style = GetWindowLongW((HWND)hwnd, GWL_STYLE);
    int result = 0;
    if (style & WS_HSCROLL) result |= 1;
    if (style & WS_VSCROLL) result |= 2;
    return result;
}

void vb6_SetScrollBars(void* hwnd, int scrollbars) {
    // ScrollBars is design-time only in VB6; store as property
    if (!hwnd) return;
    SetPropW((HWND)hwnd, L"VB6_ScrollBars", (HANDLE)(INT_PTR)scrollbars);
}

int vb6_GetMaxLength(void* hwnd) {
    if (!hwnd) return 0;
    return (int)SendMessageW((HWND)hwnd, EM_GETLIMITTEXT, 0, 0);
}

void vb6_SetMaxLength(void* hwnd, int maxlength) {
    if (!hwnd) return;
    if (maxlength <= 0) maxlength = 0x7FFFFFFE;  // VB6: 0 = unlimited
    SendMessageW((HWND)hwnd, EM_SETLIMITTEXT, (WPARAM)maxlength, 0);
}

void* vb6_GetPasswordChar(void* hwnd) {
    if (!hwnd) return SysAllocString(L"");
    WCHAR ch = (WCHAR)SendMessageW((HWND)hwnd, EM_GETPASSWORDCHAR, 0, 0);
    if (ch == 0) return SysAllocString(L"");
    WCHAR buf[2] = { ch, 0 };
    return SysAllocString(buf);
}

void vb6_SetPasswordChar(void* hwnd, void* bstrChar) {
    if (!hwnd) return;
    WCHAR ch = 0;
    if (bstrChar) {
        BSTR bs = (BSTR)bstrChar;
        if (SysStringLen(bs) > 0) ch = bs[0];
    }
    SendMessageW((HWND)hwnd, EM_SETPASSWORDCHAR, (WPARAM)ch, 0);
    InvalidateRect((HWND)hwnd, NULL, TRUE);
}

int vb6_GetLocked(void* hwnd) {
    if (!hwnd) return 0;
    LONG style = GetWindowLongW((HWND)hwnd, GWL_STYLE);
    // Locked = read-only in Win32 terms
    return (style & ES_READONLY) ? -1 : 0;  // VB6: True=-1
}

void vb6_SetLocked(void* hwnd, int locked) {
    if (!hwnd) return;
    SendMessageW((HWND)hwnd, EM_SETREADONLY, (WPARAM)(locked ? TRUE : FALSE), 0);
}

// ============================================================
// P13.5: Alignment
// ============================================================

int vb6_GetAlignment(void* hwnd) {
    if (!hwnd) return 0;
    LONG style = GetWindowLongW((HWND)hwnd, GWL_STYLE);
    if (style & ES_CENTER) return 2;
    if (style & ES_RIGHT) return 1;
    return 0;  // Left
}

void vb6_SetAlignment(void* hwnd, int align) {
    if (!hwnd) return;
    LONG style = GetWindowLongW((HWND)hwnd, GWL_STYLE);
    style &= ~(ES_LEFT | ES_CENTER | ES_RIGHT);
    switch (align) {
        case 1: style |= ES_RIGHT; break;
        case 2: style |= ES_CENTER; break;
        default: style |= ES_LEFT; break;
    }
    SetWindowLongW((HWND)hwnd, GWL_STYLE, style);
    InvalidateRect((HWND)hwnd, NULL, TRUE);
}

// ============================================================
// P13.6: TabIndex/TabStop
// ============================================================

int vb6_GetTabIndex(void* hwnd) {
    if (!hwnd) return 0;
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_TabIndex");
    if (hProp) return (int)(INT_PTR)hProp;
    return 0;
}

void vb6_SetTabIndex(void* hwnd, int index) {
    if (!hwnd) return;
    SetPropW((HWND)hwnd, L"VB6_TabIndex", (HANDLE)(INT_PTR)index);
}

int vb6_GetTabStop(void* hwnd) {
    if (!hwnd) return -1;  // Default: True
    LONG style = GetWindowLongW((HWND)hwnd, GWL_STYLE);
    return (style & WS_TABSTOP) ? -1 : 0;  // VB6: True=-1
}

void vb6_SetTabStop(void* hwnd, int tabstop) {
    if (!hwnd) return;
    LONG style = GetWindowLongW((HWND)hwnd, GWL_STYLE);
    if (tabstop) {
        style |= WS_TABSTOP;
    } else {
        style &= ~WS_TABSTOP;
    }
    SetWindowLongW((HWND)hwnd, GWL_STYLE, style);
}

// ============================================================
// P13.8: ToolTipText
// ============================================================

// Helper: ToolTip control management
static HWND vb6_GetToolTipCtrl(void) {
    // Use a shared tooltip control (lazy init)
    static HWND s_hwndTT = NULL;
    if (!s_hwndTT) {
        s_hwndTT = CreateWindowExW(0, TOOLTIPS_CLASSW, NULL,
            WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            NULL, NULL, g_hInstance, NULL);
    }
    return s_hwndTT;
}

void* vb6_GetToolTipText(void* hwnd) {
    if (!hwnd) return SysAllocString(L"");
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_ToolTipText");
    if (!hProp) return SysAllocString(L"");
    return SysAllocString((LPCWSTR)hProp);
}

void vb6_SetToolTipText(void* hwnd, void* bstrText) {
    if (!hwnd) return;
    // Free old tooltip text stored in property
    HANDLE hOld = GetPropW((HWND)hwnd, L"VB6_ToolTipText");
    if (hOld) {
        SysFreeString((BSTR)hOld);
        RemovePropW((HWND)hwnd, L"VB6_ToolTipText");
    }
    if (bstrText) {
        BSTR bs = (BSTR)bstrText;
        BSTR copy = SysAllocString(bs);
        SetPropW((HWND)hwnd, L"VB6_ToolTipText", (HANDLE)copy);
        
        // Register with tooltip control
        HWND hwndTT = vb6_GetToolTipCtrl();
        if (hwndTT) {
            TOOLINFOW ti;
            memset(&ti, 0, sizeof(ti));
            ti.cbSize = sizeof(ti);
            ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
            ti.hwnd = GetParent((HWND)hwnd);
            ti.uId = (UINT_PTR)hwnd;
            ti.lpszText = copy;
            // Try update first, then add
            SendMessageW(hwndTT, TTM_UPDATETIPTEXTW, 0, (LPARAM)&ti);
            if (SendMessageW(hwndTT, TTM_GETTOOLINFO, 0, (LPARAM)&ti) == 0) {
                // Tool not yet added
                SendMessageW(hwndTT, TTM_ADDTOOLW, 0, (LPARAM)&ti);
            }
        }
    }
}

// ============================================================
// P13.9: Tag
// ============================================================

void* vb6_GetControlTag(void* hwnd) {
    if (!hwnd) return SysAllocString(L"");
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_Tag");
    if (!hProp) return SysAllocString(L"");
    return SysAllocString((LPCWSTR)hProp);
}

void vb6_SetControlTag(void* hwnd, void* bstrTag) {
    if (!hwnd) return;
    // Free old tag
    HANDLE hOld = GetPropW((HWND)hwnd, L"VB6_Tag");
    if (hOld) {
        SysFreeString((BSTR)hOld);
        RemovePropW((HWND)hwnd, L"VB6_Tag");
    }
    BSTR copy = bstrTag ? SysAllocString((BSTR)bstrTag) : SysAllocString(L"");
    SetPropW((HWND)hwnd, L"VB6_Tag", (HANDLE)copy);
}

// ============================================================
// P13.7: MousePointer/MouseIcon
// ============================================================

// VB6 MousePointer values to Windows cursor mapping
static LPCWSTR vb6_MousePointerToCursor(int pointer) {
    switch (pointer) {
        case 0:  return NULL;              // vbDefault - use class cursor
        case 1:  return IDC_ARROW;         // vbArrow
        case 2:  return IDC_CROSS;         // vbCrosshair
        case 3:  return IDC_IBEAM;         // vbIbeam
        case 4:  return IDC_ICON;          // vbIconPointer (obsolete)
        case 5:  return IDC_SIZE;          // vbSizePointer
        case 6:  return IDC_SIZENESW;      // vbSizeNESW
        case 7:  return IDC_SIZENS;        // vbSizeNS
        case 8:  return IDC_SIZENWSE;      // vbSizeNWSE
        case 9:  return IDC_SIZEWE;        // vbSizeEW
        case 10: return IDC_UPARROW;       // vbUpArrow
        case 11: return IDC_WAIT;          // vbHourglass
        case 12: return IDC_NO;            // vbNoDrop
        case 13: return IDC_APPSTARTING;   // vbArrowHourglass
        case 14: return IDC_HELP;          // vbArrowQuestion
        case 15: return IDC_SIZEALL;       // vbSizeAll
        case 99: return NULL;              // vbCustom - use MouseIcon
        default: return NULL;
    }
}

int vb6_GetMousePointer(void* hwnd) {
    if (!hwnd) return 0;
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_MousePointer");
    if (hProp) return (int)(INT_PTR)hProp;
    return 0;  // Default
}

void vb6_SetMousePointer(void* hwnd, int pointer) {
    if (!hwnd) return;
    SetPropW((HWND)hwnd, L"VB6_MousePointer", (HANDLE)(INT_PTR)pointer);
    if (pointer == 99) {
        // Custom: use MouseIcon cursor if set
        HANDLE hIcon = GetPropW((HWND)hwnd, L"VB6_MouseIcon");
        if (hIcon) {
            SetClassLongPtrW((HWND)hwnd, GCLP_HCURSOR, (LONG_PTR)hIcon);
        }
    } else if (pointer == 0) {
        // Default: restore class cursor
        SetClassLongPtrW((HWND)hwnd, GCLP_HCURSOR, (LONG_PTR)LoadCursorW(NULL, IDC_ARROW));
    } else {
        LPCWSTR cursorName = vb6_MousePointerToCursor(pointer);
        if (cursorName) {
            HCURSOR hCur = LoadCursorW(NULL, cursorName);
            if (hCur) SetClassLongPtrW((HWND)hwnd, GCLP_HCURSOR, (LONG_PTR)hCur);
        }
    }
}

void* vb6_GetMouseIcon(void* hwnd) {
    if (!hwnd) return NULL;
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_MouseIcon");
    return hProp;  // HCURSOR handle
}

void vb6_SetMouseIcon(void* hwnd, void* hCursor) {
    if (!hwnd) return;
    SetPropW((HWND)hwnd, L"VB6_MouseIcon", (HANDLE)hCursor);
    // If MousePointer is 99 (Custom), apply immediately
    int mp = vb6_GetMousePointer(hwnd);
    if (mp == 99 && hCursor) {
        SetClassLongPtrW((HWND)hwnd, GCLP_HCURSOR, (LONG_PTR)hCursor);
    }
}

// ============================================================
// P13.10: BorderStyle
// ============================================================

int vb6_GetBorderStyle(void* hwnd) {
    if (!hwnd) return 0;
    HWND hw = (HWND)hwnd;
    WCHAR className[256] = {0};
    GetClassNameW(hw, className, 256);

    if (wcsicmp(className, L"Edit") == 0) {
        // TextBox: 0=None(No border), 1=Fixed Single
        LONG style = GetWindowLongW(hw, GWL_EXSTYLE);
        return (style & WS_EX_CLIENTEDGE) ? 1 : 0;
    }
    // Form/ComboBox/ListBox: store as property
    HANDLE hProp = GetPropW(hw, L"VB6_BorderStyle");
    if (hProp) return (int)(INT_PTR)hProp;
    // Form default is 2 (Sizable)
    if (wcsicmp(className, L"VB6_Form") == 0 || 
        GetWindowLongW(hw, GWL_STYLE) & WS_OVERLAPPEDWINDOW) {
        return 2;
    }
    return 1;  // Default Fixed Single for most controls
}

void vb6_SetBorderStyle(void* hwnd, int style) {
    if (!hwnd) return;
    HWND hw = (HWND)hwnd;
    WCHAR className[256] = {0};
    GetClassNameW(hw, className, 256);

    if (wcsicmp(className, L"Edit") == 0) {
        // TextBox: only 0 or 1
        LONG exStyle = GetWindowLongW(hw, GWL_EXSTYLE);
        if (style == 0) {
            exStyle &= ~WS_EX_CLIENTEDGE;
        } else {
            exStyle |= WS_EX_CLIENTEDGE;
        }
        SetWindowLongW(hw, GWL_EXSTYLE, exStyle);
        SetWindowPos(hw, NULL, 0, 0, 0, 0,
            SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
    } else {
        // Store as property for other controls
        SetPropW(hw, L"VB6_BorderStyle", (HANDLE)(INT_PTR)style);
    }
}

// ============================================================
// P13.11: ScrollBar properties (HScrollBar/VScrollBar)
// ============================================================

int vb6_GetScrollMin(void* hwnd) {
    if (!hwnd) return 0;
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_ScrollMin");
    if (hProp) return (int)(INT_PTR)hProp;
    return 0;
}

void vb6_SetScrollMin(void* hwnd, int min) {
    if (!hwnd) return;
    SetPropW((HWND)hwnd, L"VB6_ScrollMin", (HANDLE)(INT_PTR)min);
    // Apply to Win32 scrollbar: Win32 min is always 0, we scale
    // Store VB6 Min/Max, set Win32 range to 0..(Max-Min)
    int max = vb6_GetScrollMax(hwnd);
    int winMax = max - min;
    if (winMax < 0) winMax = 0;
    HWND hw = (HWND)hwnd;
    WCHAR className[256] = {0};
    GetClassNameW(hw, className, 256);
    if (wcsicmp(className, L"SCROLLBAR") == 0) {
        SetScrollRange(hw, SB_CTL, 0, winMax, TRUE);
    } else {
        // Form scrollbars
        SetScrollRange(hw, SB_HORZ, 0, winMax, TRUE);
    }
    // Adjust current value
    int val = vb6_GetScrollValue(hwnd);
    int winVal = val - min;
    if (wcsicmp(className, L"SCROLLBAR") == 0) {
        SetScrollPos(hw, SB_CTL, winVal, TRUE);
    }
}

int vb6_GetScrollMax(void* hwnd) {
    if (!hwnd) return 32767;
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_ScrollMax");
    if (hProp) return (int)(INT_PTR)hProp;
    return 32767;  // VB6 default
}

void vb6_SetScrollMax(void* hwnd, int max) {
    if (!hwnd) return;
    SetPropW((HWND)hwnd, L"VB6_ScrollMax", (HANDLE)(INT_PTR)max);
    int min = vb6_GetScrollMin(hwnd);
    int winMax = max - min;
    if (winMax < 0) winMax = 0;
    HWND hw = (HWND)hwnd;
    WCHAR className[256] = {0};
    GetClassNameW(hw, className, 256);
    if (wcsicmp(className, L"SCROLLBAR") == 0) {
        SetScrollRange(hw, SB_CTL, 0, winMax, TRUE);
    }
}

int vb6_GetScrollValue(void* hwnd) {
    if (!hwnd) return 0;
    HWND hw = (HWND)hwnd;
    WCHAR className[256] = {0};
    GetClassNameW(hw, className, 256);
    int winPos;
    if (wcsicmp(className, L"SCROLLBAR") == 0) {
        winPos = GetScrollPos(hw, SB_CTL);
    } else {
        winPos = GetScrollPos(hw, SB_HORZ);
    }
    int min = vb6_GetScrollMin(hwnd);
    return winPos + min;  // Convert from Win32 (0-based) to VB6
}

void vb6_SetScrollValue(void* hwnd, int value) {
    if (!hwnd) return;
    int min = vb6_GetScrollMin(hwnd);
    int winPos = value - min;
    if (winPos < 0) winPos = 0;
    HWND hw = (HWND)hwnd;
    WCHAR className[256] = {0};
    GetClassNameW(hw, className, 256);
    if (wcsicmp(className, L"SCROLLBAR") == 0) {
        SetScrollPos(hw, SB_CTL, winPos, TRUE);
    }
}

int vb6_GetLargeChange(void* hwnd) {
    if (!hwnd) return 1;
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_LargeChange");
    if (hProp) return (int)(INT_PTR)hProp;
    return 1;
}

void vb6_SetLargeChange(void* hwnd, int change) {
    if (!hwnd) return;
    SetPropW((HWND)hwnd, L"VB6_LargeChange", (HANDLE)(INT_PTR)change);
    // Win32 scroll info page size
    HWND hw = (HWND)hwnd;
    WCHAR className[256] = {0};
    GetClassNameW(hw, className, 256);
    SCROLLINFO si;
    si.cbSize = sizeof(si);
    si.fMask = SIF_PAGE;
    si.nPage = (UINT)change;
    if (wcsicmp(className, L"SCROLLBAR") == 0) {
        SetScrollInfo(hw, SB_CTL, &si, TRUE);
    }
}

int vb6_GetSmallChange(void* hwnd) {
    if (!hwnd) return 1;
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_SmallChange");
    if (hProp) return (int)(INT_PTR)hProp;
    return 1;
}

void vb6_SetSmallChange(void* hwnd, int change) {
    if (!hwnd) return;
    SetPropW((HWND)hwnd, L"VB6_SmallChange", (HANDLE)(INT_PTR)change);
    // SmallChange is handled in WM_HSCROLL/WM_VSCROLL handler, just store
}

// ============================================================
// P13.12: Timer properties
// ============================================================

int vb6_GetTimerInterval(void* hwnd) {
    if (!hwnd) return 0;
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_TimerInterval");
    if (hProp) return (int)(INT_PTR)hProp;
    return 0;
}

void vb6_SetTimerInterval(void* hwnd, int interval) {
    if (!hwnd) return;
    SetPropW((HWND)hwnd, L"VB6_TimerInterval", (HANDLE)(INT_PTR)interval);
    // Timer hwnd is actually the timer ID stored as a property
    // This is a design-time placeholder; actual timer manipulation
    // goes through vb6_SetTimer/vb6_KillTimer
}

int vb6_GetTimerEnabled(void* hwnd) {
    if (!hwnd) return -1;  // VB6 default: True
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_TimerEnabled");
    if (hProp) return (int)(INT_PTR)hProp;
    return -1;  // True
}

void vb6_SetTimerEnabled(void* hwnd, int enabled) {
    if (!hwnd) return;
    SetPropW((HWND)hwnd, L"VB6_TimerEnabled", (HANDLE)(INT_PTR)enabled);
}

// ============================================================
// P13.14: TextBox selection properties
// ============================================================

int vb6_GetSelStart(void* hwnd) {
    if (!hwnd) return 0;
    DWORD start = 0, end = 0;
    SendMessageW((HWND)hwnd, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    return (int)start;
}

void vb6_SetSelStart(void* hwnd, int start) {
    if (!hwnd) return;
    if (start < 0) start = 0;
    DWORD oldStart = 0, oldEnd = 0;
    SendMessageW((HWND)hwnd, EM_GETSEL, (WPARAM)&oldStart, (LPARAM)&oldEnd);
    // Keep selection length, move start
    DWORD selLen = oldEnd - oldStart;
    SendMessageW((HWND)hwnd, EM_SETSEL, (WPARAM)start, (LPARAM)(start + selLen));
}

int vb6_GetSelLength(void* hwnd) {
    if (!hwnd) return 0;
    DWORD start = 0, end = 0;
    SendMessageW((HWND)hwnd, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    return (int)(end - start);
}

void vb6_SetSelLength(void* hwnd, int length) {
    if (!hwnd) return;
    if (length < 0) length = 0;
    DWORD start = 0, end = 0;
    SendMessageW((HWND)hwnd, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    SendMessageW((HWND)hwnd, EM_SETSEL, (WPARAM)start, (LPARAM)(start + length));
}

void* vb6_GetSelText(void* hwnd) {
    if (!hwnd) return SysAllocString(L"");
    HWND hw = (HWND)hwnd;
    DWORD start = 0, end = 0;
    SendMessageW(hw, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    if (start >= end) return SysAllocString(L"");
    int textLen = (int)SendMessageW(hw, WM_GETTEXTLENGTH, 0, 0);
    if (textLen <= 0) return SysAllocString(L"");
    WCHAR* buf = (WCHAR*)malloc((textLen + 1) * sizeof(WCHAR));
    if (!buf) return SysAllocString(L"");
    SendMessageW(hw, WM_GETTEXT, (WPARAM)(textLen + 1), (LPARAM)buf);
    /* Extract selection from full text */
    int selLen = (int)(end - start);
    if ((int)start + selLen > textLen) selLen = textLen - (int)start;
    if (selLen < 0) selLen = 0;
    WCHAR* selBuf = (WCHAR*)malloc((selLen + 1) * sizeof(WCHAR));
    if (!selBuf) { free(buf); return SysAllocString(L""); }
    memcpy(selBuf, buf + start, selLen * sizeof(WCHAR));
    selBuf[selLen] = L'\0';
    BSTR result = SysAllocString(selBuf);
    free(selBuf);
    free(buf);
    return result;
    return result;
}ext(void* hwnd, void* bstrText) {
    if (!hwnd) return;
    HWND hw = (HWND)hwnd;
    // Replace current selection with new text
    if (!bstrText) {
        // Replace with empty = delete selection
        SendMessageW(hw, EM_REPLACESEL, TRUE, (LPARAM)L"");
    } else {
        SendMessageW(hw, EM_REPLACESEL, TRUE, (LPARAM)(BSTR)bstrText);
    }
}

// ============================================================
// P13.15: CommandButton Default/Cancel
// ============================================================

int vb6_GetDefaultButton(void* hwnd) {
    if (!hwnd) return 0;
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_DefaultButton");
    if (hProp) return (int)(INT_PTR)hProp;
    return 0;
}

void vb6_SetDefaultButton(void* hwnd, int isDefault) {
    if (!hwnd) return;
    SetPropW((HWND)hwnd, L"VB6_DefaultButton", (HANDLE)(INT_PTR)isDefault);
    // Visual feedback: default button has BS_DEFPUSHBUTTON style
    LONG style = GetWindowLongW((HWND)hwnd, GWL_STYLE);
    if (isDefault) {
        style |= BS_DEFPUSHBUTTON;
        style &= ~BS_PUSHBUTTON;
    } else {
        style &= ~BS_DEFPUSHBUTTON;
        style |= BS_PUSHBUTTON;
    }
    SetWindowLongW((HWND)hwnd, GWL_STYLE, style);
    // Redraw
    InvalidateRect((HWND)hwnd, NULL, TRUE);
}

int vb6_GetCancelButton(void* hwnd) {
    if (!hwnd) return 0;
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_CancelButton");
    if (hProp) return (int)(INT_PTR)hProp;
    return 0;
}

void vb6_SetCancelButton(void* hwnd, int isCancel) {
    if (!hwnd) return;
    SetPropW((HWND)hwnd, L"VB6_CancelButton", (HANDLE)(INT_PTR)isCancel);
    // Cancel button is a logical property only; no visual change
    // The parent form's message loop should check this on VK_ESCAPE
}
// ============================================================
// P13.13: Picture (PictureBox/Image)
// ============================================================

void* vb6_LoadPictureFromFile(const char* filePath) {
    if (!filePath || !filePath[0]) return NULL;
    /* Determine type by extension */
    const char* ext = strrchr(filePath, '.');
    if (!ext) ext = "";
    
    WCHAR wPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_ACP, 0, filePath, -1, wPath, MAX_PATH);
    
    if (_stricmp(ext, ".ico") == 0 || _stricmp(ext, ".cur") == 0) {
        /* Icon/Cursor */
        if (_stricmp(ext, ".cur") == 0) {
            return (void*)LoadCursorFromFileW(wPath);
        }
        return (void*)LoadImageW(NULL, wPath, IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
    } else if (_stricmp(ext, ".bmp") == 0) {
        return (void*)LoadImageW(NULL, wPath, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    } else if (_stricmp(ext, ".emf") == 0 || _stricmp(ext, ".wmf") == 0) {
        /* Enhanced/Regular metafile - use GetEnhMetaFile/GetMetaFile */
        HENHMETAFILE hemf = GetEnhMetaFileW(wPath);
        return (void*)hemf;
    }
    /* Try as bitmap fallback */
    return (void*)LoadImageW(NULL, wPath, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
}

void* vb6_LoadPictureFromResource(void* hInstance, int resourceId, const char* type) {
    if (!hInstance) return NULL;
    HINSTANCE hInst = (HINSTANCE)hInstance;
    WCHAR wType[64] = {0};
    if (type) MultiByteToWideChar(CP_ACP, 0, type, -1, wType, 64);
    
    if (type && _stricmp(type, "ICON") == 0) {
        return (void*)LoadIconW(hInst, MAKEINTRESOURCEW(resourceId));
    } else if (type && _stricmp(type, "BITMAP") == 0) {
        return (void*)LoadBitmapW(hInst, MAKEINTRESOURCEW(resourceId));
    } else if (type && _stricmp(type, "CURSOR") == 0) {
        return (void*)LoadCursorW(hInst, MAKEINTRESOURCEW(resourceId));
    }
    /* Try bitmap as default */
    return (void*)LoadBitmapW(hInst, MAKEINTRESOURCEW(resourceId));
}

void* vb6_GetControlPicture(void* hwnd) {
    if (!hwnd) return NULL;
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_Picture");
    return hProp;  // HBITMAP/HICON handle
}

void vb6_SetControlPicture(void* hwnd, void* hPicture) {
    if (!hwnd) return;
    HWND hw = (HWND)hwnd;
    /* Store the picture handle */
    SetPropW(hw, L"VB6_Picture", (HANDLE)hPicture);
    
    /* Apply to the control */
    WCHAR className[256] = {0};
    GetClassNameW(hw, className, 256);
    
    if (wcsicmp(className, L"STATIC") == 0) {
        /* PictureBox uses STATIC control with SS_BITMAP/SS_ICON style */
        if (hPicture) {
            /* Detect picture type - check if it's an icon by trying */
            LONG style = GetWindowLongW(hw, GWL_STYLE);
            style &= ~(SS_BITMAP | SS_ICON | SS_ENHMETAFILE);
            
            /* For now assume bitmap; icon detection would require more logic */
            style |= SS_BITMAP | SS_CENTERIMAGE;
            SetWindowLongW(hw, GWL_STYLE, style);
            SendMessageW(hw, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)hPicture);
        } else {
            /* Clear picture */
            LONG style = GetWindowLongW(hw, GWL_STYLE);
            style &= ~(SS_BITMAP | SS_ICON | SS_ENHMETAFILE);
            SetWindowLongW(hw, GWL_STYLE, style);
            SendMessageW(hw, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)NULL);
        }
        InvalidateRect(hw, NULL, TRUE);
    }
    
    /* AutoSize: if enabled, resize to fit picture */
    int autoSize = vb6_GetPictureAutoSize(hwnd);
    if (autoSize && hPicture) {
        BITMAP bm;
        HBITMAP hBmp = (HBITMAP)hPicture;
        if (GetObjectW(hBmp, sizeof(bm), &bm) != 0) {
            SetWindowPos(hw, NULL, 0, 0, bm.bmWidth, bm.bmHeight,
                SWP_NOMOVE | SWP_NOZORDER);
        }
    }
}

int vb6_GetPictureAutoSize(void* hwnd) {
    if (!hwnd) return 0;
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_AutoSize");
    if (hProp) return (int)(INT_PTR)hProp;
    return 0;
}

void vb6_SetPictureAutoSize(void* hwnd, int autoSize) {
    if (!hwnd) return;
    SetPropW((HWND)hwnd, L"VB6_AutoSize", (HANDLE)(INT_PTR)autoSize);
}
// ============================================================
// P17.2: Image.Stretch property + WM_PAINT subclass (StretchBlt)
// ============================================================

int vb6_GetImageStretch(void* hwnd) {
    if (!hwnd) return 0;
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_Stretch");
    return hProp ? (int)(INT_PTR)hProp : 0;
}

void vb6_SetImageStretch(void* hwnd, int stretch) {
    if (!hwnd) return;
    HWND hw = (HWND)hwnd;
    SetPropW(hw, L"VB6_Stretch", (HANDLE)(INT_PTR)stretch);
    if (stretch) {
        vb6_InstallImageSubclass(hwnd);
    }
    InvalidateRect(hw, NULL, TRUE);
}

/* Image control subclass WndProc for WM_PAINT (StretchBlt rendering) */
static LRESULT CALLBACK vb6_ImageSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_PAINT) {
        int stretch = (int)(INT_PTR)GetPropW(hwnd, L"VB6_Stretch");
        HANDLE hPict = GetPropW(hwnd, L"VB6_Picture");
        if (stretch && hPict) {
            /* Check if it's a bitmap (StretchBlt only works with HBITMAP) */
            DWORD objType = GetObjectType((HGDIOBJ)hPict);
            if (objType == OBJ_BITMAP) {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                RECT rc;
                GetClientRect(hwnd, &rc);
                
                HDC memDC = CreateCompatibleDC(hdc);
                HBITMAP hBmp = (HBITMAP)hPict;
                BITMAP bm;
                GetObjectW(hBmp, sizeof(bm), &bm);
                HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, hBmp);
                
                SetStretchBltMode(hdc, HALFTONE);
                SetBrushOrgEx(hdc, 0, 0, NULL);
                StretchBlt(hdc, 0, 0, rc.right, rc.bottom,
                           memDC, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
                
                SelectObject(memDC, oldBmp);
                DeleteDC(memDC);
                EndPaint(hwnd, &ps);
                return 0;
            }
            /* For icons: fall through to original STATIC proc (no stretch) */
        }
    } else if (msg == WM_DESTROY) {
        /* Remove subclass on destroy */
        WNDPROC origProc = (WNDPROC)GetPropW(hwnd, L"VB6_OrigProc");
        if (origProc) {
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)origProc);
            RemovePropW(hwnd, L"VB6_OrigProc");
        }
    }
    /* Fall through to original STATIC WndProc */
    WNDPROC origProc = (WNDPROC)GetPropW(hwnd, L"VB6_OrigProc");
    if (origProc) return CallWindowProcW(origProc, hwnd, msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void vb6_InstallImageSubclass(void* hwnd) {
    if (!hwnd) return;
    HWND hw = (HWND)hwnd;
    /* Only install once */
    if (GetPropW(hw, L"VB6_OrigProc")) return;
    WNDPROC origProc = (WNDPROC)SetWindowLongPtrW(hw, GWLP_WNDPROC, (LONG_PTR)vb6_ImageSubclassProc);
    if (origProc) SetPropW(hw, L"VB6_OrigProc", (HANDLE)origProc);
}
// // 控件数组 (P7.6)
// ============================================================

void vb6_CtrlArr_Init(vb6_CtrlArr* arr) {
    int i;
    for (i = 0; i < VB6_CTRLARR_MAX; i++) {
        arr->hwnds[i] = NULL;
    }
    arr->count = 0;
    arr->lowerBound = 0;
    arr->upperBound = -1;
}

void vb6_CtrlArr_SetAt(vb6_CtrlArr* arr, int index, void* hwnd) {
    if (index < 0 || index >= VB6_CTRLARR_MAX) return;
    if (arr->hwnds[index] == NULL && hwnd != NULL) {
        arr->count++;
    } else if (arr->hwnds[index] != NULL && hwnd == NULL) {
        arr->count--;
    }
    arr->hwnds[index] = hwnd;
    /* 更新界 */
    if (hwnd != NULL) {
        if (arr->upperBound < 0 || index > arr->upperBound) arr->upperBound = index;
        if (index < arr->lowerBound) arr->lowerBound = index;
    }
}

void* vb6_CtrlArr_GetAt(const vb6_CtrlArr* arr, int index) {
    if (index < 0 || index >= VB6_CTRLARR_MAX) return NULL;
    return arr->hwnds[index];
}

int vb6_CtrlArr_GetCount(const vb6_CtrlArr* arr) {
    return arr->count;
}

int vb6_CtrlArr_LBound(const vb6_CtrlArr* arr) {
    return arr->lowerBound;
}

int vb6_CtrlArr_UBound(const vb6_CtrlArr* arr) {
    return arr->upperBound;
}

void* vb6_CtrlArr_Load(vb6_CtrlArr* arr, int index, void* hParent, void* hInstance) {
    HWND hNew, hTemplate;
    WCHAR className[256] = {0};
    WCHAR text[1024] = {0};
    RECT rc;
    DWORD style, exStyle;
    int ctrlId;

    if (index < 0 || index >= VB6_CTRLARR_MAX) return NULL;
    if (arr->hwnds[index] != NULL) return arr->hwnds[index];  /* 已存在 */

    /* 找模板: 优先index=0, 否则第一个非空 */
    hTemplate = (HWND)vb6_CtrlArr_GetAt(arr, 0);
    if (!hTemplate) {
        int i;
        for (i = 0; i < VB6_CTRLARR_MAX; i++) {
            if (arr->hwnds[i]) { hTemplate = (HWND)arr->hwnds[i]; break; }
        }
    }
    if (!hTemplate) return NULL;

    /* 从模板复制窗口属性 */
    GetClassNameW(hTemplate, className, 256);
    GetWindowTextW(hTemplate, text, 1024);
    GetWindowRect(hTemplate, &rc);
    style = (DWORD)GetWindowLongPtrA(hTemplate, GWL_STYLE);
    exStyle = (DWORD)GetWindowLongPtrA(hTemplate, GWL_EXSTYLE);

    ctrlId = vb6_NextControlId();

    hNew = CreateWindowExW(
        exStyle, className, text, style,
        rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
        (HWND)hParent, (HMENU)(intptr_t)ctrlId, (HINSTANCE)hInstance, NULL
    );

    if (hNew) {
        HFONT hFont = (HFONT)SendMessage(hTemplate, WM_GETFONT, 0, 0);
        if (hFont) SendMessage(hNew, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(FALSE, 0));
        vb6_CtrlArr_SetAt(arr, index, (void*)hNew);
    }
    return (void*)hNew;
}

void vb6_CtrlArr_Unload(vb6_CtrlArr* arr, int index) {
    if (index < 0 || index >= VB6_CTRLARR_MAX) return;
    if (arr->hwnds[index] == NULL) return;
    /* 不能卸载设计时创建的元素(index=0或其他初始元素) — VB6也是如此 */
    DestroyWindow((HWND)arr->hwnds[index]);
    vb6_CtrlArr_SetAt(arr, index, NULL);
}

// ============================================================
// MDI窗体 (P7.7)
// ============================================================

// MDI父窗体的客户窗口句柄, 存入GWLP_USERDATA
// 用Prop也可以, 但USERDATA更快

// 自动查找MDIClient窗口的回调
static BOOL CALLBACK FindMDIClientEnumProc(HWND hwnd, LPARAM lParam) {
    HWND hMDIClient = (HWND)GetPropA(hwnd, "VB6_MDIClient");
    if (hMDIClient) {
        *(HWND*)lParam = hMDIClient;
        return FALSE;  /* 找到, 停止枚举 */
    }
    return TRUE;  /* 继续枚举 */
}

static HWND vb6_AutoFindMDIClient(void) {
    HWND hMDIClient = NULL;
    EnumWindows(FindMDIClientEnumProc, (LPARAM)&hMDIClient);
    return hMDIClient;
}
int vb6_RegisterMDIFormClass(const char* className, void* wndProc, void* hInstance, int iconResId) {
    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = (WNDPROC)wndProc;
    wc.hInstance = (HINSTANCE)hInstance;
    wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wc.lpszClassName = className;
    if (iconResId > 0) {
        wc.hIcon = LoadIconA((HINSTANCE)hInstance, MAKEINTRESOURCEA(iconResId));
    } else {
        wc.hIcon = LoadIconA(NULL, (LPCSTR)IDI_APPLICATION);
    }
    ATOM atom = RegisterClassExA(&wc);
    return (atom != 0) ? 0 : -1;
}

void* vb6_CreateMDIFormWindow(const char* className, const char* formName,
    int x, int y, int width, int height, void* hInstance) {
    HWND hwnd = CreateWindowExA(
        0, className, formName,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        x, y,
        vb6_TwipToX(width), vb6_TwipToY(height),
        NULL, NULL, (HINSTANCE)hInstance, NULL);
    if (!hwnd) return NULL;

    /* 创建MDI客户窗口 */
    CLIENTCREATESTRUCT ccs = {0};
    ccs.hWindowMenu = NULL;   /* P7.8菜单系统实现后填充 */
    ccs.idFirstChild = 1000;  /* MDI子窗体ID起始值 */

    HWND hMDIClient = CreateWindowExA(
        0, "MDICLIENT", NULL,
        WS_CHILD | WS_CLIPCHILDREN | WS_VSCROLL | WS_HSCROLL | MDIS_ALLCHILDSTYLES,
        0, 0, 0, 0,
        hwnd, (HMENU)0xCAC,   /* MDI客户窗口控件ID */
        (HINSTANCE)hInstance, &ccs);
    if (!hMDIClient) {
        DestroyWindow(hwnd);
        return NULL;
    }
    ShowWindow(hMDIClient, SW_SHOW);

    /* 保存MDI客户窗口句柄到父窗体属性 */
    SetPropA(hwnd, "VB6_MDIClient", hMDIClient);

    return (void*)hwnd;
}

void* vb6_CreateMDIChildWindow(const char* className, const char* formName,
    int x, int y, int width, int height, void* hMDIClient, void* hInstance) {
    /* P7.7: 自动查找MDIClient (当hMDIClient=NULL时枚举窗口查找) */
    if (!hMDIClient) hMDIClient = vb6_AutoFindMDIClient();
    if (!hMDIClient) return NULL;  /* 没有MDI父窗体 */
    MDICREATESTRUCTA mcs = {0};
    mcs.szClass = className;
    mcs.szTitle = formName;
    mcs.x = (x == CW_USEDEFAULT) ? CW_USEDEFAULT : vb6_TwipToX(x);
    mcs.y = (y == CW_USEDEFAULT) ? CW_USEDEFAULT : vb6_TwipToY(y);
    mcs.cx = vb6_TwipToX(width);
    mcs.cy = vb6_TwipToY(height);
    mcs.style = 0;
    mcs.lParam = 0;

    HWND hChild = (HWND)SendMessageA((HWND)hMDIClient, WM_MDICREATE, 0, (LPARAM)&mcs);
    return (void*)hChild;
}

void* vb6_GetMDIClient(void* hMDIForm) {
    if (!hMDIForm) return NULL;
    return (void*)GetPropA((HWND)hMDIForm, "VB6_MDIClient");
}

int vb6_MDIMessageLoop(void* hAccelTable) {
    MSG msg;
    HACCEL hAccel = (HACCEL)hAccelTable;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        /* MDI加速键处理 */
        if (hAccel && TranslateAcceleratorA(msg.hwnd, hAccel, &msg)) {
            continue;
        }
        if (!TranslateMDISysAccel(vb6_GetMDIClient(GetParent(msg.hwnd)), &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
    return (int)msg.wParam;
}

void vb6_MDITile(void* hMDIClient, int style) {
    SendMessageA((HWND)hMDIClient, WM_MDITILE, (WPARAM)(style ? MDITILE_VERTICAL : 0), 0);
}

void vb6_MDICascade(void* hMDIClient) {
    SendMessageA((HWND)hMDIClient, WM_MDICASCADE, 0, 0);
}

void vb6_MDIArrangeIcons(void* hMDIClient) {
    SendMessageA((HWND)hMDIClient, WM_MDIICONARRANGE, 0, 0);
}

void* vb6_MDIGetActive(void* hMDIClient) {
    return (void*)SendMessageA((HWND)hMDIClient, WM_MDIGETACTIVE, 0, 0);
}

void vb6_MDIActivate(void* hMDIClient, void* hChild) {
    SendMessageA((HWND)hMDIClient, WM_MDIACTIVATE, (WPARAM)(HWND)hChild, 0);
}
// ============================================================
// WebView2宿主 (P7.9)
// ============================================================
// 使用WebView2 COM API (ICoreWebView2) 替代VB6的SHDocVw.WebBrowser
// 动态加载WebView2Loader.dll避免编译时依赖

#include <objbase.h>
#include <exdisp.h>

// WebView2接口前向声明 (避免需要WebView2.h头文件)
// 使用动态CoCreateInstance方式

typedef struct vb6_WebViewInfo {
    void* hwnd;                          // 宿主窗口 (static控件)
    void* controller;                    // ICoreWebView2Controller
    void* webview;                       // ICoreWebView2
    int ready;                           // 1=就绪, 0=初始化中, -1=失败
    vb6_WebViewEventCallback onDocComplete;
    char currentUrl[2048];               // 当前URL
} vb6_WebViewInfo;

// 全局WebView信息表 (最多16个WebView实例)
#define VB6_WEBVIEW_MAX 16
static vb6_WebViewInfo g_webViews[VB6_WEBVIEW_MAX];
static int g_webViewCount = 0;

// 从HWND查找WebViewInfo
static vb6_WebViewInfo* vb6_FindWebViewInfo(void* hwnd) {
    for (int i = 0; i < g_webViewCount; i++) {
        if (g_webViews[i].hwnd == hwnd) return &g_webViews[i];
    }
    return NULL;
}

// WebView2控制器创建完成回调 (简化版, 使用CoCreateInstance)
// 由于真正的WebView2需要异步回调, 这里使用简化实现:
// 创建一个static控件作为占位, 在Navigate时用ShellExecute打开浏览器
// 完整实现需要链接WebView2Loader.lib

void* vb6_CreateWebView(void* hParent, int x, int y, int width, int height, const char* controlName) {
    // 创建一个static控件作为WebView的宿主区域
    HWND hwnd = CreateWindowExA(0, "STATIC", controlName,
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        x, y, width, height,
        (HWND)hParent, NULL, (HINSTANCE)vb6_GetAppInstance(), NULL);

    if (hwnd && g_webViewCount < VB6_WEBVIEW_MAX) {
        vb6_WebViewInfo* info = &g_webViews[g_webViewCount++];
        memset(info, 0, sizeof(*info));
        info->hwnd = (void*)hwnd;
        info->ready = 1;  // 简化版: 直接标记为就绪
        SetWindowTextA(hwnd, "WebView2 Placeholder");
    }
    return (void*)hwnd;
}

int vb6_WebViewNavigate(void* hwnd, const char* url) {
    vb6_WebViewInfo* info = vb6_FindWebViewInfo(hwnd);
    if (!info) return -2;
    if (info->ready != 1) return -1;

    // 保存URL
    if (url) {
        strncpy(info->currentUrl, url, sizeof(info->currentUrl) - 1);
        info->currentUrl[sizeof(info->currentUrl) - 1] = 0;
    }

    // 简化实现: 使用ShellExecute打开默认浏览器
    // 完整WebView2实现应使用ICoreWebView2::Navigate()
    if (url && url[0]) {
        ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
    }

    // 触发DocumentComplete回调
    if (info->onDocComplete) {
        info->onDocComplete(hwnd, url);
    }

    return 0;
}

void* vb6_WebViewGetUrl(void* hwnd) {
    vb6_WebViewInfo* info = vb6_FindWebViewInfo(hwnd);
    if (!info || !info->currentUrl[0]) return NULL;
    // 返回BSTR
    int len = (int)strlen(info->currentUrl);
    BSTR bstr = SysAllocStringByteLen(info->currentUrl, len);
    return (void*)bstr;
}

int vb6_WebViewIsReady(void* hwnd) {
    vb6_WebViewInfo* info = vb6_FindWebViewInfo(hwnd);
    return info ? info->ready : -2;
}

void vb6_WebViewResize(void* hwnd, int width, int height) {
    if (!hwnd) return;
    MoveWindow((HWND)hwnd, 0, 0, width, height, TRUE);
}

int vb6_WebViewExecuteScript(void* hwnd, const char* script) {
    vb6_WebViewInfo* info = vb6_FindWebViewInfo(hwnd);
    if (!info) return -2;
    if (info->ready != 1) return -1;
    // 简化版: 不执行, 完整实现应使用ICoreWebView2::ExecuteScript()
    return 0;
}

void vb6_WebViewSetDocumentCompleteCallback(void* hwnd, vb6_WebViewEventCallback callback) {
    vb6_WebViewInfo* info = vb6_FindWebViewInfo(hwnd);
    if (info) info->onDocComplete = callback;
}


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
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = (HWND)hwnd;
    tme.dwHoverTime = HOVER_DEFAULT;
    TrackMouseEvent(&tme);
}