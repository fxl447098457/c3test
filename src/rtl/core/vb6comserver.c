// vb6comserver.c - VB6 COM服务端运行时 (P6.6 ActiveX DLL)
// 实现IClassFactory、IDispatch包装、DLL导出骨架、注册表辅助

#include "vb6comserver.h"
#include <string.h>
#include <stdio.h>

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

// ============================================================
// vb6_ComObject - IDispatch包装实现
// ============================================================

// --- IUnknown ---

static HRESULT STDMETHODCALLTYPE ComObj_QueryInterface(vb6_ComObject* self, REFIID riid, void** ppv) {
    int i;
    if (!ppv) return E_POINTER;
    if (IsEqualIID(riid, &IID_IUnknown_) || IsEqualIID(riid, &IID_IDispatch_)) {
        *ppv = self;
        self->vtable->AddRef(self);
        return S_OK;
    }
    // P12.1: Check Implements interface IIDs
    if (self->desc && self->desc->ifaceCount > 0 && self->desc->ifaceIids) {
        for (i = 0; i < self->desc->ifaceCount; i++) {
            if (IsEqualIID(riid, self->desc->ifaceIids[i])) {
                *ppv = self;  // dispinterface: same IDispatch pointer
                self->vtable->AddRef(self);
                return S_OK;
            }
        }
    }
    *ppv = NULL;
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
        CoTaskMemFree(self);
    }
    return count;
}

// --- IDispatch (简化实现: 无TypeLib, GetIDsOfNames线性搜索) ---

static HRESULT STDMETHODCALLTYPE ComObj_GetTypeInfoCount(vb6_ComObject* self, UINT* pctinfo) {
    if (!pctinfo) return E_POINTER;
    *pctinfo = 0;  // 不提供TypeLib信息
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ComObj_GetTypeInfo(vb6_ComObject* self, UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo) {
    return DISP_E_BADINDEX;  // 无TypeLib
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
    
    // 注册TypeLib到注册表 (包括所有接口/coclass的TypeLib信息)
    hr = RegisterTypeLib(pTypeLib, (OLECHAR*)dllPath, NULL);
    pTypeLib->lpVtbl->Release(pTypeLib);
    
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
    pTypeLib->lpVtbl->Release(pTypeLib);
    
    return S_OK;
}