// vb6forms_picture_prop.c - vb6forms 模块拆分: Picture 属性读写 + Image Stretch + WM_PAINT 子类化
// 由 vb6forms_picture.c 拆出 (2026-09-17, 纯搬移, 零行为改动)
// 本文件内的 static vb6_ImageSubclassProc 只被 vb6_InstallImageSubclass 使用, 同文件内可见.

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


void* vb6_GetControlPicture(void* hwnd) {
    if (!hwnd) return NULL;
    HWND hw = (HWND)hwnd;
    /* If a COM IPicture was stored via SetControlPictureFromCom, return the
     * borrowed COM pointer so callers (e.g. Clipboard.SetData) can QI/use it.
     * The caller must NOT Release this borrowed reference. */
    IPicture* pPic = (IPicture*)GetPropW(hw, L"VB6_IPicture");
    if (pPic) return (void*)pPic;
    HANDLE hProp = GetPropW(hw, L"VB6_Picture");
    return hProp;  // HBITMAP/HICON handle
}

void vb6_SetControlPicture(void* hwnd, void* hPicture) {
    if (!hwnd) return;
    HWND hw = (HWND)hwnd;
    /* A raw handle is replacing any COM IPicture set earlier via
     * SetControlPictureFromCom. Release the retained COM reference so it
     * doesn't leak and won't be used by the subclass. */
    IPicture* pOldCom = (IPicture*)GetPropW(hw, L"VB6_IPicture");
    if (pOldCom) {
        pOldCom->lpVtbl->Release(pOldCom);
        RemovePropW(hw, L"VB6_IPicture");
    }
    /* Store the picture handle and type (default=bitmap) */
    SetPropW(hw, L"VB6_Picture", (HANDLE)hPicture);
    SetPropW(hw, L"VB6_PictureType", (HANDLE)1);  /* default: bitmap */
    
    /* Apply to the control */
    WCHAR className[256] = {0};
    GetClassNameW(hw, className, 256);
    
    if (wcsicmp(className, L"STATIC") == 0) {
        /* PictureBox uses STATIC control with SS_BITMAP/SS_ICON/SS_ENHMETAFILE style */
        if (hPicture) {
            /* Detect picture type - check if it's an icon by trying */
            LONG style = GetWindowLongW(hw, GWL_STYLE);
            style &= ~(SS_BITMAP | SS_ICON | SS_ENHMETAFILE);
            
            /* For now assume bitmap; icon detection would require more logic */
            style |= SS_BITMAP | SS_CENTERIMAGE;
            SetWindowLongW(hw, GWL_STYLE, style);
            SendMessageW(hw, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)hPicture);
        } else {
            /* Clear picture */
            LONG style = GetWindowLongW(hw, GWL_STYLE);
            style &= ~(SS_BITMAP | SS_ICON | SS_ENHMETAFILE);
            SetWindowLongW(hw, GWL_STYLE, style);
            SendMessageW(hw, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)NULL);
        }
        InvalidateRect(hw, NULL, TRUE);
    }
    
    /* AutoSize: if enabled, resize to fit picture */
    int autoSize = vb6_GetPictureAutoSize(hwnd);
    if (autoSize && hPicture) {
        BITMAP bm;
        HBITMAP hBmp = (HBITMAP)hPicture;
        if (GetObjectW(hBmp, sizeof(bm), &bm) != 0) {
            SetWindowPos(hw, NULL, 0, 0, bm.bmWidth, bm.bmHeight,
                SWP_NOMOVE | SWP_NOZORDER);
        }
    }
}


