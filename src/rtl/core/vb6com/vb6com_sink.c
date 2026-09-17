// vb6com_sink.c - vb6com 模块拆分: 事件连接点 (IConnectionPointContainer/Advise + IDispatch event sink)
// 由 vb6com.c 按 COM 调用层次拆分而来 (纯搬移, 零行为改动)

#include "vb6com.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vb6com_internal.h"


// ============================================================
// P13.21: IConnectionPointContainer / Advise support
// ============================================================

int vb6_ComAdvise(void* obj, const char* riidStr, void* sink, int* adviseCookie) {
    /* Use IUnknown QI to get IConnectionPointContainer, then vtable calls */
    IUnknown* pUnk = (IUnknown*)obj;
    IUnknown* pCPCUnk = NULL;
    HRESULT hr;
    IID iid;
    WCHAR wIID[64] = {0};
    void** pCPCVt;
    void** pCPVt;
    IUnknown* pCPUnk = NULL;
    DWORD cookie = 0;

    if (!obj || !riidStr || !sink || !adviseCookie) return -1;
    *adviseCookie = 0;

    MultiByteToWideChar(CP_ACP, 0, riidStr, -1, wIID, 64);
    if (IIDFromString(wIID, &iid) != S_OK) return -2;

    /* IID_IConnectionPointContainer = {B196B284-BAB4-101A-B69C-00AA00341D07} */
    static const IID IID_CPC = {0xB196B284, 0xBAB4, 0x101A, {0xB6,0x9C,0x00,0xAA,0x00,0x34,0x1D,0x07}};

    /* QI for IConnectionPointContainer */
    hr = pUnk->lpVtbl->QueryInterface(pUnk, &IID_CPC, (void**)&pCPCUnk);
    if (FAILED(hr) || !pCPCUnk) return -3;

    /* FindConnectionPoint is vtable index 4 (0=QI, 1=AddRef, 2=Release, 3=EnumConnPts, 4=FindCP) */
    pCPCVt = *(void***)pCPCUnk;
    typedef HRESULT(__stdcall* FindCPFunc)(void*, const IID*, IUnknown**);
    FindCPFunc findCP = (FindCPFunc)pCPCVt[4];
    hr = findCP(pCPCUnk, &iid, &pCPUnk);
    pCPCUnk->lpVtbl->Release(pCPCUnk);
    if (FAILED(hr) || !pCPUnk) return -4;

    /* Advise is vtable index 5 (0=QI,1=AddRef,2=Release,3=GetConnInterface,4=GetConnPtContainer,5=Advise) */
    pCPVt = *(void***)pCPUnk;
    typedef HRESULT(__stdcall* AdviseFunc)(void*, IUnknown*, DWORD*);
    AdviseFunc adviseFn = (AdviseFunc)pCPVt[5];
    hr = adviseFn(pCPUnk, (IUnknown*)sink, &cookie);
    pCPUnk->lpVtbl->Release(pCPUnk);
    if (FAILED(hr)) return -5;

    *adviseCookie = (int)cookie;
    return 0;
}

int vb6_ComUnadvise(void* obj, const char* riidStr, int adviseCookie) {
    IUnknown* pUnk = (IUnknown*)obj;
    IUnknown* pCPCUnk = NULL;
    IUnknown* pCPUnk = NULL;
    HRESULT hr;
    IID iid;
    WCHAR wIID[64] = {0};
    void** pCPCVt;
    void** pCPVt;

    if (!obj || !riidStr || adviseCookie == 0) return -1;

    MultiByteToWideChar(CP_ACP, 0, riidStr, -1, wIID, 64);
    if (IIDFromString(wIID, &iid) != S_OK) return -2;

    static const IID IID_CPC = {0xB196B284, 0xBAB4, 0x101A, {0xB6,0x9C,0x00,0xAA,0x00,0x34,0x1D,0x07}};

    hr = pUnk->lpVtbl->QueryInterface(pUnk, &IID_CPC, (void**)&pCPCUnk);
    if (FAILED(hr) || !pCPCUnk) return -3;

    pCPCVt = *(void***)pCPCUnk;
    typedef HRESULT(__stdcall* FindCPFunc)(void*, const IID*, IUnknown**);
    FindCPFunc findCP = (FindCPFunc)pCPCVt[4];
    hr = findCP(pCPCUnk, &iid, &pCPUnk);
    pCPCUnk->lpVtbl->Release(pCPCUnk);
    if (FAILED(hr) || !pCPUnk) return -4;

    /* Unadvise is vtable index 6 */
    pCPVt = *(void***)pCPUnk;
    typedef HRESULT(__stdcall* UnadviseFunc)(void*, DWORD);
    UnadviseFunc unadviseFn = (UnadviseFunc)pCPVt[6];
    hr = unadviseFn(pCPUnk, (DWORD)adviseCookie);
    pCPUnk->lpVtbl->Release(pCPUnk);
    int advResult = SUCCEEDED(hr) ? 0 : -5;
    return advResult;
}

// ============================================================
// P13.22: Generic IDispatch event sink
// ============================================================

/* Event sink structure: a minimal IDispatch implementation that
   maps DISPIDs to VB6 callback functions */

