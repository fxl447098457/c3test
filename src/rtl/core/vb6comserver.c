// vb6comserver.c - VB6 COM服务端运行时 (P6.6 ActiveX DLL)
// 实现IClassFactory、IDispatch包装、DLL导出骨架、注册表辅助

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#define COBJMACROS  /* P6.6.6: 启用C COM宏 (ITypeLib_Release等) */
#include "vb6comserver.h"

#ifndef CONNECT_E_NOCONNECTION
#define CONNECT_E_NOCONNECTION 0x80040200
#endif

// ANSI CLSID字符串 → GUID (MSVC不导出vb6_CLSIDFromStrA, 手动转宽字符)
static HRESULT vb6_CLSIDFromStrA(const char* str, CLSID* clsid) {
    wchar_t wbuf[64];
    MultiByteToWideChar(CP_ACP, 0, str, -1, wbuf, 64);
    return CLSIDFromString(wbuf, clsid);
}

// ============================================================
// 全局引用计数
// ============================================================

LONG g_vb6_cRef = 0;
LONG g_vb6_cServerLock = 0;

// g_vb6_coclasses 和 g_vb6_coclassCount 由cgen在DLL模式生成
// 非DLL模式下, vb6comserver.c不会被链接到EXE, 所以不需要弱引用

// ============================================================
// IUnknown / IDispatch 辅助 GUID
// ============================================================

static const IID IID_IUnknown_ = {0x00000000,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};
static const IID IID_IDispatch_ = {0x00020400,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};
static const IID IID_IClassFactory_ = {0x00000001,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};
static const IID IID_IConnectionPointContainer_ = {0xB196B284,0xBAB4,0x101A,{0xB6,0x9C,0x00,0xAA,0x00,0x34,0x1D,0x07}};
static const IID IID_IConnectionPoint_ = {0xB196B286,0xBAB4,0x101A,{0xB6,0x9C,0x00,0xAA,0x00,0x34,0x1D,0x07}};
static const IID IID_IProvideClassInfo2_ = {0x25F711BE,0x2E07,0x4E84,{0x97,0xF2,0x14,0x3E,0x55,0x82,0x9B,0x5C}};

#ifndef GUIDKIND_DEFAULT_SOURCE_DISP_IID
#define GUIDKIND_DEFAULT_SOURCE_DISP_IID 1
#endif

// ============================================================
// vb6_ComObject - IDispatch包装实现
// ============================================================

// P6.6.6: Debug helper - log QI/GetTypeInfo info to c3_com_debug.log
static void vb6_com_debug_log(const char* fmt, ...) {
    static const char* debugPath = "c3_com_debug.log";
    FILE* f = fopen(debugPath, "a");
    if (!f) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fprintf(f, "\n");
    fclose(f);
}

static void vb6_format_iid(REFIID riid, char* buf, size_t bufsz) {
    snprintf(buf, bufsz, "{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
        riid->Data1, riid->Data2, riid->Data3,
        riid->Data4[0], riid->Data4[1], riid->Data4[2], riid->Data4[3],
        riid->Data4[4], riid->Data4[5], riid->Data4[6], riid->Data4[7]);
}

// --- IUnknown ---

