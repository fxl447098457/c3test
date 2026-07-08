// vb6com.c - VB6 COM互操作运行时实现 (P6)
// 使用Windows原生COM API, 独立于vb6rtl.h避免VARIANT冲突

#include "vb6com.h"
#include <stdio.h>

#include <stdlib.h>
#include <string.h>


// P24-08: COM错误传播 — 将HRESULT/EXCEPINFO转换为VB6运行时错误
// VB6行为: COM方法失败时自动触发Err.Raise, 可被On Error捕获
extern void vb6_RaiseError(int32_t errNum, void* description);
extern int32_t vb6_err_resume_next;

// COM错误→VB6错误转换辅助函数
// hr: Invoke返回的HRESULT
// excep: EXCEPINFO结构 (可能包含scode/bstrDescription)
// context: 调用上下文 (用于默认错误描述, 如L"ComCall" / L"ComSetProp")
// VB6标准错误号→中文描述映射 (FormatMessageW查不到时的兜底)
static const wchar_t* vb6_StdErrorDesc(int32_t errNum) {
    switch (errNum) {
        case   5: return L"无效的过程调用或参数";
        case   6: return L"溢出";
        case   7: return L"内存不足";
        case   9: return L"下标越界";
        case  10: return L"数组长度固定或被暂时锁定";
        case  11: return L"除数为零";
        case  13: return L"类型不匹配";
        case  14: return L"字符串空间不足";
        case  18: return L"出现用户中断";
        case  20: return L"无错误时Resume";
        case  28: return L"栈空间不足";
        case  35: return L"子程序或函数未定义";
        case  48: return L"加载DLL时出错";
        case  49: return L"DLL调用约定错误";
        case  51: return L"内部错误";
        case  52: return L"文件名或文件号错误";
        case  53: return L"找不到文件";
        case  54: return L"文件模式错误";
        case  55: return L"文件已打开";
        case  57: return L"设备I/O错误";
        case  58: return L"文件已存在";
        case  59: return L"记录长度错误";
        case  61: return L"磁盘已满";
        case  62: return L"输入超出文件尾";
        case  63: return L"记录号错误";
        case  67: return L"文件过多";
        case  68: return L"设备不可用";
        case  70: return L"权限被拒绝";
        case  71: return L"磁盘未准备好";
        case  75: return L"路径/文件访问错误";
        case  76: return L"找不到路径";
        case  91: return L"对象变量或With块变量未设置";
        case  92: return L"For循环未初始化";
        case  93: return L"无效的模式字符串";
        case  94: return L"Null的使用无效";
        case 321: return L"无效的文件格式";
        case 322: return L"无法创建必要的临时文件";
        case 380: return L"属性值无效";
        case 381: return L"属性数组索引无效";
        case 422: return L"找不到属性";
        case 423: return L"找不到属性或方法";
        case 424: return L"需要对象";
        case 429: return L"ActiveX组件不能创建对象";
        case 430: return L"类不支持自动化或不支持预期接口";
        case 432: return L"自动化操作期间找不到文件名或类名";
        case 438: return L"对象不支持此属性或方法";
        case 440: return L"自动化错误";
        case 445: return L"对象不支持此操作";
        case 446: return L"对象不支持命名参数";
        case 447: return L"对象不支持当前区域设置";
        case 448: return L"找不到命名参数";
        case 449: return L"参数不是可选的";
        case 450: return L"参数个数错误或属性赋值无效";
        case 451: return L"Property let过程未定义，Property get过程未返回对象";
        case 452: return L"无效的序号";
        case 453: return L"找不到指定的DLL函数";
        case 457: return L"此键已经与该集合的一个元素关联";
        case 458: return L"变量使用了Visual Basic不支持的自动化类型";
        case 459: return L"对象或类不支持事件集";
        case 460: return L"剪贴板格式无效";
        case 461: return L"找不到方法或数据成员";
        case 462: return L"远程服务器机器不存在或不可用";
        case 463: return L"类未在本地机器上注册";
        case 481: return L"图片无效";
        case 482: return L"打印机错误";
        case 735: return L"无法将文件保存到TEMP";
        case 744: return L"找不到搜索文本";
        case 746: return L"替换内容过长";
        default: return NULL;
    }
}

// COM错误→VB6错误转换辅助函数
// hr: Invoke返回的HRESULT
// excep: EXCEPINFO结构 (可能包含scode/bstrDescription)
// context: 调用上下文 (用于默认错误描述, 如L"ComCall" / L"ComSetProp")
static void vb6_ComCheckError(HRESULT hr, EXCEPINFO* excep, const wchar_t* context) {
    int32_t errNum = (int32_t)(hr & 0xFFFF);  // VB6错误号 = HRESULT低16位
    if (errNum == 0) errNum = (int32_t)hr;  // 非标准HRESULT直接用整个值

    // 优先使用EXCEPINFO中的scode (OLE自动化错误码的低16位 = VB6错误号)
    if (excep && excep->scode) {
        errNum = (int32_t)(excep->scode & 0xFFFF);
    }

    // 构造错误描述BSTR — 四级fallback:
    //   1. EXCEPINFO.bstrDescription (COM对象直接提供)
    //   2. IErrorInfo::GetDescription (COM错误接口)
    //   3. VB6标准错误号中文查表 (scode有效时优先)
    //   4. FormatMessageW(FROM_SYSTEM) (系统HRESULT描述)
    //   5. 默认: "COM error in <context>: 0xXXXXXXXX"
    BSTR desc = NULL;
    if (excep && excep->bstrDescription) {
        // 1. EXCEPINFO有描述: 直接使用
        desc = excep->bstrDescription;
        excep->bstrDescription = NULL;  // 转移所有权
    } else {
        // 2. 尝试IErrorInfo
        IErrorInfo* perror = NULL;
        if (SUCCEEDED(GetErrorInfo(0, &perror)) && perror) {
            BSTR eiDesc = NULL;
            perror->lpVtbl->GetDescription(perror, &eiDesc);
            DWORD helpCtx = 0;
            perror->lpVtbl->GetHelpContext(perror, &helpCtx);
            if (helpCtx) errNum = (int32_t)(helpCtx & 0xFFFF);
            perror->lpVtbl->Release(perror);
            if (eiDesc) desc = eiDesc;
        }
        if (!desc) {
            // 3. VB6标准错误号中文查表 (scode提取的VB6错误号优先)
            const wchar_t* stdDesc = vb6_StdErrorDesc(errNum);
            if (stdDesc) desc = SysAllocString(stdDesc);
        }
        if (!desc) {
            // 4. FormatMessageW(FROM_SYSTEM) — 仅当VB6表查不到时使用
            //    注意: DISP_E_EXCEPTION(0x80020009)会返回"发生意外"等无用通用描述
            //    所以当scode有效(VB6错误号)时不应走到这里
            wchar_t sysBuf[512] = {0};
            DWORD fmLen = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, (DWORD)hr, 0, sysBuf, 512, NULL);
            if (fmLen > 0) {
                while (fmLen > 0 && (sysBuf[fmLen-1] == L'\r' || sysBuf[fmLen-1] == L'\n')) sysBuf[--fmLen] = L'\0';
                if (fmLen > 0) desc = SysAllocString(sysBuf);
            }
        }
        if (!desc) {
            // 5. 默认描述
            wchar_t buf[256];
            swprintf(buf, 256, L"COM error in %ls: 0x%08lX", context ? context : L"?", (unsigned long)hr);
            desc = SysAllocString(buf);
        }
    }

    // 清理EXCEPINFO中的其他BSTR (bstrDescription已转移)
    if (excep) {
        if (excep->bstrSource) { SysFreeString(excep->bstrSource); excep->bstrSource = NULL; }
        if (excep->bstrHelpFile) { SysFreeString(excep->bstrHelpFile); excep->bstrHelpFile = NULL; }
    }

    vb6_RaiseError(errNum, desc);
}
// ============================================================
// C-style vtable helpers for IUnknown (avoid C++ IUnknown method call issues)
// ============================================================
static HRESULT vb6_UnknownQI(void* obj, REFIID riid, void** ppv) {
    IUnknown* pUnk = (IUnknown*)obj;
    return pUnk->lpVtbl->QueryInterface(pUnk, riid, ppv);
}
static ULONG vb6_UnknownRelease(void* obj) {
    IUnknown* pUnk = (IUnknown*)obj;
    return pUnk->lpVtbl->Release(pUnk);
}

