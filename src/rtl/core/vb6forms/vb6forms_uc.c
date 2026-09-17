// vb6forms_uc.c - Fix 112: 工程内 UserControl (.ctl) 实例宿主 + 窗体/控件宿主对象模型
//
// 背景 (Charts 2020): 窗体上的子控件是**工程内 UserControl** (`Begin Proyecto1.ucChartBar
// ucChartBar1`)。此前 cgen 对这类控件什么也不生成 → `vb6_hwnd_ucChartBar1` 恒为 NULL,
// `ucChartBar1.AddSerie(...)` 全部落到 `vb6_ComCall(NULL, ...)` 被静默丢弃 → 白板窗体。
// 另外 `cResizer.SaveControlsPositions Me` 把窗体 `Me`(=HWND) 当 IDispatch 用
// (`.ScaleWidth/.Count/.Controls`) → `vb6_ComGetDoubleProp(HWND, ...)` 解引用野指针 → 0xC0000005。
//
// 本文件提供两块能力:
//   (1) UserControl 实例宿主: 为每个 .ctl 实例建一个真实子窗口, WM_PAINT 转发到
//       UserControl_Paint, 并把进程级 `vb6_UserControl_*` 宿主状态换入/换出;
//   (2) 宿主对象模型: 在 `vb6_Com*` 入口之前拦截「HWND/集合/字体」这类非 IDispatch
//       指针, 用 Win32 语义回答 VB6 属性/方法 (ScaleWidth/hwnd/Controls/Left/...),
//       使 `Me` 与标准控件可以像对象一样被晚期绑定代码使用。
//
// 生成代码接口 (cgen 发射):
//   vb6_UC_Register(&desc)                       — .ctl 模块自注册
//   vb6_UC_HostCreate(type, x,y,w,h, parent, hInst, ctrlName)
//   vb6_UC_InstanceOf(hwnd) / vb6_UC_HwndOf(inst)
//   vb6_Forms_RegisterControl(hwnd, name, vbTypeName, index)
//   vb6_Host_*  — 由 vb6com_* / vb6_CallByName 内部调用

#include "vb6forms.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include "vb6rtl.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// UserControl 宿主描述 (生成代码为每个 .ctl 提供)
// ============================================================

/* vb6_UserControlDesc 定义见 vb6forms.h (生成代码与 RTL 共用同一份) */

#define VB6_UC_MAX_DESC 32
#define VB6_UC_MAX_INST 64
#define VB6_UC_MAX_OBJ  128
#define VB6_UC_NAME_LEN 64

static const vb6_UserControlDesc* g_uc_descs[VB6_UC_MAX_DESC];
static int32_t g_uc_descCount = 0;

typedef struct vb6_UCRec {
    const vb6_UserControlDesc* desc;
    void*  me;
    int32_t ready;        // Initialize 完成前禁止窗口消息进入 VB6 实例方法
    HWND   hwnd;
    HWND   parent;
    int32_t scaleWidth;   // ScaleMode 单位
    int32_t scaleHeight;
    HDC    hdc;
    int16_t enabled;
    void*  font;          // vb6_ComIface_Font*
    int32_t extLeft;      // Extender.Left/Top (容器坐标, 缇)
    int32_t extTop;
    int32_t index;        // 控件数组下标, -1=非数组
    wchar_t ctrlName[VB6_UC_NAME_LEN];
} vb6_UCRec;

static vb6_UCRec g_uc_recs[VB6_UC_MAX_INST];
static int32_t g_uc_recCount = 0;
// Fix 112: 可选生命周期日志；每行立即关闭文件，异常退出也保留最后阶段。
static void vb6_uc_trace(const char* phase, const char* type, void* me) {
    const char* path = getenv("C3_UC_TRACE");
    if (!path || !*path) return;
    FILE* f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%lu %s %s me=%p\n", (unsigned long)GetTickCount(), phase, type ? type : "?", me);
    fclose(f);
}

static vb6_UCRec* g_uc_current = NULL;   // 最近一次进入的实例 (供 Refresh/PropertyChange)

// ============================================================
// 宿主对象登记表: hwnd → VB6 名 / 类型名
// ============================================================

typedef struct vb6_HostObjRec {
    void*   hwnd;
    wchar_t name[VB6_UC_NAME_LEN];
    wchar_t typeName[VB6_UC_NAME_LEN];
    int32_t isForm;
    int32_t index;        // 控件数组下标 (-1=非数组)
    void*   instance;     // 工程 UserControl 实例, 否则 NULL
} vb6_HostObjRec;

static vb6_HostObjRec g_ho[VB6_UC_MAX_OBJ];
static int32_t g_hoCount = 0;

// 合成对象: Controls 集合
typedef struct vb6_UCControls {
    int32_t tag;          // 0x0C70C70C
    void*   formHwnd;
} vb6_UCControls;
#define VB6_UC_CONTROLS_TAG 0x0C70C70C

// 合成对象: Font (stdole.StdFont 的最小形态)
typedef struct vb6_UCFontRec {
    int32_t tag;          // 0xF0A7F0A7
    void*  font;          // vb6_ComIface_Font*
    struct vb6_UCFontRec* next;
} vb6_UCFontRec;
static vb6_UCFontRec* g_uc_fonts;
extern vb6_ComIface_Font g_vb6_UserControl_FontObj;
static int32_t vb6_uc_isColl(const void* p);
#define VB6_UC_FONT_TAG 0xF0A7F0A7

// Fix 112c: 前置声明 (定义在宿主分派节, Collection 实现会用到)
static void vb6_ho_setVariantEmpty(vb6_VARIANT* out);

// 安全解引用守卫: 控件/窗体对象以 HWND 形式传入, 而 HWND 是内核句柄而非用户指针;
// 直接按 tag 结构解引用会触发 0xC0000005. 解引用 tag 前先校验目标地址可读.
static int32_t vb6_uc_ptrReadable(const void* p, size_t n) {
    if (!p) return 0;
#ifdef _WIN32
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0) return 0;
    if (mbi.State != MEM_COMMIT) return 0;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return 0;
    return ((const char*)mbi.BaseAddress + mbi.RegionSize) >= ((const char*)p + n);
#else
    (void)n;
    return 1;
#endif
}

static int32_t vb6_uc_isControls(const void* p) {
    return vb6_uc_ptrReadable(p, sizeof(int32_t)) &&
           ((const vb6_UCControls*)p)->tag == VB6_UC_CONTROLS_TAG;
}
static int32_t vb6_uc_isFont(const void* p) {
    // Fix 112e: 先按身份识别裸字体；不可将 Name 字段当 IDispatch vtable。
    if (!p) return 0;
    if (p == &g_vb6_UserControl_FontObj) return 1;
    for (vb6_UCFontRec* f = g_uc_fonts; f; f = f->next)
        if (p == f->font || p == f) return 1;
    return 0;
}

static vb6_HostObjRec* vb6_ho_find(const void* hwnd) {
    for (int32_t i = 0; i < g_hoCount; i++) {
        if (g_ho[i].hwnd == hwnd) return &g_ho[i];
    }
    return NULL;
}

