// P9: TypeLib 内建生成器实现
// 使用 CreateTypeLib2 / ICreateTypeInfo API

#include "typelib/typelib_builder.hpp"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstdint>

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
        (unsigned int)(h2 >> 48),
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
        if (iface.pTypeInfo) {
            static_cast<ITypeInfo*>(iface.pTypeInfo)->Release();
            iface.pTypeInfo = nullptr;
        }
    }
    if (libOpen_) {
        CoUninitialize();
    }
#endif
}

bool TypeLibBuilder::beginLib(const std::string& tlbPath,
                               const std::string& libidStr,
                               const std::string& helpString,
                               const std::string& libName) {
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

    // 创建 TypeLib (SYS_WIN64)
    ICreateTypeLib2* pCTL = nullptr;
    hr = CreateTypeLib2(SYS_WIN64, wPath.data(), &pCTL);
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

        // 返回类型
        fd.elemdescFunc.tdesc.vt = mapVartype(m.returnType);

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
                    paramDescs[p].tdesc.vt |= VT_BYREF;
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

        // 设置方法名+参数名 (ICreateTypeInfo::SetFuncAndParamNames: index, name, cNames, *rgsNames)
        int mwlen = MultiByteToWideChar(CP_UTF8, 0, m.name.c_str(), -1, nullptr, 0);
        std::vector<WCHAR> wMName(mwlen);
        MultiByteToWideChar(CP_UTF8, 0, m.name.c_str(), -1, wMName.data(), mwlen);

        if (!m.params.empty()) {
            // 构造名称数组: [方法名, 参数1, 参数2, ...]
            UINT cNames = 1 + (UINT)m.params.size();
            std::vector<OLECHAR*> namePtrs(cNames);
            std::vector<std::vector<WCHAR>> paramNameBufs;

            namePtrs[0] = wMName.data();
            for (size_t p = 0; p < m.params.size(); p++) {
                int pwlen = MultiByteToWideChar(CP_UTF8, 0, m.params[p].name.c_str(), -1, nullptr, 0);
                std::vector<WCHAR> wPName(pwlen);
                MultiByteToWideChar(CP_UTF8, 0, m.params[p].name.c_str(), -1, wPName.data(), pwlen);
                paramNameBufs.push_back(std::move(wPName));
                namePtrs[p + 1] = paramNameBufs.back().data();
            }

            hr = pCTI->SetFuncAndParamNames((UINT)i, namePtrs.data(), cNames);
        } else {
            // 无参数：只需方法名
            OLECHAR* names[] = { wMName.data() };
            hr = pCTI->SetFuncAndParamNames((UINT)i, names, 1);
        }

        if (FAILED(hr)) {
            lastError_ = "SetFuncAndParamNames failed for " + m.name + ": 0x" + std::to_string(hr);
            pCTI->Release();
            return false;
        }
    }

    // LayOut
    hr = pCTI->LayOut();
    if (FAILED(hr)) {
        lastError_ = "LayOut failed for " + name + ": 0x" + std::to_string(hr);
        pCTI->Release();
        return false;
    }

    // 保存 TypeInfo 指针 (供 coclass AddImplType 引用)
    ITypeInfo* pTI = nullptr;
    pCTI->QueryInterface(IID_ITypeInfo, (void**)&pTI);
    pCTI->Release();  // 释放 CreateTypeInfo, 保留 ITypeInfo

    interfaces_.push_back({name, pTI, (int32_t)interfaces_.size()});
    return true;
#else
    (void)name; (void)iidStr; (void)methods;
    lastError_ = "Not supported on this platform";
    return false;
#endif
}

// ============================================================
// 添加 coclass
// ============================================================

