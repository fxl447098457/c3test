// VB6 Win32窓体运行时实现 (P7)
// 提供Win32窗口注册、创建、消息循环、控件管理等基础功能

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
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
// 控件数组 (P7.6)
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