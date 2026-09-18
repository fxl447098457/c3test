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
    BSTR    displayNameBstr;   // Fix 116: Ambient.DisplayName 缓存 (控件实例名)
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

// Fix 119: 待应用的实例字体。VB6 里每个控件实例有独立的 Font 对象(.frm 的
// BeginProperty Font 块), 且控件会在 InitProperties 里以 UserControl.Font 为
// 默认字体基准 (如 m_TitleFont.Size = UserControl.Font.Size + 8)。因此字体必须在
// 实例初始化**之前**就位 —— cgen 在 vb6_UC_HostCreate 之前调用
// vb6_UC_SetPendingFont(), HostCreate 把它装进 r->font, push 时既成为
// vb6_UserControl_Font 也成为 Ambient.Font。
static vb6_ComIface_Font* g_uc_pendingFont = NULL;

void vb6_UC_SetPendingFont(void* f) {
    g_uc_pendingFont = (vb6_ComIface_Font*)f;
}

// Fix 122: VB6 的 z 序规则 —— .frm 中**先声明**的控件在**最上层**。
// cgen 按 .frm 顺序创建子窗口, 而 Win32 是"后创建者在上" → 顺序恰好相反:
// Form2 里 LabelPlus1 (最后声明) 于是盖住了先声明的三个 ucProgressCircular 圆环,
// 用户看到的就是"圆环不见了"。这里把每个新宿主插到"上一个宿主"**之下**, 使先声明者
// 保持在上 (同父窗口内才处理, 避免跨容器错插)。
static HWND g_uc_lastHost = NULL;
static HWND g_uc_lastHostParent = NULL;

