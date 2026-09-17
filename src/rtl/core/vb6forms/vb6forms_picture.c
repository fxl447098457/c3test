// vb6forms_picture.c - vb6forms 模块拆分: 图片/图标加载 + Picture 属性 + Image
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

// ============================================================
// P13.13: Picture (PictureBox/Image)
// ============================================================

void* vb6_LoadPictureFromFile(const char* filePath) {
    if (!filePath || !filePath[0]) return NULL;
    /* Determine type by extension */
    const char* ext = strrchr(filePath, '.');
    if (!ext) ext = "";
    
    WCHAR wPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_ACP, 0, filePath, -1, wPath, MAX_PATH);
    
    if (_stricmp(ext, ".ico") == 0 || _stricmp(ext, ".cur") == 0) {
        /* Icon/Cursor */
        if (_stricmp(ext, ".cur") == 0) {
            return (void*)LoadCursorFromFileW(wPath);
        }
        return (void*)LoadImageW(NULL, wPath, IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
    } else if (_stricmp(ext, ".bmp") == 0) {
        return (void*)LoadImageW(NULL, wPath, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    } else if (_stricmp(ext, ".emf") == 0 || _stricmp(ext, ".wmf") == 0) {
        /* Enhanced/Regular metafile - use GetEnhMetaFile/GetMetaFile */
        HENHMETAFILE hemf = GetEnhMetaFileW(wPath);
        return (void*)hemf;
    }
    /* Try as bitmap fallback */
    return (void*)LoadImageW(NULL, wPath, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
}

void* vb6_LoadPictureFromResource(void* hInstance, int resourceId, const char* type) {
    if (!hInstance) return NULL;
    HINSTANCE hInst = (HINSTANCE)hInstance;
    WCHAR wType[64] = {0};
    if (type) MultiByteToWideChar(CP_ACP, 0, type, -1, wType, 64);
    
    if (type && _stricmp(type, "ICON") == 0) {
        return (void*)LoadIconW(hInst, MAKEINTRESOURCEW(resourceId));
    } else if (type && _stricmp(type, "BITMAP") == 0) {
        return (void*)LoadBitmapW(hInst, MAKEINTRESOURCEW(resourceId));
    } else if (type && _stricmp(type, "CURSOR") == 0) {
        return (void*)LoadCursorW(hInst, MAKEINTRESOURCEW(resourceId));
    }
    /* Try bitmap as default */
    return (void*)LoadBitmapW(hInst, MAKEINTRESOURCEW(resourceId));
}

// ============================================================
// P24: UTF-8 to wide string conversion (caller must free())
// ============================================================
wchar_t* vb6_Utf8ToWide(const char* utf8) {
    if (!utf8) return NULL;
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (wlen <= 0) return NULL;
    wchar_t* wstr = (wchar_t*)malloc((size_t)wlen * sizeof(wchar_t));
    if (!wstr) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wstr, wlen);
    return wstr;
}

// ============================================================
// P24: Load picture from memory buffer
// Uses CreateStreamOnHGlobal + OleLoadPicture to support JPEG/BMP/ICO/PNG/GIF
// ============================================================
void* vb6_LoadPictureFromMemory(const void* data, int size) {
    if (!data || size <= 0) return NULL;

    /* Ensure COM is initialized for OLE picture loading */
    static int oleInited = 0;
    if (!oleInited) {
        if (SUCCEEDED(OleInitialize(NULL))) {
            oleInited = 1;
        } else {
            /* Try CoInitialize as fallback */
            CoInitialize(NULL);
            oleInited = 2;
        }
    }

    /* Create IStream from memory via HGLOBAL */
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)size);
    if (!hMem) return NULL;
    void* pMem = GlobalLock(hMem);
    if (!pMem) { GlobalFree(hMem); return NULL; }
    memcpy(pMem, data, (size_t)size);
    GlobalUnlock(hMem);

    IStream* pStream = NULL;
    HRESULT hr = CreateStreamOnHGlobal(hMem, TRUE, &pStream);
    if (FAILED(hr) || !pStream) {
        GlobalFree(hMem);
        return NULL;
    }

    /* Load picture from stream */
    IPicture* pPicture = NULL;
    /* IID_IPicture = {7BF80980-BF32-101A-8BBB-00AA00300CAB} */
    static const GUID local_IID_IPicture = 
        {0x7BF80980, 0xBF32, 0x101A, {0x8B, 0xBB, 0x00, 0xAA, 0x00, 0x30, 0x0C, 0xAB}};
    hr = OleLoadPicture(pStream, (LONG)size, FALSE, &local_IID_IPicture, (void**)&pPicture);
    
    /* Stream is released by OleLoadPicture or we release it ourselves */
    pStream->lpVtbl->Release(pStream);
    /* Note: CreateStreamOnHGlobal with TRUE means HGLOBAL is freed when stream is released */

    if (FAILED(hr) || !pPicture) {
        return NULL;
    }

    /* Get the picture handle (HBITMAP for BMP/JPEG, HICON for ICO) */
    OLE_HANDLE hHandle = 0;
    pPicture->lpVtbl->get_Handle(pPicture, &hHandle);
    
    /* Get picture type to know if it's bitmap or icon */
    short picType = 0;
    pPicture->lpVtbl->get_Type(pPicture, &picType);
    
    /* For bitmaps, we need to copy the handle because IPicture owns it */
    HANDLE result = NULL;
    if (picType == 1) {
        /* PICTURE_TYPE_BITMAP - copy the bitmap */
        HBITMAP hSrc = (HBITMAP)(LONG_PTR)hHandle;
        if (hSrc) {
            BITMAP bm;
            if (GetObjectW(hSrc, sizeof(bm), &bm) != 0) {
                HDC hdcScreen = GetDC(NULL);
                HDC hdcSrc = CreateCompatibleDC(hdcScreen);
                HDC hdcDst = CreateCompatibleDC(hdcScreen);
                HBITMAP hDst = CreateCompatibleBitmap(hdcScreen, bm.bmWidth, bm.bmHeight);
                HBITMAP oldSrc = (HBITMAP)SelectObject(hdcSrc, hSrc);
                HBITMAP oldDst = (HBITMAP)SelectObject(hdcDst, hDst);
                BitBlt(hdcDst, 0, 0, bm.bmWidth, bm.bmHeight, hdcSrc, 0, 0, SRCCOPY);
                SelectObject(hdcSrc, oldSrc);
                SelectObject(hdcDst, oldDst);
                DeleteDC(hdcSrc);
                DeleteDC(hdcDst);
                ReleaseDC(NULL, hdcScreen);
                result = (HANDLE)hDst;
            }
        }
    } else if (picType == 3) {
        /* PICTURE_TYPE_ICON - must duplicate because IPicture owns it */
        HICON hSrc = (HICON)(LONG_PTR)hHandle;
        if (hSrc) {
            result = (HANDLE)DuplicateIcon(NULL, hSrc);
        }
    } else {
        /* Metafile - use handle directly (rare for .frx) */
        result = (HANDLE)(LONG_PTR)hHandle;
    }
    
    pPicture->lpVtbl->Release(pPicture);
    return (void*)result;
}

