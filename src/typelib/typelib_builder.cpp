// P9: TypeLib 内建生成器实现
// 使用 CreateTypeLib2 / ICreateTypeInfo API

#include "typelib/typelib_builder.hpp"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstdint>
#include <cstdio>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <oleauto.h>
#endif

namespace vb6c3 {

// ============================================================
// 工具函数
// ============================================================

std::string TypeLibBuilder::generateUuid(const std::string& seed) {
    // FNV-1a hash 确定性 UUID 生成 (与 cgen.cpp generateIdl() 一致)
    uint64_t h1 = 0xcbf29ce484222325ULL;
    uint64_t h2 = 0xcbf29ce484222325ULL;
    for (char c : seed) {
        unsigned char lc = (unsigned char)tolower(c);
        h1 ^= lc; h1 *= 0x100000001b3ULL;
        h2 ^= lc; h2 *= 0x100000001b3ULL;
    }
    // UUID v4 variant
    h1 = (h1 & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    h2 = (h2 & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    char buf[64];
    snprintf(buf, sizeof(buf),
        "{%08lx-%04x-%04x-%04x-%04x%08lx}",
        (unsigned long)(h1 >> 32),
        (unsigned int)((h1 >> 16) & 0xFFFF),
        (unsigned int)(h1 & 0xFFFF),
        (unsigned int)((h2 >> 48) & 0xFFFF),
        (unsigned int)((h2 >> 32) & 0xFFFF),
        (unsigned long)(h2 & 0xFFFFFFFF));
    return buf;
}

unsigned short TypeLibBuilder::mapVartype(Vb6Type t) {
    switch (t) {
        case Vb6Type::Integer:  return VT_I2;
        case Vb6Type::Long:     return VT_I4;
        case Vb6Type::Single:   return VT_R4;
        case Vb6Type::Double:   return VT_R8;
        case Vb6Type::String:   return VT_BSTR;
        case Vb6Type::Boolean:  return VT_BOOL;
        case Vb6Type::Byte:     return VT_UI1;
        case Vb6Type::Currency: return VT_CY;
        case Vb6Type::Date:     return VT_DATE;
        case Vb6Type::Variant:  return VT_VARIANT;
        case Vb6Type::Object:   return VT_DISPATCH;
        case Vb6Type::Void:     return VT_VOID;
        case Vb6Type::Error:    return VT_HRESULT;   // VB6 Error type → HRESULT
        case Vb6Type::ULong:    return VT_UI4;
        default:                return VT_VARIANT;
    }
}

// ============================================================
// 生命周期
// ============================================================

TypeLibBuilder::TypeLibBuilder() = default;

TypeLibBuilder::~TypeLibBuilder() {
#ifdef _WIN32
    if (pCreateLib_) {
        auto* p = static_cast<ICreateTypeLib2*>(pCreateLib_);
        p->Release();
        pCreateLib_ = nullptr;
    }
    for (auto& iface : interfaces_) {
        if (iface.pCreateTypeInfo) {
            static_cast<ICreateTypeInfo*>(iface.pCreateTypeInfo)->Release();
            iface.pCreateTypeInfo = nullptr;
        }
        if (iface.pTypeInfo) {
            static_cast<ITypeInfo*>(iface.pTypeInfo)->Release();
            iface.pTypeInfo = nullptr;
        }
    }
    for (void* p : vtableInnerAllocs_) {
        delete[] static_cast<TYPEDESC*>(p);
    }
    vtableInnerAllocs_.clear();
    if (libOpen_) {
        CoUninitialize();
    }
#endif
}

bool TypeLibBuilder::beginLib(const std::string& tlbPath,
                               const std::string& libidStr,
                               const std::string& helpString,
                               const std::string& libName,
                               bool is64) {
#ifdef _WIN32
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        lastError_ = "CoInitializeEx failed: 0x" + std::to_string(hr);
        return false;
    }
    libOpen_ = true;

    // 使用完整 .tlb 文件路径作为 CreateTypeLib2 参数
    int wlen = MultiByteToWideChar(CP_UTF8, 0, tlbPath.c_str(), -1, nullptr, 0);
    std::vector<WCHAR> wPath(wlen);
    MultiByteToWideChar(CP_UTF8, 0, tlbPath.c_str(), -1, wPath.data(), wlen);

    // ai/022 B16: 位数 flag 跟 --arch 走。实测 (B16 测量③)：这枚 flag 进 .tlb 字节，且
    // oVft 是"写入端按本端指针宽存、读取端按本端指针宽换算"—— 恒 SYS_WIN64 会让 32 位
    // 客户端把 oVft=24 读成 48；反过来 SYS_WIN64 库连 12 这样的偏移都存不进去
    // (AddFuncDesc 直接 E_INVALIDARG)。默认架构 x64 下与本批之前逐字节相同。
    is64_ = is64;
    ICreateTypeLib2* pCTL = nullptr;
    hr = CreateTypeLib2(is64_ ? SYS_WIN64 : SYS_WIN32, wPath.data(), &pCTL);
    if (FAILED(hr) || !pCTL) {
        lastError_ = "CreateTypeLib2 failed: 0x" + std::to_string(hr);
        return false;
    }
    pCreateLib_ = pCTL;

    // 设置 LibID
    std::string libid = libidStr.empty() ? generateUuid(libName.empty() ? tlbPath : libName) : libidStr;
    GUID guid;
    hr = CLSIDFromString(std::wstring(libid.begin(), libid.end()).c_str(), &guid);
    if (FAILED(hr)) {
        // 尝试去掉花括号
        std::wstring wLibid = L"{" + std::wstring(libid.begin(), libid.end()) + L"}";
        hr = CLSIDFromString(wLibid.c_str(), &guid);
    }
    if (SUCCEEDED(hr)) {
        pCTL->SetGuid(guid);
    }

    // 设置 helpstring (ICreateTypeLib::SetDocString, NOT SetDocHelpString)
    if (!helpString.empty()) {
        int hwlen = MultiByteToWideChar(CP_UTF8, 0, helpString.c_str(), -1, nullptr, 0);
        std::vector<WCHAR> wHelp(hwlen);
        MultiByteToWideChar(CP_UTF8, 0, helpString.c_str(), -1, wHelp.data(), hwlen);
        pCTL->SetDocString(wHelp.data());
    }

    // 设置 TypeLib 内部名
    if (!libName.empty()) {
        int lnwlen = MultiByteToWideChar(CP_UTF8, 0, libName.c_str(), -1, nullptr, 0);
        std::vector<WCHAR> wLibName(lnwlen);
        MultiByteToWideChar(CP_UTF8, 0, libName.c_str(), -1, wLibName.data(), lnwlen);
        pCTL->SetName(wLibName.data());
    }

    // 设置版本 1.0
    pCTL->SetVersion(1, 0);

    // Lcid = English US
    pCTL->SetLcid(0x0409);

    return true;
#else
    lastError_ = "TypeLib generation only supported on Windows";
    return false;
#endif
}

// ============================================================
// 添加 dispinterface
// ============================================================

bool TypeLibBuilder::addDispInterface(const std::string& name,
                                       const std::string& iidStr,
                                       const std::vector<MethodInfo>& methods) {
#ifdef _WIN32
    if (!pCreateLib_) {
        lastError_ = "beginLib() not called";
        return false;
    }

    auto* pCTL = static_cast<ICreateTypeLib2*>(pCreateLib_);

    // 创建 TypeInfo — CreateTypeInfo 返回 ICreateTypeInfo (not ICreateTypeInfo2)
    int wlen = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, nullptr, 0);
    std::vector<WCHAR> wName(wlen);
    MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, wName.data(), wlen);