// Set Picture from COM IPictureDisp object (e.g. ImageList1.ListImages(i).Picture)
// IPictureDisp::get_Handle returns OLE_HANDLE (HBITMAP for bitmap type)
//
// All COM picture types are rendered directly via the retained IPicture COM
// pointer in the Image subclass (vb6_ImageSubclassProc). We do NOT convert
// metafiles to bitmaps here — that was the broken "control -> bitmap" path.
// VB6_Picture still keeps the native handle for legacy raw-handle callers.
void vb6_SetControlPictureFromCom(void* hwnd, void* pPictureDisp) {
    if (!hwnd) return;
    HWND hw = (HWND)hwnd;

    /* NULL COM setter clears the picture (release COM ref + clear props). */
    if (!pPictureDisp) {
        IPicture* pOld = (IPicture*)GetPropW(hw, L"VB6_IPicture");
        if (pOld) {
            pOld->lpVtbl->Release(pOld);
            RemovePropW(hw, L"VB6_IPicture");
        }
        SetPropW(hw, L"VB6_Picture", (HANDLE)NULL);
        SetPropW(hw, L"VB6_PictureType", (HANDLE)0);
        InvalidateRect(hw, NULL, TRUE);
        return;
    }

    HRESULT hr;
    /* IPictureDisp and IPicture are separate interfaces.
     * IPictureDisp inherits IDispatch, IPicture inherits IUnknown.
     * Must QI for IID_IPicture from IPictureDisp pointer. */
    IPicture* pPic = NULL;
    IUnknown* pUnk = (IUnknown*)pPictureDisp;
    hr = pUnk->lpVtbl->QueryInterface(pUnk, &IID_IPicture, (void**)&pPic);
    if (FAILED(hr) || !pPic) return;

    OLE_HANDLE hOle = 0;
    SHORT nType = 0;
    // Get picture type: 1=Bitmap, 2=Metafile, 3=Icon, 4=Enhanced Metafile
    hr = pPic->lpVtbl->get_Type(pPic, &nType);
    if (FAILED(hr)) { pPic->lpVtbl->Release(pPic); return; }
    hr = pPic->lpVtbl->get_Handle(pPic, &hOle);
    if (FAILED(hr)) { pPic->lpVtbl->Release(pPic); return; }

    /* QI already acquired the new reference, including self-assignment.
     * Transfer that reference to the control and release exactly one old ref. */
    IPicture* pOld = (IPicture*)GetPropW(hw, L"VB6_IPicture");
    if (!SetPropW(hw, L"VB6_IPicture", (HANDLE)pPic)) {
        pPic->lpVtbl->Release(pPic);
        return;
    }
    if (pOld) pOld->lpVtbl->Release(pOld);

    /* Keep the native handle for legacy raw-handle callers (GetControlPicture
     * falls back to this when no COM pointer is stored). */
    SetPropW(hw, L"VB6_Picture", (HANDLE)(UINT_PTR)hOle);
    SetPropW(hw, L"VB6_PictureType", (HANDLE)(INT_PTR)nType);

    /* The retained QI reference keeps the native handle alive for painting. */

    /* All COM picture types route through the Image subclass for rendering.
     * Remove the default STATIC bitmap/icon/emf rendering styles so WM_PAINT is
     * fully owned by our subclass. */
    LONG style = GetWindowLongW(hw, GWL_STYLE);
    style &= ~(SS_BITMAP | SS_ICON | SS_ENHMETAFILE | SS_CENTERIMAGE);
    SetWindowLongW(hw, GWL_STYLE, style);

    vb6_InstallImageSubclass(hwnd);
    InvalidateRect(hw, NULL, TRUE);
}
int vb6_GetPictureAutoSize(void* hwnd) {
    if (!hwnd) return 0;
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_AutoSize");
    if (hProp) return (int)(INT_PTR)hProp;
    return 0;
}

void vb6_SetPictureAutoSize(void* hwnd, int autoSize) {
    if (!hwnd) return;
    SetPropW((HWND)hwnd, L"VB6_AutoSize", (HANDLE)(INT_PTR)autoSize);
}
// ============================================================
// P17.2: Image.Stretch property + WM_PAINT subclass (StretchBlt)
// ============================================================

int vb6_GetImageStretch(void* hwnd) {
    if (!hwnd) return 0;
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_Stretch");
    return hProp ? (int)(INT_PTR)hProp : 0;
}

void vb6_SetImageStretch(void* hwnd, int stretch) {
    if (!hwnd) return;
    HWND hw = (HWND)hwnd;
    SetPropW(hw, L"VB6_Stretch", (HANDLE)(INT_PTR)stretch);
    if (stretch) {
        vb6_InstallImageSubclass(hwnd);
    }
    InvalidateRect(hw, NULL, TRUE);
}

/* Shared painting for WM_PAINT and WM_PRINTCLIENT.
 * Draws the retained COM IPicture (metafile/bitmap/icon) or a raw
 * HBITMAP/HICON/HENHMETAFILE handle, honoring the Stretch property.
 * hdc is the paint DC (from BeginPaint) or the DC supplied by WM_PRINTCLIENT. */
