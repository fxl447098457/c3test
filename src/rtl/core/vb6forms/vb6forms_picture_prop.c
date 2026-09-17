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
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_Picture");
    return hProp;  // HBITMAP/HICON handle
}

void vb6_SetControlPicture(void* hwnd, void* hPicture) {
    if (!hwnd) return;
    HWND hw = (HWND)hwnd;
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
void vb6_SetControlPictureFromCom(void* hwnd, void* pPictureDisp) {
    if (!hwnd || !pPictureDisp) return;
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
    // Get picture type: 1=Bitmap, 2=Metafile, 3=Icon
    hr = pPic->lpVtbl->get_Type(pPic, &nType);
    if (FAILED(hr)) { pPic->lpVtbl->Release(pPic); return; }
    hr = pPic->lpVtbl->get_Handle(pPic, &hOle);
    if (FAILED(hr) || !hOle) { pPic->lpVtbl->Release(pPic); return; }
    if (nType == 1) {
        // Bitmap: OLE_HANDLE is HBITMAP (use UINT_PTR to avoid C4312 truncation warning)
        vb6_SetControlPicture(hwnd, (void*)(UINT_PTR)hOle);
    } else if (nType == 3) {
        // Icon: OLE_HANDLE is HICON - set as icon on STATIC
        HWND hw = (HWND)hwnd;
        SetPropW(hw, L"VB6_Picture", (HANDLE)(UINT_PTR)hOle);
        SetPropW(hw, L"VB6_PictureType", (HANDLE)(INT_PTR)nType);
        LONG style = GetWindowLongW(hw, GWL_STYLE);
        style &= ~(SS_BITMAP | SS_ICON | SS_ENHMETAFILE);
        style |= SS_ICON | SS_CENTERIMAGE;
        SetWindowLongW(hw, GWL_STYLE, style);
        SendMessageW(hw, STM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)(UINT_PTR)hOle);
        InvalidateRect(hw, NULL, TRUE);
    } else if (nType == 4 || nType == 2) {
        // Enhanced Metafile (nType==4) or regular Metafile (nType==2)
        // Convert to bitmap for reliable rendering on STATIC control
        HWND hw = (HWND)hwnd;
        // Use IPicture's HIMETRIC dimensions to determine pixel size
        OLE_XSIZE_HIMETRIC hmW = 0;
        OLE_YSIZE_HIMETRIC hmH = 0;
        pPic->lpVtbl->get_Width(pPic, &hmW);
        pPic->lpVtbl->get_Height(pPic, &hmH);
        int cxPx, cyPx;
        if (hmW > 0 && hmH > 0) {
            cxPx = MulDiv((int)hmW, 96, 2540);  // HIMETRIC to pixels at 96 DPI
            cyPx = MulDiv((int)hmH, 96, 2540);
        } else {
            cxPx = 200; cyPx = 200;
        }
        if (cxPx <= 0) cxPx = 200;
        if (cyPx <= 0) cyPx = 200;
        // Render to a bitmap using IPicture::Render
        HDC hdcScreen = GetDC(NULL);
        HDC memDC = CreateCompatibleDC(hdcScreen);
        HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, cxPx, cyPx);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, hBmp);
        RECT rcRender = { 0, 0, cxPx, cyPx };
        // Fill white background
        HBRUSH hWhite = (HBRUSH)GetStockObject(WHITE_BRUSH);
        FillRect(memDC, &rcRender, hWhite);
        // Use IPicture::Render to draw onto the memory DC
        pPic->lpVtbl->Render(pPic, memDC, 0, 0, cxPx, cyPx,
                                           0, 0, hmW, hmH, NULL);
        SelectObject(memDC, oldBmp);
        DeleteDC(memDC);
        ReleaseDC(NULL, hdcScreen);
        // Store the rendered bitmap in VB6_Picture and enable Stretch for display
        SetPropW(hw, L"VB6_Picture", (HANDLE)hBmp);
        SetPropW(hw, L"VB6_PictureType", (HANDLE)1);  /* bitmap */
        SetPropW(hw, L"VB6_Stretch", (HANDLE)1);  /* enable stretch to fit */
        // Remove any icon/emf style, set bitmap style
        LONG style = GetWindowLongW(hw, GWL_STYLE);
        style &= ~(SS_ICON | SS_ENHMETAFILE);
        style |= SS_BITMAP;
        SetWindowLongW(hw, GWL_STYLE, style);
        // Install paint subclass for StretchBlt rendering
        vb6_InstallImageSubclass(hwnd);
        InvalidateRect(hw, NULL, TRUE);
    } else {
        // Regular metafile (nType==2) or other - try as bitmap (use UINT_PTR to avoid C4312)
        vb6_SetControlPicture(hwnd, (void*)(UINT_PTR)hOle);
    }
    pPic->lpVtbl->Release(pPic);
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