static vb6_UCRec* vb6_uc_findByHwnd(const void* hwnd) {
    for (int32_t i = 0; i < g_uc_recCount; i++) {
        if ((void*)g_uc_recs[i].hwnd == hwnd) return &g_uc_recs[i];
    }
    return NULL;
}

static vb6_UCRec* vb6_uc_findByInstance(const void* inst) {
    for (int32_t i = 0; i < g_uc_recCount; i++) {
        if (g_uc_recs[i].me == inst) return &g_uc_recs[i];
    }
    return NULL;
}

// ============================================================
// 宿主状态换入/换出 (进程级 vb6_UserControl_* 全局)
// ============================================================

typedef struct vb6_UCSaved {
    int32_t scaleWidth, scaleHeight, scaleMode;
    void*   hDC;
    int32_t containerHwnd;
    int16_t enabled;
    void*   font;
    void*   ambientFont;
    int32_t extLeft, extTop;
    vb6_UCRec* current;
} vb6_UCSaved;

static void vb6_uc_defaultFont(void) {
    // vb6_UserControl_Font / vb6_Ambient_Font 为空时补一个默认字体对象
    if (!vb6_UserControl_Font) {
        vb6_UserControl_Font = (vb6_ComIface_Font*)vb6_UC_NewFont();
        vb6_Ambient_Font = vb6_UserControl_Font;
    }
    if (vb6_UserControl_Font && !vb6_UserControl_Font->Name)
        vb6_UserControl_Font->Name = vb6_BSTR_FromStr(L"MS Sans Serif");
    if (!vb6_Ambient_Font) vb6_Ambient_Font = vb6_UserControl_Font;
}

static void vb6_uc_push(vb6_UCRec* r, vb6_UCSaved* saved) {
    saved->scaleWidth = vb6_UserControl_ScaleWidth;
    saved->scaleHeight = vb6_UserControl_ScaleHeight;
    saved->scaleMode = vb6_UserControl_ScaleMode;
    saved->hDC = vb6_UserControl_hDC;
    saved->containerHwnd = vb6_UserControl_ContainerHwnd;
    saved->enabled = vb6_UserControl_Enabled;
    saved->font = vb6_UserControl_Font;
    saved->ambientFont = vb6_Ambient_Font;
    saved->extLeft = vb6_Extender_Left;
    saved->extTop = vb6_Extender_Top;
    saved->current = g_uc_current;

    vb6_uc_defaultFont();
    vb6_UserControl_ScaleWidth = r->scaleWidth;
    vb6_UserControl_ScaleHeight = r->scaleHeight;
    vb6_UserControl_ScaleMode = r->desc ? r->desc->scaleMode : 1;
    vb6_UserControl_hDC = r->hdc;
    vb6_UserControl_ContainerHwnd = (int32_t)(intptr_t)r->parent;
    vb6_UserControl_Enabled = r->enabled;
    if (r->font) {
        vb6_UserControl_Font = (vb6_ComIface_Font*)r->font;
        vb6_Ambient_Font = (vb6_ComIface_Font*)r->font;
    }
    vb6_Extender_Left = r->extLeft;
    vb6_Extender_Top = r->extTop;
    g_uc_current = r;
}

static void vb6_uc_pop(const vb6_UCSaved* saved) {
    vb6_UserControl_ScaleWidth = saved->scaleWidth;
    vb6_UserControl_ScaleHeight = saved->scaleHeight;
    vb6_UserControl_ScaleMode = saved->scaleMode;
    vb6_UserControl_hDC = saved->hDC;
    vb6_UserControl_ContainerHwnd = saved->containerHwnd;
    vb6_UserControl_Enabled = saved->enabled;
    vb6_UserControl_Font = (vb6_ComIface_Font*)saved->font;
    vb6_Ambient_Font = (vb6_ComIface_Font*)saved->ambientFont;
    vb6_Extender_Left = saved->extLeft;
    vb6_Extender_Top = saved->extTop;
    g_uc_current = saved->current;
}

// 仅换入不换出 — 供「窗体代码直接调用控件公开方法」路径使用
// (vb6_ucChartBar_AddSerie(...) 内部会读 ScaleWidth / 调 UserControl.Refresh)。
void vb6_UC_Enter(void* hwnd) {
    vb6_UCRec* r = vb6_uc_findByHwnd(hwnd);
    if (!r) return;
    vb6_UCSaved saved;
    vb6_uc_push(r, &saved);   // 有意不弹栈: "当前实例"语义 = 最近进入者
}

void vb6_UC_RefreshCurrent(void) {
    if (g_uc_current && g_uc_current->hwnd) {
        InvalidateRect(g_uc_current->hwnd, NULL, FALSE);
        UpdateWindow(g_uc_current->hwnd);
    }
}

// ============================================================
// 宿主窗口过程
// ============================================================