// ============================================================
// Load picture from memory buffer as COM IDispatch* (IPictureDisp)
// Returns IDispatch* suitable for passing to COM property/method calls
// e.g. ImageList.ListImages.Add(index, key, picture)
// Caller must Release the returned IDispatch* when done (via vb6_ReleaseObject)
// ============================================================
void* vb6_LoadPictureAsCom(const void* data, int size) {
    { static int _lpcnt = 0; _lpcnt++; fwprintf(stderr, L"LOADPIC[%d]: size=%d\n", _lpcnt, size); }
    if (!data || size <= 0) return NULL;

    /* Ensure COM is initialized */
    static int oleInited2 = 0;
    if (!oleInited2) {
        if (SUCCEEDED(OleInitialize(NULL))) {
            oleInited2 = 1;
        } else {
            CoInitialize(NULL);
            oleInited2 = 2;
        }
    }

    /* Create IStream from memory */
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)size);
    if (!hMem) return NULL;
    void* pMem = GlobalLock(hMem);
    if (!pMem) { GlobalFree(hMem); return NULL; }
    memcpy(pMem, data, (size_t)size);
    GlobalUnlock(hMem);

    IStream* pStream = NULL;
    HRESULT hr = CreateStreamOnHGlobal(hMem, TRUE, &pStream);
    if (FAILED(hr) || !pStream) {
        GlobalFree(hMem);
        return NULL;
    }

    /* Load picture as IPictureDisp (IDispatch) */
    IPictureDisp* pPictureDisp = NULL;
    /* IID_IPictureDisp = {7BF80981-BF32-101A-8BBB-00AA00300CAB} */
    static const GUID local_IID_IPictureDisp =
        {0x7BF80981, 0xBF32, 0x101A, {0x8B, 0xBB, 0x00, 0xAA, 0x00, 0x30, 0x0C, 0xAB}};
    hr = OleLoadPicture(pStream, (LONG)size, FALSE, &local_IID_IPictureDisp, (void**)&pPictureDisp);

    pStream->lpVtbl->Release(pStream);

    if (FAILED(hr) || !pPictureDisp) {
        return NULL;
    }

    /* Return as IDispatch* — caller owns the reference and must Release */
    return (void*)pPictureDisp;
}
void* vb6_LoadIconFromMemory(const void* data, int size) {
    /* Load ICO data and return HICON by directly parsing .ico file format.
       .ico format: [2B reserved=0] [2B type=1] [2B count] [count*16B dir entries] [image data...]
       Each dir entry: [B w] [B h] [B colors] [B reserved] [W planes] [W bpp] [D dataSize] [D dataOffset]
    */
    if (!data || size <= 6) return NULL;
    const BYTE* p = (const BYTE*)data;
    /* Verify ICO magic */
    if (p[0] != 0 || p[1] != 0 || p[2] != 1 || p[3] != 0) {
        /* Not ICO format, fall back to OleLoadPicture */
        return vb6_LoadPictureFromMemory(data, size);
    }
    WORD count = (WORD)(p[4] | (p[5] << 8));
    if (count == 0) return NULL;
    /* Find best matching entry: prefer 32x32 or closest to SM_CXICON */
    int targetSize = GetSystemMetrics(SM_CXSMICON); /* small icon for title bar */
    int bestIdx = 0;
    int bestDiff = 9999;
    int bestBpp = 0;
    for (int i = 0; i < count && i < 20; i++) {
        const BYTE* entry = p + 6 + i * 16;
        int w = entry[0]; if (w == 0) w = 256;
        int h = entry[1]; if (h == 0) h = 256;
        int bpp = (int)(entry[6] | (entry[7] << 8));
        int diff = abs(w - targetSize) + abs(h - targetSize);
        /* Prefer higher bpp if same size */
        if (diff < bestDiff || (diff == bestDiff && bpp > bestBpp)) {
            bestDiff = diff;
            bestIdx = i;
            bestBpp = bpp;
        }
    }
    /* Read the best entry's data offset and size */
    const BYTE* bestEntry = p + 6 + bestIdx * 16;
    DWORD imgDataSize = (DWORD)(bestEntry[8] | (bestEntry[9] << 8) | (bestEntry[10] << 16) | (bestEntry[11] << 24));
    DWORD imgDataOffset = (DWORD)(bestEntry[12] | (bestEntry[13] << 8) | (bestEntry[14] << 16) | (bestEntry[15] << 24));
    if (imgDataOffset + imgDataSize > (DWORD)size || imgDataSize == 0) return NULL;
    /* CreateIconFromResourceEx expects the icon image data (BITMAPINFOHEADER + colors + XOR + AND) */
    const BYTE* pImgData = p + imgDataOffset;
    HICON hIcon = CreateIconFromResourceEx((PBYTE)pImgData, imgDataSize, TRUE, 0x00030000,
        targetSize, targetSize, LR_DEFAULTCOLOR);
    if (!hIcon) {
        /* Fallback: try with SM_CXICON */
        hIcon = CreateIconFromResourceEx((PBYTE)pImgData, imgDataSize, TRUE, 0x00030000,
            GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
    }
    return (void*)hIcon;
}

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
