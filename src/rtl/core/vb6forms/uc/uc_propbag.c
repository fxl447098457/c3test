// uc_propbag.c - vb6forms_uc 拆分片：轻量 PropertyBag (IDispatch)
//
// 内容 = vb6forms_uc.c 在 main 侧新增段（Fix 120-141 / czUI fix）的 PropertyBag 部分，
// 移植自 ferock/0.10.7 合并 origin/main 时的文件尾新增段（原为单文件实现）。
// 运行期读取设计期持久化属性：生成的 UserControl_ReadProperties 通过
// vb6_ComCall(bag, "ReadProperty", ...) late-bound 调用本实现；
// 只有 .ctl 内部的读取后同步逻辑能借此执行
// (czUI.ctl:478 `If m_Checked Then m_AnimPos = 1!` → 开关初始 ON)。
// 跨族共享符号见 vb6forms_uc_internal.h

#include "vb6forms_uc_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Vb6BagItem {
    BSTR    name;
    VARIANT val;
} Vb6BagItem;

typedef struct Vb6PropBag {
    IDispatch   disp;          // lpVtbl 在首字段
    ULONG       refs;
    Vb6BagItem* items;
    int32_t     count;
    int32_t     cap;
} Vb6PropBag;

static HRESULT STDMETHODCALLTYPE Vb6Bag_QI(IDispatch* self, REFIID riid, void** out) {
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDispatch)) {
        *out = self;
        self->lpVtbl->AddRef(self);
        return S_OK;
    }
    return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE Vb6Bag_AddRef(IDispatch* self) { return 1; }
static ULONG STDMETHODCALLTYPE Vb6Bag_Release(IDispatch* self) { return 1; }
static HRESULT STDMETHODCALLTYPE Vb6Bag_GetTypeInfoCount(IDispatch* self, UINT* n) {
    if (n) *n = 0; return S_OK;
}
static HRESULT STDMETHODCALLTYPE Vb6Bag_GetTypeInfo(IDispatch* self, UINT i, LCID l, ITypeInfo** t) {
    (void)t; return E_NOTIMPL;
}
static HRESULT STDMETHODCALLTYPE Vb6Bag_GetIDsOfNames(IDispatch* self, REFIID riid,
        LPOLESTR* names, UINT cNames, LCID lcid, DISPID* out) {
    (void)self; (void)riid; (void)lcid;
    if (!out || !cNames || !names || !names[0]) return E_POINTER;
    wchar_t buf[64];
    int i = 0;
    for (; names[0][i] && i < 63; i++) buf[i] = towlower(names[0][i]);
    buf[i] = 0;
    if (wcscmp(buf, L"readproperty") == 0)  { *out = 1; return S_OK; }
    if (wcscmp(buf, L"writeproperty") == 0) { *out = 2; return S_OK; }
    return DISP_E_UNKNOWNNAME;
}
static Vb6BagItem* Vb6Bag_Find(Vb6PropBag* bag, const wchar_t* name) {
    for (int32_t i = 0; i < bag->count; i++) {
        if (_wcsicmp(bag->items[i].name, name) == 0) return &bag->items[i];
    }
    return NULL;
}
static HRESULT STDMETHODCALLTYPE Vb6Bag_Invoke(IDispatch* self, DISPID id, REFIID riid, LCID lcid,
        WORD flags, DISPPARAMS* pd, VARIANT* result, EXCEPINFO* ei, UINT* argErr) {
    (void)riid; (void)lcid; (void)flags; (void)ei; (void)argErr;
    Vb6PropBag* bag = (Vb6PropBag*)self;
    if (!pd) return E_POINTER;
    if (id == 1) { /* ReadProperty(name[, default]) — rgvarg 逆序 */
        int c = (int)pd->cArgs;
        if (c < 1) return DISP_E_BADPARAMCOUNT;
        VARIANT* vDefault = (c >= 2) ? &pd->rgvarg[0] : NULL;
        VARIANT* vName   = &pd->rgvarg[c - 1];
        if (vName->vt != VT_BSTR || !vName->bstrVal) return DISP_E_TYPEMISMATCH;
        Vb6BagItem* it = Vb6Bag_Find(bag, vName->bstrVal);
        if (!result) return S_OK;
        VariantInit(result);
        if (it) return VariantCopy(result, &it->val);
        if (vDefault) return VariantCopy(result, vDefault);
        return S_OK;
    }
    if (id == 2) { /* WriteProperty(name, value) */
        int c = (int)pd->cArgs;
        if (c < 2) return DISP_E_BADPARAMCOUNT;
        VARIANT* vValue = &pd->rgvarg[0];
        VARIANT* vName  = &pd->rgvarg[1];
        if (vName->vt != VT_BSTR || !vName->bstrVal) return DISP_E_TYPEMISMATCH;
        Vb6BagItem* it = Vb6Bag_Find(bag, vName->bstrVal);
        if (!it) {
            if (bag->count >= bag->cap) {
                int32_t ncap = bag->cap ? bag->cap * 2 : 8;
                Vb6BagItem* ni = (Vb6BagItem*)realloc(bag->items, sizeof(Vb6BagItem) * ncap);
                if (!ni) return E_OUTOFMEMORY;
                bag->items = ni; bag->cap = ncap;
            }
            it = &bag->items[bag->count++];
            memset(it, 0, sizeof(*it));
            it->name = SysAllocString(vName->bstrVal);
        } else {
            VariantClear(&it->val);
        }
        VariantInit(&it->val);
        VariantCopy(&it->val, (VARIANT*)vValue);
        return S_OK;
    }
    return DISP_E_MEMBERNOTFOUND;
}

