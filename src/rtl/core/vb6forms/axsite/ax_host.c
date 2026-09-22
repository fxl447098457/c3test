// ax_host.c - vb6forms_axsite 拆分片：对外入口: 窗体 Dispatch 属性 / Controls.Add / 宿主创建 10 步流程 / 消息转发与绘制
//
// 内容 = 拆分前 vb6forms_axsite.c 第 22~28 / 207~276 / 532~590 / 833~1029 行，纯搬移零重排无行为改动
// 跨族共享符号见 vb6forms_axsite_internal.h

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#endif

#include "vb6forms_axsite_internal.h"
#include <stdio.h>
#include <stdarg.h>

#include <stdlib.h>   /* malloc, free */
#include <stddef.h>   /* offsetof */
#include <oleauto.h>  /* SysAllocString, BSTR */
#include <olectl.h>   /* IPicture, OleLoadPicture, OLE_HANDLE */

#ifdef __cplusplus
extern "C" {
#endif

static const wchar_t* g_FormDispatchProp = L"VB6_Form_IDispatch";

void vb6_Form_SetDispatch(void* hwnd, void* pDispatch) {
    if (!hwnd) return;
    SetPropW((HWND)hwnd, g_FormDispatchProp, (HANDLE)pDispatch);
}

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

#ifdef __cplusplus
} // extern "C"
#endif