    ICreateTypeInfo* pCTI = nullptr;
    HRESULT hr = pCTL->CreateTypeInfo(wName.data(), TKIND_DISPATCH, &pCTI);
    if (FAILED(hr) || !pCTI) {
        lastError_ = "CreateTypeInfo(dispatch) failed for " + name + ": 0x" + std::to_string(hr);
        return false;
    }

    // 设置 IID
    std::string iid = iidStr.empty() ? generateUuid(name) : iidStr;
    GUID iidGuid;
    hr = CLSIDFromString(std::wstring(iid.begin(), iid.end()).c_str(), &iidGuid);
    if (FAILED(hr)) {
        std::wstring wIid = L"{" + std::wstring(iid.begin(), iid.end()) + L"}";
        hr = CLSIDFromString(wIid.c_str(), &iidGuid);
    }
    if (SUCCEEDED(hr)) {
        pCTI->SetGuid(iidGuid);
    }

    // dispinterface 类型 - CreateTypeInfo 的 TKIND_DISPATCH 自动继承 IDispatch
    // 无需手动设置基接口

    // 添加方法
    for (size_t i = 0; i < methods.size(); i++) {
        const auto& m = methods[i];

        // 构造 FUNCDESC
        FUNCDESC fd{};
        fd.memid = m.dispid;
        fd.lprgscode = nullptr;
        fd.lprgelemdescParam = nullptr;
        fd.funckind = FUNC_DISPATCH;  // dispinterface 用 FUNC_DISPATCH
        fd.callconv = CC_STDCALL;
        fd.cParams = (SHORT)m.params.size();
        fd.cParamsOpt = 0;
        fd.oVft = 0;   // dispinterface 不使用 vtable 偏移
        fd.wFuncFlags = 0;

        // 返回类型 (Property Let/Set 无返回值)
        if (m.isPropertyPut || m.isPropertyPutRef) {
            fd.elemdescFunc.tdesc.vt = VT_VOID;
        } else {
            fd.elemdescFunc.tdesc.vt = mapVartype(m.returnType);
        }

        // 设置 INVOKE_KIND
        if (m.isPropertyGet) {
            fd.invkind = INVOKE_PROPERTYGET;
        } else if (m.isPropertyPut) {
            fd.invkind = INVOKE_PROPERTYPUT;
        } else if (m.isPropertyPutRef) {
            fd.invkind = INVOKE_PROPERTYPUTREF;
        } else {
            fd.invkind = INVOKE_FUNC;
        }

        // 参数描述
        std::vector<ELEMDESC> paramDescs;
        if (!m.params.empty()) {
            paramDescs.resize(m.params.size());
            for (size_t p = 0; p < m.params.size(); p++) {
                paramDescs[p].tdesc.vt = mapVartype(m.params[p].type);
                paramDescs[p].idldesc.dwReserved = 0;
                // ByVal 传值, ByRef 传指针
                if (m.params[p].isByVal) {
                    paramDescs[p].paramdesc.wParamFlags = PARAMFLAG_FIN;
                } else {
                    // ByRef: 标记为指针类型
                    // 注意: dispinterface 的 ByRef 参数 tdesc.vt 应保留原类型,
                    // 参数标志用 PARAMFLAG_FIN | PARAMFLAG_FOUT 表示 In/Out
                    paramDescs[p].paramdesc.wParamFlags = PARAMFLAG_FIN | PARAMFLAG_FOUT;
                }
                // 使用 VARIANT 描述可选参数
                if (m.params[p].isOptional) {
                    paramDescs[p].paramdesc.wParamFlags |= PARAMFLAG_FOPT;
                    fd.cParamsOpt++;
                }
            }
            fd.lprgelemdescParam = paramDescs.data();
        }

        hr = pCTI->AddFuncDesc((UINT)i, &fd);
        if (FAILED(hr)) {
            lastError_ = "AddFuncDesc failed for " + m.name + ": 0x" + std::to_string(hr);
            pCTI->Release();
            return false;
        }

        // 设置方法名+参数名
        // 关键: 对于 Property Put/PutRef, 最后一个参数 (RHS) 是未命名的
        // MSDN: "The last parameter for put and putref accessor functions is unnamed."
        // 所以 cNames = 1 + (cParams - 1 for propput, else cParams)
        int mwlen = MultiByteToWideChar(CP_UTF8, 0, m.name.c_str(), -1, nullptr, 0);
        std::vector<WCHAR> wMName(mwlen);
        MultiByteToWideChar(CP_UTF8, 0, m.name.c_str(), -1, wMName.data(), mwlen);

        UINT cParamNames = (UINT)m.params.size();
        // Property Put/PutRef: 最后一个参数 (RHS value) 不命名
        if ((m.isPropertyPut || m.isPropertyPutRef) && !m.params.empty()) {
            cParamNames = (UINT)m.params.size() - 1;
        }

        UINT cNames = 1 + cParamNames;
        std::vector<OLECHAR*> namePtrs(cNames);
        std::vector<std::vector<WCHAR>> paramNameBufs;

        namePtrs[0] = wMName.data();
        for (size_t p = 0; p < cParamNames; p++) {
            int pwlen = MultiByteToWideChar(CP_UTF8, 0, m.params[p].name.c_str(), -1, nullptr, 0);
            std::vector<WCHAR> wPName(pwlen);
            MultiByteToWideChar(CP_UTF8, 0, m.params[p].name.c_str(), -1, wPName.data(), pwlen);
            paramNameBufs.push_back(std::move(wPName));
            namePtrs[p + 1] = paramNameBufs.back().data();
        }

        hr = pCTI->SetFuncAndParamNames((UINT)i, namePtrs.data(), cNames);
        if (FAILED(hr)) {
            std::string detail = "SetFuncAndParamNames failed for " + m.name + ": 0x" + std::to_string(hr)
                + " [idx=" + std::to_string(i)
                + " dispid=" + std::to_string(m.dispid)
                + " get=" + (m.isPropertyGet ? "1" : "0")
                + " put=" + (m.isPropertyPut ? "1" : "0")
                + " putref=" + (m.isPropertyPutRef ? "1" : "0")
                + " nparams=" + std::to_string(m.params.size())
                + " cNames=" + std::to_string(cNames)
                + " params=[";
            for (size_t p = 0; p < m.params.size(); p++) {
                detail += m.params[p].name;
                detail += "(";
                detail += m.params[p].isByVal ? "byval" : "byref";
                detail += m.params[p].isOptional ? ",opt" : "";
                detail += "),";
            }
            detail += "]]";
            lastError_ = detail;
            pCTI->Release();
            return false;
        }
    } // end for each method