static HRESULT STDMETHODCALLTYPE ComObj_QueryInterface(vb6_ComObject* self, REFIID riid, void** ppv) {
    int i;
    char riidStr[64], defIidStr[64];
    vb6_format_iid(riid, riidStr, sizeof(riidStr));
    vb6_com_debug_log("QI: riid=%s progId=%s defaultIfaceIid=%s",
        riidStr,
        self->desc ? self->desc->progId : "(null)",
        (self->desc && self->desc->defaultIfaceIid) ? (vb6_format_iid(self->desc->defaultIfaceIid, defIidStr, sizeof(defIidStr)), defIidStr) : "(null)");
    if (!ppv) return E_POINTER;
    if (IsEqualIID(riid, &IID_IUnknown_) || IsEqualIID(riid, &IID_IDispatch_)) {
        *ppv = self;
        self->vtable->AddRef(self);
        vb6_com_debug_log("QI: matched IUnknown/IDispatch -> S_OK");
        return S_OK;
    }
    // P12.1: Check Implements interface IIDs
    if (self->desc && self->desc->ifaceCount > 0 && self->desc->ifaceIids) {
        for (i = 0; i < self->desc->ifaceCount; i++) {
            if (IsEqualIID(riid, self->desc->ifaceIids[i])) {
                *ppv = self;  // dispinterface: same IDispatch pointer
                self->vtable->AddRef(self);
                vb6_com_debug_log("QI: matched Implements iface[%d] -> S_OK", i);
                return S_OK;
            }
        }
    }
    // P6.6.6: Default dispinterface IID
    if (self->desc && self->desc->defaultIfaceIid && IsEqualIID(riid, self->desc->defaultIfaceIid)) {
        *ppv = self;  // dispinterface = same IDispatch pointer
        self->vtable->AddRef(self);
        vb6_com_debug_log("QI: matched defaultIfaceIid -> S_OK");
        return S_OK;
    }
    // P6.6: IConnectionPointContainer (only if coclass has events)
    if (self->desc && self->desc->sourceIfaceIid && IsEqualIID(riid, &IID_IConnectionPointContainer_)) {
        if (!self->cpc) {
            self->cpc = vb6_CPC_Create(self);
        }
        if (self->cpc) {
            *ppv = self->cpc;
            self->cpc->vtable->AddRef(self->cpc);
            vb6_com_debug_log("QI: matched IConnectionPointContainer -> S_OK");
            return S_OK;
        }
    }
    // P6.6: IProvideClassInfo2
    if (self->desc && IsEqualIID(riid, &IID_IProvideClassInfo2_)) {
        if (!self->pci) {
            self->pci = vb6_PCI_Create(self);
        }
        if (self->pci) {
            *ppv = self->pci;
            self->pci->vtable->AddRef(self->pci);
            vb6_com_debug_log("QI: matched IProvideClassInfo2 -> S_OK");
            return S_OK;
        }
    }
    *ppv = NULL;
    vb6_com_debug_log("QI: no match -> E_NOINTERFACE");
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE ComObj_AddRef(vb6_ComObject* self) {
    ULONG count = InterlockedIncrement(&self->refCount);
    InterlockedIncrement(&g_vb6_cRef);
    return count;
}

static ULONG STDMETHODCALLTYPE ComObj_Release(vb6_ComObject* self) {
    ULONG count = InterlockedDecrement(&self->refCount);
    InterlockedDecrement(&g_vb6_cRef);
    if (count == 0) {
        // 调用VB6 Class_Terminate + Destroy
        if (self->desc && self->desc->destroyFunc && self->vb6Instance) {
            self->desc->destroyFunc(self->vb6Instance);
        }
        self->vb6Instance = NULL;
        // P6.6: Release PCI
        if (self->pci) {
            self->pci->vtable->Release(self->pci);
            self->pci = NULL;
        }
        // P6.6: Release CPC
        if (self->cpc) {
            self->cpc->vtable->Release(self->cpc);
            self->cpc = NULL;
        }
        CoTaskMemFree(self);
    }
    return count;
}

// --- IDispatch (简化实现: 无TypeLib, GetIDsOfNames线性搜索) ---

static HRESULT STDMETHODCALLTYPE ComObj_GetTypeInfoCount(vb6_ComObject* self, UINT* pctinfo) {
    if (!pctinfo) return E_POINTER;
    *pctinfo = 1;  // 提供TypeLib信息 (早绑定需要)
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE ComObj_GetTypeInfo(vb6_ComObject* self, UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo) {
    char clsidStrBuf[64] = {0};
    if (!ppTInfo) return E_POINTER;
    if (iTInfo != 0) return DISP_E_BADINDEX;
    *ppTInfo = NULL;
    if (!self->desc) return E_FAIL;
    vb6_com_debug_log("GetTypeInfo: progId=%s clsidStr=%s", self->desc->progId, self->desc->clsidStr ? self->desc->clsidStr : "(null)");
    // 获取DLL路径, 从嵌入资源加载TypeLib
    wchar_t dllPath[MAX_PATH];
    HMODULE hMod = NULL;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)&g_vb6_cRef, &hMod)) {
        vb6_com_debug_log("GetTypeInfo: GetModuleHandleExW failed -> E_FAIL");
        return E_FAIL;
    }
    GetModuleFileNameW(hMod, dllPath, MAX_PATH);
    ITypeLib* pTypeLib = NULL;
    HRESULT hr = LoadTypeLib(dllPath, &pTypeLib);
    if (FAILED(hr) || !pTypeLib) {
        vb6_com_debug_log("GetTypeInfo: LoadTypeLib failed hr=0x%08lX -> E_FAIL", hr);
        return E_FAIL;
    }
    vb6_com_debug_log("GetTypeInfo: LoadTypeLib succeeded");
    // 通过CLSID查找coclass的ITypeInfo
    CLSID clsid;
    hr = vb6_CLSIDFromStrA(self->desc->clsidStr, &clsid);
    if (FAILED(hr)) {
        vb6_com_debug_log("GetTypeInfo: CLSIDFromStr failed hr=0x%08lX", hr);
        ITypeLib_Release(pTypeLib);
        return E_FAIL;
    }
    vb6_format_iid(&clsid, clsidStrBuf, sizeof(clsidStrBuf));
    vb6_com_debug_log("GetTypeInfo: looking up CLSID=%s", clsidStrBuf);
    hr = ITypeLib_GetTypeInfoOfGuid(pTypeLib, &clsid, ppTInfo);
    vb6_com_debug_log("GetTypeInfo: GetTypeInfoOfGuid hr=0x%08lX", hr);
    ITypeLib_Release(pTypeLib);
    return hr;
}

static HRESULT STDMETHODCALLTYPE ComObj_GetIDsOfNames(vb6_ComObject* self, REFIID riid, 
    LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgDispId) 
{
    if (!self->desc || !self->desc->methods) return DISP_E_UNKNOWNNAME;
    
    for (UINT n = 0; n < cNames; n++) {
        rgDispId[n] = DISPID_UNKNOWN;
        for (int i = 0; i < self->desc->methodCount; i++) {
            if (wcscmp(rgszNames[n], self->desc->methods[i].name) == 0) {
                rgDispId[n] = self->desc->methods[i].dispid;
                break;
            }
        }
    }
    
    return (cNames > 0 && rgDispId[0] != DISPID_UNKNOWN) ? S_OK : DISP_E_UNKNOWNNAME;
}

