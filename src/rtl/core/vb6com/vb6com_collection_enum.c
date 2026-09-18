// vb6com_collection_enum.c - 内建 Collection 的 _NewEnum 枚举器 (IEnumVARIANT)
//
// Fix 103 的第二个编译单元: vb6com_collection.c 里 IDispatch::Invoke 处理
// DISPID_NEWENUM 时装配本文件的 vtable (g_collEnumVtbl), 实例由 coll_doNewEnum
// 创建。拆开只为让两个文件都 <500 行 (台账口径), 逻辑与拆分前逐行一致。
//
// 枚举器按 0-based 下标向前推进, 并持有集合引用 (Release 时归还)。
// 与 vb6com_foreach.c 的 For Each 通路对接: 那里对返回值 QI IID_IEnumVARIANT。

#include "vb6com.h"
#include "vb6com_internal.h"

// ============================================================
// IUnknown / IEnumVARIANT
// ============================================================

static HRESULT STDMETHODCALLTYPE collEnum_QueryInterface(IEnumVARIANT* pEnum, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &IID_IEnumVARIANT) ||
        IsEqualIID(riid, &IID_IDispatch)) {
        *ppv = (void*)pEnum;
        pEnum->lpVtbl->AddRef(pEnum);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE collEnum_AddRef(IEnumVARIANT* pEnum) {
    return (ULONG)InterlockedIncrement(&((vb6_CollEnum*)pEnum)->refCount);
}

static ULONG STDMETHODCALLTYPE collEnum_Release(IEnumVARIANT* pEnum) {
    vb6_CollEnum* e = (vb6_CollEnum*)pEnum;
    LONG left = InterlockedDecrement(&e->refCount);
    if (left == 0) {
        if (e->coll) e->coll->lpVtbl->Release((IDispatch*)e->coll);
        free(e);
    }
    return (ULONG)left;
}

static HRESULT STDMETHODCALLTYPE collEnum_Next(IEnumVARIANT* pEnum, ULONG celt,
                                               VARIANT* rgVar, ULONG* pCeltFetched) {
    vb6_CollEnum* e = (vb6_CollEnum*)pEnum;
    if (pCeltFetched) *pCeltFetched = 0;
    if (!rgVar) return E_POINTER;
    if (!e->coll) return S_FALSE;

    ULONG got = 0;
    while (got < celt && e->index < e->coll->count) {
        VariantInit(&rgVar[got]);
        if (FAILED(VariantCopy(&rgVar[got], &e->coll->entries[e->index].value))) break;
        e->index++;
        got++;
    }
    if (pCeltFetched) *pCeltFetched = got;
    return (got == celt) ? S_OK : S_FALSE;
}

static HRESULT STDMETHODCALLTYPE collEnum_Skip(IEnumVARIANT* pEnum, ULONG celt) {
    vb6_CollEnum* e = (vb6_CollEnum*)pEnum;
    if (!e->coll) return S_FALSE;
    e->index += (int32_t)celt;
    if (e->index >= e->coll->count) {
        e->index = e->coll->count;
        return S_FALSE;
    }
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE collEnum_Reset(IEnumVARIANT* pEnum) {
    ((vb6_CollEnum*)pEnum)->index = 0;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE collEnum_Clone(IEnumVARIANT* pEnum, IEnumVARIANT** ppEnum) {
    vb6_CollEnum* e = (vb6_CollEnum*)pEnum;
    if (!ppEnum) return E_POINTER;
    *ppEnum = NULL;
    vb6_CollEnum* clone = (vb6_CollEnum*)calloc(1, sizeof(vb6_CollEnum));
    if (!clone) return E_OUTOFMEMORY;
    clone->lpVtbl   = &g_collEnumVtbl;
    clone->refCount = 1;
    clone->coll     = e->coll;
    clone->index    = e->index;
    if (clone->coll) clone->coll->lpVtbl->AddRef((IDispatch*)clone->coll);
    *ppEnum = (IEnumVARIANT*)clone;
    return S_OK;
}

// ============================================================
// vtable 装配 (供 vb6com_collection.c 的 coll_doNewEnum 使用)
// ============================================================

IEnumVARIANTVtbl g_collEnumVtbl = {
    collEnum_QueryInterface,
    collEnum_AddRef,
    collEnum_Release,
    collEnum_Next,
    collEnum_Skip,
    collEnum_Reset,
    collEnum_Clone,
};
