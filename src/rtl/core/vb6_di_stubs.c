// vb6_di_stubs.c - Fix 076: Declare import forwarding stubs
// C3 generates extern declarations with vb6_di_ prefix for VB6 Declare functions.
// These stubs forward from the vb6_di_ name to the real Windows API.
// Separate compilation unit to avoid #define conflicts with generated code.

#include <windows.h>
#include <oleauto.h>
#include <olectl.h>
#include <stdint.h>

// Use static CRT to match vb6rtl.lib
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")

typedef int32_t __stdcall vb6_di_fn_int32_t;

/* GDI32 forwarding stubs */
int32_t __stdcall vb6_di_CreateEnhMetaFileW(int32_t a, int32_t b, int32_t c, int32_t d) {
    return (int32_t)(uintptr_t)CreateEnhMetaFileW((HDC)(uintptr_t)a, (LPCWSTR)(uintptr_t)b, (const RECT*)(uintptr_t)c, (LPCWSTR)(uintptr_t)d);
}
int32_t __stdcall vb6_di_CloseEnhMetaFile(int32_t a) {
    return (int32_t)(uintptr_t)CloseEnhMetaFile((HDC)(uintptr_t)a);
}
int32_t __stdcall vb6_di_GetStockObject(int32_t a) {
    return (int32_t)(uintptr_t)GetStockObject(a);
}
int32_t __stdcall vb6_di_SelectObject(int32_t a, int32_t b) {
    return (int32_t)(uintptr_t)SelectObject((HDC)(uintptr_t)a, (HGDIOBJ)(uintptr_t)b);
}
int32_t __stdcall vb6_di_DeleteObject(int32_t a) {
    return (int32_t)DeleteObject((HGDIOBJ)(uintptr_t)a);
}
int32_t __stdcall vb6_di_CreateSolidBrush(int32_t a) {
    return (int32_t)(uintptr_t)CreateSolidBrush((COLORREF)(uintptr_t)a);
}
int32_t __stdcall vb6_di_CreateCompatibleDC(int32_t a) {
    return (int32_t)(uintptr_t)CreateCompatibleDC((HDC)(uintptr_t)a);
}
int32_t __stdcall vb6_di_DeleteDC(int32_t a) {
    return (int32_t)DeleteDC((HDC)(uintptr_t)a);
}
int32_t __stdcall vb6_di_CreateDIBSection(int32_t a, void* b, int32_t c, int32_t* d, int32_t e, int32_t f) {
    return (int32_t)(uintptr_t)CreateDIBSection((HDC)(uintptr_t)a, (BITMAPINFO*)b, c, (void**)d, (HANDLE)(uintptr_t)e, (DWORD)f);
}
int32_t __stdcall vb6_di_SetStretchBltMode(int32_t a, int32_t b) {
    return (int32_t)SetStretchBltMode((HDC)(uintptr_t)a, b);
}
int32_t __stdcall vb6_di_StretchBlt(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f, int32_t g, int32_t h, int32_t i, int32_t j, int32_t k) {
    StretchBlt((HDC)(uintptr_t)a, b, c, d, e, (HDC)(uintptr_t)f, g, h, i, j, k);
    return 1;
}
int32_t __stdcall vb6_di_GetDeviceCaps(int32_t a, int32_t b) {
    return (int32_t)GetDeviceCaps((HDC)(uintptr_t)a, b);
}
int32_t __stdcall vb6_di_PolyPolygon(int32_t a, void* b, void* c, int32_t d) {
    return (int32_t)PolyPolygon((HDC)(uintptr_t)a, (const POINT*)b, (const INT*)c, d);
}
int32_t __stdcall vb6_di_SetMapMode(int32_t a, int32_t b) {
    return (int32_t)SetMapMode((HDC)(uintptr_t)a, b);
}

/* USER32 forwarding stubs */
int32_t __stdcall vb6_di_FillRect(int32_t a, RECT* b, int32_t c) {
    return (int32_t)FillRect((HDC)(uintptr_t)a, b, (HBRUSH)(uintptr_t)c);
}

/* KERNEL32 forwarding stubs */
int32_t __stdcall vb6_di_WideCharToMultiByte(int32_t a, int32_t b, void* c, int32_t d, void* e, int32_t f, int32_t g, int32_t h) {
    return (int32_t)WideCharToMultiByte((UINT)a, (DWORD)b, (LPCWSTR)c, d, (LPSTR)e, f, (LPCSTR)(uintptr_t)g, (LPBOOL)(uintptr_t)h);
}

/* OLEAUT32 forwarding stubs */
int32_t __stdcall vb6_di_OleCreatePictureIndirect(void* desc, void* riid, int32_t fOwn, void** ppvObj) {
    return (int32_t)OleCreatePictureIndirect((PICTDESC*)desc, (REFIID)riid, (BOOL)fOwn, ppvObj);
}
int32_t __stdcall vb6_di_DispCallFunc(void* pvInstance, int32_t oVft, int32_t cc, int32_t vtReturn, int32_t cArgs, int32_t* prgvt, void** prgpvArg, void* pvargResult) {
    return (int32_t)DispCallFunc(pvInstance, oVft, (CALLCONV)cc, vtReturn, cArgs, (VARTYPE*)prgvt, (VARIANTARG**)prgpvArg, pvargResult);
}

/* SHLWAPI ordinal #12 forwarding stub (SHCreateMemStream) */
void* __stdcall vb6_di_ord_12(void* a, int32_t b) {
    typedef void* (WINAPI *fnSHCreateMemStream)(void*, int32_t);
    static fnSHCreateMemStream pfn = NULL;
    if (!pfn) {
        HMODULE h = GetModuleHandleW(L"shlwapi.dll");
        if (!h) h = LoadLibraryW(L"shlwapi.dll");
        if (h) pfn = (fnSHCreateMemStream)GetProcAddress(h, (LPCSTR)12);
    }
    if (pfn) return pfn(a, b);
    return NULL;
}
