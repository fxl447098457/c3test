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

static void vb6_formClassBg_set(const char* className, int bg);

int vb6_RegisterFormClassBg(const char* className, void* wndProc, void* hInstance,
                            int iconResId, int backColor) {
    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;  // 支持双击
    wc.lpfnWndProc = (WNDPROC)wndProc;
    wc.hInstance = (HINSTANCE)hInstance;
    wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    // czUI fix: .frm 的窗体级 BackColor — 用 .frm 颜色做类背景刷, 否则窗体永远
    // 是 BTNFACE 灰 (czForm Demo 深蓝底变灰底). backColor<0 = 未指定, 走 VB6 默认.
    if (backColor >= 0) {
        COLORREF cref = (backColor & 0x80000000L)
                            ? GetSysColor(backColor & 0xFF)
                            : (COLORREF)backColor;
        wc.hbrBackground = CreateSolidBrush(cref);
        vb6_formClassBg_set(className, (int)cref);
    } else {
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);  // VB6默认灰色背景
    }
    wc.lpszClassName = className;

    // czUI fix: 未指定图标时不再回退到 IDI_APPLICATION — VB6 窗体没有 Icon 属性时
    // 标题栏就是没有图标 (UserControl.Parent.Icon = Nothing, czUI 自绘标题栏因此
    // 不画图标); 类图标留空, 系统在任务栏等处自动用默认图标, 行为一致。
    if (iconResId > 0) {
        wc.hIcon = LoadIconA((HINSTANCE)hInstance, (LPCSTR)MAKEINTRESOURCEA(iconResId));
        wc.hIconSm = wc.hIcon;
    }

    if (!RegisterClassExA(&wc)) {
        return -1;
    }
    return 0;
}

int vb6_RegisterFormClass(const char* className, void* wndProc, void* hInstance, int iconResId) {
    return vb6_RegisterFormClassBg(className, wndProc, hInstance, iconResId, -1);
}

// ---- 窗体类背景色登记 (供 UserControl Ambient.BackColor 查询) ----
#define VB6_FORMBG_MAX 64
static struct { char className[128]; int bg; } g_formBg[VB6_FORMBG_MAX];
static int g_formBgCount = 0;

static void vb6_formClassBg_set(const char* className, int bg) {
    for (int i = 0; i < g_formBgCount; i++) {
        if (strcmp(g_formBg[i].className, className) == 0) { g_formBg[i].bg = bg; return; }
    }
    if (g_formBgCount < VB6_FORMBG_MAX) {
        snprintf(g_formBg[g_formBgCount].className, sizeof(g_formBg[0].className), "%s", className);
        g_formBg[g_formBgCount].bg = bg;
        g_formBgCount++;
    }
}

int vb6_Forms_QueryClassBg(const char* className) {
    for (int i = 0; i < g_formBgCount; i++) {
        if (strcmp(g_formBg[i].className, className) == 0) return g_formBg[i].bg;
    }
    return -1;
}

void* vb6_CreateFormWindowB(const char* className, const char* formName,
    int x, int y, int width, int height, void* hInstance, void* userData,
    int borderStyle);

void* vb6_CreateFormWindow(const char* className, const char* formName,
    int x, int y, int width, int height, void* hInstance, void* userData) {
    return vb6_CreateFormWindowB(className, formName, x, y, width, height,
                                 hInstance, userData, 2 /* Sizable */);
}

