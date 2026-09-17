// vb6forms_ctrlarr.c - vb6forms 模块拆分: 控件数组 + MDI 窗体
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
    int pw = vb6_TwipToX(width);
    int ph = vb6_TwipToY(height);
    DWORD mdiStyle = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    // Adjust for non-client area to get actual window size
    RECT rcM = {0, 0, pw, ph};
    AdjustWindowRectEx(&rcM, mdiStyle, FALSE, 0);
    int winW = rcM.right - rcM.left;
    int winH = rcM.bottom - rcM.top;
    int px, py;
    if (x == -1 && y == -1) {
        // M22-Issue2: CenterScreen
        px = (GetSystemMetrics(SM_CXSCREEN) - winW) / 2;
        py = (GetSystemMetrics(SM_CYSCREEN) - winH) / 2;
    } else {
        px = x;
        py = y;
    }
    HWND hwnd = CreateWindowExA(
        0, className, formName,
        mdiStyle,
        px, py,
        winW, winH,
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
    int pw = vb6_TwipToX(width);
    int ph = vb6_TwipToY(height);
    DWORD childStyle = WS_CHILD | WS_CLIPCHILDREN | WS_SYSMENU | WS_CAPTION | WS_THICKFRAME;
    RECT rcC = {0, 0, pw, ph};
    AdjustWindowRectEx(&rcC, childStyle, FALSE, 0);
    int winW = rcC.right - rcC.left;
    int winH = rcC.bottom - rcC.top;
    int px, py;
    if (x == -1 && y == -1) {
        // M22-Issue2: CenterScreen
        px = (GetSystemMetrics(SM_CXSCREEN) - winW) / 2;
        py = (GetSystemMetrics(SM_CYSCREEN) - winH) / 2;
    } else {
        px = (x == CW_USEDEFAULT) ? CW_USEDEFAULT : vb6_TwipToX(x);
        py = (y == CW_USEDEFAULT) ? CW_USEDEFAULT : vb6_TwipToY(y);
    }
    MDICREATESTRUCTA mcs = {0};
    mcs.szClass = className;
    mcs.szTitle = formName;
    mcs.x = px;
    mcs.y = py;
    mcs.cx = winW;
    mcs.cy = winH;
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