static void vb6_uc_fixZOrder(HWND hwnd) {
    HWND parent = GetParent(hwnd);
    if (g_uc_lastHost && g_uc_lastHostParent == parent && IsWindow(g_uc_lastHost))
        SetWindowPos(hwnd, g_uc_lastHost, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    g_uc_lastHost = hwnd;
    g_uc_lastHostParent = parent;
}

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
    void*   displayName;      // Fix 116: Ambient.DisplayName (BSTR)
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
    saved->displayName = (void*)vb6_Ambient_DisplayName;

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

    // Fix 116: Ambient.DisplayName = 控件实例名 (VB6 语义)。
    // 控件内常见用法: m_Title = .ReadProperty("Title", Ambient.DisplayName)
    // → 此前恒为 NULL, 于是 ucChartArea1 / ucPieChart1 / ucTreeMaps1 等标题全空。
    // 名字在实例创建时写入 r->ctrlName, 这里惰性缓存成 BSTR 复用, 避免每次
    // push 都分配 (push 在每次绘制/事件都会发生)。
    if (!r->displayNameBstr && r->ctrlName[0])
        r->displayNameBstr = SysAllocString(r->ctrlName);
    vb6_Ambient_DisplayName = r->displayNameBstr;
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
    vb6_Ambient_DisplayName = (BSTR)saved->displayName;   // Fix 116
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
// Fix 113h: 离屏 DIB 绘制捕获 (调试用, 环境变量 C3_UC_DUMPDIR 启用)
// ============================================================
// 背景: 在无交互桌面 / RDP 断连的会话里, 屏幕位图 (GetDC(0) BitBlt / PrintWindow)
// 全部读回黑色 (实测连标准 MessageBox 也是黑的), 无法用截图客观验证"图表到底画
// 没有画出来". 这里让宿主把控件改为绘制到**内存 DIB**, 再把 DIB 转存为 BMP,
// 完全不依赖桌面表面, 因而在任何会话下都能拿到真实绘制结果.
// 未设置 C3_UC_DUMPDIR 时行为与原来完全一致 (直接画到窗口 DC).
typedef struct vb6_UCDib {
    HDC     memDC;
    HBITMAP bmp;
    HBITMAP oldBmp;
    void*   bits;
    int32_t w, h, stride;
} vb6_UCDib;

static int32_t g_uc_dumpSeq = 0;

static void vb6_uc_dibCreate(vb6_UCDib* d, HDC refDC, int32_t w, int32_t h) {
    BITMAPINFO bi;
    memset(d, 0, sizeof(*d));
    d->w = w;
    d->h = h;
    d->stride = ((w * 3 + 3) & ~3);
    d->memDC = CreateCompatibleDC(refDC);
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = h;   /* bottom-up: 与 BMP 默认行序一致, 可直接转存 */
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 24;
    bi.bmiHeader.biCompression = BI_RGB;
    bi.bmiHeader.biSizeImage = (DWORD)(d->stride * h);
    d->bmp = CreateDIBSection(refDC, &bi, DIB_RGB_COLORS, &d->bits, NULL, 0);
    if (d->bmp) d->oldBmp = (HBITMAP)SelectObject(d->memDC, d->bmp);
}

static void vb6_uc_dibSaveBmp(const vb6_UCDib* d, const char* path) {
    if (!d->bits) return;
    FILE* f = fopen(path, "wb");
    if (!f) return;
    uint32_t imgSize = (uint32_t)(d->stride * d->h);
    unsigned char hdr[54];
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 'B'; hdr[1] = 'M';
    uint32_t fileSize = 54u + imgSize;
    memcpy(hdr + 2, &fileSize, 4);
    uint32_t offBits = 54u;
    memcpy(hdr + 10, &offBits, 4);
    uint32_t hdrSize = 40u;
    memcpy(hdr + 14, &hdrSize, 4);
    int32_t W = d->w, H = d->h;
    memcpy(hdr + 18, &W, 4);
    memcpy(hdr + 22, &H, 4);
    uint16_t planes = 1, bpp = 24;
    memcpy(hdr + 26, &planes, 2);
    memcpy(hdr + 28, &bpp, 2);
    memcpy(hdr + 34, &imgSize, 4);
    fwrite(hdr, 1, 54, f);
    fwrite(d->bits, 1, (size_t)imgSize, f);
    fclose(f);
}

static void vb6_uc_dibDestroy(vb6_UCDib* d) {
    if (d->memDC) {
        if (d->oldBmp) SelectObject(d->memDC, d->oldBmp);
        DeleteDC(d->memDC);
    }
    if (d->bmp) DeleteObject(d->bmp);
}

// ============================================================
// Fix 123: 整窗合成 dump (调试用)
// ============================================================
// 逐个控件 dump 只能看到"控件自己画了什么", 看不到**叠放次序**(z 序) 与相对位置。
// VB6 的 z 序规则是"先声明者在上", 而 Win32 默认"后创建者在上"(见 Fix 122),
// 一旦反了就会出现"圆环被 LabelPlus 白面板盖住"这类问题而单控件 dump 一切正常。
// 这里按窗口管理器的真实 z 序 (自下而上) 把本级所有 UserControl 宿主依次绘制并
// 贴进一张窗体大小的 DIB, 输出 FORM_*.bmp —— 即在无桌面会话下也能看到"用户所见"。
static void vb6_uc_dumpFormComposite(HWND root, const char* dumpDir) {
    if (!root || !dumpDir || !*dumpDir) return;
    RECT crc;
    if (!GetClientRect(root, &crc)) return;
    int cw = (int)(crc.right - crc.left), ch = (int)(crc.bottom - crc.top);
    if (cw <= 0 || ch <= 0 || cw > 4096 || ch > 4096) return;

    HDC refDC = GetDC(root);
    vb6_UCDib form;
    vb6_uc_dibCreate(&form, refDC, cw, ch);
    if (refDC) ReleaseDC(root, refDC);
    if (!form.memDC || !form.bits) { vb6_uc_dibDestroy(&form); return; }
    RECT full = { 0, 0, cw, ch };
    HBRUSH bg = CreateSolidBrush(RGB(229, 229, 229));   // Form2 BackColor = &H00E5E5E5
    FillRect(form.memDC, &full, bg);
    DeleteObject(bg);

    // 收集本级所有 UserControl 宿主 (窗口 z 序: GW_CHILD = 最上层)
    HWND kids[VB6_UC_MAX_INST];
    int nk = 0;
    for (HWND h = GetWindow(root, GW_CHILD); h && nk < VB6_UC_MAX_INST;
         h = GetWindow(h, GW_HWNDNEXT)) {
        kids[nk++] = h;
    }
    // 自下而上绘制 (数组尾部 = 最下层)
    for (int i = nk - 1; i >= 0; i--) {
        vb6_UCRec* r = vb6_uc_findByHwnd((void*)kids[i]);
        if (!r || !r->ready || !r->desc || !r->desc->paint) continue;
        RECT kc;
        GetClientRect(kids[i], &kc);
        int kw = (int)(kc.right - kc.left), kh = (int)(kc.bottom - kc.top);
        if (kw <= 0 || kh <= 0) continue;
        POINT pt = { 0, 0 };
        MapWindowPoints(kids[i], root, &pt, 1);

        vb6_UCDib cd;
        vb6_uc_dibCreate(&cd, form.memDC, kw, kh);
        if (!cd.memDC || !cd.bits) { vb6_uc_dibDestroy(&cd); continue; }
        RECT kfull = { 0, 0, kw, kh };
        HBRUSH wb = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(cd.memDC, &kfull, wb);
        DeleteObject(wb);

        r->hdc = cd.memDC;
        vb6_UCSaved saved;
        vb6_uc_push(r, &saved);
        r->desc->paint(r->me);
        vb6_uc_pop(&saved);
        r->hdc = NULL;

        // 手动"白=透明"合并 (避免额外链接 msimg32): 与 VB6 windowless 控件一致 —
        // 控件未绘制之处露出容器底色 (例如三个圆环之间的 LabelPlus 白面板)。
        {
            unsigned char* cd8 = (unsigned char*)cd.bits;
            for (int y = 0; y < kh; y++) {
                int dy = pt.y + y;
                if (dy < 0 || dy >= form.h) continue;
                // DIB 为 bottom-up: 内存行 = h-1-y
                unsigned char* frow = (unsigned char*)form.bits
                                    + (size_t)(form.h - 1 - dy) * form.stride;
                unsigned char* crow = cd8 + (size_t)(cd.h - 1 - y) * cd.stride;
                for (int x = 0; x < kw; x++) {
                    int dx = pt.x + x;
                    if (dx < 0 || dx >= form.w) continue;
                    unsigned char b = crow[x * 3], g = crow[x * 3 + 1], rr = crow[x * 3 + 2];
                    if (rr >= 250 && g >= 250 && b >= 250) continue;   // 近白 → 透明
                    frow[dx * 3] = b; frow[dx * 3 + 1] = g; frow[dx * 3 + 2] = rr;
                }
            }
        }
        vb6_uc_dibDestroy(&cd);
    }

    char path[1024];
    _snprintf(path, sizeof(path), "%s\\FORM_%p.bmp", dumpDir, root);
    path[sizeof(path) - 1] = '\0';
    vb6_uc_dibSaveBmp(&form, path);
    vb6_uc_dibDestroy(&form);
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
            // Fix 113h: 调试捕获 (见上方 vb6_UCDib 说明) — C3_UC_DUMPDIR 未设置时
            // 走原路径 (直接绘制到窗口 DC).
            const char* dumpDir113h = getenv("C3_UC_DUMPDIR");
            vb6_UCDib dib113h;
            int useDib113h = 0;
            if (dumpDir113h && *dumpDir113h) {
                RECT crc;
                GetClientRect(hwnd, &crc);
                int cw113h = (int)(crc.right - crc.left);
                int ch113h = (int)(crc.bottom - crc.top);
                if (cw113h > 0 && ch113h > 0) {
                    vb6_uc_dibCreate(&dib113h, hdc, cw113h, ch113h);
                    if (dib113h.memDC && dib113h.bits) {
                        RECT full113h = { 0, 0, cw113h, ch113h };
                        HBRUSH wb113h = CreateSolidBrush(RGB(255, 255, 255));
                        FillRect(dib113h.memDC, &full113h, wb113h);
                        DeleteObject(wb113h);
                        useDib113h = 1;
                    }
                }
            }
            r->hdc = useDib113h ? dib113h.memDC : hdc;
            vb6_UCSaved saved;
            vb6_uc_push(r, &saved);
            vb6_uc_trace("paint.begin", r->desc->typeName, r->me);
            r->desc->paint(r->me);
            vb6_uc_trace("paint.end", r->desc->typeName, r->me);
            vb6_uc_pop(&saved);
            if (useDib113h) {
                BitBlt(hdc, 0, 0, dib113h.w, dib113h.h, dib113h.memDC, 0, 0, SRCCOPY);
                char path113h[1024];
                _snprintf(path113h, sizeof(path113h), "%s\\%s_%d_%p.bmp", dumpDir113h,
                          r->desc->typeName, (int)++g_uc_dumpSeq, hwnd);
                path113h[sizeof(path113h) - 1] = '\0';
                vb6_uc_dibSaveBmp(&dib113h, path113h);
                vb6_uc_dibDestroy(&dib113h);
                // Fix 123: 同步输出整窗合成图 (含 z 序/相对位置)
                vb6_uc_dumpFormComposite((HWND)GetAncestor(hwnd, GA_ROOT), dumpDir113h);
            }
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
    // Fix 119: 装入 cgen 预先设定的实例字体 (.frm BeginProperty Font)
    if (g_uc_pendingFont) {
        r->font = g_uc_pendingFont;
        g_uc_pendingFont = NULL;
    }
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
                                // Fix 124: WS_CLIPSIBLINGS 必不可少 —— 没有它, 一个窗口
                                // 重绘时会画到同层兄弟控件上, 于是 z 序在下的不透明面板
                                // (Charts 2020 的 LabelPlus1 白底) 每次重绘都会把上层控件
                                // (三个 ucProgressCircular 圆环) 擦掉。离屏 dump 是各控件
                                // 单独画进自己的 DIB 再合成, 体现不出这个裁剪问题 ——
                                // 这就是"dump 里有、软件里看不到"的根因。
                                WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                                vb6_TwipToX(left), vb6_TwipToY(top),
                                vb6_TwipToX(width), vb6_TwipToY(height),
                                (HWND)hParent, NULL, (HINSTANCE)hInstance, r);
    vb6_uc_trace("window.created", typeName, hwnd);
    if (!hwnd) return NULL;
    r->hwnd = hwnd;
    g_uc_recCount++;

    // Fix 122: 修正 VB6 z 序 (先声明者在上)
    vb6_uc_fixZOrder(hwnd);

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

// 句柄首元素存指针, 必须用 intptr_t —— x64 下按 int32_t 存会截断高 32 位 (同 Collection 枚举器)
void* vb6_UC_ControlsEnumInit(void* coll) {
    intptr_t* e = (intptr_t*)malloc(sizeof(intptr_t) * 2);
    if (e) { e[0] = (intptr_t)coll; e[1] = 0; }
    return e;
}

int32_t vb6_UC_ControlsEnumNext(void* enumPtr, void* outV) {
    vb6_VARIANT* out = (vb6_VARIANT*)outV;
    intptr_t* e = (intptr_t*)enumPtr;
    if (!e) return 0;
    void* coll = (void*)e[0];
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

// Fix 125: 字体对象不是 COM 对象, 但生成代码会把 `With <font>: .Name = x` 编译成
// vb6_ComSetProp(字体指针, L"Name", ...) —— 对普通结构体做 IDispatch::Invoke 会走
// 垃圾 vtable 直接崩 (LabelPlus 的 `Property Set Font` 就是这么崩的), 于是
// ucProgressCircular 的 Caption1_Font/Caption2_Font (14.25/8.25) 一直无法应用。
// 这里向 COM 层暴露"身份判定 + 字段定位", 由 vb6com_invoke.c 直接读写字段。
int32_t vb6_UC_IsFont(const void* p) {
    return vb6_uc_fontOf((void*)p) != NULL;
}

// 返回字段地址; *kind: 0=BSTR, 1=float, 2=int16, 3=int32; 未命中返回 NULL
void* vb6_UC_FontField(void* p, const wchar_t* name, int32_t* kind) {
    vb6_ComIface_Font* f = vb6_uc_fontOf(p);
    if (!f || !name || !kind) return NULL;
    if (_wcsicmp(name, L"Name") == 0)          { *kind = 0; return &f->Name; }
    if (_wcsicmp(name, L"Size") == 0)          { *kind = 1; return &f->Size; }
    if (_wcsicmp(name, L"Bold") == 0)          { *kind = 2; return &f->Bold; }
    if (_wcsicmp(name, L"Italic") == 0)        { *kind = 2; return &f->Italic; }
    if (_wcsicmp(name, L"Underline") == 0)     { *kind = 2; return &f->Underline; }
    if (_wcsicmp(name, L"Strikethrough") == 0) { *kind = 2; return &f->Strikethrough; }
    if (_wcsicmp(name, L"Weight") == 0)        { *kind = 3; return &f->Weight; }
    if (_wcsicmp(name, L"Charset") == 0)       { *kind = 3; return &f->Charset; }
    return NULL;
}

// Fix 128: 把 src 字体的 8 个字段拷进 dst 字体对象 (原地覆写)。
// 用途: .frm 的 `BeginProperty Caption1_Font` 这类**Property Set** 型字体属性,
// 其 Set 实现体是 `With m_X_Font: .Name = New_Font.Name ... : Refresh` —— 直接调用
// 会在 CreateControls 阶段(Form_Load 尚未填数据)触发 Refresh, 使图表永久停在空状态
// (实测柱/面积/树/饼 只剩标题: 彩色像素 0.3~3%, 关闭后 10~59%)。
// 因此改为: 用该控件自己的 Property Get 取到内部字体对象, 直接覆写字段, 不触发 Set/Refresh。
void vb6_UC_FontAssign(void* dst, void* src) {
    vb6_ComIface_Font* d = vb6_uc_fontOf(dst);
    vb6_ComIface_Font* s = vb6_uc_fontOf(src);
    if (!d || !s || d == s) return;
    if (s->Name) d->Name = SysAllocString(s->Name);
    d->Size = s->Size;
    d->Bold = s->Bold;
    d->Italic = s->Italic;
    d->Underline = s->Underline;
    d->Strikethrough = s->Strikethrough;
    d->Weight = s->Weight;
    d->Charset = s->Charset;
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
    wchar_t** keys;       // BSTR[] 与 items 平行; 可为 NULL (无键集合)
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

static void vb6_coll_push(void* coll, const vb6_VARIANT* v, const wchar_t* key) {
    vb6_CollRec* c = (vb6_CollRec*)coll;
    if (!vb6_uc_isColl(c) || !v) return;
    if (c->count >= c->cap) {
        int32_t nc = c->cap ? c->cap * 2 : 8;
        vb6_VARIANT* ni = (vb6_VARIANT*)realloc(c->items, sizeof(vb6_VARIANT) * (size_t)nc);
        if (!ni) return;
        c->items = ni;
        wchar_t** nk = (wchar_t**)realloc(c->keys, sizeof(wchar_t*) * (size_t)nc);
        if (!nk) return;
        c->keys = nk;
        c->cap = nc;
    }
    if (!c->keys) {
        c->keys = (wchar_t**)calloc((size_t)(c->cap ? c->cap : 8), sizeof(wchar_t*));
    }
    vb6_VARIANT* dst = &((vb6_VARIANT*)c->items)[c->count];
    memset(dst, 0, sizeof(*dst));
    if (v->vt == vb6_vtBSTR) {
        dst->vt = vb6_vtBSTR;
        dst->bstrVal = v->bstrVal ? SysAllocString(v->bstrVal) : NULL;
    } else {
        *dst = *v;
    }
    if (c->keys) c->keys[c->count] = key ? SysAllocString(key) : NULL;
    c->count++;
}

// 实参是 Windows VARIANT* (vb6_ComPackXxx 打包), 转成 RTL 形式后存入
void vb6_Collection_Add(void* coll, const void* winVar) {
    vb6_VARIANT hv;
    vb6_Host_FromWinVariant(winVar, &hv);
    vb6_coll_push(coll, &hv, NULL);
    vb6_Host_ClearVariant(&hv);
}

// 带键 Add (VB6: 集合.Add Item, Key); 键重复时报 457
void vb6_Collection_AddKeyed(void* coll, const void* winVar, const wchar_t* key) {
    vb6_CollRec* c = (vb6_CollRec*)coll;
    if (vb6_uc_isColl(c) && key && c->keys) {
        for (int32_t i = 0; i < c->count; i++) {
            if (c->keys[i] && _wcsicmp(c->keys[i], key) == 0) {
                vb6_ErrRaiseNumber(457);
                return;
            }
        }
    }
    vb6_VARIANT hv;
    vb6_Host_FromWinVariant(winVar, &hv);
    vb6_coll_push(coll, &hv, key);
    vb6_Host_ClearVariant(&hv);
}

int32_t vb6_Collection_Count(void* coll) {
    return vb6_uc_isColl(coll) ? ((vb6_CollRec*)coll)->count : 0;
}

// Fix 134: VB6 Collection.Add 的 Before/After 位置插入。
// Charts 2020 ucTreeMaps.AddLineSeries 用 `cValues.Remove j : cValues.Add v, , i`
// 做降序排序 —— 此前 Add 忽略 Before, 排序不生效, Squarified 布局顺序与 VB6 不符。
// before1/after1 为 1 基序号; before1 使新元素占据第 before1 位, after1 插到其后。
void vb6_Collection_AddAt(void* coll, const void* winVar, int32_t pos1) {
    vb6_CollRec* c = (vb6_CollRec*)coll;
    if (!vb6_uc_isColl(c) || !winVar) return;
    vb6_Collection_Add(coll, winVar);           // 先追加到尾部
    int32_t n = c->count;
    if (n <= 1) return;
    if (pos1 < 1) pos1 = 1;
    if (pos1 > n) pos1 = n;
    vb6_VARIANT* items = (vb6_VARIANT*)c->items;
    vb6_VARIANT tmp = items[n - 1];             // 刚追加的元素
    memmove(&items[pos1 - 1 + 1], &items[pos1 - 1],
            sizeof(vb6_VARIANT) * (size_t)(n - pos1));
    items[pos1 - 1] = tmp;
    if (c->keys) {
        wchar_t* kt = c->keys[n - 1];
        memmove(&c->keys[pos1 - 1 + 1], &c->keys[pos1 - 1],
                sizeof(wchar_t*) * (size_t)(n - pos1));
        c->keys[pos1 - 1] = kt;
    }
}

void vb6_Collection_AddKeyedAt(void* coll, const void* winVar, const wchar_t* key, int32_t pos1) {
    vb6_CollRec* c = (vb6_CollRec*)coll;
    if (!vb6_uc_isColl(c) || !winVar) return;
    vb6_Collection_AddKeyed(coll, winVar, key);  // 先追加 (含键重复检查)
    int32_t n = c->count;
    if (n <= 1) return;
    if (pos1 < 1) pos1 = 1;
    if (pos1 > n) pos1 = n;
    vb6_VARIANT* items = (vb6_VARIANT*)c->items;
    vb6_VARIANT tmp = items[n - 1];
    memmove(&items[pos1 - 1 + 1], &items[pos1 - 1],
            sizeof(vb6_VARIANT) * (size_t)(n - pos1));
    items[pos1 - 1] = tmp;
    if (c->keys) {
        wchar_t* kt = c->keys[n - 1];
        memmove(&c->keys[pos1 - 1 + 1], &c->keys[pos1 - 1],
                sizeof(wchar_t*) * (size_t)(n - pos1));
        c->keys[pos1 - 1] = kt;
    }
}

void vb6_Collection_Remove(void* coll, int32_t idx1) {
    vb6_CollRec* c = (vb6_CollRec*)coll;
    if (!vb6_uc_isColl(c) || idx1 < 1 || idx1 > c->count) return;
    vb6_VARIANT* items = (vb6_VARIANT*)c->items;
    vb6_VARIANT* v = &items[idx1 - 1];
    if (v->vt == vb6_vtBSTR && v->bstrVal) { SysFreeString(v->bstrVal); }
    if (c->keys) {
        if (c->keys[idx1 - 1]) SysFreeString(c->keys[idx1 - 1]);
        memmove(&c->keys[idx1 - 1], &c->keys[idx1], sizeof(wchar_t*) * (size_t)(c->count - idx1));
    }
    memmove(&items[idx1 - 1], &items[idx1], sizeof(vb6_VARIANT) * (size_t)(c->count - idx1));
    c->count--;
}

// 按键删除 (VB6: 集合.Remove Key)
void vb6_Collection_RemoveByKey(void* coll, const wchar_t* key) {
    vb6_CollRec* c = (vb6_CollRec*)coll;
    if (!vb6_uc_isColl(c) || !key || !c->keys) return;
    for (int32_t i = 0; i < c->count; i++) {
        if (c->keys[i] && _wcsicmp(c->keys[i], key) == 0) {
            vb6_Collection_Remove(coll, i + 1);
            return;
        }
    }
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

// 按键查找 (VB6: 集合(Key)); 未找到时报错 5 (供 On Error Resume Next + Err 判定)
int32_t vb6_Collection_ItemByKey(void* coll, const wchar_t* key, void* outV) {
    vb6_VARIANT* out = (vb6_VARIANT*)outV;
    vb6_CollRec* c = (vb6_CollRec*)coll;
    if (vb6_uc_isColl(c) && key && c->keys) {
        for (int32_t i = 0; i < c->count; i++) {
            if (c->keys[i] && _wcsicmp(c->keys[i], key) == 0) {
                vb6_coll_itemCopy(&((const vb6_VARIANT*)c->items)[i], out);
                return 1;
            }
        }
    }
    vb6_ho_setVariantEmpty(out);
    vb6_ErrRaiseNumber(5);   // 无效的过程调用或参数 (键不存在)
    return 0;
}

// cgen 对 `As New Collection` 生成 vb6_cls_<Name>_New(); 内建 Collection 无项目类,
// 以此别名桥接 (见 cgen_decl_var.cpp / cgen_localdecl.cpp 的 Collection 注册).
void* vb6_cls_Collection_New(void) { return vb6_Collection_New(); }

// For Each 枚举器: {coll, nextIdx} 两元素句柄, 语义同 Controls 枚举
// 注意: 句柄首元素存的是指针, 必须用 intptr_t 存储 —— x64 下按 int32_t 存会
// 截断高 32 位, vb6_ForEach_Next 里再当指针解引用即 0xC0000005.
void* vb6_Collection_EnumInit(void* coll) {
    intptr_t* e = (intptr_t*)malloc(sizeof(intptr_t) * 2);
    if (e) { e[0] = (intptr_t)coll; e[1] = 0; }
    return e;
}

int32_t vb6_Collection_EnumNext(void* enumPtr, void* outV) {
    vb6_VARIANT* out = (vb6_VARIANT*)outV;
    intptr_t* e = (intptr_t*)enumPtr;
    if (!e) return 0;
    vb6_CollRec* c = (vb6_CollRec*)e[0];
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
            BSTR k = (argc >= 2) ? vb6_ho_variantToBstr((vb6_VARIANT*)argv[1]) : NULL;
            /* Fix 134: VB6 Add(Item[, Key, Before, After]) 的位置插入 */
            if (argc >= 3 && argv[2]) {
                vb6_VARIANT* b = (vb6_VARIANT*)argv[2];
                if (b->vt != vb6_vtEmpty) {
                    int32_t pos = vb6_ho_variantToLong(b);
                    if (k) { vb6_Collection_AddKeyedAt(obj, argv[0], k, pos); }
                    else   { vb6_Collection_AddAt(obj, argv[0], pos); }
                    return 1;
                }
            }
            if (argc >= 4 && argv[3]) {
                vb6_VARIANT* a = (vb6_VARIANT*)argv[3];
                if (a->vt != vb6_vtEmpty) {
                    int32_t pos = vb6_ho_variantToLong(a) + 1;
                    if (k) { vb6_Collection_AddKeyedAt(obj, argv[0], k, pos); }
                    else   { vb6_Collection_AddAt(obj, argv[0], pos); }
                    return 1;
                }
            }
            if (k) vb6_Collection_AddKeyed(obj, argv[0], k);
            else   vb6_Collection_Add(obj, argv[0]);
        } else if (_wcsicmp(name, L"Item") == 0 && argc >= 1) {
            vb6_VARIANT* a = (vb6_VARIANT*)argv[0];
            BSTR k = vb6_ho_variantToBstr(a);
            if (k) vb6_Collection_ItemByKey(obj, k, out);
            else   vb6_Collection_Item(obj, vb6_ho_variantToLong(a), out);
        } else if (_wcsicmp(name, L"Remove") == 0 && argc >= 1) {
            vb6_VARIANT* a = (vb6_VARIANT*)argv[0];
            BSTR k = vb6_ho_variantToBstr(a);
            if (k) vb6_Collection_RemoveByKey(obj, k);
            else   vb6_Collection_Remove(obj, vb6_ho_variantToLong(a));
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