// ============================================================
// COM初始化/退出
// ============================================================

void vb6_ComInit(void) {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
}

void vb6_ComExit(void) {
    CoUninitialize();
}

// ============================================================
// CreateObject / GetObject
// ============================================================

void* vb6_CreateObject(const wchar_t* progId) {
    if (!progId) return NULL;

    CLSID clsid;
    HRESULT hr = CLSIDFromProgID(progId, &clsid);
    if (FAILED(hr)) {
        // VB6 Error 429: ActiveX component can't create object
        wchar_t buf429[512];
        swprintf(buf429, 512, L"ActiveX component can't create object (ProgID: %ls, HRESULT: 0x%08lX)", progId, (unsigned long)hr);
        BSTR desc = SysAllocString(buf429);
        vb6_RaiseError(429, desc);
        return NULL;
    }

    IDispatch* pDisp = NULL;
    hr = CoCreateInstance(&clsid, NULL, CLSCTX_LOCAL_SERVER | CLSCTX_INPROC_SERVER,
                          &IID_IDispatch, (void**)&pDisp);
    if (FAILED(hr)) {
        BSTR desc429 = NULL;
        IErrorInfo* perror429 = NULL;
        if (SUCCEEDED(GetErrorInfo(0, &perror429)) && perror429) {
            perror429->lpVtbl->GetDescription(perror429, &desc429);
            perror429->lpVtbl->Release(perror429);
        }
        if (!desc429) {
            wchar_t buf429b[512];
            swprintf(buf429b, 512, L"ActiveX component can't create object (ProgID: %ls, HRESULT: 0x%08lX)", progId, (unsigned long)hr);
            desc429 = SysAllocString(buf429b);
        }
        vb6_RaiseError(429, desc429);
        vb6_RaiseError(429, desc429);
        return NULL;
    }

    return (void*)pDisp;
}

void* vb6_GetObject(const wchar_t* pathName, const wchar_t* progId) {
    if (!progId) return NULL;

    CLSID clsid;
    HRESULT hr = CLSIDFromProgID(progId, &clsid);
    if (FAILED(hr)) {
        fwprintf(stderr, L"vb6_GetObject: CLSIDFromProgID(\"%ls\") failed: 0x%08lX\n",
                 progId, (unsigned long)hr);
        return NULL;
    }

    IUnknown* pUnk = NULL;

    if (pathName && wcslen(pathName) > 0) {
        // 从文件加载: GetObject(pathName, progId)
        IPersistFile* pFile = NULL;
        hr = CoCreateInstance(&clsid, NULL, CLSCTX_LOCAL_SERVER | CLSCTX_INPROC_SERVER,
                              &IID_IPersistFile, (void**)&pFile);
        if (SUCCEEDED(hr) && pFile) {
            hr = pFile->lpVtbl->Load(pFile, pathName, STGM_READ);
            if (SUCCEEDED(hr)) {
                hr = pFile->lpVtbl->QueryInterface(pFile, &IID_IDispatch, (void**)&pUnk);
            }
            pFile->lpVtbl->Release(pFile);
        }
        // 回退: 尝试GetActiveObject
        if (!pUnk) {
            hr = GetActiveObject(&clsid, NULL, &pUnk);
        }
    } else {
        // 获取运行中的对象: GetObject(, progId)
        hr = GetActiveObject(&clsid, NULL, &pUnk);
    }

    if (FAILED(hr) || !pUnk) {
        fwprintf(stderr, L"vb6_GetObject: failed for \"%ls\": 0x%08lX\n",
                 progId, (unsigned long)hr);
        return NULL;
    }

    IDispatch* pDisp = NULL;
    hr = pUnk->lpVtbl->QueryInterface(pUnk, &IID_IDispatch, (void**)&pDisp);
    pUnk->lpVtbl->Release(pUnk);

    if (FAILED(hr)) {
        fwprintf(stderr, L"vb6_GetObject: QueryInterface IDispatch failed: 0x%08lX\n",
                 (unsigned long)hr);
        return NULL;
    }

    return (void*)pDisp;
}

// ============================================================
// IsNothing / ReleaseObject
// ============================================================

int32_t vb6_IsNothing(void* obj) {
    return (obj == NULL) ? -1 : 0;  // VB6 True=-1, False=0
}

void vb6_ReleaseObject(void** objPtr) {
    if (!objPtr || !*objPtr) return;

    IUnknown* pUnk = (IUnknown*)(*objPtr);
    pUnk->lpVtbl->Release(pUnk);
    *objPtr = NULL;
}

// ============================================================
// COM后期绑定: IDispatch::Invoke
// ============================================================

// DISPID缓存 (MVP: 线性查找)
typedef struct vb6_DispidCacheEntry {
    void*   obj;
    wchar_t name[64];
    DISPID  dispid;
} vb6_DispidCacheEntry;

#define VB6_DISPID_CACHE_SIZE 256
static vb6_DispidCacheEntry vb6_dispid_cache[VB6_DISPID_CACHE_SIZE];
static int32_t vb6_dispid_cache_count = 0;