    // LayOut
    hr = pCTI->LayOut();
    if (FAILED(hr)) {
        lastError_ = "LayOut failed for " + name + ": 0x" + std::to_string(hr);
        pCTI->Release();
        return false;
    }

    // 保存 TypeInfo 指针 (供 coclass AddRefTypeInfo 引用)
    // pCTI 不释放, 留到 endLib() 释放
    interfaces_.push_back({name, nullptr, pCTI, (int32_t)interfaces_.size()});
    return true;
#else
    (void)name; (void)iidStr; (void)methods;
    lastError_ = "Not supported on this platform";
    return false;
#endif
}

// ============================================================
// ai/022 B15/B16: 添加真接口 (TKIND_INTERFACE) + 契约成员
// ============================================================
// 与 addDispInterface 的差别是 kind / funckind / 槽偏移三样：这一档的 GUID 就是 QI 认的
// 那枚 IID，成员是 vtable 槽。B15 只发形状与身份（成员面 0）；B16 把成员如实发进去：
//   - `FUNC_PUREVIRTUAL` + `oVft=(3+槽号)*指针宽`。3 = IUnknown 前缀三槽（B13d 规范 IUnknown）。
//     指针宽按本端目标（`is64_`），因为实测 oVft 是"写入端按本端指针宽存、读取端按本端
//     指针宽换算"（D62-3 + B16 测量③）。
//   - `CC_STDCALL`：生成码自 B16 起整条薄面都带 `__stdcall`（B16 测量①：之前 x86 下是
//     `__cdecl`，库里没法如实写 stdcall）。
//   - 参数类型保真：ByRef 走 `VT_PTR` + `lptdesc` 真链（D62-1 实测：只置 VT_PTR 而
//     lptdesc 空会让建库进程当场崩），ByVal 直接用基础 vt。类型映射的已知缺档（LongPtr/
//     LongLong/UDT/数组 → `mapVartype` 的 VT_VARIANT 兜底）不在本批，见 022 D63。
//   - **返回值仍是原生类型**（Sub→VT_VOID、Function/Property Get→值类型），不是 canonical
//     COM 的 HRESULT + [out,retval] —— 那是另一半，本批刻意留给 B17，理由见 D63；
//     本批的口径是"广告 == 应答"，不是"广告 == MS 规范"。
// 接口那一档不吃 `SetTypeFlags(TYPEFLAG_FCANCREATE)`（D62-1 实测 `0x800288BD`），不碰 flags。