bool TypeLibBuilder::addCoClass(const std::string& name,
                                 const std::string& clsidStr,
                                 const std::string& ifaceName) {
#ifdef _WIN32
    if (!pCreateLib_) {
        lastError_ = "beginLib() not called";
        return false;
    }

    auto* pCTL = static_cast<ICreateTypeLib2*>(pCreateLib_);

    // 创建 TypeInfo (ICreateTypeInfo, not ICreateTypeInfo2)
    int wlen = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, nullptr, 0);
    std::vector<WCHAR> wName(wlen);
    MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, wName.data(), wlen);

    ICreateTypeInfo* pCTI = nullptr;
    HRESULT hr = pCTL->CreateTypeInfo(wName.data(), TKIND_COCLASS, &pCTI);
    if (FAILED(hr) || !pCTI) {
        lastError_ = "CreateTypeInfo(coclass) failed for " + name + ": 0x" + std::to_string(hr);
        return false;
    }

    // 设置 CLSID
    std::string clsid = clsidStr.empty() ? generateUuid(name) : clsidStr;
    GUID clsidGuid;
    hr = CLSIDFromString(std::wstring(clsid.begin(), clsid.end()).c_str(), &clsidGuid);
    if (FAILED(hr)) {
        std::wstring wClsid = L"{" + std::wstring(clsid.begin(), clsid.end()) + L"}";
        hr = CLSIDFromString(wClsid.c_str(), &clsidGuid);
    }
    if (SUCCEEDED(hr)) {
        pCTI->SetGuid(clsidGuid);
    }

    // 设置 type flags: can create
    pCTI->SetTypeFlags(TYPEFLAG_FCANCREATE);

    // 查找默认接口 — 用 TypeLib 内序号作为 AddImplType 参数
    int ifaceIdx = -1;
    ITypeInfo* pIfaceTI = nullptr;
    for (size_t i = 0; i < interfaces_.size(); i++) {
        if (interfaces_[i].name == ifaceName) {
            ifaceIdx = interfaces_[i].index;
            pIfaceTI = static_cast<ITypeInfo*>(interfaces_[i].pTypeInfo);
            break;
        }
    }
    if (ifaceIdx < 0 || !pIfaceTI) {
        lastError_ = "Interface not found: " + ifaceName;
        pCTI->Release();
        return false;
    }

    // AddImplType: index 为接口在 TypeLib 中的创建序号, flags 标记为默认接口
    hr = pCTI->AddImplType(ifaceIdx, IMPLTYPEFLAG_FDEFAULT);
    if (FAILED(hr)) {
        lastError_ = "AddImplType failed for " + ifaceName + ": 0x" + std::to_string(hr);
        pCTI->Release();
        return false;
    }

    // LayOut
    hr = pCTI->LayOut();
    if (FAILED(hr)) {
        lastError_ = "LayOut failed for coclass " + name + ": 0x" + std::to_string(hr);
        pCTI->Release();
        return false;
    }

    pCTI->Release();
    return true;
#else
    (void)name; (void)clsidStr; (void)ifaceName;
    lastError_ = "Not supported on this platform";
    return false;
#endif
}

// ============================================================
// 结束构建
// ============================================================

bool TypeLibBuilder::endLib(const std::string& tlbPath) {
#ifdef _WIN32
    if (!pCreateLib_) {
        lastError_ = "beginLib() not called";
        return false;
    }
    (void)tlbPath;  // 文件路径已在 beginLib 中指定

    auto* pCTL = static_cast<ICreateTypeLib2*>(pCreateLib_);

    // 保存所有更改
    HRESULT hr = pCTL->SaveAllChanges();
    if (FAILED(hr)) {
        lastError_ = "SaveAllChanges failed: 0x" + std::to_string(hr);
        return false;
    }

    // 释放
    pCTL->Release();
    pCreateLib_ = nullptr;

    // 清理接口引用
    for (auto& iface : interfaces_) {
        if (iface.pTypeInfo) {
            static_cast<ITypeInfo*>(iface.pTypeInfo)->Release();
            iface.pTypeInfo = nullptr;
        }
    }
    interfaces_.clear();

    CoUninitialize();
    libOpen_ = false;

    return true;
#else
    (void)tlbPath;
    lastError_ = "Not supported on this platform";
    return false;
#endif
}

} // namespace vb6c3