void* vb6_CreateFormWindowB(const char* className, const char* formName,
    int x, int y, int width, int height, void* hInstance, void* userData,
    int borderStyle) {
    // VB6坐标是缇, 转为像素
    int pw = vb6_TwipToX(width);
    int ph = vb6_TwipToY(height);

    // 创建窗口。
    // czUI fix: 尊重 .frm 的 BorderStyle — BorderStyle=0 (None) 是无系统标题栏
    // 的无边框窗口 (czUI 自绘标题栏此前叠在系统标题栏下面, 顶部多出一截)。
    // Fix 081k: Do NOT add WS_VISIBLE here; ShowWindow is called by vb6_ShowForm after Form_Load.
    // Fix 124: WS_CLIPCHILDREN —— 窗体自身重绘(背景填充)时必须把子控件区域裁剪掉,
    // 否则窗体重绘会把已经画好的子控件整片覆盖 (表现为"控件时有时无/干脆看不见",
    // 而离屏 dump 一切正常)。
    DWORD style;
    DWORD exStyle = 0;
    switch (borderStyle) {
        case 0:  /* None */
            style = WS_POPUP | WS_CLIPCHILDREN;
            exStyle = WS_EX_APPWINDOW;   /* 仍在任务栏显示 */
            break;
        case 1: case 3:  /* Fixed Single / Fixed Dialog */
            style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
            break;
        case 4: case 5:  /* ToolWindow */
            style = WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
            break;
        default:         /* 2 = Sizable (VB6 标准) */
            style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
            break;
    }

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

        /* Fix 145: ComboBox 下拉列表高度.
         * Win32 的 ComboBox 窗口高度 = 显示行 + 下拉列表高度; 而 .frm 里
         * ComboBox.Height 只是**显示区**的一行高度 (VB6 语义: 下拉部分由系统
         * 默认项数决定). 直接拿 .frm 高度当窗口高度会让下拉区 ≈ 0 —
         * 实测下拉弹出窗口只有 2px, 用户拉开只看到第一项.
         * 这里补足到 VB6 的观感: 下拉约显示 9~10 项 (VB6 ThunderRT6ComboBox
         * 实测下拉区 114px; 本机 itemH=12px → 10 项 ≈ 120px). */
        if (_stricmp(win32Class, "COMBOBOX") == 0) {
            int itemH = (int)SendMessage(hwnd, CB_GETITEMHEIGHT, 0, 0);
            if (itemH <= 0) itemH = (int)SendMessage(hwnd, CB_GETITEMHEIGHT, (WPARAM)-1, 0);
            if (itemH <= 0) itemH = 16;
            int listPx = ph - itemH;              /* .frm 高度里减去一行 = 下拉区 */
            if (listPx < itemH * 4) listPx = itemH * 10;  /* 不足则用 VB6 观感项数 */
            SetWindowPos(hwnd, NULL, 0, 0, pw, itemH + listPx,
                         SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            if (GetEnvironmentVariableW(L"C3_OCX_TRACE", NULL, 0) > 0) {
                RECT rr; GetWindowRect(hwnd, &rr);
                fprintf(stderr, "[C3_FIX] ComboBox itemH=%d frmH(px)=%d setH=%d actualH=%d\n",
                        itemH, ph, itemH + listPx, (int)(rr.bottom - rr.top));
            }
        }
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
// Sub Main 驻留判据 (Fix 167)
// ============================================================
// VB6 语义: `Sub Main` 返回后进程**不退出**, 运行时继续泵消息, 直到所有窗体关闭
// (或显式 End)。此前 codegen 的 Sub Main 入口模板调完 Main 直接 vb6_Exit()+return,
// 于是 `Load` 出 modeless 窗体的工程一返回就干净退出 (退出码 0) —— 表现为
// "窗口闪一下就没了", VBFlexGridDemo 正是如此。
// 判据用"本线程有没有可见窗口", 而不是另建窗体注册表: 窗体是本 RTL 在
// vb6_CreateFormWindowB 里以 RegisterClass("VB6_Form_<X>") 建的普通窗口, 归本线程所有;
// 而 `App.PrevInstance` 那一支 (激活前一个实例后返回) 只操作**别的进程**的 hwnd,
// 本线程没有窗口 → 不会误驻留。纯 .bas 控制台工程同样没有可见窗口 → 不受影响。
static BOOL CALLBACK vb6_EnumAnyVisibleWindow(HWND hwnd, LPARAM lParam) {
    if (IsWindowVisible(hwnd)) { *(int*)lParam = 1; return FALSE; }
    return TRUE;
}

int vb6_AnyThreadWindowVisible(void) {
    int found = 0;
    EnumThreadWindows(GetCurrentThreadId(), vb6_EnumAnyVisibleWindow, (LPARAM)&found);
    return found;
}

// ============================================================
// 消息循环
// ============================================================

int vb6_MessageLoop(void) {
    MSG msg;
    // czUI fix: 消息循环启动后设计器 Timer 才允许触发 (VB6 语义: Timer 事件
    // 排队等消息循环; 否则处理器在 Form_Load 前对未就绪实例运行 → AV)
    extern int vb6_uc_timersStarted;  // czUI fix (定义在 vb6forms_uc.c)
    vb6_uc_timersStarted = 1;
    while (GetMessage(&msg, NULL, 0, 0)) {
        // P24-Timer: WM_TIMER现在由WndProc分发, 消息循环不再拦截
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (GetEnvironmentVariableW(L"C3_OCX_TRACE", NULL, 0) > 0)
        fprintf(stderr, "[C3_MODAL] 主消息循环结束: msg=0x%04X hwnd=%p\n",
                msg.message, (void*)msg.hwnd);
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

// Fix 149 诊断: C3_CRASH_TRACE=1 时安装未处理异常过滤器, 把崩溃栈各帧的
// 「模块+偏移」写进 c3_crash.txt。没有调试器也能一眼看出异常是从
// Test.exe 自己的代码抛的, 还是逃出第三方 OCX (NewTab01.ocx) 的 VB6 代码。
static LONG WINAPI vb6_crashFilter(EXCEPTION_POINTERS* ep) {
    FILE* f = fopen("c3_crash.txt", "a");
    if (!f) return EXCEPTION_EXECUTE_HANDLER;
    fprintf(f, "=== EXCEPTION code=0x%08lX addr=%p ===\n",
            (unsigned long)ep->ExceptionRecord->ExceptionCode,
            ep->ExceptionRecord->ExceptionAddress);
    {   /* 触发地址所在模块 */
        HMODULE hm = NULL;
        wchar_t wp[MAX_PATH] = {0};
        char nm[80] = "?";
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCWSTR)ep->ExceptionRecord->ExceptionAddress, &hm) && hm) {
            GetModuleFileNameW(hm, wp, MAX_PATH);
            { const wchar_t* b = wcsrchr(wp, L'\\');
              WideCharToMultiByte(CP_ACP, 0, b ? b + 1 : wp, -1, nm, sizeof(nm), NULL, NULL); }
            fprintf(f, "  FAULT %s+0x%llX\n", nm,
                    (unsigned long long)((const char*)ep->ExceptionRecord->ExceptionAddress
                                         - (const char*)hm));
        } else {
            fprintf(f, "  FAULT (unknown module) %p\n", ep->ExceptionRecord->ExceptionAddress);
        }
    }
    {   void* frames[48];
        USHORT n = RtlCaptureStackBackTrace(0, 48, frames, NULL), i;
        for (i = 0; i < n; i++) {
            HMODULE hm = NULL;
            wchar_t wp[MAX_PATH] = {0};
            char nm[80] = "?";
            if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                   GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   (LPCWSTR)frames[i], &hm) && hm) {
                GetModuleFileNameW(hm, wp, MAX_PATH);
                { const wchar_t* b = wcsrchr(wp, L'\\');
                  WideCharToMultiByte(CP_ACP, 0, b ? b + 1 : wp, -1, nm, sizeof(nm), NULL, NULL); }
                fprintf(f, "  #%02d 0x%p  %s+0x%llX\n", i, frames[i], nm,
                        (unsigned long long)((const char*)frames[i] - (const char*)hm));
            } else {
                fprintf(f, "  #%02d 0x%p  (unknown)\n", i, frames[i]);
            }
        }
    }
    fflush(f);
    fclose(f);
    return EXCEPTION_EXECUTE_HANDLER;
}