/* Image control subclass WndProc for WM_PAINT (IPicture::Render / StretchBlt rendering) */
static LRESULT CALLBACK vb6_ImageSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_PAINT) {
        /* Check for stored IPicture COM pointer (EMF/Metafile from SetControlPictureFromCom) */
        IPicture* pPic = (IPicture*)GetPropW(hwnd, L"VB6_IPicture");
        if (pPic) {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            OLE_XSIZE_HIMETRIC hmW = 0;
            OLE_YSIZE_HIMETRIC hmH = 0;
            pPic->lpVtbl->get_Width(pPic, &hmW);
            pPic->lpVtbl->get_Height(pPic, &hmH);
            pPic->lpVtbl->Render(pPic, hdc, 0, 0, rc.right, rc.bottom,
                                  0, 0, hmW, hmH, NULL);
            EndPaint(hwnd, &ps);
            return 0;
        }
        int stretch = (int)(INT_PTR)GetPropW(hwnd, L"VB6_Stretch");
        HANDLE hPict = GetPropW(hwnd, L"VB6_Picture");
        int picType = (int)(INT_PTR)GetPropW(hwnd, L"VB6_PictureType");
        if (hPict) {
            if (picType == 4 || GetObjectType((HGDIOBJ)hPict) == OBJ_ENHMETAFILE) {
                /* Enhanced Metafile: use PlayEnhMetaFile (always stretch to fit) */
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                RECT rc;
                GetClientRect(hwnd, &rc);
                HENHMETAFILE hEmf = (HENHMETAFILE)hPict;
                PlayEnhMetaFile(hdc, hEmf, &rc);
                EndPaint(hwnd, &ps);
                return 0;
            } else if (stretch && GetObjectType((HGDIOBJ)hPict) == OBJ_BITMAP) {
                /* Bitmap with stretch: use StretchBlt */
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                RECT rc;
                GetClientRect(hwnd, &rc);
                
                HDC memDC = CreateCompatibleDC(hdc);
                HBITMAP hBmp = (HBITMAP)hPict;
                BITMAP bm;
                GetObjectW(hBmp, sizeof(bm), &bm);
                HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, hBmp);
                
                SetStretchBltMode(hdc, HALFTONE);
                SetBrushOrgEx(hdc, 0, 0, NULL);
                StretchBlt(hdc, 0, 0, rc.right, rc.bottom,
                           memDC, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
                
                SelectObject(memDC, oldBmp);
                DeleteDC(memDC);
                EndPaint(hwnd, &ps);
                return 0;
            }
            /* For icons: fall through to original STATIC proc (no stretch) */
        }
    } else if (msg == WM_DESTROY) {
        /* Release stored IPicture COM pointer */
        IPicture* pPic = (IPicture*)GetPropW(hwnd, L"VB6_IPicture");
        if (pPic) {
            pPic->lpVtbl->Release(pPic);
            RemovePropW(hwnd, L"VB6_IPicture");
        }
        /* Remove subclass on destroy */
        WNDPROC origProc = (WNDPROC)GetPropW(hwnd, L"VB6_OrigProc");
        if (origProc) {
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)origProc);
            RemovePropW(hwnd, L"VB6_OrigProc");
        }
    }
    /* Fall through to original STATIC WndProc */
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
