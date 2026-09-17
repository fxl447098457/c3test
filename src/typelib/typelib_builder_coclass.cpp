// P9: TypeLib 内建生成器实现 — coclass 描述 + endLib
// 由 src/typelib/typelib_builder.cpp 拆出（2026-09-17），纯搬移、零行为改动。

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
// 添加 coclass
// ============================================================

bool TypeLibBuilder::addCoClass(const std::string& name,
                                 const std::string& clsidStr,
                                 const std::string& ifaceName,
                                 const std::string& sourceIfaceName) {
#ifdef _WIN32
    if (!pCreateLib_) {
        return false;
    }

    auto* pCTL = static_cast<ICreateTypeLib2*>(pCreateLib_);

    // 创建 TypeInfo
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

    // 设置 type flags
    pCTI->SetTypeFlags(TYPEFLAG_FCANCREATE);

    // 查找默认接口 — 使用 AddRefTypeInfo 获取正确的 HREFTYPE
    int ifaceIdx = -1;
    for (size_t i = 0; i < interfaces_.size(); i++) {
        if (interfaces_[i].name == ifaceName) {
            ifaceIdx = (int)i;
            break;
        }
    }
    if (ifaceIdx < 0) {
        lastError_ = "Interface not found: " + ifaceName;
        pCTI->Release();
        return false;
    }

    // 从保存的 ICreateTypeInfo 获得 ITypeInfo (同一 TypeLib 内, AddRefTypeInfo 必须)
    {
        auto* srcCTI = static_cast<ICreateTypeInfo*>(interfaces_[ifaceIdx].pCreateTypeInfo);
        ITypeInfo* pSrcTI = nullptr;
        hr = srcCTI->QueryInterface(IID_ITypeInfo, (void**)&pSrcTI);
        if (FAILED(hr) || !pSrcTI) {
            lastError_ = "QI ITypeInfo failed for " + ifaceName;
            pCTI->Release();
            return false;
        }

        HREFTYPE hRefType = 0;
        hr = pCTI->AddRefTypeInfo(pSrcTI, &hRefType);
        pSrcTI->Release();

        if (FAILED(hr)) {
            lastError_ = "AddRefTypeInfo failed for " + ifaceName + ": 0x" + std::to_string(hr);
            pCTI->Release();
            return false;
        }

        // AddImplType: index=0 (第一个实现的接口), hRefType
        hr = pCTI->AddImplType(0, hRefType);
        if (FAILED(hr)) {
            lastError_ = "AddImplType failed for " + ifaceName + ": 0x" + std::to_string(hr) + " (hRefType=" + std::to_string(hRefType) + ")";
            pCTI->Release();
            return false;
        }
    }

    // Set default interface flag
    pCTI->SetImplTypeFlags(0, IMPLTYPEFLAG_FDEFAULT);

    // 添加事件源接口 (source dispinterface)
    if (!sourceIfaceName.empty()) {
        int srcIdx = -1;
        for (size_t i = 0; i < interfaces_.size(); i++) {
            if (interfaces_[i].name == sourceIfaceName) {
                srcIdx = (int)i;
                break;
            }
        }
        if (srcIdx >= 0) {
            auto* srcCTI = static_cast<ICreateTypeInfo*>(interfaces_[srcIdx].pCreateTypeInfo);
            ITypeInfo* pSrcTI = nullptr;
            hr = srcCTI->QueryInterface(IID_ITypeInfo, (void**)&pSrcTI);
            if (SUCCEEDED(hr) && pSrcTI) {
                HREFTYPE hSrcRefType = 0;
                hr = pCTI->AddRefTypeInfo(pSrcTI, &hSrcRefType);
                pSrcTI->Release();
                if (SUCCEEDED(hr)) {
                    hr = pCTI->AddImplType(1, hSrcRefType);
                    if (SUCCEEDED(hr)) {
                        pCTI->SetImplTypeFlags(1, IMPLTYPEFLAG_FDEFAULT | IMPLTYPEFLAG_FSOURCE);
                    }
                }
            }
        }
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
    (void)name; (void)clsidStr; (void)ifaceName; (void)sourceIfaceName;
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

    // 释放接口引用 (必须在 SaveAllChanges 之前释放所有 ICreateTypeInfo)
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
    interfaces_.clear();

    // 保存所有更改
    HRESULT hr = pCTL->SaveAllChanges();
    if (FAILED(hr)) {
        lastError_ = "SaveAllChanges failed: 0x" + std::to_string(hr);
        return false;
    }

    // 释放
    pCTL->Release();
    pCreateLib_ = nullptr;

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