static DISPID vb6_getDispid(IDispatch* pDisp, const wchar_t* name) {
    // 先查缓存
    for (int32_t i = 0; i < vb6_dispid_cache_count; i++) {
        if (vb6_dispid_cache[i].obj == (void*)pDisp &&
            wcscmp(vb6_dispid_cache[i].name, name) == 0) {
            return vb6_dispid_cache[i].dispid;
        }
    }

    // 调用GetIDsOfNames
    DISPID dispid;
    LPOLESTR names[1] = { (LPOLESTR)name };
    HRESULT hr = pDisp->lpVtbl->GetIDsOfNames(pDisp, &IID_NULL, names, 1,
                                               LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) return DISPID_UNKNOWN;

    // 存入缓存
    if (vb6_dispid_cache_count < VB6_DISPID_CACHE_SIZE) {
        vb6_dispid_cache[vb6_dispid_cache_count].obj = (void*)pDisp;
        wcsncpy(vb6_dispid_cache[vb6_dispid_cache_count].name, name, 63);
        vb6_dispid_cache[vb6_dispid_cache_count].name[63] = L'\0';
        vb6_dispid_cache[vb6_dispid_cache_count].dispid = dispid;
        vb6_dispid_cache_count++;
    }

    return dispid;
}

// COM方法调用 (返回VARIANT*, 调用方需vb6_ComVarClear释放)
// args_void: VARIANT*[] (指向已分配VARIANT的指针数组), 每个元素由vb6_ComPackXxx分配
void* vb6_ComCall(void* disp, const wchar_t* methodName,
                  void* args_void, int32_t argc) {
    VARIANT** args = (VARIANT**)args_void;
    if (!disp) return NULL;
    IDispatch* pDisp = (IDispatch*)disp;

    DISPID dispid = vb6_getDispid(pDisp, methodName);
    if (dispid == DISPID_UNKNOWN) {
        fwprintf(stderr, L"vb6_ComCall: method \"%ls\" not found\n", methodName);
        return NULL;
    }

    DISPPARAMS dp;
    memset(&dp, 0, sizeof(dp));
    dp.cArgs = (UINT)argc;

    VARIANT* result = (VARIANT*)calloc(1, sizeof(VARIANT));
    VariantInit(result);

    if (argc > 0) {
        dp.rgvarg = (VARIANTARG*)malloc((size_t)argc * sizeof(VARIANTARG));
        // COM参数逆序, 从VARIANT*数组复制VARIANT值
        for (int32_t i = 0; i < argc; i++) {
            VariantInit(&dp.rgvarg[argc - 1 - i]);
            if (args[i]) {
                dp.rgvarg[argc - 1 - i] = *args[i];
            }
        }
    }

    EXCEPINFO excep;
    memset(&excep, 0, sizeof(excep));
    UINT argErr = 0;

    HRESULT hr = pDisp->lpVtbl->Invoke(pDisp, dispid, &IID_NULL,
        LOCALE_USER_DEFAULT, DISPATCH_METHOD | DISPATCH_PROPERTYGET, &dp, result, &excep, &argErr);

    if (FAILED(hr)) {
        vb6_ComCheckError(hr, &excep, L"ComCall");
        // On Error Resume Next: continue with NULL result
        if (result) { VariantClear(result); free(result); result = NULL; }
    }

    // 清理参数副本 (dp.rgvarg是浅拷贝, VariantClear会释放内部的BSTR/对象)
    if (dp.rgvarg) {
        for (UINT i = 0; i < dp.cArgs; i++) {
            VariantClear(&dp.rgvarg[i]);
        }
        free(dp.rgvarg);
    }

    // 清理打包的VARIANT参数结构体 (内容已通过dp.rgvarg的VariantClear释放)
    // 注意: dp.rgvarg[i] = *args[i] 是浅拷贝, BSTR/对象引用已被上面的VariantClear释放
    // 所以这里只free结构体, 不能再VariantClear (否则double free)
    if (args) {
        for (int32_t i = 0; i < argc; i++) {
            if (args[i]) {
                free(args[i]);  // 仅释放结构体, 内容已在dp.rgvarg清理
            }
        }
    }

    return (void*)result;
}

// COM属性Get (返回VARIANT*)
void* vb6_ComGetProp(void* disp, const wchar_t* propName) {
    if (!disp) return NULL;
    IDispatch* pDisp = (IDispatch*)disp;

    DISPID dispid = vb6_getDispid(pDisp, propName);
    if (dispid == DISPID_UNKNOWN) {
        fwprintf(stderr, L"vb6_ComGetProp: property \"%ls\" not found\n", propName);
        return NULL;
    }

    DISPPARAMS dp;
    memset(&dp, 0, sizeof(dp));

    VARIANT* result = (VARIANT*)calloc(1, sizeof(VARIANT));
    VariantInit(result);

    EXCEPINFO excep;
    memset(&excep, 0, sizeof(excep));
    UINT argErr = 0;

    HRESULT hr = pDisp->lpVtbl->Invoke(pDisp, dispid, &IID_NULL,
        LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &dp, result, &excep, &argErr);

    if (FAILED(hr)) {
        vb6_ComCheckError(hr, &excep, L"ComGetProp");
        if (result) { VariantClear(result); free(result); result = NULL; }
    }

    return (void*)result;
}


// COM属性Get带参数 (参数化属性读取, 如Dictionary.Item(key))
// 使用DISPATCH_METHOD|DISPATCH_PROPERTYGET组合标志
// 返回VARIANT* (调用方需vb6_ComVarClear释放)
void* vb6_ComGetPropArg(void* disp, const wchar_t* propName,
                        void* args_void, int32_t argc) {
    VARIANT** args = (VARIANT**)args_void;
    if (!disp) return NULL;
    IDispatch* pDisp = (IDispatch*)disp;

    DISPID dispid = vb6_getDispid(pDisp, propName);
    if (dispid == DISPID_UNKNOWN) {
        fwprintf(stderr, L"vb6_ComGetPropArg: property \"%ls\" not found\n", propName);
        return NULL;
    }

    DISPPARAMS dp;
    memset(&dp, 0, sizeof(dp));
    dp.cArgs = (UINT)argc;

    VARIANT* result = (VARIANT*)calloc(1, sizeof(VARIANT));
    VariantInit(result);

    if (argc > 0) {
        dp.rgvarg = (VARIANTARG*)malloc((size_t)argc * sizeof(VARIANTARG));
        for (int32_t i = 0; i < argc; i++) {
            VariantInit(&dp.rgvarg[argc - 1 - i]);
            if (args[i]) {
                dp.rgvarg[argc - 1 - i] = *args[i];
            }
        }
    }

    EXCEPINFO excep;
    memset(&excep, 0, sizeof(excep));
    UINT argErr = 0;

    HRESULT hr = pDisp->lpVtbl->Invoke(pDisp, dispid, &IID_NULL,
        LOCALE_USER_DEFAULT, DISPATCH_METHOD | DISPATCH_PROPERTYGET,
        &dp, result, &excep, &argErr);

    if (FAILED(hr)) {
        vb6_ComCheckError(hr, &excep, L"ComGetPropArg");
        if (result) { VariantClear(result); free(result); result = NULL; }
    }

    if (dp.rgvarg) {
        for (UINT i = 0; i < dp.cArgs; i++) {
            VariantClear(&dp.rgvarg[i]);
        }
        free(dp.rgvarg);
    }

    return (void*)result;
}