static HRESULT STDMETHODCALLTYPE ComObj_Invoke(vb6_ComObject* self, DISPID dispIdMember, 
    REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pDispParams, 
    VARIANT* pVarResult, EXCEPINFO* pExcepInfo, UINT* puArgErr)
{
    if (!self->desc || !self->desc->methods) return DISP_E_MEMBERNOTFOUND;
    
    // Find method matching dispid AND invkind (same dispid may have Get/Let variants)
    const vb6_DispMethodDesc* method = NULL;
    for (int i = 0; i < self->desc->methodCount; i++) {
        if (self->desc->methods[i].dispid == dispIdMember) {
            int ik = self->desc->methods[i].invkind;
            int ok = 0;
            if (ik == 1 && (wFlags & DISPATCH_METHOD)) ok = 1;
            else if (ik == 2 && (wFlags & DISPATCH_PROPERTYGET)) ok = 1;
            else if ((ik == 4 || ik == 8) && (wFlags & DISPATCH_PROPERTYPUT)) ok = 1;
            if (ok) { method = &self->desc->methods[i]; break; }
        }
    }
    // Fallback: if no invkind match, try first dispid match
    if (!method) {
        for (int i = 0; i < self->desc->methodCount; i++) {
            if (self->desc->methods[i].dispid == dispIdMember) {
                method = &self->desc->methods[i];
                break;
            }
        }
    }
    if (!method) return DISP_E_MEMBERNOTFOUND;
    
    // 收集参数
    // 注意: VBScript等脚本引擎传入的VARIANT可能是VT_I2等类型,
    // 而桥接函数期望VT_I4(via lVal)。我们先将每个参数强制转为VT_I4。
    int argc = pDispParams ? (int)pDispParams->cArgs : 0;
    void** args = NULL;
    VARIANT* coercedArgs = NULL;  // 强制转换后的参数副本
    
    if (argc > 0) {
        args = (void**)CoTaskMemAlloc(argc * sizeof(void*));
        coercedArgs = (VARIANT*)CoTaskMemAlloc(argc * sizeof(VARIANT));
        if (!args || !coercedArgs) {
            if (args) CoTaskMemFree(args);
            if (coercedArgs) CoTaskMemFree(coercedArgs);
            return E_OUTOFMEMORY;
        }
        // DISPPARAMS参数是逆序的
        for (int i = 0; i < argc; i++) {
            VARIANT* src = &pDispParams->rgvarg[argc - 1 - i];
            // 对数值类型强制转为VT_I4 (桥接函数统一使用lVal读取)
            if (src->vt == VT_I2 || src->vt == VT_I1 || src->vt == VT_UI1 ||
                src->vt == VT_UI2 || src->vt == VT_BOOL || src->vt == VT_EMPTY) {
                VariantInit(&coercedArgs[i]);
                HRESULT hr2 = VariantChangeType(&coercedArgs[i], src, 0, VT_I4);
                if (SUCCEEDED(hr2)) {
                    args[i] = &coercedArgs[i];
                } else {
                    args[i] = src;  // fallback to original
                }
            } else if (src->vt == VT_R4) {
                // Float -> Double for dblVal access
                VariantInit(&coercedArgs[i]);
                HRESULT hr2 = VariantChangeType(&coercedArgs[i], src, 0, VT_R8);
                if (SUCCEEDED(hr2)) {
                    args[i] = &coercedArgs[i];
                } else {
                    args[i] = src;
                }
            } else {
                args[i] = src;  // VT_I4, VT_R8, VT_BSTR, etc. pass through
            }
        }
    }
    
    // 调用VB6方法
    if (method->invokeFunc) {
        method->invokeFunc(self->vb6Instance, args, argc, pVarResult);
    }
    
    if (coercedArgs) {
        for (int i = 0; i < argc; i++) VariantClear(&coercedArgs[i]);
        CoTaskMemFree(coercedArgs);
    }
    if (args) CoTaskMemFree(args);
    return S_OK;
}

// IDispatch vtable 实例
static const vb6_IDispatchVtable g_ComObjectVtable = {
    ComObj_QueryInterface,
    ComObj_AddRef,
    ComObj_Release,
    ComObj_GetTypeInfoCount,
    ComObj_GetTypeInfo,
    ComObj_GetIDsOfNames,
    ComObj_Invoke,
};

// 创建VB6 COM对象
vb6_ComObject* vb6_ComObject_Create(const vb6_CoClassDesc* desc) {
    if (!desc || !desc->factoryFunc) return NULL;
    
    vb6_ComObject* obj = (vb6_ComObject*)CoTaskMemAlloc(sizeof(vb6_ComObject));
    if (!obj) return NULL;
    
    obj->vtable = &g_ComObjectVtable;
    obj->refCount = 1;
    obj->desc = desc;
    obj->vb6Instance = desc->factoryFunc();  // 调用 vb6_cls_<Name>_New()
    obj->cpc = NULL;  // P6.6: lazy init CPC
    obj->pci = NULL;  // P6.6: lazy init PCI
    // P6.6.3: Set back-pointer for event support (first field of VB6 class struct = __comObj)
    if (obj->vb6Instance) {
        void** ppComObj = (void**)obj->vb6Instance;
        *ppComObj = obj;
    }
    
    if (!obj->vb6Instance) {
        CoTaskMemFree(obj);
        return NULL;
    }
    
    InterlockedIncrement(&g_vb6_cRef);
    return obj;
}

// ============================================================
// ClassFactory 实现
// ============================================================

// --- IUnknown ---

