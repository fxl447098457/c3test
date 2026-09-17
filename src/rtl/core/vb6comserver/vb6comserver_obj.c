// vb6comserver_obj.c - vb6comserver 模块拆分: IDispatch 包装层 (vb6_ComObject)
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
// vb6_ComObject - IDispatch wrapper implementation
// ============================================================

// --- IUnknown ---

static HRESULT STDMETHODCALLTYPE ComObj_QueryInterface(vb6_ComObject* self, REFIID riid, void** ppv) {
    int i;
    if (!ppv) return E_POINTER;
    if (IsEqualIID(riid, &IID_IUnknown_) || IsEqualIID(riid, &IID_IDispatch_)) {
        *ppv = self;
        self->vtable->AddRef(self);
        return S_OK;
    }
    // P12.1: Check Implements interface IIDs
    if (self->desc && self->desc->ifaceCount > 0 && self->desc->ifaceIids) {
        for (i = 0; i < self->desc->ifaceCount; i++) {
            if (IsEqualIID(riid, self->desc->ifaceIids[i])) {
                *ppv = self;  // dispinterface: same IDispatch pointer
                self->vtable->AddRef(self);
                return S_OK;
            }
        }
    }
    // Default dispinterface IID
    if (self->desc && self->desc->defaultIfaceIid && IsEqualIID(riid, self->desc->defaultIfaceIid)) {
        *ppv = self;  // dispinterface = same IDispatch pointer
        self->vtable->AddRef(self);
        return S_OK;
    }
    // IConnectionPointContainer (only if coclass has events)
    if (self->desc && self->desc->sourceIfaceIid && IsEqualIID(riid, &IID_IConnectionPointContainer_)) {
        if (!self->cpc) {
            self->cpc = vb6_CPC_Create(self);
        }
        if (self->cpc) {
            *ppv = self->cpc;
            self->cpc->vtable->AddRef(self->cpc);
            return S_OK;
        }
    }
    // IProvideClassInfo2
    if (self->desc && IsEqualIID(riid, &IID_IProvideClassInfo2_)) {
        if (!self->pci) {
            self->pci = vb6_PCI_Create(self);
        }
        if (self->pci) {
            *ppv = self->pci;
            self->pci->vtable->AddRef(self->pci);
            return S_OK;
        }
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE ComObj_AddRef(vb6_ComObject* self) {
    ULONG count = InterlockedIncrement(&self->refCount);
    InterlockedIncrement(&g_vb6_cRef);
    return count;
}

static ULONG STDMETHODCALLTYPE ComObj_Release(vb6_ComObject* self) {
    ULONG count = InterlockedDecrement(&self->refCount);
    InterlockedDecrement(&g_vb6_cRef);
    if (count == 0) {
        // Call VB6 Class_Terminate + Destroy
        if (self->desc && self->desc->destroyFunc && self->vb6Instance) {
            self->desc->destroyFunc(self->vb6Instance);
        }
        self->vb6Instance = NULL;
        // Release PCI
        if (self->pci) {
            self->pci->vtable->Release(self->pci);
            self->pci = NULL;
        }
        // Release CPC
        if (self->cpc) {
            self->cpc->vtable->Release(self->cpc);
            self->cpc = NULL;
        }
        CoTaskMemFree(self);
    }
    return count;
}

// --- IDispatch ---

static HRESULT STDMETHODCALLTYPE ComObj_GetTypeInfoCount(vb6_ComObject* self, UINT* pctinfo) {
    if (!pctinfo) return E_POINTER;
    *pctinfo = 1;  // Provide TypeLib info (needed for early binding)
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ComObj_GetTypeInfo(vb6_ComObject* self, UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo) {
    if (!ppTInfo) return E_POINTER;
    if (iTInfo != 0) return DISP_E_BADINDEX;
    *ppTInfo = NULL;
    if (!self->desc) return E_FAIL;
    // Get DLL path, load TypeLib from embedded resource
    wchar_t dllPath[MAX_PATH];
    HMODULE hMod = NULL;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)&g_vb6_cRef, &hMod)) {
        return E_FAIL;
    }
    GetModuleFileNameW(hMod, dllPath, MAX_PATH);
    ITypeLib* pTypeLib = NULL;
    HRESULT hr = LoadTypeLib(dllPath, &pTypeLib);
    if (FAILED(hr) || !pTypeLib) {
        return E_FAIL;
    }
    // Find coclass ITypeInfo by CLSID
    CLSID clsid;
    hr = vb6_CLSIDFromStrA(self->desc->clsidStr, &clsid);
    if (FAILED(hr)) {
        ITypeLib_Release(pTypeLib);
        return E_FAIL;
    }
    hr = ITypeLib_GetTypeInfoOfGuid(pTypeLib, &clsid, ppTInfo);
    ITypeLib_Release(pTypeLib);
    return hr;
}

static HRESULT STDMETHODCALLTYPE ComObj_GetIDsOfNames(vb6_ComObject* self, REFIID riid, 
    LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgDispId) 
{
    if (!self->desc || !self->desc->methods) return DISP_E_UNKNOWNNAME;
    
    for (UINT n = 0; n < cNames; n++) {
        rgDispId[n] = DISPID_UNKNOWN;
        for (int i = 0; i < self->desc->methodCount; i++) {
            if (wcscmp(rgszNames[n], self->desc->methods[i].name) == 0) {
                rgDispId[n] = self->desc->methods[i].dispid;
                break;
            }
        }
    }
    
    return (cNames > 0 && rgDispId[0] != DISPID_UNKNOWN) ? S_OK : DISP_E_UNKNOWNNAME;
}