// COM属性Set (值类型)
// COM属性Set (值类型)
void vb6_ComSetProp(void* disp, const wchar_t* propName, void* value_void) {
    VARIANT value;
    VariantInit(&value);
    // 简化: 假设value是已打包的VARIANT; MVP阶段由cgen直接传递
    if (value_void) {
        value = *(VARIANT*)value_void;
    }
    if (!disp) { free(value_void); return; }
    IDispatch* pDisp = (IDispatch*)disp;

    DISPID dispid = vb6_getDispid(pDisp, propName);
    if (dispid == DISPID_UNKNOWN) {
        free(value_void);
        fwprintf(stderr, L"vb6_ComSetProp: property \"%ls\" not found\n", propName);
        return;
    }

    DISPID putId = DISPID_PROPERTYPUT;
    DISPPARAMS dp;
    memset(&dp, 0, sizeof(dp));
    dp.cArgs = 1;
    dp.cNamedArgs = 1;
    dp.rgvarg = &value;
    dp.rgdispidNamedArgs = &putId;

    EXCEPINFO excep;
    memset(&excep, 0, sizeof(excep));
    UINT argErr = 0;

    HRESULT hr = pDisp->lpVtbl->Invoke(pDisp, dispid, &IID_NULL,
        LOCALE_USER_DEFAULT, DISPATCH_PROPERTYPUT, &dp, NULL, &excep, &argErr);

    if (FAILED(hr)) {
        vb6_ComCheckError(hr, &excep, L"ComSetProp");
    }
    free(value_void);  /* 释放ComPackXxx分配的堆VARIANT结构体 */
}

// COM属性SetRef (对象引用 -- PROPERTYPUTREF)
void vb6_ComSetRef(void* disp, const wchar_t* propName, void* objRef) {
    if (!disp) return;
    IDispatch* pDisp = (IDispatch*)disp;

    DISPID dispid = vb6_getDispid(pDisp, propName);
    if (dispid == DISPID_UNKNOWN) {
        fwprintf(stderr, L"vb6_ComSetRef: property \"%ls\" not found\n", propName);
        return;
    }

    VARIANT varObj;
    VariantInit(&varObj);
    varObj.vt = VT_DISPATCH;
    varObj.pdispVal = (IDispatch*)objRef;

    DISPID putId = DISPID_PROPERTYPUT;
    DISPPARAMS dp;
    memset(&dp, 0, sizeof(dp));
    dp.cArgs = 1;
    dp.cNamedArgs = 1;
    dp.rgvarg = &varObj;
    dp.rgdispidNamedArgs = &putId;

    EXCEPINFO excep;
    memset(&excep, 0, sizeof(excep));
    UINT argErr = 0;

    // 优先尝试PROPERTYPUTREF, 失败则回退PROPERTYPUT
    HRESULT hr = pDisp->lpVtbl->Invoke(pDisp, dispid, &IID_NULL,
        LOCALE_USER_DEFAULT, DISPATCH_PROPERTYPUTREF, &dp, NULL, &excep, &argErr);
    if (FAILED(hr)) {
        hr = pDisp->lpVtbl->Invoke(pDisp, dispid, &IID_NULL,
            LOCALE_USER_DEFAULT, DISPATCH_PROPERTYPUT, &dp, NULL, &excep, &argErr);
    }

    if (FAILED(hr)) {
        vb6_ComCheckError(hr, &excep, L"ComSetRef");
    }
}

// ============================================================
// VARIANT 封装 / 解封 (P6.2 cgen使用)
// ============================================================

// 将BSTR封装为VARIANT (返回堆分配的VARIANT*, 需vb6_ComVarClear释放)
void* vb6_ComPackBSTR(const wchar_t* bstr) {
    VARIANT* pv = (VARIANT*)calloc(1, sizeof(VARIANT));
    VariantInit(pv);
    if (bstr) {
        pv->vt = VT_BSTR;
        pv->bstrVal = SysAllocString(bstr);
    } else {
        pv->vt = VT_BSTR;
        pv->bstrVal = NULL;
    }
    return (void*)pv;
}

// 将int32_t封装为VARIANT
void* vb6_ComPackInt(int32_t val) {
    VARIANT* pv = (VARIANT*)calloc(1, sizeof(VARIANT));
    VariantInit(pv);
    pv->vt = VT_I4;
    pv->lVal = val;
    return (void*)pv;
}

// 将VB6 Boolean (int32_t: -1=True, 0=False) 封装为VARIANT VT_BOOL
// VBScript/COM 期望 VT_BOOL 而非 VT_I4
void* vb6_ComPackBool(int32_t val) {
    VARIANT* pv = (VARIANT*)calloc(1, sizeof(VARIANT));
    VariantInit(pv);
    pv->vt = VT_BOOL;
    pv->boolVal = val ? VARIANT_TRUE : VARIANT_FALSE;
    return (void*)pv;
}

// 将double封装为VARIANT
void* vb6_ComPackDouble(double val) {
    VARIANT* pv = (VARIANT*)calloc(1, sizeof(VARIANT));
    VariantInit(pv);
    pv->vt = VT_R8;
    pv->dblVal = val;
    return (void*)pv;
}

// 将void*(IDispatch*)封装为VARIANT (用于对象参数)
void* vb6_ComPackObject(void* obj) {
    VARIANT* pv = (VARIANT*)calloc(1, sizeof(VARIANT));
    VariantInit(pv);
    pv->vt = VT_DISPATCH;
    pv->pdispVal = (IDispatch*)obj;
    if (obj) ((IDispatch*)obj)->lpVtbl->AddRef((IDispatch*)obj);  /* AddRef: VariantClear will Release */
    return (void*)pv;
}

// 从VARIANT*解封BSTR (返回BSTR, 调用方需vb6_BSTR_Free释放)
wchar_t* vb6_ComUnpackBSTR(void* variant) {
    if (!variant) return NULL;
    VARIANT* pv = (VARIANT*)variant;
    if (pv->vt == VT_BSTR) {
        if (pv->bstrVal) {
            // 复制BSTR内容, 调用方用vb6_BSTR_Free释放
            return SysAllocString(pv->bstrVal);
        } else {
            // VT_BSTR + bstrVal=NULL = 空BSTR, 返回NULL (与VB6 vb6_BSTR_Empty()一致)
            return NULL;
        }
    }
    // 尝试变体转换
    if (pv->vt != VT_EMPTY && pv->vt != VT_NULL) {
        VARIANT vBstr;
        VariantInit(&vBstr);
        if (SUCCEEDED(VariantChangeType(&vBstr, pv, 0, VT_BSTR))) {
            BSTR tmp = SysAllocString(vBstr.bstrVal);
            VariantClear(&vBstr);
            return tmp;
        }
    }
    return NULL;
}

// 从VARIANT*解封int32_t
int32_t vb6_ComUnpackInt(void* variant) {
    if (!variant) return 0;
    VARIANT* pv = (VARIANT*)variant;
    if (pv->vt == VT_I4) return pv->lVal;
    if (pv->vt == VT_I2) return (int32_t)pv->iVal;
    if (pv->vt == VT_BOOL) return (pv->boolVal == VARIANT_TRUE) ? -1 : 0;
    if (pv->vt == VT_UI1) return (int32_t)pv->bVal;
    // 尝试变体转换
    if (pv->vt != VT_EMPTY && pv->vt != VT_NULL) {
        VARIANT vInt;
        VariantInit(&vInt);
        if (SUCCEEDED(VariantChangeType(&vInt, pv, 0, VT_I4))) {
            return vInt.lVal;
        }
    }
    return 0;
}