static HRESULT STDMETHODCALLTYPE CF_QueryInterface(vb6_ClassFactory* self, REFIID riid, void** ppv) {
    int i;
    if (!ppv) return E_POINTER;
    if (IsEqualIID(riid, &IID_IUnknown_) || IsEqualIID(riid, &IID_IClassFactory_)) {
        *ppv = self;
        self->vtable->AddRef(self);
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE CF_AddRef(vb6_ClassFactory* self) {
    return InterlockedIncrement(&self->refCount);
}

static ULONG STDMETHODCALLTYPE CF_Release(vb6_ClassFactory* self) {
    ULONG count = InterlockedDecrement(&self->refCount);
    if (count == 0) {
        CoTaskMemFree(self);
    }
    return count;
}

// --- IClassFactory ---

static HRESULT STDMETHODCALLTYPE CF_CreateInstance(vb6_ClassFactory* self, IUnknown* pUnkOuter, 
    REFIID riid, void** ppv) 
{
    if (!ppv) return E_POINTER;
    if (pUnkOuter) return CLASS_E_NOAGGREGATION;
    
    // 创建VB6 COM对象
    vb6_ComObject* obj = vb6_ComObject_Create(self->desc);
    if (!obj) return E_OUTOFMEMORY;
    
    // QI请求的接口
    HRESULT hr = obj->vtable->QueryInterface(obj, riid, ppv);
    obj->vtable->Release(obj);  // QI已经AddRef, Release初始引用
    return hr;
}

static HRESULT STDMETHODCALLTYPE CF_LockServer(vb6_ClassFactory* self, BOOL fLock) {
    if (fLock) {
        InterlockedIncrement(&g_vb6_cServerLock);
    } else {
        InterlockedDecrement(&g_vb6_cServerLock);
    }
    return S_OK;
}

// IClassFactory vtable 实例
static const vb6_IClassFactoryVtable g_ClassFactoryVtable = {
    CF_QueryInterface,
    CF_AddRef,
    CF_Release,
    CF_CreateInstance,
    CF_LockServer,
};

// 获取指定CLSID的ClassFactory
HRESULT vb6_GetClassFactory(REFCLSID rclsid, REFIID riid, void** ppv, 
                            const vb6_CoClassDesc* coclasses, int count) {
    if (!ppv) return E_POINTER;
    *ppv = NULL;
    
    // 查找匹配的coclass
    const vb6_CoClassDesc* found = NULL;
    for (int i = 0; i < count; i++) {
        CLSID clsid;
        if (SUCCEEDED(vb6_CLSIDFromStrA((LPCSTR)coclasses[i].clsidStr, &clsid))) {
            if (IsEqualCLSID(rclsid, &clsid)) {
                found = &coclasses[i];
                break;
            }
        }
    }
    
    if (!found) return CLASS_E_CLASSNOTAVAILABLE;
    
    // 创建ClassFactory
    vb6_ClassFactory* cf = (vb6_ClassFactory*)CoTaskMemAlloc(sizeof(vb6_ClassFactory));
    if (!cf) return E_OUTOFMEMORY;
    
    cf->vtable = &g_ClassFactoryVtable;
    cf->refCount = 1;
    cf->desc = found;
    
    // QI请求的接口
    HRESULT hr = cf->vtable->QueryInterface(cf, riid, ppv);
    cf->vtable->Release(cf);  // QI已AddRef
    return hr;
}

// ============================================================
// DllCanUnloadNow
// ============================================================

HRESULT vb6_DllCanUnloadNow(void) {
    if (g_vb6_cRef == 0 && g_vb6_cServerLock == 0) {
        return S_OK;
    }
    return S_FALSE;
}

// ============================================================
// 注册表辅助
// ============================================================

HRESULT vb6_RegisterCoClass(const vb6_CoClassDesc* desc, const wchar_t* dllPath) {
    if (!desc || !dllPath) return E_POINTER;
    
    // 字符串CLSID → GUID
    CLSID clsid;
    HRESULT hr = vb6_CLSIDFromStrA((LPCSTR)desc->clsidStr, &clsid);
    if (FAILED(hr)) return hr;
    
    // 由CLSID生成注册表键路径
    wchar_t clsidStr[64];
    StringFromGUID2(&clsid, clsidStr, 64);
    
    wchar_t keyPath[512];
    HKEY hKey;
    LONG ret;
    
    // 1. CLSID\{xxxxxxxx-...}
    swprintf(keyPath, 512, L"CLSID\\%s", clsidStr);
    ret = RegCreateKeyExW(HKEY_CLASSES_ROOT, keyPath, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL);
    if (ret != ERROR_SUCCESS) return E_FAIL;
    // 默认值=ProgID
    if (desc->progId) {
        wchar_t progIdW[256];
        MultiByteToWideChar(CP_ACP, 0, desc->progId, -1, progIdW, 256);
        RegSetValueExW(hKey, NULL, 0, REG_SZ, (BYTE*)progIdW, (DWORD)(wcslen(progIdW)+1)*2);
    }
    RegCloseKey(hKey);
    
    // 2. CLSID\{...}\InprocServer32
    swprintf(keyPath, 512, L"CLSID\\%s\\InprocServer32", clsidStr);
    ret = RegCreateKeyExW(HKEY_CLASSES_ROOT, keyPath, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL);
    if (ret != ERROR_SUCCESS) return E_FAIL;
    RegSetValueExW(hKey, NULL, 0, REG_SZ, (BYTE*)dllPath, (DWORD)(wcslen(dllPath)+1)*2);
    // ThreadingModel = Apartment
    const wchar_t* threading = L"Apartment";
    RegSetValueExW(hKey, L"ThreadingModel", 0, REG_SZ, (BYTE*)threading, (DWORD)(wcslen(threading)+1)*2);
    RegCloseKey(hKey);
    
    // 3. ProgID注册
    if (desc->progId) {
        wchar_t progIdW[256];
        MultiByteToWideChar(CP_ACP, 0, desc->progId, -1, progIdW, 256);
        
        // ProgID键
        ret = RegCreateKeyExW(HKEY_CLASSES_ROOT, progIdW, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL);
        if (ret != ERROR_SUCCESS) return E_FAIL;
        RegSetValueExW(hKey, NULL, 0, REG_SZ, (BYTE*)clsidStr, (DWORD)(wcslen(clsidStr)+1)*2);
        RegCloseKey(hKey);
        
        // ProgID\CLSID
        swprintf(keyPath, 512, L"%s\\CLSID", progIdW);
        ret = RegCreateKeyExW(HKEY_CLASSES_ROOT, keyPath, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL);
        if (ret != ERROR_SUCCESS) return E_FAIL;
        RegSetValueExW(hKey, NULL, 0, REG_SZ, (BYTE*)clsidStr, (DWORD)(wcslen(clsidStr)+1)*2);
        RegCloseKey(hKey);
    }
    
    return S_OK;
}

HRESULT vb6_UnregisterCoClass(const vb6_CoClassDesc* desc) {
    if (!desc) return E_POINTER;
    
    CLSID clsid;
    HRESULT hr = vb6_CLSIDFromStrA((LPCSTR)desc->clsidStr, &clsid);
    if (FAILED(hr)) return hr;
    
    wchar_t clsidStr[64];
    StringFromGUID2(&clsid, clsidStr, 64);
    
    wchar_t keyPath[512];
    
    // 删除CLSID子键
    swprintf(keyPath, 512, L"CLSID\\%s\\InprocServer32", clsidStr);
    RegDeleteTreeW(HKEY_CLASSES_ROOT, keyPath);
    swprintf(keyPath, 512, L"CLSID\\%s", clsidStr);
    RegDeleteTreeW(HKEY_CLASSES_ROOT, keyPath);
    
    // 删除ProgID
    if (desc->progId) {
        wchar_t progIdW[256];
        MultiByteToWideChar(CP_ACP, 0, desc->progId, -1, progIdW, 256);
        swprintf(keyPath, 512, L"%s\\CLSID", progIdW);
        RegDeleteTreeW(HKEY_CLASSES_ROOT, keyPath);
        RegDeleteTreeW(HKEY_CLASSES_ROOT, progIdW);
    }
    
    return S_OK;
}

HRESULT vb6_GetDllPath(wchar_t* path, DWORD size) {
    HMODULE hModule = NULL;
    // 获取当前DLL模块句柄
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)&vb6_GetDllPath, &hModule)) {
        return E_FAIL;
    }
    DWORD len = GetModuleFileNameW(hModule, path, size);
    if (len == 0 || len >= size) return E_FAIL;
    return S_OK;
}