bool TypeLibBuilder::addVtableInterface(const std::string& name,
                                        const std::string& iidStr,
                                        const std::vector<MethodInfo>& methods) {
#ifdef _WIN32
    if (!pCreateLib_) {
        lastError_ = "beginLib() not called";
        return false;
    }

    auto* pCTL = static_cast<ICreateTypeLib2*>(pCreateLib_);

    int wlen = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, nullptr, 0);
    std::vector<WCHAR> wName(wlen);
    MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, wName.data(), wlen);

    ICreateTypeInfo* pCTI = nullptr;
    HRESULT hr = pCTL->CreateTypeInfo(wName.data(), TKIND_INTERFACE, &pCTI);
    if (FAILED(hr) || !pCTI) {
        lastError_ = "CreateTypeInfo(interface) failed for " + name + ": 0x" + std::to_string(hr);
        return false;
    }

    std::string iid = iidStr.empty() ? generateUuid(name) : iidStr;
    GUID iidGuid;
    hr = CLSIDFromString(std::wstring(iid.begin(), iid.end()).c_str(), &iidGuid);
    if (FAILED(hr)) {
        std::wstring wIid = L"{" + std::wstring(iid.begin(), iid.end()) + L"}";
        hr = CLSIDFromString(wIid.c_str(), &iidGuid);
    }
    if (FAILED(hr)) {
        lastError_ = "Bad IID for interface " + name + ": " + iid;
        pCTI->Release();
        return false;
    }
    pCTI->SetGuid(iidGuid);

    // 每个槽一条 FUNCDESC。槽号即下标：与生成码里 vb6_ivtbl_<I> 的槽序逐位对应
    // （含继承来的槽，展平后在前 —— 两边读的是同一份 IfaceView::slots）。
    const int ptrSize = is64_ ? 8 : 4;
    for (size_t i = 0; i < methods.size(); i++) {
        const auto& m = methods[i];

        FUNCDESC fd{};
        fd.memid = m.dispid;
        fd.lprgscode = nullptr;
        fd.lprgelemdescParam = nullptr;
        fd.funckind = FUNC_PUREVIRTUAL;
        fd.callconv = CC_STDCALL;
        fd.cParams = (SHORT)m.params.size();
        fd.cParamsOpt = 0;
        fd.oVft = (SHORT)((3 + (SHORT)i) * ptrSize);
        fd.wFuncFlags = 0;

        if (m.isPropertyPut || m.isPropertyPutRef) {
            fd.elemdescFunc.tdesc.vt = VT_VOID;
        } else {
            fd.elemdescFunc.tdesc.vt = mapVartype(m.returnType);
        }

        if (m.isPropertyGet) {
            fd.invkind = INVOKE_PROPERTYGET;
        } else if (m.isPropertyPut) {
            fd.invkind = INVOKE_PROPERTYPUT;
        } else if (m.isPropertyPutRef) {
            fd.invkind = INVOKE_PROPERTYPUTREF;
        } else {
            fd.invkind = INVOKE_FUNC;
        }

        std::vector<ELEMDESC> paramDescs;
        std::vector<TYPEDESC> innerDescs;
        if (!m.params.empty()) {
            paramDescs.resize(m.params.size());
            innerDescs.resize(m.params.size());
            for (size_t p = 0; p < m.params.size(); p++) {
                innerDescs[p].vt = mapVartype(m.params[p].type);
                innerDescs[p].lptdesc = nullptr;
                innerDescs[p].lpadesc = nullptr;
                innerDescs[p].hreftype = 0;
                if (m.params[p].isByVal || m.params[p].isParamArray) {
                    paramDescs[p].tdesc.vt = innerDescs[p].vt;
                    paramDescs[p].paramdesc.wParamFlags = PARAMFLAG_FIN;
                } else {
                    // ByRef: 真链 VT_PTR -> 内层类型 (D62-1 mode 7 实测可用的建法)
                    paramDescs[p].tdesc.vt = VT_PTR;
                    paramDescs[p].tdesc.lptdesc = &innerDescs[p];
                    paramDescs[p].paramdesc.wParamFlags = PARAMFLAG_FIN | PARAMFLAG_FOUT;
                }
                paramDescs[p].idldesc.dwReserved = 0;
                if (m.params[p].isOptional) {
                    paramDescs[p].paramdesc.wParamFlags |= PARAMFLAG_FOPT;
                    fd.cParamsOpt++;
                }
            }
            // AddFuncDesc 之后这串内层 TYPEDESC 仍可能被读回 → 换到堆上, 活到析构
            TYPEDESC* keep = new TYPEDESC[innerDescs.size()];
            for (size_t p = 0; p < innerDescs.size(); p++) keep[p] = innerDescs[p];
            vtableInnerAllocs_.push_back(keep);
            for (size_t p = 0; p < paramDescs.size(); p++) {
                if (paramDescs[p].tdesc.vt == VT_PTR) paramDescs[p].tdesc.lptdesc = &keep[p];
            }
            fd.lprgelemdescParam = paramDescs.data();
        }

        hr = pCTI->AddFuncDesc((UINT)i, &fd);
        if (FAILED(hr)) {
            lastError_ = "AddFuncDesc(vtable) failed for " + name + "." + m.name +
                         " (oVft=" + std::to_string((int)fd.oVft) + "): 0x" + std::to_string(hr);
            pCTI->Release();
            return false;
        }

        int mwlen = MultiByteToWideChar(CP_UTF8, 0, m.name.c_str(), -1, nullptr, 0);
        std::vector<WCHAR> wMName(mwlen);
        MultiByteToWideChar(CP_UTF8, 0, m.name.c_str(), -1, wMName.data(), mwlen);

        // Property Put/PutRef 的最后一个参数 (RHS) 在 COM 里是不命名的 (同 addDispInterface)
        UINT cParamNames = (UINT)m.params.size();
        if ((m.isPropertyPut || m.isPropertyPutRef) && !m.params.empty()) {
            cParamNames = (UINT)m.params.size() - 1;
        }
        std::vector<std::vector<WCHAR>> paramNameStore(cParamNames);
        std::vector<OLECHAR*> namePtrs(1 + cParamNames);
        namePtrs[0] = wMName.data();
        for (UINT p = 0; p < cParamNames; p++) {
            int pw = MultiByteToWideChar(CP_UTF8, 0, m.params[p].name.c_str(), -1, nullptr, 0);
            paramNameStore[p].resize(pw);
            MultiByteToWideChar(CP_UTF8, 0, m.params[p].name.c_str(), -1, paramNameStore[p].data(), pw);
            namePtrs[1 + p] = paramNameStore[p].data();
        }
        hr = pCTI->SetFuncAndParamNames((UINT)i, namePtrs.data(), 1 + cParamNames);
        if (FAILED(hr)) {
            lastError_ = "SetFuncAndParamNames(vtable) failed for " + name + "." + m.name +
                         ": 0x" + std::to_string(hr);
            pCTI->Release();
            return false;
        }
    }

    hr = pCTI->LayOut();
    if (FAILED(hr)) {
        lastError_ = "LayOut failed for interface " + name + ": 0x" + std::to_string(hr);
        pCTI->Release();
        return false;
    }

    interfaces_.push_back({name, nullptr, pCTI, (int32_t)interfaces_.size()});
    return true;
#else
    (void)name; (void)iidStr; (void)methods;
    lastError_ = "Not supported on this platform";
    return false;
#endif
}

} // namespace vb6c3
