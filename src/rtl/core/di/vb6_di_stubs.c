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

// Use static CRT to match the generated code linkage
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

/* P6.4c: BalloonTooltips 需要的 user32 转发 (声明省略 LPARAM 名字, 按 C3 生成的
 * intptr_t 签名逐一转发). Fix 082f: 与生成头文件 vb6_di_ 原型逐参一致. */
intptr_t __stdcall vb6_di_CreateWindowExW(intptr_t dwExStyle, intptr_t lpClassName, intptr_t lpWindowName, intptr_t dwStyle, intptr_t X, intptr_t Y, intptr_t nWidth, intptr_t nHeight, intptr_t hWndParent, intptr_t hMenu, intptr_t hInstance, void* lpParam) {
    return (intptr_t)(uintptr_t)CreateWindowExW((DWORD)dwExStyle, (LPCWSTR)(uintptr_t)lpClassName, (LPCWSTR)(uintptr_t)lpWindowName, (DWORD)dwStyle, (int)X, (int)Y, (int)nWidth, (int)nHeight, (HWND)(uintptr_t)hWndParent, (HMENU)(uintptr_t)hMenu, (HINSTANCE)(uintptr_t)hInstance, lpParam);
}
intptr_t __stdcall vb6_di_GetWindowLongW(intptr_t hWnd, intptr_t nIndex) {
    return (intptr_t)(uintptr_t)GetWindowLongW((HWND)(uintptr_t)hWnd, (int)nIndex);
}
intptr_t __stdcall vb6_di_TrackMouseEvent(intptr_t lpEventTrack) {
    return (intptr_t)TrackMouseEvent((LPTRACKMOUSEEVENT)(uintptr_t)lpEventTrack);
}
/* SetWindowTheme 在 uxtheme.dll (无 user32 导出), 动态解析避免引入 uxtheme.lib. */
intptr_t __stdcall vb6_di_SetWindowTheme(intptr_t hWnd, intptr_t pszSubAppName, intptr_t pszSubIdList) {
    typedef intptr_t(WINAPI* fnSetWindowTheme)(intptr_t, intptr_t, intptr_t);
    static fnSetWindowTheme pfn = NULL;
    if (!pfn) {
        HMODULE h = GetModuleHandleW(L"uxtheme.dll");
        if (!h) h = LoadLibraryW(L"uxtheme.dll");
        if (h) pfn = (fnSetWindowTheme)GetProcAddress(h, "SetWindowTheme");
    }
    if (pfn) return pfn(hWnd, pszSubAppName, pszSubIdList);
    return 0;
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

/* Fix 093a: COMCTL32 ordinal #413 forwarding stub (DefSubclassProc).
 * VB6: Declare Function DefSubclassProc Lib "comctl32" Alias "#413" (...)
 * The SDK's comctl32.lib does not export DefSubclassProc by name, so resolve the
 * ordinal at first use (falling back to the named export on newer comctl32). */
intptr_t __stdcall vb6_di_ord_413(intptr_t hWnd, intptr_t wMsg, intptr_t wParam, intptr_t lParam) {
    typedef intptr_t (WINAPI *fnDefSubclassProc)(intptr_t, intptr_t, intptr_t, intptr_t);
    static fnDefSubclassProc pfn = NULL;
    if (!pfn) {
        HMODULE h = GetModuleHandleW(L"comctl32.dll");
        if (!h) h = LoadLibraryW(L"comctl32.dll");
        if (h) {
            pfn = (fnDefSubclassProc)GetProcAddress(h, (LPCSTR)413);
            if (!pfn) pfn = (fnDefSubclassProc)GetProcAddress(h, "DefSubclassProc");
        }
    }
    if (pfn) return pfn(hWnd, wMsg, wParam, lParam);
    return 0;
}

/* P6.4c: COMCTL32 ordinal #410/#411/#412 (SetWindowSubclass/GetWindowSubclass/
 * RemoveWindowSubclass). 同类序数优先 + 导出名兜底的动态解析. */
intptr_t __stdcall vb6_di_ord_410(intptr_t hWnd, intptr_t pfnSubclass, intptr_t uIdSubclass, intptr_t dwRefData) {
    typedef intptr_t (WINAPI *fnSetWindowSubclass)(intptr_t, intptr_t, intptr_t, intptr_t);
    static fnSetWindowSubclass pfn = NULL;
    if (!pfn) {
        HMODULE h = GetModuleHandleW(L"comctl32.dll");
        if (!h) h = LoadLibraryW(L"comctl32.dll");
        if (h) {
            pfn = (fnSetWindowSubclass)GetProcAddress(h, (LPCSTR)410);
            if (!pfn) pfn = (fnSetWindowSubclass)GetProcAddress(h, "SetWindowSubclass");
        }
    }
    if (pfn) return pfn(hWnd, pfnSubclass, uIdSubclass, dwRefData);
    return 0;
}
intptr_t __stdcall vb6_di_ord_411(intptr_t hWnd, intptr_t pfnSubclass, intptr_t uIdSubclass, int32_t* pdwRefData) {
    typedef intptr_t (WINAPI *fnGetWindowSubclass)(intptr_t, intptr_t, intptr_t, DWORD*);
    static fnGetWindowSubclass pfn = NULL;
    if (!pfn) {
        HMODULE h = GetModuleHandleW(L"comctl32.dll");
        if (!h) h = LoadLibraryW(L"comctl32.dll");
        if (h) {
            pfn = (fnGetWindowSubclass)GetProcAddress(h, (LPCSTR)411);
            if (!pfn) pfn = (fnGetWindowSubclass)GetProcAddress(h, "GetWindowSubclass");
        }
    }
    if (pfn) return pfn(hWnd, pfnSubclass, uIdSubclass, pdwRefData);
    return 0;
}
intptr_t __stdcall vb6_di_ord_412(intptr_t hWnd, intptr_t pfnSubclass, intptr_t uIdSubclass) {
    typedef intptr_t (WINAPI *fnRemoveWindowSubclass)(intptr_t, intptr_t, intptr_t);
    static fnRemoveWindowSubclass pfn = NULL;
    if (!pfn) {
        HMODULE h = GetModuleHandleW(L"comctl32.dll");
        if (!h) h = LoadLibraryW(L"comctl32.dll");
        if (h) {
            pfn = (fnRemoveWindowSubclass)GetProcAddress(h, (LPCSTR)412);
            if (!pfn) pfn = (fnRemoveWindowSubclass)GetProcAddress(h, "RemoveWindowSubclass");
        }
    }
    if (pfn) return pfn(hWnd, pfnSubclass, uIdSubclass);
    return 0;
}

/* ============================================================
 * Fix 092z-2: msvbvm60 (VB6 运行时) 原生实现
 * ------------------------------------------------------------
 * VBMAN 源码有 10 个模块 14 处 `Declare ... Lib "msvbvm60"`, 但:
 *   1. VB6 运行时只有 32 位 (SysWOW64\msvbvm60.dll), x64 下无导入库可链;
 *   2. C3 生成的引用名是内部名 vb6_di_<name>, MSVBVM60.DLL 导出表里是
 *      VarPtr / __vbaObjSetAddref / 序号 —— 二者无法互相解析。
 * 故这些符号一律在 RTL 原生实现, 不做 LoadLibrary 转发。
 * 序号对照 (dumpbin /exports C:\Windows\SysWOW64\msvbvm60.dll):
 *   644 = VarPtr             350 = __vbaObjSetAddref
 * ============================================================ */

/* ArrPtr: `Private Declare Function ArrPtr Lib "msvbvm60" Alias "VarPtr" (Ptr() As Any) As Long`
 *
 * VB6 语义: VarPtr(x) = &x —— 返回变量自身的地址。
 * 数组 ByRef 传递时, C3 传给本函数的实参已经是数组变量指针的地址
 * (生成声明为 vb6_SafeArray1D**), 即 VB6 的 &baBuffer, 故恒等返回即正确。
 *
 * 调用点语义核对 (mdTlsThunks.bas):
 *   CopyMemory(lBufPtr, ByVal ArrPtr(baBuffer), LenB(lBufPtr))
 *       -> 读出 SAFEARRAY* (数组未分配时为 0, 代码据此判断)
 *   CopyMemory(ByVal ArrPtr(baBuffer), ByVal ArrPtr(baInput), 4)
 *       -> 交换两个 SAFEARRAY*
 * 两者都要求返回「数组变量地址」而非 SAFEARRAY* 本身, 恒等返回满足。 */
intptr_t __stdcall vb6_di_VarPtr(void* Ptr) {
    return (intptr_t)Ptr;
}

/* vbaObjSetAddref:
 *   Private Declare Function vbaObjSetAddref Lib "msvbvm60" Alias "__vbaObjSetAddref"
 *       (oDest As Any, ByVal lSrcPtr As Long) As Long
 *
 * VB6 运行时语义: if (psrc) psrc->AddRef(); *ppdst = psrc; 返回 HRESULT。
 * 旧值不在此处 Release (原版 VB6 由调用侧处理), 保持同一行为以避免误 Release
 * 非持有引用而崩溃; 代价是覆盖旧值时可能泄漏一次引用, 与原版一致。
 * 调用点: vbaObjSetAddref((void*)&(oCallback), _vb6_with_60->ClientCertCallback) */
intptr_t __stdcall vb6_di_vb6___vbaObjSetAddref(void* oDest, intptr_t lSrcPtr) {
    if (lSrcPtr) {
        IUnknown* pSrc = (IUnknown*)(uintptr_t)lSrcPtr;
        pSrc->lpVtbl->AddRef(pSrc);
    }
    *(void**)oDest = (void*)(uintptr_t)lSrcPtr;
    return 0; /* S_OK */
}

/* 序号 644 别名 (VBMAN cToolsArray.cls):
 *   Private Declare Function SplitLongToBytes Lib "msvbvm60" Alias "#644"
 *       (ByVal lngNum As Long) As longByteType
 * 序号 644 实测就是 VarPtr。x86 下 VarPtr 直接把入参(此处即 Long 的值)当返回值放回 EAX,
 * 而 VB6 对 4 字节 UDT 返回值同样取 EAX, 于是该声明成了「Long 按 4 字节位重解释」的技巧
 * (小端: a1 = 最低字节)。x64 下按值返回同布局的 4 字节结构。
 * 该符号在 VBMAN 中只声明未使用, 实现以保证链接并保持语义一致。 */
typedef struct vb6_di_longByteType {
    uint8_t a1;
    uint8_t a2;
    uint8_t a3;
    uint8_t a4;
} vb6_di_longByteType;

vb6_di_longByteType __stdcall vb6_di_ord_644(intptr_t lngNum) {
    vb6_di_longByteType r;
    uint32_t v = (uint32_t)lngNum;

    r.a1 = (uint8_t)(v & 0xFF);
    r.a2 = (uint8_t)((v >> 8) & 0xFF);
    r.a3 = (uint8_t)((v >> 16) & 0xFF);
    r.a4 = (uint8_t)((v >> 24) & 0xFF);
    return r;
}

/* cryptdlg.dll 无导入库 (Windows SDK 10.0.26100.0 实测不提供 cryptdlg.lib),
 * 故本符号走动态加载, 不生成 #pragma comment(lib, ...) (见 cgen_decl.cpp)。
 * 对应 VBMAN cTlsSocket.cls:
 *   Private Declare Function CertSelectCertificate Lib "cryptdlg"
 *       Alias "CertSelectCertificateW" (pCertSelectInfo As Any) As Long
 * 原型: BOOL WINAPI CertSelectCertificateW(PCCERT_SELECT_STRUCT_W pCertSelectInfo)
 * (dumpbin /exports C:\Windows\System32\cryptdlg.dll -> 15 CertSelectCertificateW) */
intptr_t __stdcall vb6_di_CertSelectCertificateW(void* pCertSelectInfo) {
    typedef BOOL(WINAPI* fnCertSelectCertificateW)(void*);
    static fnCertSelectCertificateW pfn = NULL;
    if (!pfn) {
        HMODULE h = GetModuleHandleW(L"cryptdlg.dll");
        if (!h) h = LoadLibraryW(L"cryptdlg.dll");
        if (h) pfn = (fnCertSelectCertificateW)GetProcAddress(h, "CertSelectCertificateW");
    }
    if (pfn) return (intptr_t)pfn(pCertSelectInfo);
    return 0;
}