static IDispatchVtbl g_vb6BagVtbl = {
    Vb6Bag_QI, Vb6Bag_AddRef, Vb6Bag_Release,
    Vb6Bag_GetTypeInfoCount, Vb6Bag_GetTypeInfo, Vb6Bag_GetIDsOfNames, Vb6Bag_Invoke
};

void* vb6_UC_PropBagCreate(void) {
    Vb6PropBag* bag = (Vb6PropBag*)calloc(1, sizeof(Vb6PropBag));
    if (!bag) return NULL;
    bag->disp.lpVtbl = &g_vb6BagVtbl;
    bag->refs = 1;
    return bag;
}

void vb6_UC_PropBagFree(void* bagPtr) {
    Vb6PropBag* bag = (Vb6PropBag*)bagPtr;
    if (!bag) return;
    for (int32_t i = 0; i < bag->count; i++) {
        if (bag->items[i].name) SysFreeString(bag->items[i].name);
        VariantClear(&bag->items[i].val);
    }
    free(bag->items);
    free(bag);
}

static void vb6_BagPut(Vb6PropBag* bag, const wchar_t* name, const VARIANT* v) {
    Vb6BagItem* it = Vb6Bag_Find(bag, name);
    if (!it) {
        if (bag->count >= bag->cap) {
            int32_t ncap = bag->cap ? bag->cap * 2 : 8;
            Vb6BagItem* ni = (Vb6BagItem*)realloc(bag->items, sizeof(Vb6BagItem) * ncap);
            if (!ni) return;
            bag->items = ni; bag->cap = ncap;
        }
        it = &bag->items[bag->count++];
        memset(it, 0, sizeof(*it));
        it->name = SysAllocString(name);
    } else {
        VariantClear(&it->val);
    }
    VariantInit(&it->val);
    VariantCopy(&it->val, (VARIANT*)v);
}

void vb6_UC_BagPutStr(void* bag, const wchar_t* name, const wchar_t* value) {
    if (!bag || !name) return;
    VARIANT v; VariantInit(&v);
    v.vt = VT_BSTR; v.bstrVal = SysAllocString(value ? value : L"");
    vb6_BagPut((Vb6PropBag*)bag, name, &v);
    VariantClear(&v);
}
void vb6_UC_BagPutInt(void* bag, const wchar_t* name, int32_t value) {
    if (!bag || !name) return;
    VARIANT v; VariantInit(&v);
    v.vt = VT_I4; v.lVal = value;
    vb6_BagPut((Vb6PropBag*)bag, name, &v);
    VariantClear(&v);
}
void vb6_UC_BagPutDbl(void* bag, const wchar_t* name, double value) {
    if (!bag || !name) return;
    VARIANT v; VariantInit(&v);
    v.vt = VT_R8; v.dblVal = value;
    vb6_BagPut((Vb6PropBag*)bag, name, &v);
    VariantClear(&v);
}
void vb6_UC_BagPutBool(void* bag, const wchar_t* name, int32_t value) {
    if (!bag || !name) return;
    VARIANT v; VariantInit(&v);
    v.vt = VT_BOOL; v.boolVal = value ? VARIANT_TRUE : VARIANT_FALSE;
    vb6_BagPut((Vb6PropBag*)bag, name, &v);
    VariantClear(&v);
}

#ifdef __cplusplus
} // extern "C"
#endif
