// vb6comserver_factory.c - vb6comserver 模块拆分: 类工厂 (IClassFactory)
// 由 vb6comserver.c 按 COM 接口家族拆分而来 (纯搬移, 零行为改动)

#include <string.h>
#define COBJMACROS  /* Enable C COM macros (ITypeLib_Release etc.) */
#include "vb6comserver.h"
#include "vb6comserver_internal.h"

#ifndef CONNECT_E_NOCONNECTION
#define CONNECT_E_NOCONNECTION 0x80040200
#endif

#ifndef GUIDKIND_DEFAULT_SOURCE_DISP_IID
#define GUIDKIND_DEFAULT_SOURCE_DISP_IID 1
#endif


// ============================================================
// ClassFactory implementation
// ============================================================

// --- IUnknown ---

static HRESULT STDMETHODCALLTYPE CF_QueryInterface(vb6_ClassFactory* self, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (IsEqualIID(riid, &IID_IUnknown_) || IsEqualIID(riid, &IID_IClassFactory_)) {
        *ppv = self;
        self->vtable->AddRef(self);
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE CF_AddRef(vb6_ClassFactory* self) {
    return InterlockedIncrement(&self->refCount);
}

static ULONG STDMETHODCALLTYPE CF_Release(vb6_ClassFactory* self) {
    ULONG count = InterlockedDecrement(&self->refCount);
    if (count == 0) {
        CoTaskMemFree(self);
    }
    return count;
}

// --- IClassFactory ---

static HRESULT STDMETHODCALLTYPE CF_CreateInstance(vb6_ClassFactory* self, IUnknown* pUnkOuter, 
    REFIID riid, void** ppv) 
{
    if (!ppv) return E_POINTER;
    if (pUnkOuter) return CLASS_E_NOAGGREGATION;
    
    // Create VB6 COM object
    vb6_ComObject* obj = vb6_ComObject_Create(self->desc);
    if (!obj) return E_OUTOFMEMORY;
    
    // QI for requested interface
    HRESULT hr = obj->vtable->QueryInterface(obj, riid, ppv);
    obj->vtable->Release(obj);  // QI already AddRef'd, release initial ref
    return hr;
}

static HRESULT STDMETHODCALLTYPE CF_LockServer(vb6_ClassFactory* self, BOOL fLock) {
    if (fLock) {
        InterlockedIncrement(&g_vb6_cServerLock);
    } else {
        InterlockedDecrement(&g_vb6_cServerLock);
    }
    return S_OK;
}

// IClassFactory vtable instance
static const vb6_IClassFactoryVtable g_ClassFactoryVtable = {
    CF_QueryInterface,
    CF_AddRef,
    CF_Release,
    CF_CreateInstance,
    CF_LockServer,
};

// Get ClassFactory for specified CLSID
HRESULT vb6_GetClassFactory(REFCLSID rclsid, REFIID riid, void** ppv, 
                            const vb6_CoClassDesc* coclasses, int count) {
    if (!ppv) return E_POINTER;
    *ppv = NULL;
    
    // Find matching coclass
    const vb6_CoClassDesc* found = NULL;
    for (int i = 0; i < count; i++) {
        CLSID clsid;
        if (SUCCEEDED(vb6_CLSIDFromStrA((LPCSTR)coclasses[i].clsidStr, &clsid))) {
            if (IsEqualCLSID(rclsid, &clsid)) {
                found = &coclasses[i];
                break;
            }
        }
    }
    
    if (!found) return CLASS_E_CLASSNOTAVAILABLE;
    
    // Create ClassFactory
    vb6_ClassFactory* cf = (vb6_ClassFactory*)CoTaskMemAlloc(sizeof(vb6_ClassFactory));
    if (!cf) return E_OUTOFMEMORY;
    
    cf->vtable = &g_ClassFactoryVtable;
    cf->refCount = 1;
    cf->desc = found;
    
    // QI for requested interface
    HRESULT hr = cf->vtable->QueryInterface(cf, riid, ppv);
    cf->vtable->Release(cf);  // QI already AddRef'd
    return hr;
}
