// vb6forms_axsite.c - vb6forms 模块拆分: 窗体 Dispatch 属性 + ActiveX 站点 (axSite)
// 由 vb6forms.c 按控件/窗体功能家族拆分而来 (纯搬移, 零行为改动)

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#endif

#include "vb6forms.h"
#include "vb6forms_internal.h"
#include <stdio.h>
#include <stdarg.h>

#include <stdlib.h>   /* malloc, free */
#include <oleauto.h>  /* SysAllocString, BSTR */
#include <olectl.h>   /* IPicture, OleLoadPicture, OLE_HANDLE */

static const wchar_t* g_FormDispatchProp = L"VB6_Form_IDispatch";

void vb6_Form_SetDispatch(void* hwnd, void* pDispatch) {
    if (!hwnd) return;
    SetPropW((HWND)hwnd, g_FormDispatchProp, (HANDLE)pDispatch);
}

/* ===== 简化 ActiveX 控件宿主 ===== */

/* Minimal IOleClientSite + IOleInPlaceSite + IOleInPlaceFrame
   用于 Controls.Add 动态加载 ActiveX 控件并嵌入 Form 窗口 */

typedef struct Vb6AxSite Vb6AxSite;

struct Vb6AxSite {
    /* IOleClientSite */
    const IOleClientSiteVtbl* lpVtblClientSite;
    /* IOleInPlaceSite (通过 QI 返回同一个对象) */
    const IOleInPlaceSiteVtbl* lpVtblInPlaceSite;
    /* IOleInPlaceFrame (通过 QI 返回同一个对象) */
    const IOleInPlaceFrameVtbl* lpVtblInPlaceFrame;
    LONG ref;
    HWND hwndForm;        /* 宿主 Form 窗口句柄 */
    IOleObject* pOleObj;  /* 被托管的控件 (在SetClientSite后保存) */
};