// 从VARIANT*解封double
double vb6_ComUnpackDouble(void* variant) {
    if (!variant) return 0.0;
    VARIANT* pv = (VARIANT*)variant;
    if (pv->vt == VT_R8) return pv->dblVal;
    if (pv->vt == VT_R4) return (double)pv->fltVal;
    if (pv->vt == VT_I4) return (double)pv->lVal;
    // 尝试变体转换
    if (pv->vt != VT_EMPTY && pv->vt != VT_NULL) {
        VARIANT vDbl;
        VariantInit(&vDbl);
        if (SUCCEEDED(VariantChangeType(&vDbl, pv, 0, VT_R8))) {
            return vDbl.dblVal;
        }
    }
    return 0.0;
}

// 从VARIANT*解封对象(void*/IDispatch*)
void* vb6_ComUnpackObject(void* variant) {
    if (!variant) return NULL;
    VARIANT* pv = (VARIANT*)variant;
    if (pv->vt == VT_DISPATCH) return (void*)pv->pdispVal;
    if (pv->vt == VT_UNKNOWN) return (void*)pv->punkVal;
    return NULL;
}

// 释放ComCall/ComGetProp返回的VARIANT* (值类型安全: 释清BSTR/数字等)
void vb6_ComVarClear(void* variant) {
    if (!variant) return;
    VARIANT* pv = (VARIANT*)variant;
    VariantClear(pv);
    free(pv);
}

// 仅释放VARIANT结构体, 不清除内容 (对象所有权转移: pdispVal已被取走)
void vb6_ComVarFree(void* variant) {
    if (!variant) return;
    free(variant);  // 不调用VariantClear, 对象引用已转移给调用方
}

// ============================================================
// 一体化COM辅助函数 (P6.2 cgen直接使用, 内部处理临时VARIANT清理)
// ============================================================

// COM方法调用, 返回对象 (IDispatch*) — 内部完成Unpack+VarFree
void* vb6_ComCallObject(void* disp, const wchar_t* methodName,
                        void* args_void, int32_t argc) {
    VARIANT* pv = (VARIANT*)vb6_ComCall(disp, methodName, args_void, argc);
    if (!pv) return NULL;
    void* obj = NULL;
    if (pv->vt == VT_DISPATCH) {
        obj = (void*)pv->pdispVal;  // 转移引用所有权
    } else if (pv->vt == VT_UNKNOWN) {
        pv->punkVal->lpVtbl->QueryInterface(pv->punkVal, &IID_IDispatch, &obj);
        // QueryInterface增加了refcount, 需要Release原引用
        // 但原引用在VarFree中不会Release, 所以不需要额外操作
    } else if (pv->vt != VT_EMPTY && pv->vt != VT_NULL) {
        // 尝试转换为IDispatch
        VARIANT vObj;
        VariantInit(&vObj);
        if (SUCCEEDED(VariantChangeType(&vObj, pv, 0, VT_DISPATCH))) {
            obj = (void*)vObj.pdispVal;
        }
        VariantClear(&vObj);
    }
    vb6_ComVarFree(pv);  // 仅释放结构体, 不Release pdispVal
    return obj;
}

// COM方法调用, 返回BSTR — 内部完成Unpack+VarClear
wchar_t* vb6_ComCallBSTR(void* disp, const wchar_t* methodName,
                         void* args_void, int32_t argc) {
    VARIANT* pv = (VARIANT*)vb6_ComCall(disp, methodName, args_void, argc);
    if (!pv) return NULL;
    BSTR result = vb6_ComUnpackBSTR(pv);
    vb6_ComVarClear(pv);  // 安全: UnpackBSTR已复制字符串内容
    return result;
}

// COM方法调用, 返回int32_t — 内部完成Unpack+VarClear
int32_t vb6_ComCallInt(void* disp, const wchar_t* methodName,
                       void* args_void, int32_t argc) {
    VARIANT* pv = (VARIANT*)vb6_ComCall(disp, methodName, args_void, argc);
    if (!pv) return 0;
    int32_t result = vb6_ComUnpackInt(pv);
    vb6_ComVarClear(pv);
    return result;
}

// COM方法调用, 返回double — 内部完成Unpack+VarClear
double vb6_ComCallDouble(void* disp, const wchar_t* methodName,
                         void* args_void, int32_t argc) {
    VARIANT* pv = (VARIANT*)vb6_ComCall(disp, methodName, args_void, argc);
    if (!pv) return 0.0;
    double result = vb6_ComUnpackDouble(pv);
    vb6_ComVarClear(pv);
    return result;
}

// COM属性Get, 返回BSTR — 内部完成Unpack+VarClear
wchar_t* vb6_ComGetStringProp(void* disp, const wchar_t* propName) {
    VARIANT* pv = (VARIANT*)vb6_ComGetProp(disp, propName);
    if (!pv) return NULL;
    BSTR result = vb6_ComUnpackBSTR(pv);
    vb6_ComVarClear(pv);  // 安全: UnpackBSTR已复制字符串内容
    return result;
}

// COM属性Get, 返回int32_t — 内部完成Unpack+VarClear
int32_t vb6_ComGetIntProp(void* disp, const wchar_t* propName) {
    VARIANT* pv = (VARIANT*)vb6_ComGetProp(disp, propName);
    if (!pv) return 0;
    int32_t result = vb6_ComUnpackInt(pv);
    vb6_ComVarClear(pv);
    return result;
}

// COM属性Get, 返回double — 内部完成Unpack+VarClear
double vb6_ComGetDoubleProp(void* disp, const wchar_t* propName) {
    VARIANT* pv = (VARIANT*)vb6_ComGetProp(disp, propName);
    if (!pv) return 0.0;
    double result = vb6_ComUnpackDouble(pv);
    vb6_ComVarClear(pv);
    return result;
}

// COM属性Get, 返回对象 — 内部完成Unpack+VarFree
void* vb6_ComGetObjectProp(void* disp, const wchar_t* propName) {
    VARIANT* pv = (VARIANT*)vb6_ComGetProp(disp, propName);
    if (!pv) return NULL;
    void* obj = NULL;
    if (pv->vt == VT_DISPATCH) {
        obj = (void*)pv->pdispVal;
    } else if (pv->vt == VT_UNKNOWN) {
        pv->punkVal->lpVtbl->QueryInterface(pv->punkVal, &IID_IDispatch, &obj);
    } else if (pv->vt != VT_EMPTY && pv->vt != VT_NULL) {
        VARIANT vObj;
        VariantInit(&vObj);
        if (SUCCEEDED(VariantChangeType(&vObj, pv, 0, VT_DISPATCH))) {
            obj = (void*)vObj.pdispVal;
        }
        VariantClear(&vObj);
    }
    vb6_ComVarFree(pv);  // 仅释放结构体, 不Release pdispVal
    return obj;
}

