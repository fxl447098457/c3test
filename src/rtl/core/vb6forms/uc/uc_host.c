// uc_host.c - vb6forms_uc 拆分片：UserControl 宿主实例状态 + 公开 API
//
// 内容 = 拆分前 vb6forms_uc.c 第 41~298 / 613~629 / 631~739 / 741~753 行，纯搬移零重排无行为改动
// 跨族共享符号见 vb6forms_uc_internal.h

#include "vb6forms_uc_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// UserControl 宿主描述 (生成代码为每个 .ctl 提供)
// ============================================================

/* vb6_UserControlDesc 定义见 vb6forms.h (生成代码与 RTL 共用同一份) */

const vb6_UserControlDesc* g_uc_descs[VB6_UC_MAX_DESC];
int32_t g_uc_descCount = 0;

vb6_UCRec g_uc_recs[VB6_UC_MAX_INST];
int32_t g_uc_recCount = 0;
// Fix 112: 可选生命周期日志；每行立即关闭文件，异常退出也保留最后阶段。
void vb6_uc_trace(const char* phase, const char* type, void* me) {
    const char* path = getenv("C3_UC_TRACE");
    if (!path || !*path) return;
    FILE* f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%lu %s %s me=%p\n", (unsigned long)GetTickCount(), phase, type ? type : "?", me);
    fclose(f);
}

vb6_UCRec* g_uc_current = NULL;   // 最近一次进入的实例 (供 Refresh/PropertyChange)

// Fix 119: 待应用的实例字体。VB6 里每个控件实例有独立的 Font 对象(.frm 的
// BeginProperty Font 块), 且控件会在 InitProperties 里以 UserControl.Font 为
// 默认字体基准 (如 m_TitleFont.Size = UserControl.Font.Size + 8)。因此字体必须在
// 实例初始化**之前**就位 —— cgen 在 vb6_UC_HostCreate 之前调用
// vb6_UC_SetPendingFont(), HostCreate 把它装进 r->font, push 时既成为
// vb6_UserControl_Font 也成为 Ambient.Font。
vb6_ComIface_Font* g_uc_pendingFont = NULL;

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

vb6_HostObjRec g_ho[VB6_UC_MAX_OBJ];
int32_t g_hoCount = 0;

// 合成对象: Controls 集合

// 合成对象: Font (stdole.StdFont 的最小形态)

vb6_UCFontRec* g_uc_fonts;
extern vb6_ComIface_Font g_vb6_UserControl_FontObj;

// Fix 112c: 前置声明 (定义在宿主分派节, Collection 实现会用到)

// 安全解引用守卫: 控件/窗体对象以 HWND 形式传入, 而 HWND 是内核句柄而非用户指针;
// 直接按 tag 结构解引用会触发 0xC0000005. 解引用 tag 前先校验目标地址可读.
int32_t vb6_uc_ptrReadable(const void* p, size_t n) {
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

int32_t vb6_uc_isControls(const void* p) {
    return vb6_uc_ptrReadable(p, sizeof(int32_t)) &&
           ((const vb6_UCControls*)p)->tag == VB6_UC_CONTROLS_TAG;
}
int32_t vb6_uc_isFont(const void* p) {
    // Fix 112e: 先按身份识别裸字体；不可将 Name 字段当 IDispatch vtable。
    if (!p) return 0;
    if (p == &g_vb6_UserControl_FontObj) return 1;
    for (vb6_UCFontRec* f = g_uc_fonts; f; f = f->next)
        if (p == f->font || p == f) return 1;
    return 0;
}

vb6_HostObjRec* vb6_ho_find(const void* hwnd) {
    for (int32_t i = 0; i < g_hoCount; i++) {
        if (g_ho[i].hwnd == hwnd) return &g_ho[i];
    }
    return NULL;
}

vb6_UCRec* vb6_uc_findByHwnd(const void* hwnd) {
    for (int32_t i = 0; i < g_uc_recCount; i++) {
        if ((void*)g_uc_recs[i].hwnd == hwnd) return &g_uc_recs[i];
    }
    return NULL;
}

vb6_UCRec* vb6_uc_findByInstance(const void* inst) {
    for (int32_t i = 0; i < g_uc_recCount; i++) {
        if (g_uc_recs[i].me == inst) return &g_uc_recs[i];
    }
    return NULL;
}

// ============================================================
// 宿主状态换入/换出 (进程级 vb6_UserControl_* 全局)
// ============================================================

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

void vb6_uc_push(vb6_UCRec* r, vb6_UCSaved* saved) {
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

void vb6_uc_pop(const vb6_UCSaved* saved) {
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
#include "uc_host_create.inc"

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

#ifdef __cplusplus
} // extern "C"
#endif
