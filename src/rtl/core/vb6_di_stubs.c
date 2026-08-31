// vb6_di_stubs.c - Fix 076: Declare import forwarding stubs
// C3 generates extern declarations with vb6_di_ prefix for VB6 Declare functions.
// These stubs forward from the vb6_di_ name to the real Windows API.
// Separate compilation unit to avoid #define conflicts with generated code.
//
// Fix 081e: ByVal Long参数在x64下映射为intptr_t (8字节), 以正确传递指针/句柄。
// ByRef Long参数保持int32_t*不变。
// Fix 082f: 所有stub参数类型与C3生成的头文件声明完全一致。

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

/* GDI32 forwarding stubs */
intptr_t __stdcall vb6_di_CreateEnhMetaFileW(intptr_t a, intptr_t b, intptr_t c, intptr_t d) {
    return (intptr_t)(uintptr_t)CreateEnhMetaFileW((HDC)(uintptr_t)a, (LPCWSTR)(uintptr_t)b, (const RECT*)(uintptr_t)c, (LPCWSTR)(uintptr_t)d);
}
intptr_t __stdcall vb6_di_CloseEnhMetaFile(intptr_t a) {
    return (intptr_t)(uintptr_t)CloseEnhMetaFile((HDC)(uintptr_t)a);
}
intptr_t __stdcall vb6_di_GetStockObject(intptr_t a) {
    return (intptr_t)(uintptr_t)GetStockObject((int)a);
}
intptr_t __stdcall vb6_di_SelectObject(intptr_t a, intptr_t b) {
    return (intptr_t)(uintptr_t)SelectObject((HDC)(uintptr_t)a, (HGDIOBJ)(uintptr_t)b);
}
intptr_t __stdcall vb6_di_DeleteObject(intptr_t a) {
    return (intptr_t)DeleteObject((HGDIOBJ)(uintptr_t)a);
}
intptr_t __stdcall vb6_di_CreateSolidBrush(intptr_t a) {
    return (intptr_t)(uintptr_t)CreateSolidBrush((COLORREF)(uintptr_t)a);
}
intptr_t __stdcall vb6_di_CreateCompatibleDC(intptr_t a) {
    return (intptr_t)(uintptr_t)CreateCompatibleDC((HDC)(uintptr_t)a);
}
intptr_t __stdcall vb6_di_DeleteDC(intptr_t a) {
    return (intptr_t)DeleteDC((HDC)(uintptr_t)a);
}
intptr_t __stdcall vb6_di_CreateDIBSection(intptr_t a, void* b, intptr_t c, intptr_t* d, intptr_t e, intptr_t f) {
    return (intptr_t)(uintptr_t)CreateDIBSection((HDC)(uintptr_t)a, (BITMAPINFO*)b, (UINT)c, (void**)d, (HANDLE)(uintptr_t)e, (DWORD)f);
}
intptr_t __stdcall vb6_di_SetStretchBltMode(intptr_t a, intptr_t b) {
    return (intptr_t)SetStretchBltMode((HDC)(uintptr_t)a, (int)b);
}
intptr_t __stdcall vb6_di_StretchBlt(intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t e, intptr_t f, intptr_t g, intptr_t h, intptr_t i, intptr_t j, intptr_t k) {
    StretchBlt((HDC)(uintptr_t)a, (int)b, (int)c, (int)d, (int)e, (HDC)(uintptr_t)f, (int)g, (int)h, (int)i, (int)j, (DWORD)k);
    return 1;
}
intptr_t __stdcall vb6_di_GetDeviceCaps(intptr_t a, intptr_t b) {
    return (intptr_t)GetDeviceCaps((HDC)(uintptr_t)a, (int)b);
}
intptr_t __stdcall vb6_di_PolyPolygon(intptr_t a, void* b, void* c, intptr_t d) {
    return (intptr_t)PolyPolygon((HDC)(uintptr_t)a, (const POINT*)b, (const INT*)c, (int)d);
}
intptr_t __stdcall vb6_di_SetMapMode(intptr_t a, intptr_t b) {
    return (intptr_t)SetMapMode((HDC)(uintptr_t)a, (int)b);
}

/* USER32 forwarding stubs */
intptr_t __stdcall vb6_di_FillRect(intptr_t a, RECT* b, intptr_t c) {
    return (intptr_t)FillRect((HDC)(uintptr_t)a, b, (HBRUSH)(uintptr_t)c);
}

/* KERNEL32 forwarding stubs */
intptr_t __stdcall vb6_di_WideCharToMultiByte(intptr_t a, intptr_t b, intptr_t c, intptr_t d, void* e, intptr_t f, intptr_t g, intptr_t h) {
    return (intptr_t)WideCharToMultiByte((UINT)a, (DWORD)b, (LPCWSTR)(uintptr_t)c, (int)d, (LPSTR)(uintptr_t)e, (int)f, (LPCSTR)(uintptr_t)g, (LPBOOL)(uintptr_t)h);
}

/* OLEAUT32 forwarding stubs */
intptr_t __stdcall vb6_di_OleCreatePictureIndirect(void* desc, void* riid, intptr_t fOwn, void** ppvObj) {
    return (intptr_t)OleCreatePictureIndirect((PICTDESC*)desc, (REFIID)riid, (BOOL)fOwn, ppvObj);
}
intptr_t __stdcall vb6_di_DispCallFunc(intptr_t pvInstance, intptr_t oVft, intptr_t lCc, int32_t vtReturn, intptr_t cActuals, void* prgVt, void* prgpVarg, void* pvargResult) {
    return (intptr_t)DispCallFunc((void*)(uintptr_t)pvInstance, (LONG_PTR)oVft, (CALLCONV)lCc, vtReturn, (UINT)cActuals, (VARTYPE*)prgVt, (VARIANTARG**)prgpVarg, pvargResult);
}

/* SHLWAPI ordinal #12 forwarding stub (SHCreateMemStream) */
void* __stdcall vb6_di_ord_12(void* a, intptr_t b) {
    typedef void* (WINAPI *fnSHCreateMemStream)(void*, int32_t);
    static fnSHCreateMemStream pfn = NULL;
    if (!pfn) {
        HMODULE h = GetModuleHandleW(L"shlwapi.dll");
        if (!h) h = LoadLibraryW(L"shlwapi.dll");
        if (h) pfn = (fnSHCreateMemStream)GetProcAddress(h, (LPCSTR)12);
    }
    if (pfn) return pfn(a, (int32_t)b);
    return NULL;
}
