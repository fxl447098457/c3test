// uc_controls.c - vb6forms_uc 拆分片：Controls 集合 + Font 对象
//
// 内容 = 拆分前 vb6forms_uc.c 第 821~891 / 893~962 行，纯搬移零重排无行为改动
// 跨族共享符号见 vb6forms_uc_internal.h

#include "vb6forms_uc_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

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

vb6_UCControls* vb6_uc_newControls(void* formHwnd) {
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

/* Fix 148: 按 VB6 名查找控件 — 容器 Controls("txtDoc(0)") 语义.
 * VB6 控件 (如 NewTab 的 TDIMode) 通过 UserControl.Parent.Controls(name, tabIdx)
 * 拿到宿主窗体上的控件 (用于 SetParent 重新父化到 Tab 页).
 * name 支持 "Name" 与 "Name(idx)" 两种写法; tabIdx<0 表示不限.
 * 返回控件 hwnd (未找到 NULL). */
void* vb6_UC_ControlsItemByName(void* coll, const wchar_t* name, int32_t tabIdx) {
    (void)tabIdx;
    if (!name || !*name) return NULL;
    wchar_t base[VB6_UC_NAME_LEN];
    int wantIdx = -1;
    /* 拆 "Name(idx)" */
    const wchar_t* lp = wcschr(name, L'(');
    if (lp) {
        size_t n = (size_t)(lp - name);
        if (n >= VB6_UC_NAME_LEN) n = VB6_UC_NAME_LEN - 1;
        wcsncpy(base, name, n); base[n] = 0;
        wantIdx = _wtoi(lp + 1);
    } else {
        wcsncpy(base, name, VB6_UC_NAME_LEN - 1); base[VB6_UC_NAME_LEN - 1] = 0;
    }
    HWND formHwnd = (coll && vb6_uc_isControls(coll)) ? (HWND)vb6_uc_controlsForm(coll) : NULL;
    /* 优先在已注册宿主对象表里找 (名字精确, 不受父窗口层级影响) */
    for (int32_t i = 0; i < g_hoCount; i++) {
        vb6_HostObjRec* r = &g_ho[i];
        if (r->isForm || !r->hwnd) continue;
        if (formHwnd && GetAncestor((HWND)r->hwnd, GA_ROOT) != formHwnd
            && !IsChild(formHwnd, (HWND)r->hwnd)) {
            /* 不在该窗体下 (可能是别的窗体的同名控件) — 跳过 */
            if (!IsWindow((HWND)r->hwnd)) continue;
        }
        if (_wcsicmp(r->name, base) != 0) continue;
        if (wantIdx >= 0 && r->index != wantIdx) continue;
        return r->hwnd;
    }
    return NULL;
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

vb6_ComIface_Font* vb6_uc_fontOf(void* p) {
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

#ifdef __cplusplus
} // extern "C"
#endif