/* --- IOleClientSite --- */
static HRESULT STDMETHODCALLTYPE axSite_CS_QueryInterface(IOleClientSite* This, REFIID riid, void** ppv) {
    Vb6AxSite* s = (Vb6AxSite*)This;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IOleClientSite)) {
        *ppv = &s->lpVtblClientSite; s->ref++; return S_OK;
    }
    if (IsEqualIID(riid, &IID_IOleInPlaceSite)) {
        *ppv = &s->lpVtblInPlaceSite; s->ref++; return S_OK;
    }
    if (IsEqualIID(riid, &IID_IOleInPlaceFrame)) {
        *ppv = &s->lpVtblInPlaceFrame; s->ref++; return S_OK;
    }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE axSite_CS_AddRef(IOleClientSite* This) {
    Vb6AxSite* s = (Vb6AxSite*)This; return InterlockedIncrement(&s->ref);
}
static ULONG STDMETHODCALLTYPE axSite_CS_Release(IOleClientSite* This) {
    Vb6AxSite* s = (Vb6AxSite*)This; LONG r = InterlockedDecrement(&s->ref);
    if (r <= 0) { free(s); } return r;
}
static HRESULT STDMETHODCALLTYPE axSite_CS_SaveObject(IOleClientSite* This) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE axSite_CS_GetMoniker(IOleClientSite* This, DWORD a, DWORD b, IMoniker** ppM) { *ppM=NULL; return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE axSite_CS_GetContainer(IOleClientSite* This, IOleContainer** ppC) { *ppC=NULL; return E_NOINTERFACE; }
static HRESULT STDMETHODCALLTYPE axSite_CS_ShowObject(IOleClientSite* This) { return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_CS_OnShowWindow(IOleClientSite* This, BOOL f) { return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_CS_RequestNewObjectLayout(IOleClientSite* This) { return E_NOTIMPL; }

static const IOleClientSiteVtbl g_axSiteClientSiteVtbl = {
    axSite_CS_QueryInterface, axSite_CS_AddRef, axSite_CS_Release,
    axSite_CS_SaveObject, axSite_CS_GetMoniker, axSite_CS_GetContainer,
    axSite_CS_ShowObject, axSite_CS_OnShowWindow, axSite_CS_RequestNewObjectLayout
};

/* --- IOleInPlaceSite --- */
static HRESULT STDMETHODCALLTYPE axSite_IPS_QueryInterface(IOleInPlaceSite* This, REFIID riid, void** ppv) {
    return axSite_CS_QueryInterface((IOleClientSite*)This, riid, ppv);
}
static ULONG STDMETHODCALLTYPE axSite_IPS_AddRef(IOleInPlaceSite* This) {
    return axSite_CS_AddRef((IOleClientSite*)This);
}
static ULONG STDMETHODCALLTYPE axSite_IPS_Release(IOleInPlaceSite* This) {
    return axSite_CS_Release((IOleClientSite*)This);
}
static HRESULT STDMETHODCALLTYPE axSite_IPS_GetWindow(IOleInPlaceSite* This, HWND* phwnd) {
    Vb6AxSite* s = (Vb6AxSite*)This; *phwnd = s->hwndForm; return S_OK;
}
static HRESULT STDMETHODCALLTYPE axSite_IPS_ContextSensitiveHelp(IOleInPlaceSite* This, BOOL f) { return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_IPS_CanInPlaceActivate(IOleInPlaceSite* This) { return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_IPS_OnInPlaceActivate(IOleInPlaceSite* This) { return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_IPS_OnUIActivate(IOleInPlaceSite* This) { return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_IPS_GetWindowContext(IOleInPlaceSite* This,
    IOleInPlaceFrame** ppFrame, IOleInPlaceUIWindow** ppDoc,
    LPRECT lprcPosRect, LPRECT lprcClipRect, LPOLEINPLACEFRAMEINFO lpFrameInfo) {
    Vb6AxSite* s = (Vb6AxSite*)This;
    *ppFrame = (IOleInPlaceFrame*)&s->lpVtblInPlaceFrame;
    s->ref++; /* AddRef for the out param */
    *ppDoc = NULL;
    if (lprcPosRect) { RECT r={0,0,400,300}; memcpy(lprcPosRect,&r,sizeof(RECT)); }
    if (lprcClipRect)  { RECT r={0,0,400,300}; memcpy(lprcClipRect,&r,sizeof(RECT)); }
    if (lpFrameInfo) { lpFrameInfo->fMDIApp=FALSE; lpFrameInfo->hwndFrame=s->hwndForm; lpFrameInfo->haccel=0; lpFrameInfo->cAccelEntries=0; }
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE axSite_IPS_Scroll(IOleInPlaceSite* This, SIZE s) { return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_IPS_OnUIDeactivate(IOleInPlaceSite* This, BOOL f) { return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_IPS_OnInPlaceDeactivate(IOleInPlaceSite* This) { return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_IPS_DeactivateAndUndo(IOleInPlaceSite* This) { return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_IPS_OnPosRectChange(IOleInPlaceSite* This, LPCRECT lprc) { return S_OK; }

static const IOleInPlaceSiteVtbl g_axSiteInPlaceSiteVtbl = {
    axSite_IPS_QueryInterface, axSite_IPS_AddRef, axSite_IPS_Release,
    axSite_IPS_GetWindow, axSite_IPS_ContextSensitiveHelp,
    axSite_IPS_CanInPlaceActivate, axSite_IPS_OnInPlaceActivate, axSite_IPS_OnUIActivate,
    axSite_IPS_GetWindowContext, axSite_IPS_Scroll,
    axSite_IPS_OnUIDeactivate, axSite_IPS_OnInPlaceDeactivate,
    axSite_IPS_DeactivateAndUndo, axSite_IPS_OnPosRectChange
};

/* --- IOleInPlaceFrame --- */
static HRESULT STDMETHODCALLTYPE axSite_IPF_QueryInterface(IOleInPlaceFrame* This, REFIID riid, void** ppv) {
    return axSite_CS_QueryInterface((IOleClientSite*)This, riid, ppv);
}
static ULONG STDMETHODCALLTYPE axSite_IPF_AddRef(IOleInPlaceFrame* This) {
    return axSite_CS_AddRef((IOleClientSite*)This);
}
static ULONG STDMETHODCALLTYPE axSite_IPF_Release(IOleInPlaceFrame* This) {
    return axSite_CS_Release((IOleClientSite*)This);
}
static HRESULT STDMETHODCALLTYPE axSite_IPF_GetWindow(IOleInPlaceFrame* This, HWND* phwnd) {
    Vb6AxSite* s = (Vb6AxSite*)This; *phwnd = s->hwndForm; return S_OK;
}
static HRESULT STDMETHODCALLTYPE axSite_IPF_ContextSensitiveHelp(IOleInPlaceFrame* This, BOOL f) { return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_IPF_GetBorder(IOleInPlaceFrame* This, LPRECT lprc) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE axSite_IPF_RequestBorderSpace(IOleInPlaceFrame* This, LPCBORDERWIDTHS lpbw) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE axSite_IPF_SetBorderSpace(IOleInPlaceFrame* This, LPCBORDERWIDTHS lpbw) { return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_IPF_SetActiveObject(IOleInPlaceFrame* This, IOleInPlaceActiveObject* pObj, LPCOLESTR pszObj) { return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_IPF_InsertMenus(IOleInPlaceFrame* This, HMENU hShared, LPOLEMENUGROUPWIDTHS mw) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE axSite_IPF_SetMenu(IOleInPlaceFrame* This, HMENU hShared, HOLEMENU hOLEMenu, HWND hwndActiveObj) { return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_IPF_RemoveMenus(IOleInPlaceFrame* This, HMENU hShared) { return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_IPF_SetStatusText(IOleInPlaceFrame* This, LPCOLESTR psz) { return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_IPF_EnableModeless(IOleInPlaceFrame* This, BOOL f) { return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_IPF_TranslateAccelerators(IOleInPlaceFrame* This, LPMSG pMsg, WORD wID) { return S_FALSE; }

static const IOleInPlaceFrameVtbl g_axSiteInPlaceFrameVtbl = {
    axSite_IPF_QueryInterface, axSite_IPF_AddRef, axSite_IPF_Release,
    axSite_IPF_GetWindow, axSite_IPF_ContextSensitiveHelp,
    axSite_IPF_GetBorder, axSite_IPF_RequestBorderSpace, axSite_IPF_SetBorderSpace,
    axSite_IPF_SetActiveObject,
    axSite_IPF_InsertMenus, axSite_IPF_SetMenu, axSite_IPF_RemoveMenus,
    axSite_IPF_SetStatusText, axSite_IPF_EnableModeless, axSite_IPF_TranslateAccelerators
};

void* vb6_Form_ControlsAdd(void* hwnd, const wchar_t* progId, const wchar_t* ctrlName) {
    if (!hwnd || !progId) return NULL;
    (void)ctrlName;

    /* 1. CLSIDFromProgID */
    CLSID clsid;
    HRESULT hr = CLSIDFromProgID(progId, &clsid);
    if (FAILED(hr)) {
        return NULL;
    }

    /* 2. CoCreateInstance */
    IUnknown* pUnk = NULL;
    hr = CoCreateInstance(&clsid, NULL, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
                          &IID_IUnknown, (void**)&pUnk);
    if (FAILED(hr) || !pUnk) {
        return NULL;
    }

    /* 3. Get IDispatch */
    IDispatch* pDisp = NULL;
    hr = pUnk->lpVtbl->QueryInterface(pUnk, &IID_IDispatch, (void**)&pDisp);
    if (FAILED(hr) || !pDisp) {
        pUnk->lpVtbl->Release(pUnk);
        return NULL;
    }

    /* 4. Get IOleObject */
    IOleObject* pOleObj = NULL;
    hr = pDisp->lpVtbl->QueryInterface(pDisp, &IID_IOleObject, (void**)&pOleObj);
    if (FAILED(hr) || !pOleObj) {
        /* Non-ActiveX COM object: just return IDispatch (no embedding) */
        pUnk->lpVtbl->Release(pUnk);
        return (void*)pDisp;
    }

    /* 5. Create simple ActiveX site */
    Vb6AxSite* site = (Vb6AxSite*)calloc(1, sizeof(Vb6AxSite));
    if (!site) { pOleObj->lpVtbl->Release(pOleObj); pUnk->lpVtbl->Release(pUnk); return (void*)pDisp; }
    site->lpVtblClientSite = &g_axSiteClientSiteVtbl;
    site->lpVtblInPlaceSite = &g_axSiteInPlaceSiteVtbl;
    site->lpVtblInPlaceFrame = &g_axSiteInPlaceFrameVtbl;
    site->ref = 1;
    site->hwndForm = (HWND)hwnd;
    site->pOleObj = pOleObj;

    /* 6. SetClientSite */
    pOleObj->lpVtbl->SetClientSite(pOleObj, (IOleClientSite*)&site->lpVtblClientSite);

    /* 7. Set initial extent (default 400x300 pixels, ~267x200 HIMETRIC) */
    SIZEL sz = { 26700, 20000 };  /* HIMETRIC units */
    pOleObj->lpVtbl->SetExtent(pOleObj, DVASPECT_CONTENT, &sz);

    /* 8. Skip DoVerb for now — WMP crashes on INPLACEACTIVATE.
       Just return IDispatch for COM late-binding property access. */
    /* TODO: proper ActiveX hosting with IOleInPlaceSite frame etc. */

    /* 9. Set control position */
    IOleInPlaceObject* pIPO = NULL;
    hr = pOleObj->lpVtbl->QueryInterface(pOleObj, &IID_IOleInPlaceObject, (void**)&pIPO);
    if (SUCCEEDED(hr) && pIPO) {
        RECT rc = { 0, 0, 400, 300 };
        pIPO->lpVtbl->SetObjectRects(pIPO, &rc, &rc);
        pIPO->lpVtbl->Release(pIPO);
    }

    pUnk->lpVtbl->Release(pUnk);
    return (void*)pDisp;
}