typedef struct VB6EventSink {
    /* Pointer to IDispatch vtable (11 methods: 7 IUnknown + 4 IDispatch) */
    void** vtable;
    /* Ref count */
    LONG refCount;
    /* DISPID -> callback mapping */
    int* dispids;
    void (**callbacks)(void*, VARIANT*, int, VARIANT*);
    int count;
    /* Consumer object (VB6 class instance) passed to each callback */
    void* handler;
    /* Source interface IID (dispinterface event sinks need to respond to QI for it) */
    IID sourceIid;
    int hasSourceIid;
} VB6EventSink;

/* Forward declarations for x86 compat (sink_AddRef/Release used before definition) */
static ULONG STDMETHODCALLTYPE sink_AddRef(IDispatch* This);
static ULONG STDMETHODCALLTYPE sink_Release(IDispatch* This);

/* IDispatch vtable methods */
static HRESULT STDMETHODCALLTYPE sink_QueryInterface(IDispatch* This, REFIID riid, void** ppv) {
    VB6EventSink* s = (VB6EventSink*)This;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDispatch) || (s->hasSourceIid && IsEqualIID(riid, &s->sourceIid))) {
        *ppv = This;
        sink_AddRef(This);
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE sink_AddRef(IDispatch* This) {
    VB6EventSink* s = (VB6EventSink*)This;
    return InterlockedIncrement(&s->refCount);
}

static ULONG STDMETHODCALLTYPE sink_Release(IDispatch* This) {
    VB6EventSink* s = (VB6EventSink*)((char*)This - offsetof(VB6EventSink, vtable));
    ULONG c = InterlockedDecrement(&s->refCount);
    if (c == 0) {
        CoTaskMemFree(s->dispids);
        CoTaskMemFree(s->callbacks);
        CoTaskMemFree(s);
    }
    return c;
}

static HRESULT STDMETHODCALLTYPE sink_GetTypeInfoCount(IDispatch* This, UINT* pctinfo) {
    if (pctinfo) *pctinfo = 0;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sink_GetTypeInfo(IDispatch* This, UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo) {
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE sink_GetIDsOfNames(IDispatch* This, REFIID riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgDispId) {
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE sink_Invoke(IDispatch* This, DISPID dispIdMember, REFIID riid,
    LCID lcid, WORD wFlags, DISPPARAMS* pDispParams, VARIANT* pVarResult,
    EXCEPINFO* pExcepInfo, UINT* puArgErr) {

    VB6EventSink* s = (VB6EventSink*)((char*)This - offsetof(VB6EventSink, vtable));
    int i, argc, j;
    VARIANT result;
    VARIANT* vbArgs = NULL;
    
    /* Find callback by DISPID */
    for (i = 0; i < s->count; i++) {
        if (s->dispids[i] == (int)dispIdMember) {
            VariantInit(&result);
            if (s->callbacks[i]) {
                argc = (int)pDispParams->cArgs;
                vbArgs = NULL;
                if (argc > 0) {
                    vbArgs = (VARIANT*)malloc(argc * sizeof(VARIANT));
                    for (j = 0; j < argc; j++) {
                        VariantInit(&vbArgs[j]);
                        VariantCopy(&vbArgs[j], &pDispParams->rgvarg[argc - 1 - j]);
                    }
                }
                s->callbacks[i](s->handler, vbArgs, argc, &result);
                if (vbArgs) {
                    for (j = 0; j < argc; j++) VariantClear(&vbArgs[j]);
                    free(vbArgs);
                }
            }
            if (pVarResult) VariantCopy(pVarResult, &result);
            VariantClear(&result);
            return S_OK;
        }
    }
    return S_OK;  /* Unknown dispid: silently ignore */
}

/* Vtable template */
static void* g_eventSinkVtable[11] = {
    sink_QueryInterface,
    sink_AddRef,
    sink_Release,
    sink_GetTypeInfoCount,
    sink_GetTypeInfo,
    sink_GetIDsOfNames,
    sink_Invoke
};

void* vb6_CreateEventSink(const int* dispids, void** callbacks, int count, const IID* sourceIid, void* handler) {
    VB6EventSink* s = (VB6EventSink*)CoTaskMemAlloc(sizeof(VB6EventSink));
    if (!s) return NULL;
    memset(s, 0, sizeof(VB6EventSink));
    
    /* Copy vtable template */
    s->vtable = g_eventSinkVtable;
    
    s->refCount = 1;
    s->count = count;
    s->handler = handler;
    
    /* Copy source interface IID if provided */
    if (sourceIid) {
        s->sourceIid = *sourceIid;
        s->hasSourceIid = 1;
    }
    
    /* Copy DISPID mappings */
    s->dispids = (int*)CoTaskMemAlloc(count * sizeof(int));
    s->callbacks = (void(**)(void*,VARIANT*,int,VARIANT*))CoTaskMemAlloc(count * sizeof(void(*)(void*,VARIANT*,int,VARIANT*)));
    if (!s->dispids || !s->callbacks) {
        CoTaskMemFree(s->dispids);
        CoTaskMemFree(s->callbacks);
        CoTaskMemFree(s);
        return NULL;
    }
    memcpy(s->dispids, dispids, count * sizeof(int));
    memcpy(s->callbacks, callbacks, count * sizeof(void(*)()));
    
    /* Return pointer to the vtable (IDispatch*) */
    return &s->vtable;
}

void vb6_FreeEventSink(void* sink) {
    if (!sink) return;
    IDispatch* pDisp = (IDispatch*)sink;
    pDisp->lpVtbl->Release(pDisp);
}