// ============================================================
// 参数化属性Get (带参数, 如Dictionary.Item(key))
// ============================================================

// 参数化属性Get→BSTR
wchar_t* vb6_ComGetPropertyString(void* disp, const wchar_t* propName,
                                   void* args, int32_t argc) {
    VARIANT* pv = (VARIANT*)vb6_ComGetPropArg(disp, propName, args, argc);
    if (!pv) return NULL;
    BSTR result = vb6_ComUnpackBSTR(pv);
    vb6_ComVarClear(pv);
    return result;
}

// 参数化属性Get→int32_t
int32_t vb6_ComGetPropertyInt(void* disp, const wchar_t* propName,
                               void* args, int32_t argc) {
    VARIANT* pv = (VARIANT*)vb6_ComGetPropArg(disp, propName, args, argc);
    if (!pv) return 0;
    int32_t result = vb6_ComUnpackInt(pv);
    vb6_ComVarClear(pv);
    return result;
}

// 参数化属性Get→double
double vb6_ComGetPropertyDouble(void* disp, const wchar_t* propName,
                                 void* args, int32_t argc) {
    VARIANT* pv = (VARIANT*)vb6_ComGetPropArg(disp, propName, args, argc);
    if (!pv) return 0.0;
    double result = vb6_ComUnpackDouble(pv);
    vb6_ComVarClear(pv);
    return result;
}

// 参数化属性Get→对象
void* vb6_ComGetPropertyObject(void* disp, const wchar_t* propName,
                                void* args, int32_t argc) {
    VARIANT* pv = (VARIANT*)vb6_ComGetPropArg(disp, propName, args, argc);
    if (!pv) return NULL;
    void* obj = NULL;
    if (pv->vt == VT_DISPATCH) {
        obj = (void*)pv->pdispVal;
    } else if (pv->vt == VT_UNKNOWN) {
        pv->punkVal->lpVtbl->QueryInterface(pv->punkVal, &IID_IDispatch, &obj);
    } else if (pv->vt != VT_EMPTY && pv->vt != VT_NULL) {
        VARIANT vObj;
        VariantInit(&vObj);
        if (SUCCEEDED(VariantChangeType(&vObj, pv, 0, VT_DISPATCH))) {
            obj = (void*)vObj.pdispVal;
        }
        VariantClear(&vObj);
    }
    vb6_ComVarFree(pv);
    return obj;
}

// 参数化属性Get→VARIANT (保留原样, 用于Variant变量赋值)
// 注意: 返回的VARIANT*需要调用方用vb6_ComVarClear释放
void* vb6_ComGetPropertyVariant(void* disp, const wchar_t* propName,
                                 void* args, int32_t argc) {
    return vb6_ComGetPropArg(disp, propName, args, argc);
}

// ============================================================
// P6.3: COM前期绑定运行时 (vtable直接调用)

// ============================================================
// P6.3: COM前期绑定运行时 (vtable直接调用)
// ============================================================

void* vb6_ComQI(void* obj, const char* iidStr) {
    if (!obj || !iidStr) return NULL;
    IID iid;
    wchar_t iidWide[64];
    int len = 0;
    for (; iidStr[len] && len < 63; len++) iidWide[len] = (wchar_t)iidStr[len];
    iidWide[len] = 0;
    HRESULT hr = IIDFromString(iidWide, &iid);
    if (FAILED(hr)) return NULL;
    void* pv = NULL;
    hr = vb6_UnknownQI(obj, &iid, &pv);
    if (FAILED(hr)) return NULL;
    return pv;
}

void* vb6_ComCreateTyped(const wchar_t* progId, const char* iidStr) {
    void* disp = vb6_CreateObject(progId);
    if (!disp) return NULL;
    void* iface = vb6_ComQI(disp, iidStr);
    vb6_UnknownRelease(disp);
    return iface;
}

void vb6_ComReleaseTyped(void** objPtr) {
    if (objPtr && *objPtr) {
        vb6_UnknownRelease(*objPtr);
        *objPtr = NULL;
    }
}

void vb6_ComVtableCallVoid(void* obj, int32_t vtIndex, ...) {
    if (!obj) return;
    void** vtable = *(void***)obj;
    if (!vtable || vtIndex < 0) return;
}

wchar_t* vb6_ComVtableGetBSTR(void* obj, int32_t vtIndex, ...) {
    if (!obj) return NULL;
    void** vtable = *(void***)obj;
    if (!vtable || vtIndex < 0) return NULL;
    typedef HRESULT(__stdcall* GetBSTRMethod)(void*, BSTR*);
    GetBSTRMethod fn = (GetBSTRMethod)vtable[vtIndex];
    BSTR result = NULL;
    fn(obj, &result);
    if (!result) return NULL;
    return result;  // Already OLE BSTR, caller uses vb6_BSTR_Free (SysFreeString)
}

int32_t vb6_ComVtableGetInt(void* obj, int32_t vtIndex, ...) {
    if (!obj) return 0;
    void** vtable = *(void***)obj;
    if (!vtable || vtIndex < 0) return 0;
    typedef HRESULT(__stdcall* GetVarMethod)(void*, VARIANT*);
    GetVarMethod fn = (GetVarMethod)vtable[vtIndex];
    VARIANT v; VariantInit(&v); fn(obj, &v);
    int32_t result = 0;
    if (v.vt == VT_I4) result = v.lVal;
    else if (v.vt == VT_I2) result = v.iVal;
    else if (v.vt == VT_BOOL) result = (v.boolVal != 0) ? -1 : 0;
    else if (v.vt == VT_R8) result = (int32_t)v.dblVal;
    else if (v.vt == VT_EMPTY || v.vt == VT_NULL) result = 0;
    else { VARIANT vO; VariantInit(&vO); if(SUCCEEDED(VariantChangeType(&vO,&v,0,VT_I4))) result=vO.lVal; VariantClear(&vO); }
    VariantClear(&v);
    return result;
}

double vb6_ComVtableGetDouble(void* obj, int32_t vtIndex, ...) {
    if (!obj) return 0.0;
    void** vtable = *(void***)obj;
    if (!vtable || vtIndex < 0) return 0.0;
    typedef HRESULT(__stdcall* GetVarMethod)(void*, VARIANT*);
    GetVarMethod fn = (GetVarMethod)vtable[vtIndex];
    VARIANT v; VariantInit(&v); fn(obj, &v);
    double result = 0.0;
    if (v.vt == VT_R8) result = v.dblVal;
    else if (v.vt == VT_R4) result = v.fltVal;
    else if (v.vt == VT_I4) result = (double)v.lVal;
    else if (v.vt == VT_I2) result = (double)v.iVal;
    else { VARIANT vO; VariantInit(&vO); if(SUCCEEDED(VariantChangeType(&vO,&v,0,VT_R8))) result=vO.dblVal; VariantClear(&vO); }
    VariantClear(&v);
    return result;
}