// ============================================================
// P6.13: TypeLib注册辅助
// ============================================================

HRESULT vb6_RegisterTypeLib(const wchar_t* dllPath) {
    if (!dllPath) return E_POINTER;
    
    // 从DLL资源加载TypeLib (资源类型=TYPELIB, ID=1)
    ITypeLib* pTypeLib = NULL;
    HRESULT hr = LoadTypeLib(dllPath, &pTypeLib);
    if (FAILED(hr)) {
        // TypeLib资源可能不存在, 不算致命错误
        return S_FALSE;
    }
    
    // 注册TypeLib到注册表
    hr = RegisterTypeLib(pTypeLib, (OLECHAR*)dllPath, NULL);
    if (SUCCEEDED(hr)) {
        // P6.6: Also add TypeLib subkey to each coclass CLSID entry
        // RegisterTypeLib may not add this if the CLSID entry already exists
        TLIBATTR* pAttr = NULL;
        hr = pTypeLib->lpVtbl->GetLibAttr(pTypeLib, &pAttr);
        if (SUCCEEDED(hr) && pAttr) {
            UINT typeCount = pTypeLib->lpVtbl->GetTypeInfoCount(pTypeLib);
            for (UINT i = 0; i < typeCount; i++) {
                TYPEKIND tk;
                pTypeLib->lpVtbl->GetTypeInfoType(pTypeLib, i, &tk);
                if (tk == TKIND_COCLASS) {
                    ITypeInfo* pInfo = NULL;
                    if (SUCCEEDED(pTypeLib->lpVtbl->GetTypeInfo(pTypeLib, i, &pInfo))) {
                        TYPEATTR* pTA = NULL;
                        if (SUCCEEDED(pInfo->lpVtbl->GetTypeAttr(pInfo, &pTA))) {
                            // Add CLSID\{clsid}\TypeLib = {LibID}
                            wchar_t clsidStr[64];
                            StringFromGUID2(&pTA->guid, clsidStr, 64);
                            wchar_t libidStr[64];
                            StringFromGUID2(&pAttr->guid, libidStr, 64);
                            wchar_t keyPath[512];
                            HKEY hKey;
                            swprintf(keyPath, 512, L"CLSID\\%s\\TypeLib", clsidStr);
                            if (RegCreateKeyExW(HKEY_CLASSES_ROOT, keyPath, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
                                RegSetValueExW(hKey, NULL, 0, REG_SZ, (BYTE*)libidStr, (DWORD)(wcslen(libidStr)+1)*2);
                                RegCloseKey(hKey);
                            }
                            pInfo->lpVtbl->ReleaseTypeAttr(pInfo, pTA);
                        }
                        pInfo->lpVtbl->Release(pInfo);
                    }
                }
            }
            pTypeLib->lpVtbl->ReleaseTLibAttr(pTypeLib, pAttr);
        }
    }
    ITypeLib_Release(pTypeLib);
    return hr;
}

HRESULT vb6_UnregisterTypeLib(const wchar_t* dllPath) {
    if (!dllPath) return E_POINTER;
    
    // 先加载TypeLib获取LibID和版本号
    ITypeLib* pTypeLib = NULL;
    HRESULT hr = LoadTypeLib(dllPath, &pTypeLib);
    if (FAILED(hr)) return S_FALSE;
    
    // 获取TypeLib属性
    TLIBATTR* pAttr = NULL;
    hr = pTypeLib->lpVtbl->GetLibAttr(pTypeLib, &pAttr);
    if (SUCCEEDED(hr) && pAttr) {
        // 反注册TypeLib
        UnRegisterTypeLib(&pAttr->guid, pAttr->wMajorVerNum, pAttr->wMinorVerNum,
                         SYS_WIN64, pAttr->lcid);
        pTypeLib->lpVtbl->ReleaseTLibAttr(pTypeLib, pAttr);
    }
    ITypeLib_Release(pTypeLib);
    
    return S_OK;
}


// ============================================================
// P6.6: IProvideClassInfo2 实现
// ============================================================

// Helper: Load ITypeInfo for this coclass from the registered TypeLib
static HRESULT vb6_LoadCoClassTypeInfo(const vb6_CoClassDesc* desc, ITypeInfo** ppTypeInfo) {
    HKEY hKey;
    LONG ret;
    wchar_t dllPath[MAX_PATH];
    DWORD sz;
    ITypeLib* pTypeLib;
    CLSID clsid;
    HRESULT hr;
    UINT count, i;
    
    if (!desc || !ppTypeInfo) return E_POINTER;
    *ppTypeInfo = NULL;
    
    // Parse CLSID
    hr = vb6_CLSIDFromStrA(desc->clsidStr, &clsid);
    if (FAILED(hr)) return hr;
    
    // Get DLL path from CLSID\InprocServer32
    {
        wchar_t clsidStr[64];
        wchar_t keyPath[256];
        StringFromGUID2(&clsid, clsidStr, 64);
        swprintf(keyPath, 256, L"CLSID\\%s\\InprocServer32", clsidStr);
        hKey = NULL;
        ret = RegOpenKeyExW(HKEY_CLASSES_ROOT, keyPath, 0, KEY_READ, &hKey);
        if (ret != ERROR_SUCCESS) return TYPE_E_REGISTRYACCESS;
        sz = sizeof(dllPath);
        ret = RegQueryValueExW(hKey, NULL, NULL, NULL, (LPBYTE)dllPath, &sz);
        RegCloseKey(hKey);
        if (ret != ERROR_SUCCESS) return TYPE_E_REGISTRYACCESS;
    }
    
    // Load TypeLib directly from the DLL file
    hr = LoadTypeLib(dllPath, &pTypeLib);
    if (FAILED(hr)) return hr;
    
    // Find the coclass ITypeInfo by CLSID
    count = pTypeLib->lpVtbl->GetTypeInfoCount(pTypeLib);
    for (i = 0; i < count; i++) {
        ITypeInfo* pInfo = NULL;
        hr = pTypeLib->lpVtbl->GetTypeInfo(pTypeLib, i, &pInfo);
        if (FAILED(hr)) continue;
        
        TYPEATTR* pAttr = NULL;
        hr = pInfo->lpVtbl->GetTypeAttr(pInfo, &pAttr);
        if (SUCCEEDED(hr) && pAttr) {
            if (pAttr->typekind == TKIND_COCLASS && IsEqualIID(&pAttr->guid, &clsid)) {
                *ppTypeInfo = pInfo;
                pInfo->lpVtbl->ReleaseTypeAttr(pInfo, pAttr);
                ITypeLib_Release(pTypeLib);
                return S_OK;
            }
            pInfo->lpVtbl->ReleaseTypeAttr(pInfo, pAttr);
        }
        pInfo->lpVtbl->Release(pInfo);
    }
    
    ITypeLib_Release(pTypeLib);
    return TYPE_E_ELEMENTNOTFOUND;
}
// ============================================================
// P6.6: IConnectionPointContainer + IConnectionPoint 实现
// ============================================================

// --- IConnectionPoint methods ---

static HRESULT STDMETHODCALLTYPE CP_QueryInterface(vb6_ConnectionPoint* self, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (IsEqualIID(riid, &IID_IUnknown_) || IsEqualIID(riid, &IID_IConnectionPoint_)) {
        *ppv = self;
        self->vtable->AddRef(self);
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE CP_AddRef(vb6_ConnectionPoint* self) {
    return InterlockedIncrement(&self->refCount);
}

static ULONG STDMETHODCALLTYPE CP_Release(vb6_ConnectionPoint* self) {
    ULONG c = InterlockedDecrement(&self->refCount);
    if (c == 0) {
        int i;
        for (i = 0; i < self->connCount; i++) {
            if (self->sinks[i]) self->sinks[i]->lpVtbl->Release(self->sinks[i]);
        }
        CoTaskMemFree(self->cookies);
        CoTaskMemFree(self->sinks);
        CoTaskMemFree(self);
    }
    return c;
}

static HRESULT STDMETHODCALLTYPE CP_GetConnectionInterface(vb6_ConnectionPoint* self, IID* pIID) {
    if (!pIID) return E_POINTER;
    *pIID = self->sourceIid;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE CP_GetConnectionPointContainer(vb6_ConnectionPoint* self, void** ppCPC) {
    if (!ppCPC) return E_POINTER;
    if (!self->container) return E_FAIL;
    *ppCPC = self->container;
    self->container->vtable->AddRef(self->container);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE CP_Advise(vb6_ConnectionPoint* self, IUnknown* pUnkSink, DWORD* pdwCookie) {
    int i;
    if (!pUnkSink || !pdwCookie) return E_POINTER;
    *pdwCookie = 0;
    // Grow arrays if needed
    if (self->connCount >= self->connCapacity) {
        int newCap = self->connCapacity ? self->connCapacity * 2 : 4;
        DWORD* newCookies = (DWORD*)CoTaskMemRealloc(self->cookies, newCap * sizeof(DWORD));
        IUnknown** newSinks = (IUnknown**)CoTaskMemRealloc(self->sinks, newCap * sizeof(IUnknown*));
        if (!newCookies || !newSinks) return E_OUTOFMEMORY;
        self->cookies = newCookies;
        self->sinks = newSinks;
        self->connCapacity = newCap;
    }
    i = self->connCount++;
    self->cookies[i] = ++self->nextCookie;
    pUnkSink->lpVtbl->AddRef(pUnkSink);
    self->sinks[i] = pUnkSink;
    *pdwCookie = self->cookies[i];
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE CP_Unadvise(vb6_ConnectionPoint* self, DWORD dwCookie) {
    int i;
    if (dwCookie == 0) return E_POINTER;
    for (i = 0; i < self->connCount; i++) {
        if (self->cookies[i] == dwCookie) {
            if (self->sinks[i]) {
                self->sinks[i]->lpVtbl->Release(self->sinks[i]);
                self->sinks[i] = NULL;
            }
            self->cookies[i] = 0;
            return S_OK;
        }
    }
    return CONNECT_E_NOCONNECTION;
}

static HRESULT STDMETHODCALLTYPE CP_EnumConnections(vb6_ConnectionPoint* self, void** ppEnum) {
    if (!ppEnum) return E_POINTER;
    *ppEnum = NULL;
    return E_NOTIMPL;
}

static const vb6_IConnectionPointVtable g_CPVtable = {
    CP_QueryInterface,
    CP_AddRef,
    CP_Release,
    CP_GetConnectionInterface,
    CP_GetConnectionPointContainer,
    CP_Advise,
    CP_Unadvise,
    CP_EnumConnections,
};

// --- IConnectionPointContainer methods ---

static HRESULT STDMETHODCALLTYPE CPC_QueryInterface(vb6_ConnectionPointContainer* self, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (IsEqualIID(riid, &IID_IUnknown_) || IsEqualIID(riid, &IID_IConnectionPointContainer_)) {
        *ppv = self;
        self->vtable->AddRef(self);
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE CPC_AddRef(vb6_ConnectionPointContainer* self) {
    return InterlockedIncrement(&self->refCount);
}

static ULONG STDMETHODCALLTYPE CPC_Release(vb6_ConnectionPointContainer* self) {
    ULONG c = InterlockedDecrement(&self->refCount);
    if (c == 0) {
        if (self->connPoint) self->connPoint->vtable->Release(self->connPoint);
        CoTaskMemFree(self);
    }
    return c;
}

static HRESULT STDMETHODCALLTYPE CPC_EnumConnectionPoints(vb6_ConnectionPointContainer* self, void** ppEnum) {
    if (!ppEnum) return E_POINTER;
    *ppEnum = NULL;
    return E_NOTIMPL;  // P6.6: deferred — not needed for basic event support
}

static HRESULT STDMETHODCALLTYPE CPC_FindConnectionPoint(vb6_ConnectionPointContainer* self, REFIID riid, void** ppCP) {
    if (!ppCP) return E_POINTER;
    *ppCP = NULL;
    if (!self->connPoint) return CONNECT_E_NOCONNECTION;
    // Check if requested IID matches our source interface
    if (IsEqualIID(riid, &self->connPoint->sourceIid)) {
        *ppCP = self->connPoint;
        self->connPoint->vtable->AddRef(self->connPoint);
        return S_OK;
    }
    return CONNECT_E_NOCONNECTION;
}

static const vb6_IConnectionPointContainerVtable g_CPCVtable = {
    CPC_QueryInterface,
    CPC_AddRef,
    CPC_Release,
    CPC_EnumConnectionPoints,
    CPC_FindConnectionPoint,
};


// Create ConnectionPointContainer for a COM object
vb6_ConnectionPointContainer* vb6_CPC_Create(vb6_ComObject* comObj) {
    if (!comObj || !comObj->desc || !comObj->desc->sourceIfaceIid) return NULL;
    
    vb6_ConnectionPointContainer* cpc = (vb6_ConnectionPointContainer*)CoTaskMemAlloc(sizeof(vb6_ConnectionPointContainer));
    if (!cpc) return NULL;
    
    cpc->vtable = &g_CPCVtable;
    cpc->refCount = 1;
    cpc->comObj = comObj;
    cpc->connPoint = NULL;
    
    // Create the single ConnectionPoint
    vb6_ConnectionPoint* cp = (vb6_ConnectionPoint*)CoTaskMemAlloc(sizeof(vb6_ConnectionPoint));
    if (!cp) {
        CoTaskMemFree(cpc);
        return NULL;
    }
    cp->vtable = &g_CPVtable;
    cp->refCount = 1;
    // Parse source IID from string
    {
        wchar_t wbuf[64];
        MultiByteToWideChar(CP_ACP, 0, comObj->desc->sourceIfaceIid, -1, wbuf, 64);
        CLSIDFromString(wbuf, &cp->sourceIid);
    }
    cp->container = cpc;
    cp->cookies = NULL;
    cp->sinks = NULL;
    cp->connCount = 0;
    cp->connCapacity = 0;
    cp->nextCookie = 0;
    
    cpc->connPoint = cp;
    return cpc;
}

// Fire an event to all connected sinks
void vb6_FireEvent(vb6_ComObject* comObj, int32_t dispid, VARIANT* args, int argc) {
    int i, j;
    if (!comObj || !comObj->cpc || !comObj->cpc->connPoint) return;
    vb6_ConnectionPoint* cp = comObj->cpc->connPoint;
    
    for (i = 0; i < cp->connCount; i++) {
        if (!cp->sinks[i]) continue;
        // QI for IDispatch
        IDispatch* pDisp = NULL;
        HRESULT hr = cp->sinks[i]->lpVtbl->QueryInterface(cp->sinks[i], &IID_IDispatch_, (void**)&pDisp);
        if (SUCCEEDED(hr) && pDisp) {
            DISPPARAMS dp = {0};
            VARIANT* reversedArgs = NULL;
            if (argc > 0) {
                reversedArgs = (VARIANT*)CoTaskMemAlloc(argc * sizeof(VARIANT));
                for (j = 0; j < argc; j++) {
                    VariantInit(&reversedArgs[j]);
                    VariantCopy(&reversedArgs[j], &args[argc - 1 - j]);
                }
                dp.rgvarg = reversedArgs;
                dp.cArgs = (UINT)argc;
            }
            pDisp->lpVtbl->Invoke(pDisp, dispid, &IID_NULL, LOCALE_USER_DEFAULT,
                DISPATCH_METHOD, &dp, NULL, NULL, NULL);
            if (reversedArgs) {
                for (j = 0; j < argc; j++) VariantClear(&reversedArgs[j]);
                CoTaskMemFree(reversedArgs);
            }
            pDisp->lpVtbl->Release(pDisp);
        }
    }
}

// ============================================================
// P6.6: IProvideClassInfo2 实现 (minimal — GetGUID only)
// ============================================================

static HRESULT STDMETHODCALLTYPE PCI_QueryInterface(vb6_ProvideClassInfo2* self, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (IsEqualIID(riid, &IID_IUnknown_) || IsEqualIID(riid, &IID_IProvideClassInfo2_)) {
        *ppv = self;
        self->vtable->AddRef(self);
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE PCI_AddRef(vb6_ProvideClassInfo2* self) {
    return InterlockedIncrement(&self->refCount);
}

static ULONG STDMETHODCALLTYPE PCI_Release(vb6_ProvideClassInfo2* self) {
    ULONG c = InterlockedDecrement(&self->refCount);
    if (c == 0) {
        CoTaskMemFree(self);
    }
    return c;
}

static HRESULT STDMETHODCALLTYPE PCI_GetClassInfo(vb6_ProvideClassInfo2* self, ITypeInfo** ppTI) {
    if (!ppTI) return E_POINTER;
    *ppTI = NULL;
    // Load ITypeInfo for this coclass from registered TypeLib
    if (!self->comObj || !self->comObj->desc) return E_FAIL;
    return vb6_LoadCoClassTypeInfo(self->comObj->desc, ppTI);
}

static HRESULT STDMETHODCALLTYPE PCI_GetGUID(vb6_ProvideClassInfo2* self, DWORD dwGuidKind, GUID* pGUID) {
    if (!pGUID) return E_POINTER;
    if (dwGuidKind != GUIDKIND_DEFAULT_SOURCE_DISP_IID) return E_FAIL;
    if (!self->comObj || !self->comObj->desc || !self->comObj->desc->sourceIfaceIid) return E_FAIL;
    // Parse source IID from string
    wchar_t wbuf[64];
    MultiByteToWideChar(CP_ACP, 0, self->comObj->desc->sourceIfaceIid, -1, wbuf, 64);
    return CLSIDFromString(wbuf, pGUID);
}

static const vb6_IProvideClassInfo2Vtable g_PCIVtable = {
    PCI_QueryInterface,
    PCI_AddRef,
    PCI_Release,
    PCI_GetClassInfo,
    PCI_GetGUID,
};

vb6_ProvideClassInfo2* vb6_PCI_Create(vb6_ComObject* comObj) {
    if (!comObj) return NULL;
    vb6_ProvideClassInfo2* pci = (vb6_ProvideClassInfo2*)CoTaskMemAlloc(sizeof(vb6_ProvideClassInfo2));
    if (!pci) return NULL;
    pci->vtable = &g_PCIVtable;
    pci->refCount = 1;
    pci->comObj = comObj;
    return pci;
}
