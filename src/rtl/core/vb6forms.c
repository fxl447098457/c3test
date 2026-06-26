// VB6 Win32窗体运行时实现 (P7)
// 提供Win32窗口注册、创建、消息循环、控件管理等基础功能

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "vb6forms.h"
#include <stdio.h>

// 全局变量
static HINSTANCE g_hInstance = NULL;
static int g_nextControlId = 100;  // 控件ID从100开始 (1-99保留给菜单)

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
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);  // VB6默认灰色背景
    wc.lpszClassName = className;

    if (iconResId > 0) {
        wc.hIcon = LoadIconA((HINSTANCE)hInstance, MAKEINTRESOURCEA(iconResId));
    } else {
        wc.hIcon = LoadIconA(NULL, IDI_APPLICATION);
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
    // VB6窗体样式: 标题栏+系统菜单+最小化/最大化按钮+可调边框
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
            // 回退: 创建MS Sans Serif 8pt字体
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
// 消息循环
// ============================================================

int vb6_MessageLoop(void) {
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

int vb6_DoEvents(void) {
    MSG msg;
    int count = 0;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        count++;
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
        // 模态: 禁用父窗口, 进入消息循环直到窗体关闭
        // 简化实现: 暂不支持模态
    }
}

void vb6_UnloadForm(void* hwnd) {
    if (!hwnd) return;
    DestroyWindow((HWND)hwnd);
}