static void vb6_ImagePaintHelper(HWND hwnd, HDC hdc) {
    RECT rc;
    GetClientRect(hwnd, &rc);

    int stretch = (int)(INT_PTR)GetPropW(hwnd, L"VB6_Stretch");

    /* Default background: system button face (control background). */
    HBRUSH hBg = (HBRUSH)GetSysColorBrush(COLOR_BTNFACE);
    FillRect(hdc, &rc, hBg);

    /* Preferred path: retained COM IPicture (from SetControlPictureFromCom).
     * Works for bitmap, icon, and metafile uniformly via IPicture::Render. */
    IPicture* pPic = (IPicture*)GetPropW(hwnd, L"VB6_IPicture");
    if (pPic) {
        OLE_XSIZE_HIMETRIC hmW = 0;
        OLE_YSIZE_HIMETRIC hmH = 0;
        pPic->lpVtbl->get_Width(pPic, &hmW);
        pPic->lpVtbl->get_Height(pPic, &hmH);

        /* Metafiles / QR codes typically expect a white background. */
        FillRect(hdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));

        int dstW, dstH;
        if (stretch) {
            /* Stretch fills the whole client area. */
            dstW = rc.right;
            dstH = rc.bottom;
        } else {
            /* Non-stretch: draw at the picture's natural pixel size, converting
             * HIMETRIC (2540 HIMETRIC per logical inch) using the DC's DPI. */
            int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
            int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
            dstW = MulDiv((int)hmW, dpiX, 2540);
            dstH = MulDiv((int)hmH, dpiY, 2540);
            if (dstW <= 0) dstW = rc.right;
            if (dstH <= 0) dstH = rc.bottom;
        }
        /* HIMETRIC y points up, so flip the source rect: (0, hmH, hmW, -hmH). */
        pPic->lpVtbl->Render(pPic, hdc, 0, 0, dstW, dstH,
                             0, hmH, hmW, -hmH, NULL);
        return;
    }

    HANDLE hPict = GetPropW(hwnd, L"VB6_Picture");
    int picType = (int)(INT_PTR)GetPropW(hwnd, L"VB6_PictureType");
    if (!hPict) return;

    DWORD objType = GetObjectType((HGDIOBJ)hPict);
    if (objType == OBJ_ENHMETAFILE) {
        PlayEnhMetaFile(hdc, (HENHMETAFILE)hPict, &rc);
        return;
    }
    if (objType == OBJ_BITMAP) {
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP hBmp = (HBITMAP)hPict;
        BITMAP bm;
        GetObjectW(hBmp, sizeof(bm), &bm);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, hBmp);
        if (stretch) {
            SetStretchBltMode(hdc, HALFTONE);
            SetBrushOrgEx(hdc, 0, 0, NULL);
            StretchBlt(hdc, 0, 0, rc.right, rc.bottom,
                       memDC, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
        } else {
            BitBlt(hdc, 0, 0, bm.bmWidth, bm.bmHeight, memDC, 0, 0, SRCCOPY);
        }
        SelectObject(memDC, oldBmp);
        DeleteDC(memDC);
        return;
    }
    if (picType == 3) {
        DrawIconEx(hdc, 0, 0, (HICON)hPict, 0, 0, 0, NULL, DI_NORMAL);
        return;
    }
}

/* Image control subclass WndProc for WM_PAINT / WM_PRINTCLIENT rendering. */
static LRESULT CALLBACK vb6_ImageSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (hdc) {
            vb6_ImagePaintHelper(hwnd, hdc);
            EndPaint(hwnd, &ps);
        }
        return 0;
    } else if (msg == WM_PRINTCLIENT) {
        /* Printing / theming asks us to render into the provided DC. */
        HDC hdc = (HDC)wp;
        if (hdc) vb6_ImagePaintHelper(hwnd, hdc);
        return 0;
    } else if (msg == WM_DESTROY) {
        /* Capture the original WndProc BEFORE removing props, then forward the
         * message to it. The old code removed the prop and then looked it up
         * again at the fall-through, so WM_DESTROY was lost to DefWindowProc. */
        WNDPROC origProc = (WNDPROC)GetPropW(hwnd, L"VB6_OrigProc");

        IPicture* pPic = (IPicture*)GetPropW(hwnd, L"VB6_IPicture");
        if (pPic) {
            pPic->lpVtbl->Release(pPic);
            RemovePropW(hwnd, L"VB6_IPicture");
        }
        if (origProc) {
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)origProc);
            RemovePropW(hwnd, L"VB6_OrigProc");
            return CallWindowProcW(origProc, hwnd, msg, wp, lp);
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    /* Fall through to original STATIC WndProc for all other messages. */
    WNDPROC origProc = (WNDPROC)GetPropW(hwnd, L"VB6_OrigProc");
    if (origProc) return CallWindowProcW(origProc, hwnd, msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void vb6_InstallImageSubclass(void* hwnd) {
    if (!hwnd) return;
    HWND hw = (HWND)hwnd;
    /* Only install once */
    if (GetPropW(hw, L"VB6_OrigProc")) return;
    WNDPROC origProc = (WNDPROC)SetWindowLongPtrW(hw, GWLP_WNDPROC, (LONG_PTR)vb6_ImageSubclassProc);
    if (origProc) SetPropW(hw, L"VB6_OrigProc", (HANDLE)origProc);
}