static void vb6_installCrashTrace(void) {
    static int done = 0;
    if (done) return;
    done = 1;
    if (GetEnvironmentVariableW(L"C3_CRASH_TRACE", NULL, 0) > 0)
        SetUnhandledExceptionFilter(vb6_crashFilter);
}

void vb6_ShowForm(void* hwnd, int modal) {
    vb6_installCrashTrace();
    if (GetEnvironmentVariableW(L"C3_OCX_TRACE", NULL, 0) > 0) {
        char cap[160] = {0};
        GetWindowTextA((HWND)hwnd, cap, 159);
        fprintf(stderr, "[C3_MODAL] ShowForm hwnd=%p modal=%d cap='%s'\n", hwnd, modal, cap);
    }
    if (!hwnd) return;

    // Fix 115: 恢复 VB6 的 "先 Form_Load, 后 Show" 顺序。
    // 编译器把 Form_Load 用 PostMessageA(hwnd, 0x7FF0, 0, 0) 延迟到消息队列
    // (见 cgen_form_wndproc_create.inc 的 WM_CREATE 处理), 而这里的
    // ShowWindow/UpdateWindow 会在队列消息派发**之前**强制首次 WM_PAINT。
    // 于是 UserControl 的 Draw 会在"Form_Load 尚未添加数据系列"的状态下执行,
    // 对空数组取 m_Serie(0) → 空指针崩溃 (Charts 2020 ucChartArea 在
    // LegendAlign=LA_TOP 时必经该分支)。先把挂起的延迟 Form_Load 派发掉。
    {
        const UINT kDeferredFormLoad = 0x7FF0;   // 编译器生成的"延迟 Form_Load"消息
        MSG msg;
        while (PeekMessageA(&msg, (HWND)hwnd, kDeferredFormLoad, kDeferredFormLoad, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    ShowWindow((HWND)hwnd, SW_SHOWDEFAULT);
    UpdateWindow((HWND)hwnd);

    // Fix 137: VB6 启动的窗体会出现在最前并获得焦点; 我们只 ShowWindow 的话
    // 窗口常落在已有窗口后面 (双击 exe 时尤甚 —— 没有前台权限继承)。
    // 显式提到顶层并请求前台 (SetForegroundWindow 受系统限制时 BringWindowToTop
    // 至少保证同 Z 序应用内最前)。
    BringWindowToTop((HWND)hwnd);
    if (SetForegroundWindow((HWND)hwnd) == 0) {
        /* 前台锁: 挂到当前前台线程的输入队列再试一次 (经典 workaround) */
        HWND fg = GetForegroundWindow();
        DWORD fgTid = fg ? GetWindowThreadProcessId(fg, NULL) : 0;
        DWORD myTid = GetCurrentThreadId();
        if (fgTid && fgTid != myTid &&
            AttachThreadInput(myTid, fgTid, TRUE)) {
            BringWindowToTop((HWND)hwnd);
            SetForegroundWindow((HWND)hwnd);
            SetFocus((HWND)hwnd);
            AttachThreadInput(myTid, fgTid, FALSE);
        }
    }
    SetActiveWindow((HWND)hwnd);

    if (modal) {
        int traceModal = (GetEnvironmentVariableW(L"C3_OCX_TRACE", NULL, 0) > 0);
        // 模态窗体: 禁用所有者, 进入本地消息循环
        HWND owner = GetWindow((HWND)hwnd, GW_OWNER);
        if (owner) {
            g_modalOwner = owner;
            EnableWindow(owner, FALSE);
        }

        // 本地消息循环 (直到窗体被销毁)
        MSG msg;
        int traceMsg = traceModal;
        int msgCount = 0;
        /* Fix 144b: IsDialogMessageA 会吞掉非对话框窗口的键盘/命令消息, 可用
         * C3_OCX_NO_DLGMSG=1 关闭 (对照实验/排障). */
        int useDlgMsg = (GetEnvironmentVariableW(L"C3_OCX_NO_DLGMSG", NULL, 0) <= 0);
        while (IsWindow((HWND)hwnd) && GetMessage(&msg, NULL, 0, 0)) {
            if (traceMsg && msgCount < 80) {
                wchar_t cap[128] = {0};
                GetWindowTextW((HWND)hwnd, cap, 128);
                fprintf(stderr, "[C3_MODAL] msg[%d] 0x%04X hwnd=%p wp=%p lp=%p | ownerWin alive=%d cap='%ls'\n",
                        msgCount, msg.message, (void*)msg.hwnd,
                        (void*)msg.wParam, (void*)msg.lParam,
                        IsWindow((HWND)hwnd) ? 1 : 0, cap);
            }
            msgCount++;
            // P24-Timer: WM_TIMER现在由WndProc分发, 模态循环不再拦截
            // 模态Tab键导航 (IsDialogMessage处理对话框键盘导航)
            if (!useDlgMsg || !IsDialogMessageA((HWND)hwnd, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            if (msg.message == WM_QUIT && traceModal)
                fprintf(stderr, "[C3_MODAL] 收到 WM_QUIT, 模态循环结束 (hwnd=%p)\n", hwnd);
        }
        if (traceModal)
            fprintf(stderr, "[C3_MODAL] 模态循环退出: IsWindow=%d (hwnd=%p, owner=%p)\n",
                    IsWindow((HWND)hwnd) ? 1 : 0, hwnd, owner);

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
    if (GetEnvironmentVariableW(L"C3_OCX_TRACE", NULL, 0) > 0)
        fprintf(stderr, "[C3_MODAL] UnloadForm hwnd=%p\n", hwnd);
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