void* vb6_ComVtableGetObject(void* obj, int32_t vtIndex, ...) {
    if (!obj) return NULL;
    void** vtable = *(void***)obj;
    if (!vtable || vtIndex < 0) return NULL;
    typedef HRESULT(__stdcall* GetVarMethod)(void*, VARIANT*);
    GetVarMethod fn = (GetVarMethod)vtable[vtIndex];
    VARIANT v; VariantInit(&v); fn(obj, &v);
    void* result = NULL;
    if (v.vt == VT_DISPATCH && v.pdispVal) result = v.pdispVal;
    else if (v.vt == VT_UNKNOWN && v.punkVal) result = v.punkVal;
    if (v.vt == VT_DISPATCH || v.vt == VT_UNKNOWN) v.pdispVal = NULL;
    VariantClear(&v);
    return result;
}

void* vb6_ComVtableGetVoid(void* obj, int32_t vtIndex, ...) {
    if (!obj) return NULL;
    void** vtable = *(void***)obj;
    if (!vtable || vtIndex < 0) return NULL;
    typedef HRESULT(__stdcall* GetVarMethod)(void*, VARIANT*);
    GetVarMethod fn = (GetVarMethod)vtable[vtIndex];
    VARIANT* pv = (VARIANT*)malloc(sizeof(VARIANT));
    VariantInit(pv); fn(obj, pv);
    return (void*)pv;
}

// ============================================================
// P13.21: IConnectionPointContainer / Advise support
// ============================================================

int vb6_ComAdvise(void* obj, const char* riidStr, void* sink, int* adviseCookie) {
    /* Use IUnknown QI to get IConnectionPointContainer, then vtable calls */
    IUnknown* pUnk = (IUnknown*)obj;
    IUnknown* pCPCUnk = NULL;
    HRESULT hr;
    IID iid;
    WCHAR wIID[64] = {0};
    void** pCPCVt;
    void** pCPVt;
    IUnknown* pCPUnk = NULL;
    DWORD cookie = 0;

    if (!obj || !riidStr || !sink || !adviseCookie) return -1;
    *adviseCookie = 0;

    MultiByteToWideChar(CP_ACP, 0, riidStr, -1, wIID, 64);
    if (IIDFromString(wIID, &iid) != S_OK) return -2;

    /* IID_IConnectionPointContainer = {B196B284-BAB4-101A-B69C-00AA00341D07} */
    static const IID IID_CPC = {0xB196B284, 0xBAB4, 0x101A, {0xB6,0x9C,0x00,0xAA,0x00,0x34,0x1D,0x07}};

    /* QI for IConnectionPointContainer */
    hr = pUnk->lpVtbl->QueryInterface(pUnk, &IID_CPC, (void**)&pCPCUnk);
    if (FAILED(hr) || !pCPCUnk) return -3;

    /* FindConnectionPoint is vtable index 4 (0=QI, 1=AddRef, 2=Release, 3=EnumConnPts, 4=FindCP) */
    pCPCVt = *(void***)pCPCUnk;
    typedef HRESULT(__stdcall* FindCPFunc)(void*, const IID*, IUnknown**);
    FindCPFunc findCP = (FindCPFunc)pCPCVt[4];
    hr = findCP(pCPCUnk, &iid, &pCPUnk);
    pCPCUnk->lpVtbl->Release(pCPCUnk);
    if (FAILED(hr) || !pCPUnk) return -4;

    /* Advise is vtable index 5 (0=QI,1=AddRef,2=Release,3=GetConnInterface,4=GetConnPtContainer,5=Advise) */
    pCPVt = *(void***)pCPUnk;
    typedef HRESULT(__stdcall* AdviseFunc)(void*, IUnknown*, DWORD*);
    AdviseFunc adviseFn = (AdviseFunc)pCPVt[5];
    hr = adviseFn(pCPUnk, (IUnknown*)sink, &cookie);
    pCPUnk->lpVtbl->Release(pCPUnk);
    if (FAILED(hr)) return -5;

    *adviseCookie = (int)cookie;
    return 0;
}

int vb6_ComUnadvise(void* obj, const char* riidStr, int adviseCookie) {
    IUnknown* pUnk = (IUnknown*)obj;
    IUnknown* pCPCUnk = NULL;
    IUnknown* pCPUnk = NULL;
    HRESULT hr;
    IID iid;
    WCHAR wIID[64] = {0};
    void** pCPCVt;
    void** pCPVt;

    if (!obj || !riidStr || adviseCookie == 0) return -1;

    MultiByteToWideChar(CP_ACP, 0, riidStr, -1, wIID, 64);
    if (IIDFromString(wIID, &iid) != S_OK) return -2;

    static const IID IID_CPC = {0xB196B284, 0xBAB4, 0x101A, {0xB6,0x9C,0x00,0xAA,0x00,0x34,0x1D,0x07}};

    hr = pUnk->lpVtbl->QueryInterface(pUnk, &IID_CPC, (void**)&pCPCUnk);
    if (FAILED(hr) || !pCPCUnk) return -3;

    pCPCVt = *(void***)pCPCUnk;
    typedef HRESULT(__stdcall* FindCPFunc)(void*, const IID*, IUnknown**);
    FindCPFunc findCP = (FindCPFunc)pCPCVt[4];
    hr = findCP(pCPCUnk, &iid, &pCPUnk);
    pCPCUnk->lpVtbl->Release(pCPCUnk);
    if (FAILED(hr) || !pCPUnk) return -4;

    /* Unadvise is vtable index 6 */
    pCPVt = *(void***)pCPUnk;
    typedef HRESULT(__stdcall* UnadviseFunc)(void*, DWORD);
    UnadviseFunc unadviseFn = (UnadviseFunc)pCPVt[6];
    hr = unadviseFn(pCPUnk, (DWORD)adviseCookie);
    pCPUnk->lpVtbl->Release(pCPUnk);
    return SUCCEEDED(hr) ? 0 : -5;
}

// ============================================================
// P13.22: Generic IDispatch event sink
// ============================================================

/* Event sink structure: a minimal IDispatch implementation that
   maps DISPIDs to VB6 callback functions */

typedef struct VB6EventSink {
    /* Pointer to IDispatch vtable (11 methods: 7 IUnknown + 4 IDispatch) */
    void** vtable;
    /* Ref count */
    LONG refCount;
    /* DISPID → callback mapping */
    int* dispids;
    void (**callbacks)(VARIANT*, int, VARIANT*);
    int count;
} VB6EventSink;

/* Forward declarations for x86 compat (sink_AddRef/Release used before definition) */
static ULONG STDMETHODCALLTYPE sink_AddRef(IDispatch* This);
static ULONG STDMETHODCALLTYPE sink_Release(IDispatch* This);

