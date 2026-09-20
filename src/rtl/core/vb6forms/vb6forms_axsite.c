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
#include <stddef.h>   /* offsetof */
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
    RECT rcCtrl;          /* Fix 143: 控件在窗体客户区的矩形 (GetWindowContext 用) */
    /* Fix 143d: 无窗口 (windowless) 控件宿主必需的三件套.
     * VB6 UserControl 激活时会向 site 索要这三个接口 (实测 NewTab01.ocx):
     *   IOleInPlaceSiteWindowless {40A050A0-3C31-101B-A82E-08002B2B2337}
     *   IOleControlSite          {B196B289-BAB4-101A-B69C-00AA00341D07}
     *   IDispatch (ambient 属性)
     * 缺任一个时 DoVerb 仍返回 S_OK, 但控件不进入激活态 → 窗体上什么都不画. */
    const IOleControlSiteVtbl* lpVtblControlSite;
    const IOleInPlaceSiteWindowlessVtbl* lpVtblInPlaceSiteWindowless;
    const IDispatchVtbl* lpVtblDispatch;
    int inPlaceActive;
    int captured;         /* IOleInPlaceSiteWindowless::SetCapture 状态 */
    int focused;          /* IOleInPlaceSiteWindowless::SetFocus 状态 */
    void* pInPlaceObj;    /* IOleInPlaceObjectWindowless* — 消息转发用 */
    void* pViewObj;       /* IViewObject* — 宿主 WM_PAINT 里绘制控件用 */
    wchar_t ctrlName[128];/* Fix 148: 控件实例名 (容器对象模型用) */
    void* pExtender;      /* Fix 148: Extender 缓存 (IOleControlSite::GetExtendedControl) */
};

/* Fix 143d: 从接口指针反推宿主对象.
 * COM 多重接口共用同一个对象, 但 QI 返回的是**对象内某个 vtable 字段的地址**,
 * 所以每个方法里必须减去该字段的 offset 才能拿到对象首地址
 * (原实现直接 (Vb6AxSite*)This 只在首字段 IOleClientSite 上恰好正确,
 *  其余接口 (IOleInPlaceSite/Frame 等) 全都偏移错位). */
#define SITE_OF(thisptr, field) ((Vb6AxSite*)((char*)(thisptr) - offsetof(Vb6AxSite, field)))

/* --- IOleClientSite --- */

/* Fix 143d 诊断: C3_OCX_TRACE=1 时打印容器 QI 收到的 IID + 应答结果.
 * 用于判断控件到底在向宿主索要哪些接口 (Windowless/ControlSite/Ambient 等). */
static void ocxTraceQI(const char* iface, REFIID riid, HRESULT hr) {
    if (GetEnvironmentVariableW(L"C3_OCX_TRACE", NULL, 0) <= 0) return;
    wchar_t guid[64] = {0};
    StringFromGUID2(riid, guid, 64);
    fprintf(stderr, "[C3_OCX]   QI %s %ls -> 0x%08lX\n", iface, guid, (unsigned long)hr);
}