static HRESULT STDMETHODCALLTYPE ComObj_Invoke(vb6_ComObject* self, DISPID dispIdMember, 
    REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pDispParams, 
    VARIANT* pVarResult, EXCEPINFO* pExcepInfo, UINT* puArgErr)
{
    if (!self->desc || !self->desc->methods) return DISP_E_MEMBERNOTFOUND;
    
    // Find method matching dispid AND invkind (same dispid may have Get/Let variants)
    const vb6_DispMethodDesc* method = NULL;
    for (int i = 0; i < self->desc->methodCount; i++) {
        if (self->desc->methods[i].dispid == dispIdMember) {
            int ik = self->desc->methods[i].invkind;
            int ok = 0;
            if (ik == 1 && (wFlags & DISPATCH_METHOD)) ok = 1;
            else if (ik == 2 && (wFlags & DISPATCH_PROPERTYGET)) ok = 1;
            else if ((ik == 4 || ik == 8) && (wFlags & DISPATCH_PROPERTYPUT)) ok = 1;
            if (ok) { method = &self->desc->methods[i]; break; }
        }
    }
    // Fallback: if no invkind match, try first dispid match
    if (!method) {
        for (int i = 0; i < self->desc->methodCount; i++) {
            if (self->desc->methods[i].dispid == dispIdMember) {
                method = &self->desc->methods[i];
                break;
            }
        }
    }
    if (!method) return DISP_E_MEMBERNOTFOUND;
    
    // Collect arguments
    // Note: Script engines like VBScript may pass VT_I2 etc.,
    // while bridge functions expect VT_I4 (via lVal). Coerce each param to VT_I4.
    int argc = pDispParams ? (int)pDispParams->cArgs : 0;
    void** args = NULL;
    VARIANT* coercedArgs = NULL;
    
    if (argc > 0) {
        args = (void**)CoTaskMemAlloc(argc * sizeof(void*));
        coercedArgs = (VARIANT*)CoTaskMemAlloc(argc * sizeof(VARIANT));
        if (!args || !coercedArgs) {
            if (args) CoTaskMemFree(args);
            if (coercedArgs) CoTaskMemFree(coercedArgs);
            return E_OUTOFMEMORY;
        }
        // DISPPARAMS args are in reverse order
        for (int i = 0; i < argc; i++) {
            VARIANT* src = &pDispParams->rgvarg[argc - 1 - i];
            // Coerce numeric types to VT_I4 (bridge functions use lVal)
            if (src->vt == VT_I2 || src->vt == VT_I1 || src->vt == VT_UI1 ||
                src->vt == VT_UI2 || src->vt == VT_BOOL || src->vt == VT_EMPTY) {
                VariantInit(&coercedArgs[i]);
                HRESULT hr2 = VariantChangeType(&coercedArgs[i], src, 0, VT_I4);
                if (SUCCEEDED(hr2)) {
                    args[i] = &coercedArgs[i];
                } else {
                    args[i] = src;
                }
            } else if (src->vt == VT_R4) {
                // Float -> Double for dblVal access
                VariantInit(&coercedArgs[i]);
                HRESULT hr2 = VariantChangeType(&coercedArgs[i], src, 0, VT_R8);
                if (SUCCEEDED(hr2)) {
                    args[i] = &coercedArgs[i];
                } else {
                    args[i] = src;
                }
            } else {
                args[i] = src;  // VT_I4, VT_R8, VT_BSTR, etc. pass through
            }
        }
    }
    
    // Call VB6 method
    if (method->invokeFunc) {
        method->invokeFunc(self->vb6Instance, args, argc, pVarResult);
    }
    
    if (coercedArgs) {
        for (int i = 0; i < argc; i++) VariantClear(&coercedArgs[i]);
        CoTaskMemFree(coercedArgs);
    }
    if (args) CoTaskMemFree(args);
    return S_OK;
}

// IDispatch vtable instance
static const vb6_IDispatchVtable g_ComObjectVtable = {
    ComObj_QueryInterface,
    ComObj_AddRef,
    ComObj_Release,
    ComObj_GetTypeInfoCount,
    ComObj_GetTypeInfo,
    ComObj_GetIDsOfNames,
    ComObj_Invoke,
};

// Create VB6 COM object
vb6_ComObject* vb6_ComObject_Create(const vb6_CoClassDesc* desc) {
    if (!desc || !desc->factoryFunc) return NULL;
    
    vb6_ComObject* obj = (vb6_ComObject*)CoTaskMemAlloc(sizeof(vb6_ComObject));
    if (!obj) return NULL;
    
    obj->vtable = &g_ComObjectVtable;
    obj->refCount = 1;
    obj->desc = desc;
    obj->vb6Instance = desc->factoryFunc();  // Call vb6_cls_<Name>_New()
    obj->cpc = NULL;  // Lazy init CPC
    obj->pci = NULL;  // Lazy init PCI
    // Set back-pointer for event support (first field of VB6 class struct = __comObj)
    if (obj->vb6Instance) {
        void** ppComObj = (void**)obj->vb6Instance;
        *ppComObj = obj;
    }
    
    if (!obj->vb6Instance) {
        CoTaskMemFree(obj);
        return NULL;
    }
    
    InterlockedIncrement(&g_vb6_cRef);
    return obj;
}