/* IDispatch vtable methods */
static HRESULT STDMETHODCALLTYPE sink_QueryInterface(IDispatch* This, REFIID riid, void** ppv) {
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDispatch)) {
        *ppv = This;
        sink_AddRef(This);
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE sink_AddRef(IDispatch* This) {
    VB6EventSink* s = (VB6EventSink*)This;
    return InterlockedIncrement(&s->refCount);
}

static ULONG STDMETHODCALLTYPE sink_Release(IDispatch* This) {
    VB6EventSink* s = (VB6EventSink*)((char*)This - offsetof(VB6EventSink, vtable));
    ULONG c = InterlockedDecrement(&s->refCount);
    if (c == 0) {
        free(s->dispids);
        free(s->callbacks);
        free(s);
    }
    return c;
}

static HRESULT STDMETHODCALLTYPE sink_GetTypeInfoCount(IDispatch* This, UINT* pctinfo) {
    if (pctinfo) *pctinfo = 0;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sink_GetTypeInfo(IDispatch* This, UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo) {
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE sink_GetIDsOfNames(IDispatch* This, REFIID riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgDispId) {
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE sink_Invoke(IDispatch* This, DISPID dispIdMember, REFIID riid,
    LCID lcid, WORD wFlags, DISPPARAMS* pDispParams, VARIANT* pVarResult,
    EXCEPINFO* pExcepInfo, UINT* puArgErr) {
    
    VB6EventSink* s = (VB6EventSink*)((char*)This - offsetof(VB6EventSink, vtable));
    int i, argc, j;
    VARIANT result;
    VARIANT* vbArgs = NULL;
    
    /* Find callback by DISPID */
    for (i = 0; i < s->count; i++) {
        if (s->dispids[i] == (int)dispIdMember) {
            VariantInit(&result);
            if (s->callbacks[i]) {
                argc = (int)pDispParams->cArgs;
                vbArgs = NULL;
                if (argc > 0) {
                    vbArgs = (VARIANT*)malloc(argc * sizeof(VARIANT));
                    for (j = 0; j < argc; j++) {
                        VariantInit(&vbArgs[j]);
                        VariantCopy(&vbArgs[j], &pDispParams->rgvarg[argc - 1 - j]);
                    }
                }
                s->callbacks[i](vbArgs, argc, &result);
                if (vbArgs) {
                    for (j = 0; j < argc; j++) VariantClear(&vbArgs[j]);
                    free(vbArgs);
                }
            }
            if (pVarResult) VariantCopy(pVarResult, &result);
            VariantClear(&result);
            return S_OK;
        }
    }
    return S_OK;  /* Unknown dispid: silently ignore */
}

/* Vtable template */
static void* g_eventSinkVtable[11] = {
    sink_QueryInterface,
    sink_AddRef,
    sink_Release,
    sink_GetTypeInfoCount,
    sink_GetTypeInfo,
    sink_GetIDsOfNames,
    sink_Invoke
};

void* vb6_CreateEventSink(const int* dispids, void** callbacks, int count) {
    VB6EventSink* s = (VB6EventSink*)calloc(1, sizeof(VB6EventSink));
    if (!s) return NULL;
    
    /* Copy vtable template */
    s->vtable = g_eventSinkVtable;
    
    s->refCount = 1;
    s->count = count;
    
    /* Copy DISPID mappings */
    s->dispids = (int*)malloc(count * sizeof(int));
    s->callbacks = (void(**)(VARIANT*,int,VARIANT*))malloc(count * sizeof(void(*)(VARIANT*,int,VARIANT*)));
    if (!s->dispids || !s->callbacks) {
        free(s->dispids);
        free(s->callbacks);
        free(s);
        return NULL;
    }
    memcpy(s->dispids, dispids, count * sizeof(int));
    memcpy(s->callbacks, callbacks, count * sizeof(void(*)()));
    
    /* Return pointer to the vtable (IDispatch*) */
    return &s->vtable;
}

void vb6_FreeEventSink(void* sink) {
    if (!sink) return;
    IDispatch* pDisp = (IDispatch*)sink;
    pDisp->lpVtbl->Release(pDisp);
}

// ============================================================
// P22-11: For Each COM collection (IEnumVARIANT)
// ============================================================

// For Each Init: Call _NewEnum (DISPID -4) on collection object to get IEnumVARIANT
// Returns IEnumVARIANT* (or NULL on failure)
void* vb6_ForEach_Init(void* disp) {
    if (!disp) return NULL;
    IDispatch* pDisp = (IDispatch*)disp;

    // DISPID -4 is the standard _NewEnum dispid in VB6/Automation
    DISPPARAMS dp = { NULL, NULL, 0, 0 };
    VARIANT result;
    VariantInit(&result);

    HRESULT hr = pDisp->lpVtbl->Invoke(pDisp, (DISPID)-4,
        &IID_NULL, LOCALE_USER_DEFAULT,
        DISPATCH_METHOD | DISPATCH_PROPERTYGET,
        &dp, &result, NULL, NULL);

    if (FAILED(hr) || (V_VT(&result) != VT_UNKNOWN && V_VT(&result) != VT_DISPATCH)) {
        // Try named invocation as fallback
        OLECHAR* names[] = { L"_NewEnum" };
        DISPID dispid;
        hr = pDisp->lpVtbl->GetIDsOfNames(pDisp, &IID_NULL, names, 1,
            LOCALE_USER_DEFAULT, &dispid);
        if (SUCCEEDED(hr)) {
            hr = pDisp->lpVtbl->Invoke(pDisp, dispid,
                &IID_NULL, LOCALE_USER_DEFAULT,
                DISPATCH_METHOD | DISPATCH_PROPERTYGET,
                &dp, &result, NULL, NULL);
        }
    }

    if (FAILED(hr)) {
        VariantClear(&result);
        return NULL;
    }

    // Get IEnumVARIANT from the returned IUnknown/IDispatch
    IEnumVARIANT* pEnum = NULL;
    if (V_VT(&result) == VT_UNKNOWN && V_UNKNOWN(&result)) {
        hr = V_UNKNOWN(&result)->lpVtbl->QueryInterface(
            V_UNKNOWN(&result), &IID_IEnumVARIANT, (void**)&pEnum);
    } else if (V_VT(&result) == VT_DISPATCH && V_DISPATCH(&result)) {
        hr = V_DISPATCH(&result)->lpVtbl->QueryInterface(
            V_DISPATCH(&result), &IID_IEnumVARIANT, (void**)&pEnum);
    }

    VariantClear(&result);
    return (void*)pEnum;
}

// For Each Next: Fetch one element from IEnumVARIANT
// Returns 1 if element fetched, 0 if enumeration complete
int32_t vb6_ForEach_Next(void* enumPtr, VARIANT* outVar) {
    if (!enumPtr || !outVar) return 0;
    IEnumVARIANT* pEnum = (IEnumVARIANT*)enumPtr;

    VariantInit(outVar);
    ULONG fetched = 0;
    HRESULT hr = pEnum->lpVtbl->Next(pEnum, 1, outVar, &fetched);
    if (FAILED(hr) || fetched == 0) {
        VariantClear(outVar);
        return 0;
    }
    return 1;
}

// For Each Release: Release IEnumVARIANT
void vb6_ForEach_Release(void* enumPtr) {
    if (!enumPtr) return;
    IEnumVARIANT* pEnum = (IEnumVARIANT*)enumPtr;
    pEnum->lpVtbl->Release(pEnum);
}