// uc_hostmodel.c - vb6forms_uc 拆分片：窗体/控件宿主对象模型（注册/判定/分派）
//
// 内容 = 拆分前 vb6forms_uc.c 第 755~819 / 1187~1300 / 1303~1303 / 1391~1391 / 1423~1423 / 1497~1542 行，纯搬移零重排无行为改动
// 跨族共享符号见 vb6forms_uc_internal.h

#include "vb6forms_uc_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

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

vb6_HostObjRec* vb6_ho_findWindow(const void* hwnd) {
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

void vb6_ho_setVariantLong(vb6_VARIANT* out, int32_t v) {
    memset(out, 0, sizeof(*out));
    out->vt = vb6_vtLong;   // 由调用方按 VT_I4 解释
    out->lVal = v;
}

void vb6_ho_setVariantBstr(vb6_VARIANT* out, BSTR s) {
    memset(out, 0, sizeof(*out));
    out->vt = vb6_vtBSTR;
    out->bstrVal = s;
}

void vb6_ho_setVariantDispatch(vb6_VARIANT* out, void* p) {
    // 注意: 宿主合成的"对象"(Controls 集合 / Font 代理 / 控件 HWND)不是真 COM 对象,
    // 调用方 vb6_ComVarClear → VariantClear 会对 VT_DISPATCH 调 Release → 崩溃.
    // 因此这里一律返回 Empty, 让上层走 Nothing 分支 (等价 VB6 On Error Resume Next).
    (void)p;
    memset(out, 0, sizeof(*out));
    out->vt = vb6_vtEmpty;
}

void vb6_ho_setVariantEmpty(vb6_VARIANT* out) {
    memset(out, 0, sizeof(*out));
    out->vt = vb6_vtEmpty;
}

void vb6_ho_setVariantDouble(vb6_VARIANT* out, double v) {
    memset(out, 0, sizeof(*out));
    out->vt = vb6_vtDouble;
    out->dblVal = v;
}

int32_t vb6_ho_variantToLong(const vb6_VARIANT* v) {
    if (!v) return 0;
    switch (v->vt) {
        case vb6_vtInteger: case vb6_vtLong: case vb6_vtBoolean: case vb6_vtByte:
            return v->lVal;
        case vb6_vtSingle: case vb6_vtDouble: return (int32_t)v->dblVal;
        default: return 0;
    }
}

double vb6_ho_variantToDouble(const vb6_VARIANT* v) {
    if (!v) return 0.0;
    switch (v->vt) {
        case vb6_vtSingle: case vb6_vtDouble: return v->dblVal;
        case vb6_vtInteger: case vb6_vtLong: case vb6_vtBoolean: return (double)v->lVal;
        default: return 0.0;
    }
}

BSTR vb6_ho_variantToBstr(const vb6_VARIANT* v) {
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

int32_t vb6_Host_GetProp(void* obj, const wchar_t* name, void* outV) {
#include "uc_hostmodel_getprop.inc"

}

int32_t vb6_Host_SetProp(void* obj, const wchar_t* name, const void* inV) {
#include "uc_hostmodel_setprop.inc"

}

int32_t vb6_Host_Call(void* obj, const wchar_t* name, int32_t argc, void** argv, void* outV) {
#include "uc_hostmodel_call.inc"

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
