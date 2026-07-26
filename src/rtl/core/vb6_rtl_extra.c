// vb6_rtl_extra.c - Fix 076: Extra RTL functions missing from pre-compiled lib
// These functions exist in vb6rtl.c source but the pre-compiled .lib is outdated.
// Compiled with /MT to match existing vb6rtl.lib CRT linkage.

#include <windows.h>
#include <oleauto.h>
#include <olectl.h>
#include <stdint.h>

/* From vb6rtl.h - minimal type definitions */
#define vb6_vtEmpty   0
#define vb6_vtNull    1
#define vb6_vtLong    3
#define vb6_vtString  8
#define vb6_vtDispatch 9
#define vb6_vtArray   0x2000

typedef struct vb6_SafeArray1D vb6_SafeArray1D;

typedef struct tagvb6_VARIANT {
    int32_t vt;
    union {
        int32_t lVal;
        double dblVal;
        void* pdispVal;
        BSTR bstrVal;
        vb6_SafeArray1D* parray;
    };
} vb6_VARIANT;

/* vb6_LenB_BSTR - byte length of BSTR (wchar count * 2) */
int32_t vb6_LenB_BSTR(BSTR s) {
    if (!s) return 0;
    return (int32_t)(SysStringLen(s) * sizeof(wchar_t));
}

/* vb6_VariantToSafeArray1D - extract SafeArray pointer from Variant */
struct vb6_SafeArray1D* vb6_VariantToSafeArray1D(vb6_VARIANT v) {
    if ((v.vt & vb6_vtArray) && v.parray) {
        return v.parray;
    }
    return NULL;
}

/* vb6_Clipboard_SetData - copy IPicture bitmap to clipboard */
void vb6_Clipboard_SetData(void* pPicture) {
    if (!pPicture) return;
    if (!OpenClipboard(NULL)) return;
    EmptyClipboard();
    HBITMAP hBmp = NULL;
    IPicture* pPic = (IPicture*)pPicture;
    OLE_HANDLE hOleHandle = 0;
    pPic->lpVtbl->get_Handle(pPic, &hOleHandle);
    hBmp = (HBITMAP)(uintptr_t)hOleHandle;
    if (hBmp) {
        SetClipboardData(CF_BITMAP, hBmp);
    }
    CloseClipboard();
}
