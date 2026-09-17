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
#include "vb6forms_internal.h"
#include <stdio.h>
#include <stdarg.h>

#include <stdlib.h>   /* malloc, free */
#include <oleauto.h>  /* SysAllocString, BSTR */
#include <olectl.h>   /* IPicture, OleLoadPicture, OLE_HANDLE */

// 全局变量
HINSTANCE g_hInstance = NULL;
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
    HWND hwnd;                  // P24-Timer: 关联的窗体句柄 (窗口关联定时器)
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
    int pw = vb6_TwipToX(width);
    int ph = vb6_TwipToY(height);

    // 创建窗口, 使用WS_OVERLAPPEDWINDOW样式 (VB6标准窗口)
    // Fix 081k: Do NOT add WS_VISIBLE here; ShowWindow is called by vb6_ShowForm after Form_Load.
    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD exStyle = 0;

    // 调整窗口大小使客户区匹配指定大小
    RECT rc = {0, 0, pw, ph};
    AdjustWindowRectEx(&rc, style, FALSE, exStyle);
    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;

    int px, py;
    if (x == -1 && y == -1) {
        // M22-Issue2: CenterScreen -- 用窗口实际尺寸计算居中位置
        px = (GetSystemMetrics(SM_CXSCREEN) - winW) / 2;
        py = (GetSystemMetrics(SM_CYSCREEN) - winH) / 2;
    } else {
        px = (x == CW_USEDEFAULT) ? CW_USEDEFAULT : vb6_TwipToX(x);
        py = (y == CW_USEDEFAULT) ? CW_USEDEFAULT : vb6_TwipToY(y);
    }

    HWND hwnd = CreateWindowExA(
        exStyle,
        className,
        formName,
        style,
        px, py,
        winW, winH,
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

    // Fix 081l: Use CreateWindowExA to match the ANSI form window registered with RegisterClassExA.
    // Mixing CreateWindowExW controls with CreateWindowExA forms causes ANSI/Unicode mismatch
    // that can lead to heap corruption when sending text messages.
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

int vb6_SetTimer(void* hwnd, int interval, void* callback) {
    if (g_timerCount >= VB6_MAX_TIMERS) return -1;
    int id = g_nextControlId++;
    g_timerTable[g_timerCount].timerId = id;
    g_timerTable[g_timerCount].hwnd = (HWND)hwnd;
    g_timerTable[g_timerCount].callback = (vb6_TimerCallback)callback;
    g_timerCount++;
    // P24-Timer: 使用窗口关联定时器, WM_TIMER由WndProc分发
    SetTimer((HWND)hwnd, id, interval, NULL);
    return id;
}

void vb6_KillTimer(int timerId) {
    // P24-Timer: 鏌ユ壘鍏宠仈hwnd鐢ㄤ簬KillTimer
    HWND killHwnd = NULL;
    for (int i = 0; i < g_timerCount; i++) {
        if (g_timerTable[i].timerId == timerId) {
            killHwnd = g_timerTable[i].hwnd;
            break;
        }
    }
    KillTimer(killHwnd, timerId);
    // 从回调表移除
    for (int i = 0; i < g_timerCount; i++) {
        if (g_timerTable[i].timerId == timerId) {
            g_timerTable[i] = g_timerTable[g_timerCount - 1];
            g_timerCount--;
            break;
        }
    }
}

// P24-Timer: WndProc中分发WM_TIMER (替代消息循环拦截)
void vb6_DispatchTimer(int timerId) {
    for (int i = 0; i < g_timerCount; i++) {
        if (g_timerTable[i].timerId == timerId) {
            if (g_timerTable[i].callback) {
                g_timerTable[i].callback();
            }
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
        // P24-Timer: WM_TIMER现在由WndProc分发, 消息循环不再拦截
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

int vb6_DoEvents(void) {
    MSG msg;
    int count = 0;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        // P24-Timer: WM_TIMER现在由WndProc分发, DoEvents不再拦截
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
            // P24-Timer: WM_TIMER现在由WndProc分发, 模态循环不再拦截
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

// M22-Issue6: 窗体表面Print
// VB6的"Print expr"语句在窗体表面绘制文本
// 维护CurrentX/CurrentY用于定位下一次输出
void vb6_Form_Print(void* hwnd, void* bstrText) {
    if (!hwnd) return;
    HWND hw = (HWND)hwnd;
    BSTR text = (BSTR)bstrText;
    
    // Get CurrentX/CurrentY from window properties (stored as pixels)
    float currentX = vb6_GetCurrentX(hwnd);
    float currentY = vb6_GetCurrentY(hwnd);
    
    HDC hdc = GetDC(hw);
    if (!hdc) return;
    
    // Set text color and background mode (transparent for form printing)
    SetBkMode(hdc, TRANSPARENT);
    
    int len = text ? (int)SysStringLen(text) : 0;
    if (len > 0) {
        // Calculate text size for advancing CurrentX
        SIZE size;
        TEXTMETRICW tm;
        GetTextExtentPoint32W(hdc, text, len, &size);
        GetTextMetricsW(hdc, &tm);
        
        // Draw text at CurrentX, CurrentY
        TextOutW(hdc, (int)currentX, (int)currentY, text, len);
        
        // VB6 behavior: Print automatically advances to next line (newline)
        // CurrentY += line height, CurrentX reset to 0
        vb6_SetCurrentY(hwnd, currentY + (float)tm.tmHeight);
        vb6_SetCurrentX(hwnd, 0.0f);
    } else {
        // Empty Print = newline: advance CurrentY by font height, reset CurrentX
        TEXTMETRICW tm;
        GetTextMetricsW(hdc, &tm);
        vb6_SetCurrentY(hwnd, currentY + (float)tm.tmHeight);
        vb6_SetCurrentX(hwnd, 0.0f);
    }
    
    ReleaseDC(hw, hdc);
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