static HRESULT STDMETHODCALLTYPE axSite_CS_QueryInterface(IOleClientSite* This, REFIID riid, void** ppv) {
    Vb6AxSite* s = SITE_OF(This, lpVtblClientSite);
    HRESULT r = E_NOINTERFACE;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IOleClientSite)) {
        *ppv = &s->lpVtblClientSite; s->ref++; r = S_OK;
    } else if (IsEqualIID(riid, &IID_IOleInPlaceSite)) {
        *ppv = &s->lpVtblInPlaceSite; s->ref++; r = S_OK;
    } else if (IsEqualIID(riid, &IID_IOleInPlaceFrame)) {
        *ppv = &s->lpVtblInPlaceFrame; s->ref++; r = S_OK;
    } else if (IsEqualIID(riid, &IID_IDispatch)) {
        /* Fix 143d: ambient 属性通道 (控件据此取 BackColor/Font/UserMode) */
        *ppv = &s->lpVtblDispatch; s->ref++; r = S_OK;
    } else if (IsEqualIID(riid, &IID_IOleControlSite)) {
        *ppv = &s->lpVtblControlSite; s->ref++; r = S_OK;
    } else if (IsEqualIID(riid, &IID_IOleInPlaceSiteWindowless)) {
        /* Fix 143d: 无窗口控件激活必需 */
        *ppv = &s->lpVtblInPlaceSiteWindowless; s->ref++; r = S_OK;
    } else {
        *ppv = NULL;
    }
    ocxTraceQI("site", riid, r);
    return r;
}
static ULONG STDMETHODCALLTYPE axSite_CS_AddRef(IOleClientSite* This) {
    Vb6AxSite* s = SITE_OF(This, lpVtblClientSite); return InterlockedIncrement(&s->ref);
}
static ULONG STDMETHODCALLTYPE axSite_CS_Release(IOleClientSite* This) {
    Vb6AxSite* s = SITE_OF(This, lpVtblClientSite); LONG r = InterlockedDecrement(&s->ref);
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
    return axSite_CS_AddRef((IOleClientSite*)SITE_OF(This, lpVtblInPlaceSite));
}
static ULONG STDMETHODCALLTYPE axSite_IPS_Release(IOleInPlaceSite* This) {
    return axSite_CS_Release((IOleClientSite*)SITE_OF(This, lpVtblInPlaceSite));
}
static HRESULT STDMETHODCALLTYPE axSite_IPS_GetWindow(IOleInPlaceSite* This, HWND* phwnd) {
    Vb6AxSite* s = SITE_OF(This, lpVtblInPlaceSite); *phwnd = s->hwndForm; return S_OK;
}
static HRESULT STDMETHODCALLTYPE axSite_IPS_ContextSensitiveHelp(IOleInPlaceSite* This, BOOL f) { return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_IPS_CanInPlaceActivate(IOleInPlaceSite* This) { return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_IPS_OnInPlaceActivate(IOleInPlaceSite* This) { return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_IPS_OnUIActivate(IOleInPlaceSite* This) { return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_IPS_GetWindowContext(IOleInPlaceSite* This,
    IOleInPlaceFrame** ppFrame, IOleInPlaceUIWindow** ppDoc,
    LPRECT lprcPosRect, LPRECT lprcClipRect, LPOLEINPLACEFRAMEINFO lpFrameInfo) {
    Vb6AxSite* s = SITE_OF(This, lpVtblInPlaceSite);
    *ppFrame = (IOleInPlaceFrame*)&s->lpVtblInPlaceFrame;
    s->ref++; /* AddRef for the out param */
    *ppDoc = NULL;
    /* Fix 143: 用控件真实矩形, 不再硬编码 400x300 */
    RECT rFallback; rFallback.left=0; rFallback.top=0; rFallback.right=400; rFallback.bottom=300;
    const RECT* rc = (s->rcCtrl.right > s->rcCtrl.left) ? &s->rcCtrl : &rFallback;
    if (lprcPosRect)  memcpy(lprcPosRect, rc, sizeof(RECT));
    if (lprcClipRect) memcpy(lprcClipRect, rc, sizeof(RECT));
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
    return axSite_CS_AddRef((IOleClientSite*)SITE_OF(This, lpVtblInPlaceFrame));
}
static ULONG STDMETHODCALLTYPE axSite_IPF_Release(IOleInPlaceFrame* This) {
    return axSite_CS_Release((IOleClientSite*)SITE_OF(This, lpVtblInPlaceFrame));
}
static HRESULT STDMETHODCALLTYPE axSite_IPF_GetWindow(IOleInPlaceFrame* This, HWND* phwnd) {
    Vb6AxSite* s = SITE_OF(This, lpVtblInPlaceFrame); *phwnd = s->hwndForm; return S_OK;
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

/* ===== Fix 143: 第三方 OCX 控件真宿主 ===== */

/* --- 简易 IPropertyBag: 把生成代码提供的设计期属性灌给控件 --- */

typedef struct Vb6PropBag {
    const IPropertyBagVtbl* lpVtbl;
    LONG ref;
    const Vb6OcxProp* props;
    int count;
} Vb6PropBag;

static HRESULT STDMETHODCALLTYPE pb_QueryInterface(IPropertyBag* This, REFIID riid, void** ppv) {
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IPropertyBag)) {
        *ppv = This; This->lpVtbl->AddRef(This); return S_OK;
    }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE pb_AddRef(IPropertyBag* This) {
    Vb6PropBag* b = (Vb6PropBag*)This; return InterlockedIncrement(&b->ref);
}
static ULONG STDMETHODCALLTYPE pb_Release(IPropertyBag* This) {
    Vb6PropBag* b = (Vb6PropBag*)This;
    LONG r = InterlockedDecrement(&b->ref);
    if (r == 0) free(b);
    return (ULONG)r;
}
static HRESULT STDMETHODCALLTYPE pb_Read(IPropertyBag* This, LPCOLESTR pszPropName, VARIANT* pVar, IErrorLog* pErrLog) {
    (void)pErrLog;
    Vb6PropBag* b = (Vb6PropBag*)This;
    if (!pszPropName || !pVar) return E_POINTER;
    int dbg = (GetEnvironmentVariableW(L"C3_OCX_TRACE", NULL, 0) > 0);
    if (dbg) fprintf(stderr, "[C3_BAG] Read '%ls' wantVT=%d count=%d\n",
                     pszPropName, (int)pVar->vt, b->count);
    for (int i = 0; i < b->count; i++) {
        const Vb6OcxProp* p = &b->props[i];
        if (wcscmp(pszPropName, p->name) != 0) continue;
        if (dbg) fprintf(stderr, "[C3_BAG]   hit '%ls' ourVT=%d\n", p->name, (int)p->vt);
        /* 请求的类型与我们的存储类型可能不同 (控件按文档类型要 VT_EMPTY=任意) */
        VARTYPE want = pVar->vt;
        /* 关键: 这里**不能** VariantClear(pVar) — pVar 是控件(调用者)提供的
         * VARIANT, 其内容未定义; 若它恰好带着垃圾 vt (如 VT_BSTR), VariantClear
         * 会按该 vt 去 SysFreeString/Release 一个野指针 → 堆损坏, 异常从 WndProc
         * 逃逸, 进程被杀 (0xC000041D). 实测 NewTab01.ocx 的 TDIMode 属性读取必崩.
         * 正确做法: 用 VariantInit 清零 (不释放任何东西) 再填值. */
        VariantInit(pVar);

        /* Fix 149: 设计期字体 (BeginProperty Font / IconFont(n)).
         * 返回 oleaut32 的真 IFont (与 VB6 的 StdFont 是同一个实现), VB6 控件
         * 拿到后可直接 .Name/.Size 读写. 返回 Nothing 会让控件解引用空指针. */
        if (p->vt == VT_DISPATCH || (p->fontName && want == VT_DISPATCH)) {
            static const GUID kIID_IFont =
                {0xBEF6E003, 0xA874, 0x101A, {0x8B, 0xBA, 0x00, 0xAA, 0x00, 0x30, 0x0C, 0xAB}};
            static const GUID kIID_IFontDisp =
                {0xBEF6E002, 0xA874, 0x101A, {0x8B, 0xBA, 0x00, 0xAA, 0x00, 0x30, 0x0C, 0xAB}};
            FONTDESC fd;
            IFont* pFont = NULL;
            memset(&fd, 0, sizeof(fd));
            fd.cbSizeofstruct = sizeof(FONTDESC);
            fd.lpstrName = (LPOLESTR)(p->fontName ? p->fontName : L"Tahoma");
            fd.cySize.int64 = (LONGLONG)((p->fontSize > 0 ? p->fontSize : 9.0) * 10000.0);
            fd.sWeight = (short)(p->fontWeight > 0 ? p->fontWeight : 400);
            fd.sCharset = (short)p->fontCharset;
            fd.fItalic = (p->fontFlags & 1) ? TRUE : FALSE;
            fd.fUnderline = (p->fontFlags & 2) ? TRUE : FALSE;
            fd.fStrikethrough = (p->fontFlags & 4) ? TRUE : FALSE;
            HRESULT hrF = OleCreateFontIndirect(&fd, &kIID_IFont, (void**)&pFont);
            if (FAILED(hrF) || !pFont)
                hrF = OleCreateFontIndirect(&fd, &kIID_IFontDisp, (void**)&pFont);
            pVar->vt = VT_DISPATCH;
            pVar->pdispVal = (IDispatch*)pFont;
            if (dbg) fprintf(stderr, "[C3_BAG]   -> IFont '%ls' %.1fpt hr=0x%08lX obj=%p\n",
                             fd.lpstrName, (double)fd.cySize.int64 / 10000.0,
                             (unsigned long)hrF, (void*)pFont);
            return S_OK;
        }

        switch (want) {
        case VT_I4:
        case VT_I2:
        case VT_UI2:
        case VT_UI4:
        case VT_INT:
        case VT_UINT:
            pVar->vt = VT_I4;
            pVar->lVal = (p->vt == VT_R4) ? (long)p->fVal : p->iVal;
            return S_OK;
        case VT_R4:
        case VT_R8:
            pVar->vt = VT_R4;
            pVar->fltVal = (p->vt == VT_R4) ? p->fVal : (float)p->iVal;
            return S_OK;
        case VT_BOOL:
            pVar->vt = VT_BOOL;
            pVar->boolVal = p->iVal ? VARIANT_TRUE : VARIANT_FALSE;
            return S_OK;
        case VT_BSTR:
            pVar->vt = VT_BSTR;
            pVar->bstrVal = (p->vt == VT_BSTR) ? SysAllocString(p->sVal)
                                               : SysAllocStringLen(NULL, 12); /* 空串 */
            if (p->vt != VT_BSTR && pVar->bstrVal) pVar->bstrVal[0] = 0;
            return S_OK;
        default:
            /* 控件要 VARIANT/其它: 按我们自己存的类型给 */
            break;
        }
        switch (p->vt) {
        case VT_BSTR:
            pVar->vt = VT_BSTR;
            pVar->bstrVal = SysAllocString(p->sVal);
            return pVar->bstrVal ? S_OK : E_OUTOFMEMORY;
        case VT_R4:
            pVar->vt = VT_R4; pVar->fltVal = p->fVal; return S_OK;
        case VT_BOOL:
            pVar->vt = VT_BOOL; pVar->boolVal = p->iVal ? VARIANT_TRUE : VARIANT_FALSE; return S_OK;
        default:
            pVar->vt = VT_I4; pVar->lVal = p->iVal; return S_OK;
        }
    }
    return E_INVALIDARG;  /* 属性不存在 — 控件用默认值 (VB6 语义) */
}
static HRESULT STDMETHODCALLTYPE pb_Write(IPropertyBag* This, LPCOLESTR pszPropName, VARIANT* pVar) {
    (void)This; (void)pszPropName; (void)pVar;
    return S_OK;  /* 只读包 */
}

static const IPropertyBagVtbl g_pbVtbl = {
    pb_QueryInterface, pb_AddRef, pb_Release, pb_Read, pb_Write
};

/* --- 按 CLSID 字符串实例化: 优先免注册 LoadLibrary(ocxPath) --- */

static HRESULT ocxCreateFromPath(const wchar_t* ocxPath, REFCLSID rclsid, void** ppUnk) {
    *ppUnk = NULL;
    if (!ocxPath || !*ocxPath) return ((HRESULT)0x800401F1L);
    HMODULE hMod = LoadLibraryW(ocxPath);
    if (!hMod) return ((HRESULT)0x800401F1L);
    typedef HRESULT (__stdcall *PFN_DllGetClassObject)(REFCLSID, REFIID, void**);
    PFN_DllGetClassObject pGet = (PFN_DllGetClassObject)GetProcAddress(hMod, "DllGetClassObject");
    if (!pGet) { FreeLibrary(hMod); return ((HRESULT)0x800401F1L); }
    IClassFactory* cf = NULL;
    HRESULT hr = pGet(rclsid, &IID_IClassFactory, (void**)&cf);
    if (FAILED(hr) || !cf) { /* FreeLibrary 不做: DllGetClassObject 成功后对象可能回引 DLL */ return hr; }
    hr = cf->lpVtbl->CreateInstance(cf, NULL, &IID_IUnknown, ppUnk);
    cf->lpVtbl->Release(cf);
    return hr;
}

/* 多路径实例化 (Fix 143):
 *   1) <exe目录>\<ocx文件名>  — 便携分发 (ocx 随 exe 走, 免注册)
 *   2) 编译期 bake 的 ocx 绝对路径 — 开发机场景
 *   3) CoCreateInstance — 已注册场景 (INPROC + LOCAL_SERVER 兼容 surrogate)
 *   4) 裸文件名 LoadLibrary — 走系统 DLL 搜索路径
 * 顺序保证: 同一 CLSID 已注册时也优先用随 exe 的文件, 与 VB6
 * "注册表指向哪用哪"不同 — 便携部署下注册表常指向不存在/旧版路径.
 * 用环境变量 C3_OCX_PREFER_REG=1 可改为注册表优先. */
static HRESULT ocxCreateAny(const wchar_t* ocxPath, REFCLSID rclsid, void** ppUnk) {
    *ppUnk = NULL;
    HRESULT hr;

    /* exe 所在目录 (结尾带反斜杠) — 用 GetModuleFileName 取, 与 CWD 无关 */
    wchar_t exeDir[MAX_PATH];
    exeDir[0] = 0;
    DWORD n = GetModuleFileNameW(NULL, exeDir, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        wchar_t* es = wcsrchr(exeDir, L'\\');
        if (es) es[1] = 0;   /* 保留结尾 '\' */
        else exeDir[0] = 0;
    } else {
        exeDir[0] = 0;
    }

    /* 从 bake 的 ocxPath 提取纯文件名 */
    const wchar_t* fname = ocxPath ? ocxPath : L"";
    if (fname && *fname) {
        const wchar_t* s = wcsrchr(fname, L'\\');
        if (!s) s = wcsrchr(fname, L'/');
        if (s) fname = s + 1;
    }

    /* 候选 1: <exe目录>\<文件名>  (OCX 与 exe 同目录, 兼容旧 bake) */
    wchar_t exeSide[MAX_PATH * 2];
    exeSide[0] = 0;
    if (exeDir[0] && fname[0] && wcslen(exeDir) + wcslen(fname) + 1 < MAX_PATH * 2) {
        wcscpy(exeSide, exeDir);
        wcscat(exeSide, fname);
    }

    /* 候选 2: <exe目录>\<ocxPath相对部分>  (VBP 写 "bin\X.ocx" → exe目录\bin\X.ocx, 免注册相对 exe 加载, 不依赖 CWD) */
    wchar_t exeRel[MAX_PATH * 2];
    exeRel[0] = 0;
    if (exeDir[0] && ocxPath && *ocxPath) {
        /* 仅对相对路径拼接 exe 目录; 绝对/UNC/盘符根路径保持原样, 不重复拼 */
        int isAbs = (wcschr(ocxPath, L':') != NULL) || (ocxPath[0] == L'\\');
        if (!isAbs && wcslen(exeDir) + wcslen(ocxPath) + 1 < MAX_PATH * 2) {
            wcscpy(exeRel, exeDir);
            wcscat(exeRel, ocxPath);
        }
    }

    static int preferReg = -1;
    if (preferReg < 0) {
        preferReg = (GetEnvironmentVariableW(L"C3_OCX_PREFER_REG", NULL, 0) > 0) ? 1 : 0;
    }

    if (!preferReg) {
        /* 便携优先 (默认): 先试"相对 exe"解析 (不依赖 CWD), 再试同目录, 再试 CWD, 最后注册表 */
        if (exeRel[0]) {
            hr = ocxCreateFromPath(exeRel, rclsid, ppUnk);
            if (SUCCEEDED(hr) && *ppUnk) return hr;
        }
        if (exeSide[0]) {
            hr = ocxCreateFromPath(exeSide, rclsid, ppUnk);
            if (SUCCEEDED(hr) && *ppUnk) return hr;
        }
        hr = ocxCreateFromPath(ocxPath, rclsid, ppUnk);
        if (SUCCEEDED(hr) && *ppUnk) return hr;
        hr = CoCreateInstance(rclsid, NULL, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
                              &IID_IUnknown, ppUnk);
        if (SUCCEEDED(hr) && *ppUnk) return hr;
        /* 4) 裸文件名 (系统搜索路径) */
        if (ocxPath) {
            const wchar_t* slash = wcsrchr(ocxPath, L'\\');
            if (!slash) slash = wcsrchr(ocxPath, L'/');
            if (slash) return ocxCreateFromPath(slash + 1, rclsid, ppUnk);
        }
        return hr;
    } else {
        /* 注册表优先 (C3_OCX_PREFER_REG=1) */
        hr = CoCreateInstance(rclsid, NULL, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
                              &IID_IUnknown, ppUnk);
        if (SUCCEEDED(hr) && *ppUnk) return hr;
        if (exeRel[0]) {
            hr = ocxCreateFromPath(exeRel, rclsid, ppUnk);
            if (SUCCEEDED(hr) && *ppUnk) return hr;
        }
        if (exeSide[0]) {
            hr = ocxCreateFromPath(exeSide, rclsid, ppUnk);
            if (SUCCEEDED(hr) && *ppUnk) return hr;
        }
        return ocxCreateFromPath(ocxPath, rclsid, ppUnk);
    }
}

/* twips → HIMETRIC (VB6: himetric = twips * 2540 / 1440) */
static long twipsToHimetric(long twips) { return (long)((__int64)twips * 2540 / 1440); }

/* ===== Fix 143d: 已创建的 OCX 宿主登记表 (windowless 控件消息转发用) ===== */
#define VB6_MAX_AXSITES 64
static Vb6AxSite* g_axSites[VB6_MAX_AXSITES];
static int g_axSiteCount = 0;

static void axSiteRegister(Vb6AxSite* s) {
    if (g_axSiteCount < VB6_MAX_AXSITES) g_axSites[g_axSiteCount++] = s;
}

/* 把窗体消息转发给该窗体上所有无窗口 OCX 控件.
 * 返回 1 = 至少一个控件处理了该消息 (调用方应把 *plResult 作为结果).
 * windowless 控件不接收窗口消息: 绘制 (WM_PAINT)、鼠标、键盘全靠宿主转发,
 * 不转发则控件永不刷新 — 表现为窗体上一片空白. */
int32_t vb6_OcxHost_ForwardMessage(void* hwndForm, unsigned int msg, uintptr_t wp,
                                   intptr_t lp, intptr_t* plResult) {
    int handled = 0;
    for (int i = 0; i < g_axSiteCount; i++) {
        Vb6AxSite* s = g_axSites[i];
        if (!s || s->hwndForm != (HWND)hwndForm || !s->pInPlaceObj) continue;
        IOleInPlaceObjectWindowless* p = (IOleInPlaceObjectWindowless*)s->pInPlaceObj;
        LRESULT lr = 0;
        HRESULT hr = p->lpVtbl->OnWindowMessage(p, (UINT)msg, (WPARAM)wp, (LPARAM)lp, &lr);
        if (hr == S_OK) {
            if (plResult) *plResult = (intptr_t)lr;
            handled = 1;
        }
    }
    return handled;
}

/* 主动让该窗体上的无窗口控件重绘 (窗体首次呈现 / 尺寸变化后) */
void vb6_OcxHost_InvalidateAll(void* hwndForm) {
    for (int i = 0; i < g_axSiteCount; i++) {
        Vb6AxSite* s = g_axSites[i];
        if (!s || s->hwndForm != (HWND)hwndForm) continue;
        InvalidateRect(s->hwndForm, &s->rcCtrl, TRUE);
    }
}

/* Fix 143d: 把该窗体上所有 OCX 控件画到给定 DC (在 WM_PAINT 的
 * BeginPaint/EndPaint 之间调用). 用 IViewObject::Draw — 这是窗口化与
 * 无窗口控件都支持的通用绘制入口, 比转发 WM_PAINT 更可靠.
 * 注: windowless 控件的绘制矩形必须换算成 DC 坐标 (DC 原点=窗体客户区左上),
 * 我们的 rcCtrl 本身就是客户区坐标, 直接用. */
void vb6_OcxHost_PaintAll(void* hwndForm, void* hdc) {
    HDC dc = (HDC)hdc;
    if (!dc) return;
    for (int i = 0; i < g_axSiteCount; i++) {
        Vb6AxSite* s = g_axSites[i];
        if (!s || s->hwndForm != (HWND)hwndForm || !s->pViewObj) continue;
        /* 只画真正的无窗口控件! 有自己窗口的控件 (VB6 UserControl 绝大多数是
         * windowed, 窗口类 ThunderRT6UserControlDC) 会自己绘制; 宿主再对它调
         * IViewObject::Draw 会与控件自绘冲突, 实测点击按钮触发重绘时
         * 异常从 WndProc 逃逸 → 进程被 user32 杀掉 (0xC000041D). */
        if (!s->pInPlaceObj) continue;
        IViewObject* pView = (IViewObject*)s->pViewObj;
        RECTL rc;
        rc.left   = s->rcCtrl.left;
        rc.top    = s->rcCtrl.top;
        rc.right  = s->rcCtrl.right;
        rc.bottom = s->rcCtrl.bottom;
        int saved = SaveDC(dc);
        IntersectClipRect(dc, (int)rc.left, (int)rc.top, (int)rc.right, (int)rc.bottom);
        pView->lpVtbl->Draw(pView, DVASPECT_CONTENT, -1, NULL, NULL, NULL, dc, &rc, NULL, NULL, 0);
        RestoreDC(dc, saved);
    }
}

/* ===== Fix 143d: 无窗口控件宿主的三件套接口 =====
 * 前置声明 (实现中互相引用) */
static const IDispatchVtbl g_axSiteDispatchVtbl;
static const IOleControlSiteVtbl g_axSiteControlSiteVtbl;
static const IOleInPlaceSiteWindowlessVtbl g_axSiteInPlaceSiteWindowlessVtbl;

/* --- IDispatch: ambient 属性 (控件通过它取容器的 BackColor/Font/UserMode 等) --- */
static HRESULT STDMETHODCALLTYPE axSite_D_QueryInterface(IDispatch* This, REFIID riid, void** ppv) {
    return axSite_CS_QueryInterface((IOleClientSite*)This, riid, ppv);
}
static ULONG STDMETHODCALLTYPE axSite_D_AddRef(IDispatch* This) { return axSite_CS_AddRef((IOleClientSite*)SITE_OF(This, lpVtblDispatch)); }
static ULONG STDMETHODCALLTYPE axSite_D_Release(IDispatch* This) { return axSite_CS_Release((IOleClientSite*)SITE_OF(This, lpVtblDispatch)); }
static HRESULT STDMETHODCALLTYPE axSite_D_GetTypeInfoCount(IDispatch* This, UINT* p) { (void)This; *p = 0; return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_D_GetTypeInfo(IDispatch* This, UINT i, LCID l, ITypeInfo** p) {
    (void)This; (void)i; (void)l; *p = NULL; return E_NOTIMPL;
}
static HRESULT STDMETHODCALLTYPE axSite_D_GetIDsOfNames(IDispatch* This, REFIID riid, LPOLESTR* names,
                                                        UINT cNames, LCID lcid, DISPID* dispids) {
    (void)This; (void)riid; (void)lcid;
    if (!names || !dispids) return E_POINTER;
    for (UINT i = 0; i < cNames; i++) {
        const wchar_t* n = names[i];
        DISPID d = DISPID_UNKNOWN;
        if (_wcsicmp(n, L"BackColor") == 0) d = DISPID_AMBIENT_BACKCOLOR;
        else if (_wcsicmp(n, L"ForeColor") == 0) d = DISPID_AMBIENT_FORECOLOR;
        else if (_wcsicmp(n, L"Font") == 0) d = DISPID_AMBIENT_FONT;
        else if (_wcsicmp(n, L"UserMode") == 0) d = DISPID_AMBIENT_USERMODE;
        else if (_wcsicmp(n, L"Appearance") == 0) d = DISPID_AMBIENT_APPEARANCE;
        else if (_wcsicmp(n, L"ScaleUnits") == 0) d = DISPID_AMBIENT_SCALEUNITS;
        else if (_wcsicmp(n, L"LocaleID") == 0) d = DISPID_AMBIENT_LOCALEID;
        else if (_wcsicmp(n, L"DisplayName") == 0) d = DISPID_AMBIENT_DISPLAYNAME;
        else if (_wcsicmp(n, L"TextAlign") == 0) d = DISPID_AMBIENT_TEXTALIGN;
        else if (_wcsicmp(n, L"DisplayAsDefault") == 0) d = DISPID_AMBIENT_DISPLAYASDEFAULT;
        else if (_wcsicmp(n, L"UIDead") == 0) d = DISPID_AMBIENT_UIDEAD;
        else if (_wcsicmp(n, L"SupportsMnemonics") == 0) d = DISPID_AMBIENT_SUPPORTSMNEMONICS;
        else if (_wcsicmp(n, L"ShowGrabHandles") == 0) d = DISPID_AMBIENT_SHOWGRABHANDLES;
        else if (_wcsicmp(n, L"ShowHatching") == 0) d = DISPID_AMBIENT_SHOWHATCHING;
        else if (_wcsicmp(n, L"MessageReflect") == 0) d = DISPID_AMBIENT_MESSAGEREFLECT;
        else if (_wcsicmp(n, L"AutoClip") == 0) d = -713;
        else if (_wcsicmp(n, L"Palette") == 0) d = DISPID_AMBIENT_PALETTE;
        dispids[i] = d;
    }
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE axSite_D_Invoke(IDispatch* This, DISPID dispid, REFIID riid, LCID lcid,
                                                 WORD flags, DISPPARAMS* dp, VARIANT* result,
                                                 EXCEPINFO* ei, UINT* ae) {
    (void)This; (void)riid; (void)lcid; (void)flags; (void)dp; (void)ei; (void)ae;
    if (!result) return S_OK;
    VariantInit(result);
    switch (dispid) {
        case DISPID_AMBIENT_BACKCOLOR:          result->vt = VT_I4;  result->lVal = 0x8000000F; return S_OK;
        case DISPID_AMBIENT_FORECOLOR:          result->vt = VT_I4;  result->lVal = 0x80000012; return S_OK;
        case DISPID_AMBIENT_APPEARANCE:         result->vt = VT_I2;  result->iVal = 1;          return S_OK;
        case DISPID_AMBIENT_USERMODE:           result->vt = VT_BOOL; result->boolVal = VARIANT_TRUE; return S_OK;
        case DISPID_AMBIENT_LOCALEID:           result->vt = VT_I4;  result->lVal = 0x409;      return S_OK;
        case DISPID_AMBIENT_TEXTALIGN:          result->vt = VT_I2;  result->iVal = 0;          return S_OK;
        case DISPID_AMBIENT_DISPLAYASDEFAULT:   result->vt = VT_BOOL; result->boolVal = VARIANT_FALSE; return S_OK;
        case DISPID_AMBIENT_UIDEAD:             result->vt = VT_BOOL; result->boolVal = VARIANT_FALSE; return S_OK;
        case DISPID_AMBIENT_SUPPORTSMNEMONICS:  result->vt = VT_BOOL; result->boolVal = VARIANT_TRUE;  return S_OK;
        case DISPID_AMBIENT_SHOWGRABHANDLES:    result->vt = VT_BOOL; result->boolVal = VARIANT_FALSE; return S_OK;
        case DISPID_AMBIENT_SHOWHATCHING:       result->vt = VT_BOOL; result->boolVal = VARIANT_FALSE; return S_OK;
        case DISPID_AMBIENT_MESSAGEREFLECT:     result->vt = VT_BOOL; result->boolVal = VARIANT_FALSE; return S_OK;
        case DISPID_AMBIENT_SCALEUNITS:         result->vt = VT_BSTR; result->bstrVal = SysAllocString(L"twip"); return S_OK;
        case DISPID_AMBIENT_DISPLAYNAME:        result->vt = VT_BSTR; result->bstrVal = SysAllocString(L"");     return S_OK;
        default:                                result->vt = VT_EMPTY; return DISP_E_MEMBERNOTFOUND;
    }
}

static const IDispatchVtbl g_axSiteDispatchVtbl = {
    axSite_D_QueryInterface, axSite_D_AddRef, axSite_D_Release,
    axSite_D_GetTypeInfoCount, axSite_D_GetTypeInfo,
    axSite_D_GetIDsOfNames, axSite_D_Invoke
};

/* --- IOleControlSite --- */
static HRESULT STDMETHODCALLTYPE axSite_OCS_QueryInterface(IOleControlSite* This, REFIID riid, void** ppv) {
    return axSite_CS_QueryInterface((IOleClientSite*)This, riid, ppv);
}
static ULONG STDMETHODCALLTYPE axSite_OCS_AddRef(IOleControlSite* This) { return axSite_CS_AddRef((IOleClientSite*)SITE_OF(This, lpVtblControlSite)); }
static ULONG STDMETHODCALLTYPE axSite_OCS_Release(IOleControlSite* This) { return axSite_CS_Release((IOleClientSite*)SITE_OF(This, lpVtblControlSite)); }
static HRESULT STDMETHODCALLTYPE axSite_OCS_OnControlInfoChanged(IOleControlSite* This) { (void)This; return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_OCS_LockInPlaceActive(IOleControlSite* This, BOOL f) { (void)This; (void)f; return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_OCS_GetExtendedControl(IOleControlSite* This, IDispatch** ppDisp) {
    Vb6AxSite* s = SITE_OF(This, lpVtblControlSite);
    if (!ppDisp) return E_POINTER;
    /* Fix 148: VB6 控件用 UserControl.Extender 访问容器
     * (Extender.Container / Extender.TabIndex). 返回 E_NOTIMPL 时 VB6 运行时
     * 在其内部解引用空指针 — 实测 NewTab01.ocx 读 TDIMode 属性即崩
     * (0xC000041D 从 WndProc 逃逸). 这里给出轻量 Extender. */
    if (GetEnvironmentVariableW(L"C3_OCX_TRACE", NULL, 0) > 0)
        fprintf(stderr, "[C3_OCX]   GetExtendedControl called (ctrl='%ls')\n", s->ctrlName);
    if (!s->pExtender) {
        s->pExtender = vb6_AxContainer_CreateExtender(s->hwndForm, s->ctrlName);
    }
    if (!s->pExtender) { *ppDisp = NULL; return E_OUTOFMEMORY; }
    *ppDisp = (IDispatch*)s->pExtender;
    ((IDispatch*)s->pExtender)->lpVtbl->AddRef((IDispatch*)s->pExtender);
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE axSite_OCS_TransformCoords(IOleControlSite* This, POINTL* ptlHimetric,
                                                            POINTF* ptfContainer, DWORD flags) {
    (void)This;
    /* VB6 的 XFORMCOORDS_* 语义: HIMETRIC ↔ 容器像素 (96dpi: 1 himetric = 1/2646 px) */
    const double k = 96.0 / 2540.0;   /* himetric → pixel */
    if (!ptlHimetric || !ptfContainer) return E_POINTER;
    if (flags & XFORMCOORDS_HIMETRICTOCONTAINER) {
        ptfContainer->x = (float)(ptlHimetric->x * k);
        ptfContainer->y = (float)(ptlHimetric->y * k);
    } else {
        ptlHimetric->x = (LONG)(ptfContainer->x / k);
        ptlHimetric->y = (LONG)(ptfContainer->y / k);
    }
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE axSite_OCS_TranslateAccelerator(IOleControlSite* This, MSG* pMsg, DWORD grfModifiers) {
    (void)This; (void)pMsg; (void)grfModifiers;
    return S_FALSE;   /* 不处理, 交给容器默认流程 */
}
static HRESULT STDMETHODCALLTYPE axSite_OCS_OnFocus(IOleControlSite* This, BOOL fGotFocus) { (void)This; (void)fGotFocus; return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_OCS_ShowPropertyFrame(IOleControlSite* This) { (void)This; return E_NOTIMPL; }

static const IOleControlSiteVtbl g_axSiteControlSiteVtbl = {
    axSite_OCS_QueryInterface, axSite_OCS_AddRef, axSite_OCS_Release,
    axSite_OCS_OnControlInfoChanged, axSite_OCS_LockInPlaceActive,
    axSite_OCS_GetExtendedControl, axSite_OCS_TransformCoords,
    axSite_OCS_TranslateAccelerator, axSite_OCS_OnFocus, axSite_OCS_ShowPropertyFrame
};

/* --- IOleInPlaceSiteWindowless (继承 IOleInPlaceSite 的 10 个方法 + 11 个扩展) --- */
static HRESULT STDMETHODCALLTYPE axSite_IPSW_QueryInterface(IOleInPlaceSiteWindowless* This, REFIID riid, void** ppv) {
    return axSite_CS_QueryInterface((IOleClientSite*)This, riid, ppv);
}
static ULONG STDMETHODCALLTYPE axSite_IPSW_AddRef(IOleInPlaceSiteWindowless* This) { return axSite_CS_AddRef((IOleClientSite*)SITE_OF(This, lpVtblInPlaceSiteWindowless)); }
static ULONG STDMETHODCALLTYPE axSite_IPSW_Release(IOleInPlaceSiteWindowless* This) { return axSite_CS_Release((IOleClientSite*)SITE_OF(This, lpVtblInPlaceSiteWindowless)); }
static HRESULT STDMETHODCALLTYPE axSite_IPSW_GetWindow(IOleInPlaceSiteWindowless* This, HWND* phwnd) {
    Vb6AxSite* s = SITE_OF(This, lpVtblInPlaceSiteWindowless); *phwnd = s->hwndForm; return S_OK;
}
static HRESULT STDMETHODCALLTYPE axSite_IPSW_ContextSensitiveHelp(IOleInPlaceSiteWindowless* This, BOOL f) { (void)This; (void)f; return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_IPSW_CanInPlaceActivate(IOleInPlaceSiteWindowless* This) { (void)This; return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_IPSW_OnInPlaceActivate(IOleInPlaceSiteWindowless* This) {
    Vb6AxSite* s = SITE_OF(This, lpVtblInPlaceSiteWindowless); s->inPlaceActive = 1; return S_OK;
}
static HRESULT STDMETHODCALLTYPE axSite_IPSW_OnUIActivate(IOleInPlaceSiteWindowless* This) { (void)This; return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_IPSW_GetWindowContext(IOleInPlaceSiteWindowless* This,
    IOleInPlaceFrame** ppFrame, IOleInPlaceUIWindow** ppDoc,
    LPRECT lprcPosRect, LPRECT lprcClipRect, LPOLEINPLACEFRAMEINFO lpFrameInfo) {
    Vb6AxSite* s = SITE_OF(This, lpVtblInPlaceSiteWindowless);
    if (ppFrame) { *ppFrame = (IOleInPlaceFrame*)&s->lpVtblInPlaceFrame; s->ref++; }
    if (ppDoc) *ppDoc = NULL;
    RECT rb; rb.left=0; rb.top=0; rb.right=400; rb.bottom=300;
    const RECT* rc = (s->rcCtrl.right > s->rcCtrl.left) ? &s->rcCtrl : &rb;
    if (lprcPosRect)  memcpy(lprcPosRect, rc, sizeof(RECT));
    if (lprcClipRect) memcpy(lprcClipRect, rc, sizeof(RECT));
    if (lpFrameInfo) { lpFrameInfo->fMDIApp=FALSE; lpFrameInfo->hwndFrame=s->hwndForm;
                       lpFrameInfo->haccel=0; lpFrameInfo->cAccelEntries=0; }
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE axSite_IPSW_Scroll(IOleInPlaceSiteWindowless* This, SIZE sz) { (void)This; (void)sz; return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_IPSW_OnUIDeactivate(IOleInPlaceSiteWindowless* This, BOOL f) { (void)This; (void)f; return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_IPSW_OnInPlaceDeactivate(IOleInPlaceSiteWindowless* This) {
    Vb6AxSite* s = SITE_OF(This, lpVtblInPlaceSiteWindowless); s->inPlaceActive = 0; return S_OK;
}
static HRESULT STDMETHODCALLTYPE axSite_IPSW_DeactivateAndUndo(IOleInPlaceSiteWindowless* This) { (void)This; return S_OK; }
static HRESULT STDMETHODCALLTYPE axSite_IPSW_OnPosRectChange(IOleInPlaceSiteWindowless* This, LPCRECT r) { (void)This; (void)r; return S_OK; }

/* ---- windowless 扩展 ---- */
static HRESULT STDMETHODCALLTYPE axSite_IPSW_CanWindowlessActivate(IOleInPlaceSiteWindowless* This) {
    (void)This; return S_OK;   /* 允许无窗口激活: 控件直接画到窗体 DC */
}
static HRESULT STDMETHODCALLTYPE axSite_IPSW_GetCapture(IOleInPlaceSiteWindowless* This) {
    Vb6AxSite* s = SITE_OF(This, lpVtblInPlaceSiteWindowless);
    return (s->captured && GetCapture() == s->hwndForm) ? S_OK : S_FALSE;
}
static HRESULT STDMETHODCALLTYPE axSite_IPSW_SetCapture(IOleInPlaceSiteWindowless* This, BOOL fCapture) {
    Vb6AxSite* s = SITE_OF(This, lpVtblInPlaceSiteWindowless);
    s->captured = fCapture ? 1 : 0;
    if (fCapture) SetCapture(s->hwndForm); else ReleaseCapture();
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE axSite_IPSW_GetFocus(IOleInPlaceSiteWindowless* This) {
    Vb6AxSite* s = SITE_OF(This, lpVtblInPlaceSiteWindowless);
    return (GetFocus() == s->hwndForm) ? S_OK : S_FALSE;
}
static HRESULT STDMETHODCALLTYPE axSite_IPSW_SetFocus(IOleInPlaceSiteWindowless* This, BOOL fFocus) {
    Vb6AxSite* s = SITE_OF(This, lpVtblInPlaceSiteWindowless);
    s->focused = fFocus ? 1 : 0;
    if (fFocus) SetFocus(s->hwndForm);
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE axSite_IPSW_GetDC(IOleInPlaceSiteWindowless* This, LPCRECT prc, DWORD flags, HDC* phdc) {
    (void)prc;
    Vb6AxSite* s = SITE_OF(This, lpVtblInPlaceSiteWindowless);
    if (!phdc) return E_POINTER;
    *phdc = GetDC(s->hwndForm);
    if (!*phdc) return E_FAIL;
    if (flags & OLEDC_NODRAW) return S_OK;
    /* 裁剪到控件矩形, 保证控件不会画到窗体其它区域 */
    if (s->rcCtrl.right > s->rcCtrl.left) {
        IntersectClipRect(*phdc, s->rcCtrl.left, s->rcCtrl.top, s->rcCtrl.right, s->rcCtrl.bottom);
    }
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE axSite_IPSW_ReleaseDC(IOleInPlaceSiteWindowless* This, HDC hdc) {
    Vb6AxSite* s = SITE_OF(This, lpVtblInPlaceSiteWindowless);
    if (hdc) ReleaseDC(s->hwndForm, hdc);
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE axSite_IPSW_InvalidateRect(IOleInPlaceSiteWindowless* This, LPCRECT prc, BOOL fErase) {
    Vb6AxSite* s = SITE_OF(This, lpVtblInPlaceSiteWindowless);
    InvalidateRect(s->hwndForm, prc, fErase);
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE axSite_IPSW_InvalidateRgn(IOleInPlaceSiteWindowless* This, HRGN hRgn, BOOL fErase) {
    Vb6AxSite* s = SITE_OF(This, lpVtblInPlaceSiteWindowless);
    InvalidateRgn(s->hwndForm, hRgn, fErase);
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE axSite_IPSW_ScrollRect(IOleInPlaceSiteWindowless* This, INT dx, INT dy, LPCRECT prc, LPCRECT prcScroll) {
    (void)This; (void)dx; (void)dy; (void)prc; (void)prcScroll;
    return E_NOTIMPL;
}
static HRESULT STDMETHODCALLTYPE axSite_IPSW_AdjustRect(IOleInPlaceSiteWindowless* This, LPRECT prc) { (void)This; (void)prc; return S_FALSE; }
static HRESULT STDMETHODCALLTYPE axSite_IPSW_OnDefWindowMessage(IOleInPlaceSiteWindowless* This, UINT msg, WPARAM wp, LPARAM lp, LRESULT* pl) {
    Vb6AxSite* s = SITE_OF(This, lpVtblInPlaceSiteWindowless);
    *pl = DefWindowProcW(s->hwndForm, msg, wp, lp);
    return S_OK;
}

static const IOleInPlaceSiteWindowlessVtbl g_axSiteInPlaceSiteWindowlessVtbl = {
    axSite_IPSW_QueryInterface, axSite_IPSW_AddRef, axSite_IPSW_Release,
    axSite_IPSW_GetWindow, axSite_IPSW_ContextSensitiveHelp,
    axSite_IPSW_CanInPlaceActivate, axSite_IPSW_OnInPlaceActivate, axSite_IPSW_OnUIActivate,
    axSite_IPSW_GetWindowContext, axSite_IPSW_Scroll,
    axSite_IPSW_OnUIDeactivate, axSite_IPSW_OnInPlaceDeactivate,
    axSite_IPSW_DeactivateAndUndo, axSite_IPSW_OnPosRectChange,
    axSite_IPSW_CanWindowlessActivate, axSite_IPSW_GetCapture, axSite_IPSW_SetCapture,
    axSite_IPSW_GetFocus, axSite_IPSW_SetFocus, axSite_IPSW_GetDC, axSite_IPSW_ReleaseDC,
    axSite_IPSW_InvalidateRect, axSite_IPSW_InvalidateRgn, axSite_IPSW_ScrollRect,
    axSite_IPSW_AdjustRect, axSite_IPSW_OnDefWindowMessage
};

void* vb6_OcxHost_Create(void* hwndForm, const wchar_t* clsidStr, const wchar_t* altClsidStr,
                         const wchar_t* ocxPath, int x, int y, int w, int h,
                         const wchar_t* ctrlName, const Vb6OcxProp* props, int propCount) {
    if (!hwndForm || !clsidStr) return NULL;

    /* C3_OCX_TRACE=1: 打印实例化/激活各步 HRESULT (诊断宿主问题) */
    int ocxTrace = (GetEnvironmentVariableW(L"C3_OCX_TRACE", NULL, 0) > 0);

    /* 1. 实例化 (多路径 + 多候选 CLSID)
     * 候选 1 = clsidStr: 来自 OCX typelib 的 coclass GUID (typelib 真实类, 最可靠)
     * 候选 2 = altClsidStr: 来自 vbp Object= 的 GUID (可能指向旧版本/已注册副本)
     * 两者都可能对也可能过期 — 依次尝试, 谁先成功用谁. */
    IUnknown* pUnk = NULL;
    HRESULT hr = ((HRESULT)0x800401F1L);
    {
        const wchar_t* cand[2];
        int nCand = 0;
        cand[nCand++] = clsidStr;
        if (altClsidStr && *altClsidStr && wcscmp(altClsidStr, clsidStr) != 0)
            cand[nCand++] = altClsidStr;
        for (int ci = 0; ci < nCand && !pUnk; ci++) {
            CLSID c;
            if (FAILED(CLSIDFromString((LPOLESTR)cand[ci], &c))) continue;
            hr = ocxCreateAny(ocxPath, &c, (void**)&pUnk);
            if ((FAILED(hr) || !pUnk) && nCand > 1 && ci + 1 < nCand) {
                /* 第一个候选失败 → 试下一个 */
                if (ocxTrace) fprintf(stderr, "[C3_OCX]   clsid[%d] failed hr=0x%08lX, trying next\n",
                                      ci, (unsigned long)hr);
            }
        }
    }
    if (ocxTrace) fprintf(stderr, "[C3_OCX] %ls instantiate hr=0x%08lX pUnk=%p\n",
                          ctrlName ? ctrlName : L"?", (unsigned long)hr, (void*)pUnk);
    if (FAILED(hr) || !pUnk) return NULL;

    /* 2. IDispatch (返回值, vb6_hwnd_<name> 语义升级为"控件对象") */
    IDispatch* pDisp = NULL;
    {
        HRESULT hrQI = pUnk->lpVtbl->QueryInterface(pUnk, &IID_IDispatch, (void**)&pDisp);
        if (ocxTrace) fprintf(stderr, "[C3_OCX]   QI(IDispatch) hr=0x%08lX pDisp=%p\n",
                              (unsigned long)hrQI, (void*)pDisp);
        if (FAILED(hrQI) || !pDisp) {
            pUnk->lpVtbl->Release(pUnk);
            return NULL;
        }
    }

    /* 3. IOleObject + site */
    IOleObject* pOleObj = NULL;
    {
        HRESULT hrOle = pDisp->lpVtbl->QueryInterface(pDisp, &IID_IOleObject, (void**)&pOleObj);
        if (ocxTrace) fprintf(stderr, "[C3_OCX]   QI(IOleObject) hr=0x%08lX p=%p\n",
                              (unsigned long)hrOle, (void*)pOleObj);
    }
    if (pOleObj) {
        Vb6AxSite* site = (Vb6AxSite*)calloc(1, sizeof(Vb6AxSite));
        IOleClientSite* siteCS = NULL;
        if (site) {
            site->lpVtblClientSite = &g_axSiteClientSiteVtbl;
            site->lpVtblInPlaceSite = &g_axSiteInPlaceSiteVtbl;
            site->lpVtblInPlaceFrame = &g_axSiteInPlaceFrameVtbl;
            /* Fix 143d: windowless 控件宿主三件套 */
            site->lpVtblControlSite = &g_axSiteControlSiteVtbl;
            site->lpVtblInPlaceSiteWindowless = &g_axSiteInPlaceSiteWindowlessVtbl;
            site->lpVtblDispatch = &g_axSiteDispatchVtbl;
            site->ref = 1;
            site->hwndForm = (HWND)hwndForm;
            site->pOleObj = pOleObj;
            if (ctrlName) { wcsncpy(site->ctrlName, ctrlName, 127); site->ctrlName[127] = 0; }
            site->rcCtrl.left = x; site->rcCtrl.top = y;
            site->rcCtrl.right = x + w; site->rcCtrl.bottom = y + h;
            siteCS = (IOleClientSite*)&site->lpVtblClientSite;
            axSiteRegister(site);   /* Fix 143d: 登记以供窗体消息转发 */
            if (ocxTrace) fprintf(stderr, "[C3_OCX]   SetClientSite begin\n");
            pOleObj->lpVtbl->SetClientSite(pOleObj, siteCS);
            if (ocxTrace) fprintf(stderr, "[C3_OCX]   SetClientSite done\n");
        }
        /* 4. 设计期大小 (HIMETRIC) */
        SIZEL sz = { twipsToHimetric(w * 15), twipsToHimetric(h * 15) };
        pOleObj->lpVtbl->SetExtent(pOleObj, DVASPECT_CONTENT, &sz);
        if (ocxTrace) fprintf(stderr, "[C3_OCX]   SetExtent done\n");

        /* 4b. Fix 143c 踩坑记录: 不要对 VB6 UserControl 调
         * IPersistStreamInit::InitNew —— 实测 NewTab01.ocx 返回 0x800A9C68
         * (VB6 运行时错误), 且之后 DoVerb 也一并失败, 控件彻底不可用.
         * VB6 容器的初始化由下方 IPersistPropertyBag::Load 承担. */

        /* 5. 设计期属性 (IPersistPropertyBag) */
        if (props && propCount > 0) {
            IPersistPropertyBag* pPPB = NULL;
            if (SUCCEEDED(pDisp->lpVtbl->QueryInterface(pDisp, &IID_IPersistPropertyBag, (void**)&pPPB)) && pPPB) {
                Vb6PropBag* bag = (Vb6PropBag*)calloc(1, sizeof(Vb6PropBag));
                if (bag) {
                    bag->lpVtbl = &g_pbVtbl;
                    bag->ref = 1;
                    bag->props = props;
                    bag->count = propCount;
                    if (ocxTrace) {
                        fprintf(stderr, "[C3_OCX]   PropertyBag::Load begin (%d props):", propCount);
                        for (int pi = 0; pi < propCount; pi++)
                            fprintf(stderr, " %ls", props[pi].name);
                        fprintf(stderr, "\n");
                    }
                    pPPB->lpVtbl->Load(pPPB, (IPropertyBag*)bag, NULL);
                    if (ocxTrace) fprintf(stderr, "[C3_OCX]   PropertyBag::Load done\n");
                    ((IPropertyBag*)bag)->lpVtbl->Release((IPropertyBag*)bag);
                }
                pPPB->lpVtbl->Release(pPPB);
            } else if (ocxTrace) {
                fprintf(stderr, "[C3_OCX]   no IPersistPropertyBag\n");
            }
        }

        /* 6. 原地激活 (控件由此创建自己的子窗口并渲染).
         * 注意: 只做 INPLACEACTIVATE. OLEIVERB_UIACTIVATE 会让 VB6 UserControl
         * 挂起 (实测 NewTab01.ocx: DoVerb 不返回) — 它需要容器实现完整的
         * IOleInPlaceUIWindow / 菜单合并 / IOleInPlaceActiveObject 服务. */
        hr = pOleObj->lpVtbl->DoVerb(pOleObj, OLEIVERB_INPLACEACTIVATE, NULL, siteCS,
                                     -1, (HWND)hwndForm, NULL);
        if (ocxTrace) fprintf(stderr, "[C3_OCX] DoVerb(INPLACEACTIVATE) hr=0x%08lX\n", (unsigned long)hr);
        if (FAILED(hr)) {
            hr = pOleObj->lpVtbl->DoVerb(pOleObj, OLEIVERB_SHOW, NULL, siteCS,
                                         -1, (HWND)hwndForm, NULL);
            if (ocxTrace) fprintf(stderr, "[C3_OCX] DoVerb(SHOW) hr=0x%08lX\n", (unsigned long)hr);
        }
        if (ocxTrace) {
            int nSub = 0;
            HWND cw = FindWindowExW((HWND)hwndForm, NULL, NULL, NULL);
            while (cw) {
                wchar_t cls[128] = {0}, txt[128] = {0};
                GetClassNameW(cw, cls, 128);
                GetWindowTextW(cw, txt, 128);
                RECT rr; GetWindowRect(cw, &rr);
                fprintf(stderr, "[C3_OCX]   child %d: class='%ls' text='%ls' rect=(%ld,%ld,%ld,%ld) vis=%d\n",
                        nSub, cls, txt, (long)rr.left, (long)rr.top,
                        (long)(rr.right - rr.left), (long)(rr.bottom - rr.top),
                        IsWindowVisible(cw) ? 1 : 0);
                nSub++;
                cw = FindWindowExW((HWND)hwndForm, cw, NULL, NULL);
            }
            fprintf(stderr, "[C3_OCX] form child windows after activate: %d\n", nSub);
        }
    }

    /* 7. 位置 */
    {
        IOleInPlaceObject* pIPO = NULL;
        if (SUCCEEDED(pDisp->lpVtbl->QueryInterface(pDisp, &IID_IOleInPlaceObject, (void**)&pIPO)) && pIPO) {
            RECT rc = { x, y, x + w, y + h };
            pIPO->lpVtbl->SetObjectRects(pIPO, &rc, &rc);
            pIPO->lpVtbl->Release(pIPO);
        }
    }

    /* 8. Fix 143d: 保存 IOleInPlaceObjectWindowless (消息转发用) + 登记宿主.
     * windowless 控件激活后不会自己收窗口消息, 由宿主在窗体 WndProc 里
     * 调 vb6_OcxHost_ForwardMessage 转发 WM_PAINT/鼠标/键盘. */
    {
        IOleInPlaceObjectWindowless* pIPOW = NULL;
        if (SUCCEEDED(pDisp->lpVtbl->QueryInterface(pDisp, &IID_IOleInPlaceObjectWindowless,
                                                    (void**)&pIPOW)) && pIPOW) {
            /* 找一个属于本窗体的 site 记录 (site 在步骤 3 创建, 用 QI 反查不可靠,
             * 故这里按 hwndForm 从登记表里找最新一条) */
            for (int i = g_axSiteCount - 1; i >= 0; i--) {
                if (g_axSites[i] && g_axSites[i]->hwndForm == (HWND)hwndForm) {
                    g_axSites[i]->pInPlaceObj = pIPOW;
                    break;
                }
            }
            if (ocxTrace) fprintf(stderr, "[C3_OCX]   IOleInPlaceObjectWindowless OK (windowless 控件)\n");
        } else if (ocxTrace) {
            fprintf(stderr, "[C3_OCX]   no IOleInPlaceObjectWindowless (windowed 控件)\n");
        }
    }

    /* 9. Fix 143d: 保存 IViewObject — 宿主 WM_PAINT 里用它绘制控件 */
    {
        IViewObject* pView = NULL;
        if (SUCCEEDED(pDisp->lpVtbl->QueryInterface(pDisp, &IID_IViewObject, (void**)&pView)) && pView) {
            for (int i = g_axSiteCount - 1; i >= 0; i--) {
                if (g_axSites[i] && g_axSites[i]->hwndForm == (HWND)hwndForm) {
                    g_axSites[i]->pViewObj = pView;
                    break;
                }
            }
            if (ocxTrace) fprintf(stderr, "[C3_OCX]   IViewObject OK (可用于宿主绘制)\n");
        } else if (ocxTrace) {
            fprintf(stderr, "[C3_OCX]   no IViewObject\n");
        }
    }

    /* 10. 请求首帧重绘 */
    InvalidateRect((HWND)hwndForm, NULL, TRUE);

    pUnk->lpVtbl->Release(pUnk);
    return (void*)pDisp;
}