static LRESULT CALLBACK vb6_uc_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    vb6_UCRec* r = (vb6_UCRec*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
        case WM_NCCREATE: {
            CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
            break;
        }
        case WM_SIZE: {
            if (!r || !r->ready) break;
            r->scaleWidth = (int32_t)LOWORD(lParam);
            r->scaleHeight = (int32_t)HIWORD(lParam);
            if (r->desc && r->desc->resize) {
                vb6_UCSaved saved;
                vb6_uc_push(r, &saved);
                vb6_uc_trace("resize.begin", r->desc->typeName, r->me);
                r->desc->resize(r->me);
                vb6_uc_trace("resize.end", r->desc->typeName, r->me);
                vb6_uc_pop(&saved);
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_SHOWWINDOW:
            if (r && r->ready && wParam && r->desc && r->desc->show) {
                vb6_UCSaved saved;
                vb6_uc_push(r, &saved);
                vb6_uc_trace("show.begin", r->desc->typeName, r->me);
                r->desc->show(r->me);
                vb6_uc_trace("show.end", r->desc->typeName, r->me);
                vb6_uc_pop(&saved);
            }
            return 0;
        case WM_PAINT: {
            if (!r || !r->ready) break;
            if (!r->desc || !r->desc->paint) break;
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            r->hdc = hdc;
            vb6_UCSaved saved;
            vb6_uc_push(r, &saved);
            vb6_uc_trace("paint.begin", r->desc->typeName, r->me);
            r->desc->paint(r->me);
            vb6_uc_trace("paint.end", r->desc->typeName, r->me);
            vb6_uc_pop(&saved);
            r->hdc = NULL;
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            if (r && r->ready) {
                // 用控件 BackColor 填充, 避免闪烁/黑底
                RECT rc; GetClientRect(hwnd, &rc);
                HBRUSH b = CreateSolidBrush(RGB(255, 255, 255));
                FillRect((HDC)wParam, &rc, b);
                DeleteObject(b);
            }
            return 1;
        case WM_DESTROY:
            if (r && r->desc && r->desc->terminate) {
                vb6_UCSaved saved;
                vb6_uc_push(r, &saved);
                r->desc->terminate(r->me);
                vb6_uc_pop(&saved);
            }
            break;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Fix 112b: GDI+ 进程级初始化. 图表控件的绘制走 GdipCreateFromHDC, 而
// Charts 2020 自带的 ManageGDIToken 在打完补丁后会 GdiplusShutdown 掉它自己
// 启动的 token —— RTL 这里保一个常驻 token, 保证绘制始终可用.
// gdiplus.h 是 C++ 专用头, 这里按桩文件的做法用 LoadLibrary + GetProcAddress.
typedef struct vb6_GdiplusStartupInput {
    uint32_t GdiplusVersion;
    void*    DebugEventCallback;
    int32_t  SuppressBackgroundThread;
    int32_t  SuppressExternalCodecs;
} vb6_GdiplusStartupInput;

static void vb6_uc_gdiplusInit(void) {
    static int done = 0;
    if (done) return;
    done = 1;
    HMODULE mod = LoadLibraryA("gdiplus.dll");
    if (!mod) return;
    long (__stdcall *pStartup)(ULONG_PTR*, const void*, void*) =
        (long (__stdcall *)(ULONG_PTR*, const void*, void*))GetProcAddress(mod, "GdiplusStartup");
    if (!pStartup) return;
    vb6_GdiplusStartupInput si;
    memset(&si, 0, sizeof(si));
    si.GdiplusVersion = 1;
    ULONG_PTR token = 0;
    pStartup(&token, &si, NULL);   /* 常驻 token, 不配对 Shutdown */
}

static void vb6_uc_registerClass(HINSTANCE hInst) {
    static int done = 0;
    if (done) return;
    WNDCLASSW wc;
    memset(&wc, 0, sizeof(wc));
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = vb6_uc_wndproc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"VB6_UserControlHost";
    RegisterClassW(&wc);
    done = 1;
}

// ============================================================
// 公开 API
// ============================================================

void vb6_UC_Register(const vb6_UserControlDesc* desc) {
    if (!desc || !desc->typeName) return;
    for (int32_t i = 0; i < g_uc_descCount; i++) {
        if (_stricmp(g_uc_descs[i]->typeName, desc->typeName) == 0) return;  // 已注册
    }
    if (g_uc_descCount < VB6_UC_MAX_DESC) g_uc_descs[g_uc_descCount++] = desc;
}

static const vb6_UserControlDesc* vb6_uc_findDesc(const char* typeName) {
    if (!typeName) return NULL;
    const char* dot = strrchr(typeName, '.');
    if (dot) typeName = dot + 1;   // 接受 "Proyecto1.ucChartBar"
    for (int32_t i = 0; i < g_uc_descCount; i++) {
        if (_stricmp(g_uc_descs[i]->typeName, typeName) == 0) return g_uc_descs[i];
    }
    return NULL;
}

void* vb6_UC_HostCreate(const char* typeName, int32_t left, int32_t top,
                        int32_t width, int32_t height, void* hParent, void* hInstance,
                        const char* ctrlName, int32_t index) {
    vb6_uc_trace("create.begin", typeName, NULL);
    const vb6_UserControlDesc* desc = vb6_uc_findDesc(typeName);
    if (!desc) return NULL;
    if (g_uc_recCount >= VB6_UC_MAX_INST) return NULL;

    vb6_uc_registerClass((HINSTANCE)hInstance);
    vb6_uc_gdiplusInit();

    vb6_UCRec* r = &g_uc_recs[g_uc_recCount];
    memset(r, 0, sizeof(*r));
    r->desc = desc;
    r->parent = (HWND)hParent;
    r->enabled = -1;
    r->index = index;
    r->scaleWidth = vb6_TwipToX(width);
    r->scaleHeight = vb6_TwipToY(height);
    r->extLeft = left;
    r->extTop = top;
    if (ctrlName) {
        size_t n = strlen(ctrlName);
        if (n >= VB6_UC_NAME_LEN) n = VB6_UC_NAME_LEN - 1;
        for (size_t i = 0; i < n; i++) r->ctrlName[i] = (wchar_t)ctrlName[i];
        r->ctrlName[n] = 0;
    }

    HWND hwnd = CreateWindowExW(0, L"VB6_UserControlHost", L"",
                                WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
                                vb6_TwipToX(left), vb6_TwipToY(top),
                                vb6_TwipToX(width), vb6_TwipToY(height),
                                (HWND)hParent, NULL, (HINSTANCE)hInstance, r);
    vb6_uc_trace("window.created", typeName, hwnd);
    if (!hwnd) return NULL;
    r->hwnd = hwnd;
    g_uc_recCount++;

    // 登记: 供 Controls 枚举 / TypeName / 宿主对象分派
    if (g_hoCount < VB6_UC_MAX_OBJ) {
        vb6_HostObjRec* h = &g_ho[g_hoCount++];
        memset(h, 0, sizeof(*h));
        h->hwnd = (void*)hwnd;
        h->isForm = 0;
        h->index = index;
        h->instance = r->me;
        for (size_t i = 0; r->ctrlName[i] && i < VB6_UC_NAME_LEN - 1; i++) h->name[i] = r->ctrlName[i];
        {
            const char* p = desc->typeName;
            const char* dot = strrchr(desc->typeName, '.');
            if (dot) p = dot + 1;
            size_t n = strlen(p);
            if (n >= VB6_UC_NAME_LEN) n = VB6_UC_NAME_LEN - 1;
            for (size_t i = 0; i < n; i++) h->typeName[i] = (wchar_t)(unsigned char)p[i];
            h->typeName[n] = 0;
        }
    }

    // 实例化 + 初始化 (换入宿主状态, 让 UserControl_Initialize 读到正确的 ScaleMode)
    {
        vb6_UCSaved saved;
        vb6_uc_push(r, &saved);
        r->me = desc->create ? desc->create() : NULL;
        if (r->me && desc->init) desc->init(r->me);
        r->ready = 1;   // Fix 112: 之后窗口消息 (Resize/Show/Paint) 才允许进入实例
        vb6_uc_trace("instance.ready", desc->typeName, r->me);
        vb6_uc_pop(&saved);
        if (g_hoCount) g_ho[g_hoCount - 1].instance = r->me;
    }

    ShowWindow(hwnd, SW_SHOW);
    // Fix 112: 窗口创建期间被 ready=0 拦下的 Resize/Show 在此补发
    if (desc->resize) {
        RECT rc; GetClientRect(hwnd, &rc);
        r->scaleWidth = rc.right;
        r->scaleHeight = rc.bottom;
        vb6_UCSaved saved;
        vb6_uc_push(r, &saved);
        vb6_uc_trace("resize.begin", desc->typeName, r->me);
        desc->resize(r->me);
        vb6_uc_trace("resize.end", desc->typeName, r->me);
        vb6_uc_pop(&saved);
    }
    if (desc->show) {
        vb6_UCSaved saved;
        vb6_uc_push(r, &saved);
        vb6_uc_trace("show.begin", desc->typeName, r->me);
        desc->show(r->me);
        vb6_uc_trace("show.end", desc->typeName, r->me);
        vb6_uc_pop(&saved);
    }
    vb6_uc_trace("create.end", typeName, r->me);
    InvalidateRect(hwnd, NULL, FALSE);
    return (void*)hwnd;
}

void* vb6_UC_InstanceOf(void* hwnd) {
    vb6_UCRec* r = vb6_uc_findByHwnd(hwnd);
    return r ? r->me : NULL;
}

void* vb6_UC_HwndOf(void* instance) {
    vb6_UCRec* r = vb6_uc_findByInstance(instance);
    return r ? (void*)r->hwnd : NULL;
}

int32_t vb6_UC_IsHostHwnd(void* hwnd) {
    return vb6_uc_findByHwnd(hwnd) != NULL;
}

// ============================================================
// 窗体/控件登记 (供宿主对象分派)
// ============================================================

void vb6_HostObj_Register(void* hwnd, const char* name, const char* vbTypeName,
                          int32_t isForm, int32_t index) {
    if (!hwnd) return;
    if (vb6_ho_find(hwnd)) return;
    if (g_hoCount >= VB6_UC_MAX_OBJ) return;
    vb6_HostObjRec* h = &g_ho[g_hoCount++];
    memset(h, 0, sizeof(*h));
    h->hwnd = hwnd;
    h->isForm = isForm;
    h->index = index;
    if (name) {
        size_t n = strlen(name);
        if (n >= VB6_UC_NAME_LEN) n = VB6_UC_NAME_LEN - 1;
        for (size_t i = 0; i < n; i++) h->name[i] = (wchar_t)name[i];
    }
    if (vbTypeName) {
        const char* dot = strrchr(vbTypeName, '.');
        if (dot) vbTypeName = dot + 1;
        size_t n = strlen(vbTypeName);
        if (n >= VB6_UC_NAME_LEN) n = VB6_UC_NAME_LEN - 1;
        for (size_t i = 0; i < n; i++) h->typeName[i] = (wchar_t)vbTypeName[i];
    }
}

static vb6_HostObjRec* vb6_ho_findWindow(const void* hwnd) {
    return vb6_ho_find(hwnd);
}

int32_t vb6_Host_IsHostObject(void* obj) {
    if (!obj) return 0;
    // 真实窗口 (窗体/标准控件) HWND: IsWindow 仅查句柄表, 不解引用, 对任意指针安全
    if (IsWindow((HWND)obj)) return 1;
    if (vb6_uc_isControls(obj)) return 1;
    if (vb6_uc_isFont(obj)) return 1;
    if (vb6_uc_isColl(obj)) return 1;   // Fix 112c: RTL 内建 Collection
    if (vb6_uc_findByHwnd(obj)) return 1;
    return vb6_ho_find(obj) != NULL;
}

const wchar_t* vb6_Host_TypeNameOf(void* obj) {
    if (vb6_uc_isControls(obj)) return L"Collection";
    if (vb6_uc_isFont(obj)) return L"Font";
    if (vb6_uc_isColl(obj)) return L"Collection";   // Fix 112c
    vb6_HostObjRec* h = vb6_ho_find(obj);
    if (h) {
        if (h->typeName[0]) return h->typeName;
        return h->isForm ? L"Form" : L"Control";
    }
    vb6_UCRec* r = vb6_uc_findByHwnd(obj);
    if (r && r->desc && r->desc->typeName) {
        const char* dot = strrchr(r->desc->typeName, '.');
        static wchar_t buf[VB6_UC_NAME_LEN];
        const char* p = dot ? dot + 1 : r->desc->typeName;
        size_t i = 0;
        for (; p[i] && i < VB6_UC_NAME_LEN - 1; i++) buf[i] = (wchar_t)(unsigned char)p[i];
        buf[i] = 0;
        return buf;
    }
    if (IsWindow((HWND)obj)) return L"Control";
    return NULL;
}

// ============================================================
// Controls 集合
// ============================================================

static void* vb6_uc_controlsForm(const void* coll) {
    return ((const vb6_UCControls*)coll)->formHwnd;
}

// 收集某窗体上的全部子控件 HWND (含 UserControl 宿主窗口)
static int32_t vb6_uc_collectChildren(void* formHwnd, void** out, int32_t max) {
    int32_t n = 0;
    for (int32_t i = 0; i < g_hoCount && n < max; i++) {
        if (!g_ho[i].isForm && g_ho[i].hwnd &&
            GetParent((HWND)g_ho[i].hwnd) == (HWND)formHwnd) {
            out[n++] = g_ho[i].hwnd;
        }
    }
    // 兜底: 枚举真实子窗口 (vb6_CreateControl 创建的标准控件未必逐个登记)
    HWND child = GetWindow((HWND)formHwnd, GW_CHILD);
    while (child && n < max) {
        int32_t seen = 0;
        for (int32_t i = 0; i < n; i++) if (out[i] == (void*)child) { seen = 1; break; }
        if (!seen) out[n++] = (void*)child;
        child = GetWindow(child, GW_HWNDNEXT);
    }
    return n;
}

static vb6_UCControls* vb6_uc_newControls(void* formHwnd) {
    vb6_UCControls* c = (vb6_UCControls*)malloc(sizeof(vb6_UCControls));
    if (c) { c->tag = VB6_UC_CONTROLS_TAG; c->formHwnd = formHwnd; }
    return c;
}

int32_t vb6_UC_ControlsIsCollection(void* p) { return vb6_uc_isControls(p); }

int32_t vb6_UC_ControlsCount(void* coll) {
    if (!vb6_uc_isControls(coll)) return 0;
    void* kids[VB6_UC_MAX_OBJ];
    return vb6_uc_collectChildren(vb6_uc_controlsForm(coll), kids, VB6_UC_MAX_OBJ);
}

void* vb6_UC_ControlsItem(void* coll, int32_t index) {
    if (!vb6_uc_isControls(coll)) return NULL;
    void* kids[VB6_UC_MAX_OBJ];
    int32_t n = vb6_uc_collectChildren(vb6_uc_controlsForm(coll), kids, VB6_UC_MAX_OBJ);
    if (index < 1 || index > n) return NULL;
    return kids[index - 1];
}

void* vb6_UC_ControlsEnumInit(void* coll) {
    int32_t* e = (int32_t*)malloc(sizeof(int32_t) * 2);
    if (e) { e[0] = (int32_t)(intptr_t)coll; e[1] = 0; }
    return e;
}

int32_t vb6_UC_ControlsEnumNext(void* enumPtr, void* outV) {
    vb6_VARIANT* out = (vb6_VARIANT*)outV;
    int32_t* e = (int32_t*)enumPtr;
    if (!e) return 0;
    void* coll = (void*)(intptr_t)e[0];
    void* kids[VB6_UC_MAX_OBJ];
    int32_t n = vb6_uc_collectChildren(vb6_uc_controlsForm(coll), kids, VB6_UC_MAX_OBJ);
    if (e[1] >= n) return 0;
    void* hwnd = kids[e[1]++];
    memset(out, 0, sizeof(*out));
    out->vt = vb6_vtDispatch;
    out->pdispVal = hwnd;   // 控件对象 = 其 HWND (宿主分派层识别)
    return 1;
}

// ============================================================
// Font 对象 (stdole.StdFont 最小实现)
// ============================================================

void* vb6_UC_NewFont(void) {
    vb6_UCFontRec* fr = (vb6_UCFontRec*)malloc(sizeof(vb6_UCFontRec));
    if (!fr) return NULL;
    vb6_ComIface_Font* f = (vb6_ComIface_Font*)calloc(1, sizeof(vb6_ComIface_Font));
    if (!f) { free(fr); return NULL; }
    f->Name = vb6_BSTR_FromStr(L"MS Sans Serif");
    f->Size = 8.25f;
    f->Charset = 0;
    f->Weight = 400;
    fr->tag = VB6_UC_FONT_TAG;
    fr->font = f;
    fr->next = g_uc_fonts;
    g_uc_fonts = fr;
    return f; // 与 cgen 的 vb6_ComIface_Font* 直接字段访问保持一致
}

static vb6_ComIface_Font* vb6_uc_fontOf(void* p) {
    if (p == &g_vb6_UserControl_FontObj) return &g_vb6_UserControl_FontObj;
    for (vb6_UCFontRec* f = g_uc_fonts; f; f = f->next)
        if (p == f || p == f->font) return (vb6_ComIface_Font*)f->font;
    return NULL;
}

// ============================================================
// Fix 112c: RTL 内建 Collection (VB6 内建类)
// ============================================================
// 根因 (Charts 2020): `Set cAxisItem = New Collection` 发射为
// vb6_NewObject(L"Collection") → vb6_CreateObject → CLSIDFromProgID 必失败
// (Collection 是 VB6 语言内建, 无注册 ProgID) → vb6_RaiseError(429) →
// GUI 程序弹模态错误框并 exit(429) → 表现为"白板窗体+挂起".
// 这里提供纯 RTL 的 Collection, 挂进 Fix 112 宿主分派管道
// (vb6_ComCall/GetProp/SetProp/ForEach 已在这些合成对象上工作), 完全绕开 COM.
// 内部存 RTL vb6_VARIANT; 入参出参都是 Windows VARIANT (由 To/FromWinVariant 转换).

typedef struct vb6_CollRec {
    int32_t tag;          // 0xC01C01C0
    void*   items;        // vb6_VARIANT[] (RTL 简化布局)
    int32_t count;
    int32_t cap;
} vb6_CollRec;
#define VB6_UC_COLL_TAG 0xC01C01C0

static int32_t vb6_uc_isColl(const void* p) {
    return vb6_uc_ptrReadable(p, sizeof(int32_t)) &&
           ((const vb6_CollRec*)p)->tag == VB6_UC_COLL_TAG;
}

void* vb6_Collection_New(void) {
    vb6_CollRec* c = (vb6_CollRec*)calloc(1, sizeof(vb6_CollRec));
    if (c) c->tag = VB6_UC_COLL_TAG;
    return c;
}

int32_t vb6_Collection_IsCollection(void* p) { return vb6_uc_isColl(p); }

static void vb6_coll_push(void* coll, const vb6_VARIANT* v) {
    vb6_CollRec* c = (vb6_CollRec*)coll;
    if (!vb6_uc_isColl(c) || !v) return;
    if (c->count >= c->cap) {
        int32_t nc = c->cap ? c->cap * 2 : 8;
        vb6_VARIANT* ni = (vb6_VARIANT*)realloc(c->items, sizeof(vb6_VARIANT) * (size_t)nc);
        if (!ni) return;
        c->items = ni; c->cap = nc;
    }
    vb6_VARIANT* dst = &((vb6_VARIANT*)c->items)[c->count++];
    memset(dst, 0, sizeof(*dst));
    if (v->vt == vb6_vtBSTR) {
        dst->vt = vb6_vtBSTR;
        dst->bstrVal = v->bstrVal ? SysAllocString(v->bstrVal) : NULL;
    } else {
        *dst = *v;
    }
}

// 实参是 Windows VARIANT* (vb6_ComPackXxx 打包), 转成 RTL 形式后存入
void vb6_Collection_Add(void* coll, const void* winVar) {
    vb6_VARIANT hv;
    vb6_Host_FromWinVariant(winVar, &hv);
    vb6_coll_push(coll, &hv);
    vb6_Host_ClearVariant(&hv);
}

int32_t vb6_Collection_Count(void* coll) {
    return vb6_uc_isColl(coll) ? ((vb6_CollRec*)coll)->count : 0;
}

void vb6_Collection_Remove(void* coll, int32_t idx1) {
    vb6_CollRec* c = (vb6_CollRec*)coll;
    if (!vb6_uc_isColl(c) || idx1 < 1 || idx1 > c->count) return;
    vb6_VARIANT* items = (vb6_VARIANT*)c->items;
    vb6_VARIANT* v = &items[idx1 - 1];
    if (v->vt == vb6_vtBSTR && v->bstrVal) { SysFreeString(v->bstrVal); }
    memmove(&items[idx1 - 1], &items[idx1], sizeof(vb6_VARIANT) * (size_t)(c->count - idx1));
    c->count--;
}

// Item(i): 1-based; BSTR 需复制 (宿主管道的 ClearVariant 会 SysFreeString)
static void vb6_coll_itemCopy(const vb6_VARIANT* src, vb6_VARIANT* out) {
    memset(out, 0, sizeof(*out));
    if (src->vt == vb6_vtBSTR && src->bstrVal) {
        out->vt = vb6_vtBSTR;
        out->bstrVal = SysAllocString(src->bstrVal);
    } else {
        *out = *src;
    }
}

void vb6_Collection_Item(void* coll, int32_t idx1, void* outV) {
    vb6_VARIANT* out = (vb6_VARIANT*)outV;
    vb6_CollRec* c = (vb6_CollRec*)coll;
    if (!vb6_uc_isColl(c) || idx1 < 1 || idx1 > c->count) {
        vb6_ho_setVariantEmpty(out);
        return;
    }
    vb6_coll_itemCopy(&((const vb6_VARIANT*)c->items)[idx1 - 1], out);
}

// For Each 枚举器: {coll, nextIdx} 两元素句柄, 语义同 Controls 枚举
void* vb6_Collection_EnumInit(void* coll) {
    int32_t* e = (int32_t*)malloc(sizeof(int32_t) * 2);
    if (e) { e[0] = (int32_t)(intptr_t)coll; e[1] = 0; }
    return e;
}

int32_t vb6_Collection_EnumNext(void* enumPtr, void* outV) {
    vb6_VARIANT* out = (vb6_VARIANT*)outV;
    int32_t* e = (int32_t*)enumPtr;
    if (!e) return 0;
    vb6_CollRec* c = (vb6_CollRec*)(intptr_t)e[0];
    if (!vb6_uc_isColl(c) || e[1] >= c->count) return 0;
    vb6_coll_itemCopy(&((const vb6_VARIANT*)c->items)[e[1]++], out);
    return 1;
}

// ============================================================
// 宿主对象属性/方法分派
// ============================================================

static int32_t vb6_ho_isForm(const void* hwnd) {
    vb6_HostObjRec* h = vb6_ho_find(hwnd);
    return (h && h->isForm) ? 1 : 0;
}

static int32_t vb6_ho_isControl(const void* hwnd) {
    vb6_HostObjRec* h = vb6_ho_find(hwnd);
    return (h && !h->isForm) ? 1 : 0;
}

static void vb6_ho_setVariantLong(vb6_VARIANT* out, int32_t v) {
    memset(out, 0, sizeof(*out));
    out->vt = vb6_vtLong;   // 由调用方按 VT_I4 解释
    out->lVal = v;
}

static void vb6_ho_setVariantBstr(vb6_VARIANT* out, BSTR s) {
    memset(out, 0, sizeof(*out));
    out->vt = vb6_vtBSTR;
    out->bstrVal = s;
}

static void vb6_ho_setVariantDispatch(vb6_VARIANT* out, void* p) {
    // 注意: 宿主合成的"对象"(Controls 集合 / Font 代理 / 控件 HWND)不是真 COM 对象,
    // 调用方 vb6_ComVarClear → VariantClear 会对 VT_DISPATCH 调 Release → 崩溃.
    // 因此这里一律返回 Empty, 让上层走 Nothing 分支 (等价 VB6 On Error Resume Next).
    (void)p;
    memset(out, 0, sizeof(*out));
    out->vt = vb6_vtEmpty;
}

static void vb6_ho_setVariantEmpty(vb6_VARIANT* out) {
    memset(out, 0, sizeof(*out));
    out->vt = vb6_vtEmpty;
}

static void vb6_ho_setVariantDouble(vb6_VARIANT* out, double v) {
    memset(out, 0, sizeof(*out));
    out->vt = vb6_vtDouble;
    out->dblVal = v;
}

static int32_t vb6_ho_variantToLong(const vb6_VARIANT* v) {
    if (!v) return 0;
    switch (v->vt) {
        case vb6_vtInteger: case vb6_vtLong: case vb6_vtBoolean: case vb6_vtByte:
            return v->lVal;
        case vb6_vtSingle: case vb6_vtDouble: return (int32_t)v->dblVal;
        default: return 0;
    }
}

static double vb6_ho_variantToDouble(const vb6_VARIANT* v) {
    if (!v) return 0.0;
    switch (v->vt) {
        case vb6_vtSingle: case vb6_vtDouble: return v->dblVal;
        case vb6_vtInteger: case vb6_vtLong: case vb6_vtBoolean: return (double)v->lVal;
        default: return 0.0;
    }
}

static BSTR vb6_ho_variantToBstr(const vb6_VARIANT* v) {
    return (v && v->vt == vb6_vtBSTR) ? v->bstrVal : NULL;
}

// 控件的几何: 相对父窗口客户区, 缇
static void vb6_ho_ctrlRect(void* hwnd, int32_t* l, int32_t* t, int32_t* w, int32_t* h) {
    RECT rc; GetWindowRect((HWND)hwnd, &rc);
    POINT pt = { rc.left, rc.top };
    HWND p = GetParent((HWND)hwnd);
    ScreenToClient(p ? p : hwnd, &pt);
    if (l) *l = pt.x * 15;
    if (t) *t = pt.y * 15;
    if (w) *w = (rc.right - rc.left) * 15;
    if (h) *h = (rc.bottom - rc.top) * 15;
}

static void vb6_ho_clientTwips(void* hwnd, int32_t* w, int32_t* h) {
    RECT rc; GetClientRect((HWND)hwnd, &rc);
    if (w) *w = (rc.right - rc.left) * 15;
    if (h) *h = (rc.bottom - rc.top) * 15;
}

static int32_t vb6_ho_getFontMember(void* fontProxy, const wchar_t* name, vb6_VARIANT* out) {
    vb6_ComIface_Font* f = vb6_uc_fontOf(fontProxy);
    if (!f) return 0;
    if (_wcsicmp(name, L"Size") == 0)      { vb6_ho_setVariantDouble(out, f->Size); return 1; }
    if (_wcsicmp(name, L"Bold") == 0)      { vb6_ho_setVariantLong(out, f->Bold ? -1 : 0); return 1; }
    if (_wcsicmp(name, L"Italic") == 0)    { vb6_ho_setVariantLong(out, f->Italic ? -1 : 0); return 1; }
    if (_wcsicmp(name, L"Underline") == 0) { vb6_ho_setVariantLong(out, f->Underline ? -1 : 0); return 1; }
    if (_wcsicmp(name, L"Strikethrough") == 0) { vb6_ho_setVariantLong(out, f->Strikethrough ? -1 : 0); return 1; }
    if (_wcsicmp(name, L"Weight") == 0)    { vb6_ho_setVariantLong(out, f->Weight); return 1; }
    if (_wcsicmp(name, L"Charset") == 0)   { vb6_ho_setVariantLong(out, f->Charset); return 1; }
    if (_wcsicmp(name, L"Name") == 0)      { vb6_ho_setVariantBstr(out, SysAllocString(f->Name)); return 1; }
    return 0;
}

static int32_t vb6_ho_putFontMember(void* fontProxy, const wchar_t* name, const vb6_VARIANT* v) {
    vb6_ComIface_Font* f = vb6_uc_fontOf(fontProxy);
    if (!f) return 0;
    if (_wcsicmp(name, L"Size") == 0)      { f->Size = (float)vb6_ho_variantToDouble(v); return 1; }
    if (_wcsicmp(name, L"Bold") == 0)      { f->Bold = (int16_t)(vb6_ho_variantToLong(v) ? -1 : 0); return 1; }
    if (_wcsicmp(name, L"Italic") == 0)    { f->Italic = (int16_t)(vb6_ho_variantToLong(v) ? -1 : 0); return 1; }
    if (_wcsicmp(name, L"Underline") == 0) { f->Underline = (int16_t)(vb6_ho_variantToLong(v) ? -1 : 0); return 1; }
    if (_wcsicmp(name, L"Strikethrough") == 0) { f->Strikethrough = (int16_t)(vb6_ho_variantToLong(v) ? -1 : 0); return 1; }
    if (_wcsicmp(name, L"Weight") == 0)    { f->Weight = vb6_ho_variantToLong(v); return 1; }
    if (_wcsicmp(name, L"Charset") == 0)   { f->Charset = vb6_ho_variantToLong(v); return 1; }
    if (_wcsicmp(name, L"Name") == 0)      { f->Name = vb6_ho_variantToBstr(v); return 1; }
    return 0;
}

// 窗体/控件/字体 的属性读取. 返回 1=已处理
int32_t vb6_Host_GetProp(void* obj, const wchar_t* name, void* outV) {
    vb6_VARIANT* out = (vb6_VARIANT*)outV;
    if (!obj || !name) return 0;
    vb6_ho_setVariantEmpty(out);

    if (vb6_uc_isControls(obj)) {
        if (_wcsicmp(name, L"Count") == 0) { vb6_ho_setVariantLong(out, vb6_UC_ControlsCount(obj)); return 1; }
        return 1;  // 其它成员当作无值, 避免 C 崩溃
    }
    if (vb6_uc_isColl(obj)) {   // Fix 112c: RTL Collection
        if (_wcsicmp(name, L"Count") == 0) { vb6_ho_setVariantLong(out, vb6_Collection_Count(obj)); return 1; }
        return 1;
    }
    if (vb6_uc_isFont(obj)) return vb6_ho_getFontMember(obj, name, out);

    vb6_UCRec* r = vb6_uc_findByHwnd(obj);
    vb6_HostObjRec* h = vb6_ho_find(obj);
    if (!r && !h && !IsWindow((HWND)obj)) return 0;

    int32_t isForm = r ? 0 : vb6_ho_isForm(obj);
    int32_t scaleMode = r ? (r->desc ? r->desc->scaleMode : 1) : 1;

    if (_wcsicmp(name, L"hwnd") == 0 || _wcsicmp(name, L"hWnd") == 0) {
        vb6_ho_setVariantLong(out, (int32_t)(intptr_t)obj); return 1;
    }
    if (_wcsicmp(name, L"Name") == 0) {
        vb6_ho_setVariantBstr(out, vb6_BSTR_FromStr(h && h->name[0] ? h->name : (r && r->ctrlName[0] ? r->ctrlName : L"")));
        return 1;
    }
    if (_wcsicmp(name, L"Index") == 0) {
        vb6_ho_setVariantLong(out, h ? h->index : (r ? r->index : -1)); return 1;
    }
    if (_wcsicmp(name, L"ScaleMode") == 0) { vb6_ho_setVariantLong(out, scaleMode); return 1; }
    if (_wcsicmp(name, L"ScaleWidth") == 0 || _wcsicmp(name, L"ScaleHeight") == 0 ||
        _wcsicmp(name, L"Width") == 0   || _wcsicmp(name, L"Height") == 0 ||
        _wcsicmp(name, L"Left") == 0    || _wcsicmp(name, L"Top") == 0) {
        int32_t w = 0, hh = 0;
        if (_wcsicmp(name, L"ScaleWidth") == 0 || _wcsicmp(name, L"ScaleHeight") == 0) {
            vb6_ho_clientTwips(obj, &w, &hh);
        } else if (isForm) {
            RECT rc; GetWindowRect((HWND)obj, &rc);
            w = (rc.right - rc.left) * 15; hh = (rc.bottom - rc.top) * 15;
            if (_wcsicmp(name, L"Left") == 0 || _wcsicmp(name, L"Top") == 0) {
                w = rc.left * 15; hh = rc.top * 15;
            }
        } else {
            vb6_ho_ctrlRect(obj, NULL, NULL, &w, &hh);
            if (_wcsicmp(name, L"Left") == 0 || _wcsicmp(name, L"Top") == 0) {
                int32_t l = 0, t = 0; vb6_ho_ctrlRect(obj, &l, &t, NULL, NULL);
                w = l; hh = t;
            }
        }
        int32_t val = (_wcsicmp(name, L"ScaleWidth") == 0 || _wcsicmp(name, L"Width") == 0 ||
                       _wcsicmp(name, L"Left") == 0) ? w : hh;
        // ScaleWidth/ScaleHeight 需按对象自身 ScaleMode 换算
        if (_wcsicmp(name, L"ScaleWidth") == 0 || _wcsicmp(name, L"ScaleHeight") == 0) {
            if (scaleMode == 3) val = val / 15;   // Pixel
        }
        vb6_ho_setVariantLong(out, val); return 1;
    }
    if (_wcsicmp(name, L"Visible") == 0)  { vb6_ho_setVariantLong(out, IsWindowVisible((HWND)obj) ? -1 : 0); return 1; }
    if (_wcsicmp(name, L"Enabled") == 0)  { vb6_ho_setVariantLong(out, IsWindowEnabled((HWND)obj) ? -1 : 0); return 1; }
    if (_wcsicmp(name, L"hDC") == 0)      { vb6_ho_setVariantLong(out, (int32_t)(intptr_t)GetDC((HWND)obj)); return 1; }
    if (_wcsicmp(name, L"Count") == 0)    { vb6_ho_setVariantLong(out, 0); return 1; }
    if (_wcsicmp(name, L"Controls") == 0) {
        vb6_UCControls* c = vb6_uc_newControls(obj);
        vb6_ho_setVariantDispatch(out, c); return 1;
    }
    if (_wcsicmp(name, L"Caption") == 0 || _wcsicmp(name, L"Text") == 0) {
        char buf[256]; int n = GetWindowTextA((HWND)obj, buf, sizeof(buf));
        wchar_t wbuf[256];
        for (int i = 0; i < n && i < 255; i++) wbuf[i] = (wchar_t)(unsigned char)buf[i];
        wbuf[n < 255 ? n : 255] = 0;
        vb6_ho_setVariantBstr(out, vb6_BSTR_FromStr(wbuf)); return 1;
    }
    // 字体属性 (Font / TitleFont / *Font) → 返回 Font 代理
    {
        size_t ln = wcslen(name);
        if (_wcsicmp(name, L"Font") == 0 ||
            (ln > 4 && _wcsicmp(name + ln - 4, L"Font") == 0)) {
            vb6_UCFontRec* fr = (vb6_UCFontRec*)vb6_UC_NewFont();
            vb6_ho_setVariantDispatch(out, fr); return 1;
        }
    }
    return 1;   // 宿主对象: 未知属性一律返回 Empty (不崩)
}

// 窗体/控件/字体 的属性写入. 返回 1=已处理
int32_t vb6_Host_SetProp(void* obj, const wchar_t* name, const void* inV) {
    const vb6_VARIANT* v = (const vb6_VARIANT*)inV;
    if (!obj || !name) return 0;
    if (vb6_uc_isFont(obj)) return vb6_ho_putFontMember(obj, name, v);
    if (vb6_uc_isControls(obj)) return 1;
    if (vb6_uc_isColl(obj)) return 1;   // Fix 112c: Collection 只读容器

    if (!vb6_ho_find(obj) && !vb6_uc_findByHwnd(obj) && !IsWindow((HWND)obj)) return 0;

    if (_wcsicmp(name, L"Caption") == 0 || _wcsicmp(name, L"Text") == 0) {
        BSTR s = vb6_ho_variantToBstr(v);
        if (s) SetWindowTextW((HWND)obj, s);
        InvalidateRect((HWND)obj, NULL, TRUE);
        return 1;
    }
    if (_wcsicmp(name, L"Visible") == 0) { ShowWindow((HWND)obj, vb6_ho_variantToLong(v) ? SW_SHOW : SW_HIDE); return 1; }
    if (_wcsicmp(name, L"Enabled") == 0) { EnableWindow((HWND)obj, vb6_ho_variantToLong(v) ? TRUE : FALSE); return 1; }
    if (_wcsicmp(name, L"Left") == 0 || _wcsicmp(name, L"Top") == 0 ||
        _wcsicmp(name, L"Width") == 0 || _wcsicmp(name, L"Height") == 0) {
        int32_t l = 0, t = 0, w = 0, hh = 0;
        vb6_ho_ctrlRect(obj, &l, &t, &w, &hh);
        if (_wcsicmp(name, L"Left") == 0)   l = vb6_ho_variantToLong(v);
        if (_wcsicmp(name, L"Top") == 0)    t = vb6_ho_variantToLong(v);
        if (_wcsicmp(name, L"Width") == 0)  w = vb6_ho_variantToLong(v);
        if (_wcsicmp(name, L"Height") == 0) hh = vb6_ho_variantToLong(v);
        MoveWindow((HWND)obj, l / 15, t / 15, w / 15, hh / 15, TRUE);
        return 1;
    }
    return 1;   // 宿主对象: 未知属性写入一律忽略
}

// 窗体/控件 的方法调用. argv 为已打包 VARIANT 指针数组. 返回 1=已处理
int32_t vb6_Host_Call(void* obj, const wchar_t* name, int32_t argc, void** argv, void* outV) {
    vb6_VARIANT* out = (vb6_VARIANT*)outV;
    (void)argc;
    if (!obj || !name) return 0;
    vb6_ho_setVariantEmpty(out);
    if (vb6_uc_isFont(obj)) return 1;
    if (vb6_uc_isColl(obj)) {   // Fix 112c: Collection 方法
        if (_wcsicmp(name, L"Add") == 0 && argc >= 1) {
            vb6_Collection_Add(obj, argv[0]);
        } else if (_wcsicmp(name, L"Item") == 0 && argc >= 1) {
            vb6_VARIANT* a = (vb6_VARIANT*)argv[0];
            vb6_Collection_Item(obj, vb6_ho_variantToLong(a), out);
        } else if (_wcsicmp(name, L"Remove") == 0 && argc >= 1) {
            vb6_Collection_Remove(obj, vb6_ho_variantToLong((vb6_VARIANT*)argv[0]));
        }
        return 1;
    }
    if (vb6_uc_isControls(obj)) {
        if (_wcsicmp(name, L"Item") == 0 && argc >= 1) {
            vb6_VARIANT* a = (vb6_VARIANT*)argv[0];
            void* it = vb6_UC_ControlsItem(obj, vb6_ho_variantToLong(a));
            vb6_ho_setVariantDispatch(out, it);
        }
        return 1;
    }
    if (!vb6_ho_find(obj) && !vb6_uc_findByHwnd(obj) && !IsWindow((HWND)obj)) return 0;

    if (_wcsicmp(name, L"Refresh") == 0) {
        InvalidateRect((HWND)obj, NULL, TRUE); UpdateWindow((HWND)obj);
        return 1;
    }
    if (_wcsicmp(name, L"Cls") == 0) {
        HDC dc = GetDC((HWND)obj);
        if (dc) { RECT rc; GetClientRect((HWND)obj, &rc);
                  HBRUSH b = CreateSolidBrush(RGB(255,255,255));
                  FillRect(dc, &rc, b); DeleteObject(b); ReleaseDC((HWND)obj, dc); }
        return 1;
    }
    if (_wcsicmp(name, L"SetFocus") == 0) { SetFocus((HWND)obj); return 1; }
    if (_wcsicmp(name, L"ZOrder") == 0 || _wcsicmp(name, L"Move") == 0 ||
        _wcsicmp(name, L"Show") == 0 || _wcsicmp(name, L"Hide") == 0 ||
        _wcsicmp(name, L"Print") == 0 || _wcsicmp(name, L"Line") == 0) {
        return 1;
    }
    return 1;   // 宿主对象: 未知方法一律空实现
}


// ============================================================
// Windows VARIANT ↔ vb6_VARIANT (供 vb6_com_invoke/wrap 挂接点使用)
// ============================================================

void vb6_Host_ToWinVariant(const void* inV, void* outV) {
    const vb6_VARIANT* in = (const vb6_VARIANT*)inV;
    VARIANT* out = (VARIANT*)outV;
    VariantInit(out);
    if (!in) return;
    switch (in->vt) {
        case vb6_vtInteger:  V_VT(out) = VT_I2; V_I2(out) = (short)in->iVal; break;
        case vb6_vtLong:     V_VT(out) = VT_I4; V_I4(out) = in->lVal; break;
        case vb6_vtBoolean:  V_VT(out) = VT_BOOL; V_BOOL(out) = in->boolVal; break;
        case vb6_vtByte:     V_VT(out) = VT_UI1; V_UI1(out) = in->bVal; break;
        case vb6_vtSingle:   V_VT(out) = VT_R4; V_R4(out) = in->fltVal; break;
        case vb6_vtDouble:   V_VT(out) = VT_R8; V_R8(out) = in->dblVal; break;
        case vb6_vtBSTR:     V_VT(out) = VT_BSTR; V_BSTR(out) = SysAllocString(in->bstrVal); break;
        default:             V_VT(out) = VT_EMPTY; break;
    }
}

void vb6_Host_FromWinVariant(const void* inV, void* outV) {
    const VARIANT* in = (const VARIANT*)inV;
    vb6_VARIANT* out = (vb6_VARIANT*)outV;
    memset(out, 0, sizeof(*out));
    if (!in) { out->vt = vb6_vtEmpty; return; }
    switch (in->vt) {
        case VT_I2:    out->vt = vb6_vtInteger; out->iVal = in->iVal; break;
        case VT_I4:    out->vt = vb6_vtLong; out->lVal = in->lVal; break;
        case VT_BOOL:  out->vt = vb6_vtBoolean; out->boolVal = in->boolVal; break;
        case VT_UI1:   out->vt = vb6_vtByte; out->bVal = in->bVal; break;
        case VT_R4:    out->vt = vb6_vtSingle; out->fltVal = in->fltVal; break;
        case VT_R8:    out->vt = vb6_vtDouble; out->dblVal = in->dblVal; break;
        case VT_BSTR:  out->vt = vb6_vtBSTR; out->bstrVal = SysAllocString(in->bstrVal); break;
        default:       out->vt = vb6_vtEmpty; break;
    }
}

// 释放 vb6_Host_Call/GetProp 填出的 vb6_VARIANT (BSTR 需 SysFreeString)
void vb6_Host_ClearVariant(void* v) {
    vb6_VARIANT* p = (vb6_VARIANT*)v;
    if (!p) return;
    if (p->vt == vb6_vtBSTR && p->bstrVal) { SysFreeString(p->bstrVal); }
    memset(p, 0, sizeof(*p));
    p->vt = vb6_vtEmpty;
}

#ifdef __cplusplus
} // extern "C"
#endif
