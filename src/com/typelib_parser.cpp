// VB6 TypeLib解析器实现 - P6.3 前期绑定支持
// 编译期使用Windows LoadTypeLib/ITypeInfo API

#include "com/typelib_parser.hpp"
#include <algorithm>
#include <windows.h>
#include <oleauto.h>

namespace vb6c3 {

// ============================================================
// 构造/析构
// ============================================================

TypeLibParser::TypeLibParser(Diagnostics& diag)
    : diag_(diag) {}

TypeLibParser::~TypeLibParser() = default;

// ============================================================
// 公共接口
// ============================================================

std::unique_ptr<TypeLibResult> TypeLibParser::loadByProgId(const std::string& progId) {
    // 1. 查找缓存
    ComCoClassInfo* cached = findCachedCoClass(progId);
    if (cached) {
        // 已缓存, 返回所在TypeLib的拷贝引用 (无需重新解析)
        // 返回nullptr表示已在缓存中, 调用方应使用findCachedCoClass
        return nullptr;
    }

    // 2. 从注册表查找TypeLib路径
    std::string tlbPath = findTypeLibPathForProgId(progId);
    if (tlbPath.empty()) {
        diag_.warn(DiagnosticID::CodeGenUnsupportedFeature, SourceLocation{}, "TypeLib not found for ProgID: " + progId);
        return nullptr;
    }

    // 3. 按路径加载
    return loadByPath(tlbPath);
}

std::unique_ptr<TypeLibResult> TypeLibParser::loadByPath(const std::string& tlbPath) {
    // P24-05: 规范化路径(短路径→长路径), 避免同DLL不同路径导致缓存未命中
    std::wstring tlbPathW(tlbPath.begin(), tlbPath.end());
    wchar_t canonicalPath[MAX_PATH];
    DWORD len = GetLongPathNameW(tlbPathW.c_str(), canonicalPath, MAX_PATH);
    std::string canonPath;
    if (len > 0 && len < MAX_PATH) {
        // 统一转小写用于缓存比较 (Windows路径不区分大小写)
        canonPath = std::string(canonicalPath, canonicalPath + len);
        for (auto& c : canonPath) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    } else {
        canonPath = tlbPath;
        for (auto& c : canonPath) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    // 检查缓存 (用规范化路径比较)
    for (auto& cached : cache_) {
        if (cached->canonPath == canonPath) {
            return nullptr;  // 已缓存
        }
    }

    // 加载TypeLib
    ITypeLib* pTypeLib = nullptr;
    HRESULT hr = LoadTypeLibEx(
        tlbPathW.c_str(),
        REGKIND_NONE,
        &pTypeLib
    );

    if (FAILED(hr) || !pTypeLib) {
        diag_.warn(DiagnosticID::CodeGenUnsupportedFeature, SourceLocation{},
                   "Failed to load TypeLib: " + tlbPath +
                   " (hr=0x" + std::to_string(hr) + ")");
        return nullptr;
    }

    // 解析
    auto result = std::make_unique<TypeLibResult>();
    result->tlbPath = tlbPath;
    result->canonPath = canonPath;

    bool ok = parseTypeLib(pTypeLib, *result);
    pTypeLib->Release();

    if (!ok) {
        diag_.warn(DiagnosticID::CodeGenUnsupportedFeature, SourceLocation{},
                   "Failed to parse TypeLib: " + tlbPath);
        return nullptr;
    }

    // 链接coclass → default interface
    for (auto& cc : result->coclasses) {
        if (!cc->defaultIfaceName.empty()) {
            cc->defaultIface = result->findInterface(cc->defaultIfaceName);
        // P13.20: Link default source (event) interface
        if (!cc->defaultSourceIfaceName.empty()) {
            cc->defaultSourceIface = result->findInterface(cc->defaultSourceIfaceName);
        }
        }
    }

    // 缓存
    cache_.push_back(std::move(result));
    // 返回nullptr表示已缓存, 调用方应使用findCachedCoClass或cachedResults()
    return nullptr;
}

std::unique_ptr<TypeLibResult> TypeLibParser::loadByClsid(const std::string& clsidStr) {
    // P24-05: Object=行的GUID可能是TypeLib ID或CoClass CLSID
    // 先尝试按TypeLib ID直接查找 HKCR\TypeLib\{guid}, 失败再走CLSID反查
    std::wstring guidW(clsidStr.begin(), clsidStr.end());

    // --- 尝试1: 直接按TypeLib ID查找 ---
    {
        std::wstring tlbidKey = L"TypeLib\\" + guidW;
        HKEY hTlbKey;
        if (RegOpenKeyExW(HKEY_CLASSES_ROOT, tlbidKey.c_str(), 0, KEY_READ, &hTlbKey) == ERROR_SUCCESS) {
            // TypeLib ID有效, 枚举版本找win32路径
            wchar_t version[32];
            DWORD verSz = sizeof(version);
            DWORD idx = 0;
            std::wstring latestPath;
            while (RegEnumKeyExW(hTlbKey, idx, version, &verSz, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
                verSz = sizeof(version);
                std::wstring pathKey = tlbidKey + L"\\" + version + L"\\0\\win32";
                HKEY hPathKey;
                if (RegOpenKeyExW(HKEY_CLASSES_ROOT, pathKey.c_str(), 0, KEY_READ, &hPathKey) == ERROR_SUCCESS) {
                    wchar_t path[MAX_PATH];
                    DWORD pathSz = sizeof(path);
                    if (RegQueryValueExW(hPathKey, nullptr, nullptr, nullptr, (LPBYTE)path, &pathSz) == ERROR_SUCCESS) {
                        latestPath = path;
                    }
                    RegCloseKey(hPathKey);
                }
                idx++;
            }
            RegCloseKey(hTlbKey);
            if (!latestPath.empty()) {
                std::string pathStr(latestPath.begin(), latestPath.end());
                return loadByPath(pathStr);
            }
        }
    }

    // --- 尝试2: 按CoClass CLSID反查 TypeLib ---
    // 格式: HKCR\CLSID\{...}\TypeLib → {typelibid}
    CLSID clsid;
    HRESULT hr = CLSIDFromString(guidW.c_str(), &clsid);
    if (FAILED(hr)) return nullptr;

    // 查TypeLib子键
    std::wstring keyPath = L"CLSID\\" + guidW + L"\\TypeLib";
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CLASSES_ROOT, keyPath.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return nullptr;
    }
    wchar_t tlbidStr[64];
    DWORD sz = sizeof(tlbidStr);
    if (RegQueryValueExW(hKey, nullptr, nullptr, nullptr, (LPBYTE)tlbidStr, &sz) != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return nullptr;
    }
    RegCloseKey(hKey);

    // TypeLib ID → 查TypeLib版本路径
    std::wstring tlbidKey2 = L"TypeLib\\" + std::wstring(tlbidStr);
    HKEY hTlbKey2;
    if (RegOpenKeyExW(HKEY_CLASSES_ROOT, tlbidKey2.c_str(), 0, KEY_READ, &hTlbKey2) != ERROR_SUCCESS) {
        return nullptr;
    }
    wchar_t version2[32];
    DWORD idx2 = 0;
    std::wstring latestPath2;
    while (RegEnumKeyExW(hTlbKey2, idx2, version2, &sz, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        sz = sizeof(version2);
        std::wstring pathKey = tlbidKey2 + L"\\" + version2 + L"\\0\\win32";
        HKEY hPathKey;
        if (RegOpenKeyExW(HKEY_CLASSES_ROOT, pathKey.c_str(), 0, KEY_READ, &hPathKey) == ERROR_SUCCESS) {
            wchar_t path[MAX_PATH];
            DWORD pathSz = sizeof(path);
            if (RegQueryValueExW(hPathKey, nullptr, nullptr, nullptr, (LPBYTE)path, &pathSz) == ERROR_SUCCESS) {
                latestPath2 = path;
            }
            RegCloseKey(hPathKey);
        }
        idx2++;
    }
    RegCloseKey(hTlbKey2);

    if (latestPath2.empty()) return nullptr;

    std::string pathStr2(latestPath2.begin(), latestPath2.end());
    return loadByPath(pathStr2);
}

std::unique_ptr<TypeLibResult> TypeLibParser::loadByName(const std::string& name,
                                                          const std::string& version) {
    // 按TypeLib名称从注册表查找
    // 枚举 HKCR\TypeLib 下的子键, 匹配名称
    std::wstring nameW(name.begin(), name.end());
    std::wstring verW(version.begin(), version.end());

    HKEY hTlbRoot;
    if (RegOpenKeyExW(HKEY_CLASSES_ROOT, L"TypeLib", 0, KEY_READ, &hTlbRoot) != ERROR_SUCCESS) {
        return nullptr;
    }

    wchar_t tlbid[64];
    DWORD tlbidSz = sizeof(tlbid);
    DWORD idx = 0;
    while (RegEnumKeyExW(hTlbRoot, idx, tlbid, &tlbidSz, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        tlbidSz = sizeof(tlbid);
        // 检查此TLBID下是否有匹配版本
        std::wstring verKey = std::wstring(L"TypeLib\\") + tlbid + L"\\" + verW;
        HKEY hVer;
        if (RegOpenKeyExW(HKEY_CLASSES_ROOT, verKey.c_str(), 0, KEY_READ, &hVer) == ERROR_SUCCESS) {
            // 检查名称
            wchar_t tlbName[256];
            DWORD nameSz = sizeof(tlbName);
            if (RegQueryValueExW(hVer, nullptr, nullptr, nullptr, (LPBYTE)tlbName, &nameSz) == ERROR_SUCCESS) {
                if (_wcsicmp(tlbName, nameW.c_str()) == 0) {
                    RegCloseKey(hVer);
                    RegCloseKey(hTlbRoot);
                    // 找到! 加载win32路径
                    std::wstring pathKey = verKey + L"\\0\\win32";
                    HKEY hPath;
                    if (RegOpenKeyExW(HKEY_CLASSES_ROOT, pathKey.c_str(), 0, KEY_READ, &hPath) == ERROR_SUCCESS) {
                        wchar_t path[MAX_PATH];
                        DWORD pathSz = sizeof(path);
                        if (RegQueryValueExW(hPath, nullptr, nullptr, nullptr, (LPBYTE)path, &pathSz) == ERROR_SUCCESS) {
                            RegCloseKey(hPath);
                            // wchar_t → std::string (窄字符转换)
                            std::string pathStr;
                            DWORD charCount = pathSz / sizeof(wchar_t);
                            for (DWORD k = 0; k < charCount && path[k] != 0; k++) {
                                pathStr += (char)path[k];
                            }
                            return loadByPath(pathStr);
                        }
                        RegCloseKey(hPath);
                    }
                    return nullptr;
                }
            }
            RegCloseKey(hVer);
        }
        idx++;
    }
    RegCloseKey(hTlbRoot);
    return nullptr;
}

ComCoClassInfo* TypeLibParser::findCachedCoClass(const std::string& name) const {
    for (auto& tl : cache_) {
        ComCoClassInfo* cc = tl->findCoClass(name);
        if (cc) return cc;
    }
    return nullptr;
}

// ============================================================
// 内部: ProgID → TypeLib路径
// ============================================================

std::string TypeLibParser::findTypeLibPathForProgId(const std::string& progId) {
    // HKCR\ProgID\CLSID → CLSID值
    // HKCR\CLSID\{clsid}\TypeLib → TypeLib ID
    // HKCR\TypeLib\{tlbid}\{version}\0\win32 → 路径

    std::wstring progIdW(progId.begin(), progId.end());

    // 1. ProgID → CLSID
    CLSID clsid;
    HRESULT hr = CLSIDFromProgID(progIdW.c_str(), &clsid);
    if (FAILED(hr)) return {};

    // 2. CLSID → TypeLib ID
    OLECHAR clsidStr[64];
    StringFromGUID2(clsid, clsidStr, 64);
    std::wstring clsidKey = L"CLSID\\" + std::wstring(clsidStr) + L"\\TypeLib";

    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CLASSES_ROOT, clsidKey.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return {};
    }

    wchar_t tlbidStr[64];
    DWORD sz = sizeof(tlbidStr);
    if (RegQueryValueExW(hKey, nullptr, nullptr, nullptr, (LPBYTE)tlbidStr, &sz) != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return {};
    }
    RegCloseKey(hKey);

    // 3. TypeLib ID → 找最新版本 → win32路径
    std::wstring tlbKey = L"TypeLib\\" + std::wstring(tlbidStr);
    HKEY hTlbKey;
    if (RegOpenKeyExW(HKEY_CLASSES_ROOT, tlbKey.c_str(), 0, KEY_READ, &hTlbKey) != ERROR_SUCCESS) {
        return {};
    }

    // 枚举版本子键, 选最大的
    wchar_t bestVer[32] = {};
    DWORD idx = 0;
    wchar_t verBuf[32];
    DWORD verSz = sizeof(verBuf);
    while (RegEnumKeyExW(hTlbKey, idx, verBuf, &verSz, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        if (wcscmp(verBuf, bestVer) > 0) {
            wcscpy_s(bestVer, verBuf);
        }
        verSz = sizeof(verBuf);
        idx++;
    }
    RegCloseKey(hTlbKey);

    if (bestVer[0] == 0) return {};

    // 4. win32路径
    std::wstring pathKey = tlbKey + L"\\" + bestVer + L"\\0\\win32";
    HKEY hPathKey;
    if (RegOpenKeyExW(HKEY_CLASSES_ROOT, pathKey.c_str(), 0, KEY_READ, &hPathKey) != ERROR_SUCCESS) {
        return {};
    }

    wchar_t path[MAX_PATH];
    DWORD pathSz = sizeof(path);
    if (RegQueryValueExW(hPathKey, nullptr, nullptr, nullptr, (LPBYTE)path, &pathSz) != ERROR_SUCCESS) {
        RegCloseKey(hPathKey);
        return {};
    }
    RegCloseKey(hPathKey);

    // 转换为窄字符串
    std::string result;
    for (DWORD i = 0; i < pathSz / sizeof(wchar_t); i++) {
        if (path[i] == 0) break;
        result += (char)path[i];
    }
    return result;
}

// ============================================================
// 内部: 解析TypeLib
// ============================================================

bool TypeLibParser::parseTypeLib(void* pTypeLib, TypeLibResult& result) {
    ITypeLib* pTL = static_cast<ITypeLib*>(pTypeLib);

    // 获取TypeLib属性
    TLIBATTR* pTlbAttr = nullptr;
    HRESULT hr = pTL->GetLibAttr(&pTlbAttr);
    if (SUCCEEDED(hr) && pTlbAttr) {
        // 版本
        result.version = std::to_string(pTlbAttr->wMajorVerNum) + "." +
                         std::to_string(pTlbAttr->wMinorVerNum);
        pTL->ReleaseTLibAttr(pTlbAttr);
    }

    // 获取名称
    BSTR tlbName = nullptr;
    pTL->GetDocumentation(-1, &tlbName, nullptr, nullptr, nullptr);
    if (tlbName) {
        // 宽字符→窄字符
        result.name.clear();
        for (UINT i = 0; i < SysStringLen(tlbName); i++) {
            result.name += (char)tlbName[i];
        }
        SysFreeString(tlbName);
    }

    // P24-04: 捕获TypeLib项目名 (VB6 ActiveX DLL的Name=属性, 如"VBMANLIB")
    result.typeLibProjectName = result.name;

    // 枚举所有类型
    UINT count = pTL->GetTypeInfoCount();
    for (UINT i = 0; i < count; i++) {
        TYPEKIND typeKind;
        hr = pTL->GetTypeInfoType(i, &typeKind);
        if (FAILED(hr)) continue;

        ITypeInfo* pTI = nullptr;
        hr = pTL->GetTypeInfo(i, &pTI);
        if (FAILED(hr) || !pTI) continue;

        // 获取类型名
        BSTR typeBStr = nullptr;
        pTI->GetDocumentation(MEMBERID_NIL, &typeBStr, nullptr, nullptr, nullptr);
        std::string typeName;
        if (typeBStr) {
            for (UINT j = 0; j < SysStringLen(typeBStr); j++) {
                typeName += (char)typeBStr[j];
            }
            SysFreeString(typeBStr);
        }

        switch (typeKind) {
            case TKIND_INTERFACE:
            case TKIND_DISPATCH: {
                auto iface = parseInterface(pTI, typeName);
                if (iface) {
                    result.interfaces.push_back(std::move(iface));
                }
                break;
            }
            case TKIND_COCLASS: {
                auto coclass = parseCoClass(pTI, typeName);
                if (coclass) {
                    result.coclasses.push_back(std::move(coclass));
                }
                break;
            }
            case TKIND_MODULE: {
                // P24-04: 解析ActiveX DLL全局模块函数
                auto mod = parseModule(pTI, typeName, result.tlbPath);
                if (mod) {
                    result.modules.push_back(std::move(mod));
                }
                break;
            }
            case TKIND_ALIAS:
            case TKIND_RECORD:
            default:
                break;
        }

        pTI->Release();
    }

    // P24-04: 链接coclass → default interface (从 loadByPath 移至此处)
    // 必须在GlobalNameSpace检测之前完成, 因为检测依赖 defaultIface 指针
    for (auto& cc : result.coclasses) {
        if (!cc->defaultIfaceName.empty()) {
            cc->defaultIface = result.findInterface(cc->defaultIfaceName);
            // P13.20: Link default source (event) interface
            if (!cc->defaultSourceIfaceName.empty()) {
                cc->defaultSourceIface = result.findInterface(cc->defaultSourceIfaceName);
            }
        }
    }

    // P24-04: GlobalNameSpace检测后处理
    // VB_GlobalNameSpace=True的coclass, 其默认接口的Public方法提升为全局符号
    // 检测启发式: TYPEFLAG_FPREDECLID(0x0008) + 默认接口有方法, 且满足以下之一:
    //   (a) coclass名包含"Global" (如sGlobal)
    //   (b) 默认接口有方法名与TypeLib项目名匹配 (如VBMAN库的VBMAN()方法)
    // 对于每个检测到的GlobalNameSpace coclass, 设置isGlobalNamespace=true
    // 并将默认接口的custom方法(排除IUnknown/IDispatch标准方法)记录为promoted methods
    if (!result.typeLibProjectName.empty()) {
        std::string tlbNameLower = result.typeLibProjectName;
        std::transform(tlbNameLower.begin(), tlbNameLower.end(), tlbNameLower.begin(), ::tolower);
        for (auto& cc : result.coclasses) {
            // 初步筛选: coclass名含"Global" (不区分大小写)
            std::string ccNameLower = cc->name;
            std::transform(ccNameLower.begin(), ccNameLower.end(), ccNameLower.begin(), ::tolower);
            bool nameHasGlobal = ccNameLower.find("global") != std::string::npos;
            // 初步筛选: 默认接口有与TypeLib名同名的方法
            bool hasMatchingMethod = false;
            if (cc->defaultIface) {
                for (auto& m : cc->defaultIface->members) {
                    std::string mLower = m.realName;
                    std::transform(mLower.begin(), mLower.end(), mLower.begin(), ::tolower);
                    if (mLower == tlbNameLower) {
                        hasMatchingMethod = true;
                        break;
                    }
                }
            }
            if ((nameHasGlobal || hasMatchingMethod) && cc->defaultIface) {
                cc->isGlobalNamespace = true;
            }
        }
    }

    return !result.interfaces.empty() || !result.coclasses.empty() || !result.modules.empty();
}

// ============================================================
// 内部: 解析接口
// ============================================================

std::unique_ptr<ComInterfaceInfo> TypeLibParser::parseInterface(void* pTypeInfo,
                                                                 const std::string& name) {
    ITypeInfo* pTI = static_cast<ITypeInfo*>(pTypeInfo);
    auto iface = std::make_unique<ComInterfaceInfo>();
    iface->name = name;

    // 获取TYPEATTR
    TYPEATTR* pTypeAttr = nullptr;
    HRESULT hr = pTI->GetTypeAttr(&pTypeAttr);
    if (FAILED(hr) || !pTypeAttr) return nullptr;

    // IID
    iface->iidStr = iidToString((const uint8_t*)&pTypeAttr->guid);

    // 接口类型
    if (pTypeAttr->typekind == TKIND_DISPATCH) {
        iface->isDispatch = true;
    }
    if (pTypeAttr->wTypeFlags & TYPEFLAG_FDUAL) {
        iface->isDual = true;
    }

    // vtable起始偏移 (IUnknown=3, IDispatch=4+3=7)
    int vtableBase = 7;  // 对于dispinterface, vtable从第7个槽开始
    if (pTypeAttr->typekind == TKIND_INTERFACE) {
        // 纯IUnknown派生, vtable从第3个槽开始
        // 但如果有基接口, 需要调整
        if (pTypeAttr->cImplTypes > 0) {
            // 假设基接口是IUnknown, 3个方法
            vtableBase = 3;
        }
    }

    // 保存需要的值后释放
    UINT cFuncs = pTypeAttr->cFuncs;
    UINT cVars = pTypeAttr->cVars;
    TYPEKIND typeKindSaved = pTypeAttr->typekind;
    pTI->ReleaseTypeAttr(pTypeAttr);

    // 枚举方法/属性
    for (UINT i = 0; i < cFuncs; i++) {
        FUNCDESC* pFuncDesc = nullptr;
        hr = pTI->GetFuncDesc(i, &pFuncDesc);
        if (FAILED(hr) || !pFuncDesc) continue;

        ComMemberInfo member = parseFuncDesc(pTI, pFuncDesc, i);

        // vtable索引: 统一使用FUNCDESC.oVft / sizeof(void*)
        // 注意: 对于dispinterface/dual, 枚举序号i不等于vtable位置,
        // oVft才是正确的vtable字节偏移 (包含IUnknown+IDispatch槽位)
        if (pFuncDesc->oVft > 0) {
            member.vtableIndex = pFuncDesc->oVft / (int)sizeof(void*);
        } else {
            // 兜底: 对于没有oVft的接口, 使用计算值
            member.vtableIndex = vtableBase + (int)i;
        }

        iface->members.push_back(std::move(member));
        pTI->ReleaseFuncDesc(pFuncDesc);
    }

    // 枚举变量(属性)
    for (UINT i = 0; i < cVars; i++) {
        VARDESC* pVarDesc = nullptr;
        hr = pTI->GetVarDesc(i, &pVarDesc);
        if (FAILED(hr) || !pVarDesc) continue;

        ComMemberInfo member = parseVarDesc(pTI, pVarDesc);
        iface->members.push_back(std::move(member));
        pTI->ReleaseVarDesc(pVarDesc);
    }

    return iface;
}

// ============================================================
// 内部: 解析coclass
// ============================================================

std::unique_ptr<ComCoClassInfo> TypeLibParser::parseCoClass(void* pTypeInfo,
                                                             const std::string& name) {
    ITypeInfo* pTI = static_cast<ITypeInfo*>(pTypeInfo);
    auto cc = std::make_unique<ComCoClassInfo>();
    cc->name = name;

    // 获取TYPEATTR
    TYPEATTR* pTypeAttr = nullptr;
    HRESULT hr = pTI->GetTypeAttr(&pTypeAttr);
    if (FAILED(hr) || !pTypeAttr) return nullptr;

    cc->clsidStr = iidToString((const uint8_t*)&pTypeAttr->guid);

    // 查找默认接口 (IMPLTYPEFLAG_FDEFAULT)
    for (UINT i = 0; i < pTypeAttr->cImplTypes; i++) {
        INT implFlags = 0;
        HREFTYPE refType = 0;
        hr = pTI->GetImplTypeFlags(i, &implFlags);
        if (FAILED(hr)) continue;

        // 获取实现的接口类型
        hr = pTI->GetRefTypeOfImplType(i, &refType);
        if (FAILED(hr)) continue;

        ITypeInfo* pImplTI = nullptr;
        hr = pTI->GetRefTypeInfo(refType, &pImplTI);
        if (FAILED(hr) || !pImplTI) continue;

        BSTR implName = nullptr;
        pImplTI->GetDocumentation(MEMBERID_NIL, &implName, nullptr, nullptr, nullptr);
        if (implName) {
            std::string ifaceName;
            for (UINT j = 0; j < SysStringLen(implName); j++) {
                ifaceName += (char)implName[j];
            }
            SysFreeString(implName);

                        // P13.20: Event source interface recognition
            bool isSource = (implFlags & IMPLTYPEFLAG_FSOURCE) != 0;
            bool isDefault = (implFlags & IMPLTYPEFLAG_FDEFAULT) != 0;
            
            if (isSource) {
                // Event source interface
                cc->sourceIfaceNames.push_back(ifaceName);
                if (isDefault || cc->defaultSourceIfaceName.empty()) {
                    cc->defaultSourceIfaceName = ifaceName;
                }
            } else {
                // Regular (outgoing) interface
                if (isDefault) {
                    cc->defaultIfaceName = ifaceName;
                }
                // If no FDEFAULT flag, take first non-source interface
                if (cc->defaultIfaceName.empty()) {
                    cc->defaultIfaceName = ifaceName;
                }
            }
        }  // end if (implName)
        pImplTI->Release();
    }

    // ProgID: 从CLSID反查注册表获取真实ProgID
    {
        CLSID clsid = pTypeAttr->guid;
        LPOLESTR progIdW = nullptr;
        HRESULT progHr = ProgIDFromCLSID(clsid, &progIdW);
        if (SUCCEEDED(progHr) && progIdW) {
            std::string progIdStr;
            for (const wchar_t* p = progIdW; *p; p++) {
                progIdStr += (char)*p;
            }
            cc->progId = progIdStr;
            CoTaskMemFree(progIdW);
        } else {
            cc->progId = name;  // fallback: 使用coclass短名
        }
    }

    pTI->ReleaseTypeAttr(pTypeAttr);

    return cc;
}


// ============================================================
// P24-04: 内部: 解析模块 (TKIND_MODULE → ActiveX DLL全局函数)
// ============================================================

std::unique_ptr<ComModuleInfo> TypeLibParser::parseModule(void* pTypeInfo,
                                                           const std::string& name,
                                                           const std::string& dllPath) {
    ITypeInfo* pTI = static_cast<ITypeInfo*>(pTypeInfo);
    auto mod = std::make_unique<ComModuleInfo>();
    mod->name = name;

    // 推断DLL路径: TypeLib通常嵌在DLL中, 用tlbPath的目录+DLL名推断
    // 如果tlbPath本身是DLL文件, 直接使用
    if (!dllPath.empty()) {
        // tlbPath可能是 "C:\path\VBMAN.dll" 或注册表中的tlb路径
        // 对ActiveX DLL, TypeLib就嵌在DLL中
        if (dllPath.size() >= 4 &&
            (dllPath.substr(dllPath.size()-4) == ".dll" ||
             dllPath.substr(dllPath.size()-4) == ".DLL" ||
             dllPath.substr(dllPath.size()-4) == ".ocx" ||
             dllPath.substr(dllPath.size()-4) == ".OCX")) {
            mod->dllPath = dllPath;
        }
        // 否则不设dllPath, 运行时通过注册表查找
    }

    // 获取TYPEATTR
    TYPEATTR* pTypeAttr = nullptr;
    HRESULT hr = pTI->GetTypeAttr(&pTypeAttr);
    if (FAILED(hr) || !pTypeAttr) return nullptr;

    UINT cFuncs = pTypeAttr->cFuncs;
    UINT cVars = pTypeAttr->cVars;

    pTI->ReleaseTypeAttr(pTypeAttr);

    // 枚举函数 (模块级Public函数)
    for (UINT i = 0; i < cFuncs; i++) {
        FUNCDESC* pFuncDesc = nullptr;
        hr = pTI->GetFuncDesc(i, &pFuncDesc);
        if (FAILED(hr) || !pFuncDesc) continue;

        ComMemberInfo member = parseFuncDesc(pTI, pFuncDesc, i);
        // 模块函数无vtable概念
        member.vtableIndex = -1;
        mod->functions.push_back(std::move(member));
        pTI->ReleaseFuncDesc(pFuncDesc);
    }

    // 枚举常量 (模块级Public Const)
    for (UINT i = 0; i < cVars; i++) {
        VARDESC* pVarDesc = nullptr;
        hr = pTI->GetVarDesc(i, &pVarDesc);
        if (FAILED(hr) || !pVarDesc) continue;

        ComMemberInfo member = parseVarDesc(pTI, pVarDesc);
        mod->constants.push_back(std::move(member));
        pTI->ReleaseVarDesc(pVarDesc);
    }

    // 如果没有函数也没有常量, 仍保留模块(可能仅作为命名空间)
    return mod;
}

// ============================================================
// 内部: 解析方法
// ============================================================

ComMemberInfo TypeLibParser::parseFuncDesc(void* pTypeInfo, void* pFuncDesc, int index) {
    ITypeInfo* pTI = static_cast<ITypeInfo*>(pTypeInfo);
    FUNCDESC* pFD = static_cast<FUNCDESC*>(pFuncDesc);

    ComMemberInfo member;
    member.memid = pFD->memid;

    // 获取名称
    BSTR funcName = nullptr;
    UINT cNames = 0;
    pTI->GetNames(pFD->memid, &funcName, 1, &cNames);
    if (funcName) {
        member.realName.clear();
        for (UINT i = 0; i < SysStringLen(funcName); i++) {
            member.realName += (char)funcName[i];
        }
        SysFreeString(funcName);
    }
    member.name = member.realName;
    std::transform(member.name.begin(), member.name.end(), member.name.begin(), ::tolower);

    // 成员类别
    switch (pFD->invkind) {
        case INVOKE_FUNC:
            member.kind = ComMemberKind::Method;
            break;
        case INVOKE_PROPERTYGET:
            member.kind = ComMemberKind::PropertyGet;
            break;
        case INVOKE_PROPERTYPUT:
            member.kind = ComMemberKind::PropertyPut;
            break;
        case INVOKE_PROPERTYPUTREF:
            member.kind = ComMemberKind::PropertyPutRef;
            break;
        default:
            member.kind = ComMemberKind::Method;
            break;
    }

    // 调用约定
    switch (pFD->callconv) {
        case CC_CDECL:   member.callConv = CallConv::CDecl;   break;
        case CC_STDCALL: member.callConv = CallConv::StdCall; break;
        default:         member.callConv = CallConv::StdCall; break;
    }

    // 返回类型
    member.returnType = mapTypeDesc(&pFD->elemdescFunc.tdesc, pTI);

    // 参数
    // 获取参数名
    std::vector<std::string> paramNames;
    if (pFD->cParams > 0) {
        std::vector<BSTR> nameBuf(pFD->cParams + 1);
        UINT cNamesOut = 0;
        pTI->GetNames(pFD->memid, nameBuf.data(), pFD->cParams + 1, &cNamesOut);
        // nameBuf[0]是函数名, [1..cParams]是参数名
        for (UINT i = 1; i < cNamesOut && i <= pFD->cParams; i++) {
            std::string pname;
            if (nameBuf[i]) {
                for (UINT j = 0; j < SysStringLen(nameBuf[i]); j++) {
                    pname += (char)nameBuf[i][j];
                }
                SysFreeString(nameBuf[i]);
            }
            paramNames.push_back(pname);
        }
        // 释放未使用的BSTR
        for (UINT i = cNamesOut; i <= pFD->cParams; i++) {
            if (nameBuf[i]) SysFreeString(nameBuf[i]);
        }
    }

    for (UINT i = 0; i < pFD->cParams; i++) {
        std::string pname = (i < paramNames.size()) ? paramNames[i] : ("p" + std::to_string(i));
        ComParamInfo param = mapElemDesc(&pFD->lprgelemdescParam[i], pname, pTI);

        // [out, retval] 是返回值参数
        if (pFD->lprgelemdescParam[i].paramdesc.wParamFlags & PARAMFLAG_FRETVAL) {
            param.direction = ComParamDir::RetVal;
            member.returnType = param.type;
        }

        member.params.push_back(std::move(param));
    }

    return member;
}

// ============================================================
// 内部: 解析变量属性
// ============================================================

ComMemberInfo TypeLibParser::parseVarDesc(void* pTypeInfo, void* pVarDesc) {
    ITypeInfo* pTI = static_cast<ITypeInfo*>(pTypeInfo);
    VARDESC* pVD = static_cast<VARDESC*>(pVarDesc);

    ComMemberInfo member;
    member.memid = pVD->memid;
    member.kind = ComMemberKind::PropertyGet;
    member.vtableIndex = -1;  // 变量属性无vtable偏移

    // 获取名称
    BSTR varName = nullptr;
    pTI->GetDocumentation(pVD->memid, &varName, nullptr, nullptr, nullptr);
    if (varName) {
        member.realName.clear();
        for (UINT i = 0; i < SysStringLen(varName); i++) {
            member.realName += (char)varName[i];
        }
        SysFreeString(varName);
    }
    member.name = member.realName;
    std::transform(member.name.begin(), member.name.end(), member.name.begin(), ::tolower);

    // 类型
    if (pVD->varkind == VAR_PERINSTANCE) {
        member.returnType = mapTypeDesc(&pVD->elemdescVar.tdesc, pTI);
    }

    return member;
}

// ============================================================
// 内部: TYPEDESC → Vb6Type
// ============================================================

Vb6Type TypeLibParser::mapTypeDesc(void* pTypeDesc, void* pTypeInfo) {
    if (!pTypeDesc) return Vb6Type::Variant;
    TYPEDESC* pTD = static_cast<TYPEDESC*>(pTypeDesc);
    ITypeInfo* pTI = static_cast<ITypeInfo*>(pTypeInfo);

    VARTYPE vt = pTD->vt;

    // 处理VT_PTR / VT_SAFEARRAY / VT_USERDEFINED 等间接类型
    if (vt == VT_PTR) {
        // 指针类型, 递归解引用
        if (pTD->lptdesc) {
            Vb6Type inner = mapTypeDesc(pTD->lptdesc, pTypeInfo);
            if (inner == Vb6Type::Object) return Vb6Type::Object;
            // 字符串指针 → String
            if (inner == Vb6Type::String) return Vb6Type::String;
            // 其他指针 → Object (接口指针)
            return Vb6Type::Object;
        }
        return Vb6Type::Object;
    }

    if (vt == VT_SAFEARRAY) {
        // SAFEARRAY → 数组
        if (pTD->lptdesc) {
            Vb6Type inner = mapTypeDesc(pTD->lptdesc, pTypeInfo);
            return (Vb6Type)((int)Vb6Type::Array | (int)inner);
        }
        return Vb6Type::Variant;
    }

    if (vt == VT_USERDEFINED) {
        // 用户定义类型, 通过HREFTYPE解析
        if (pTD->hreftype && pTI) {
            ITypeInfo* pRefTI = nullptr;
            HRESULT hr = pTI->GetRefTypeInfo(pTD->hreftype, &pRefTI);
            if (SUCCEEDED(hr) && pRefTI) {
                Vb6Type refResult = Vb6Type::UserDefinedType;
                TYPEATTR* pRefAttr = nullptr;
                hr = pRefTI->GetTypeAttr(&pRefAttr);
                if (SUCCEEDED(hr) && pRefAttr) {
                    if (pRefAttr->typekind == TKIND_INTERFACE ||
                        pRefAttr->typekind == TKIND_DISPATCH) {
                        refResult = Vb6Type::Object;  // 接口 → Object
                    } else if (pRefAttr->typekind == TKIND_ENUM) {
                        refResult = Vb6Type::Long;  // Enum → Long
                    } else if (pRefAttr->typekind == TKIND_RECORD) {
                        refResult = Vb6Type::UserDefinedType;  // UDT
                    } else if (pRefAttr->typekind == TKIND_ALIAS) {
                        // TKIND_ALIAS: 递归解析别名指向的实际类型
                        // tdescAlias字段包含别名的底层TYPEDESC
                        refResult = mapTypeDesc(&pRefAttr->tdescAlias, pRefTI);
                    }
                    pRefTI->ReleaseTypeAttr(pRefAttr);
                }
                pRefTI->Release();
                return refResult;
            }
        }
        return Vb6Type::Variant;
    }

    // 直接VARTYPE映射
    switch (vt & VT_TYPEMASK) {
        case VT_EMPTY:    return Vb6Type::Empty;
        case VT_NULL:     return Vb6Type::Null;
        case VT_I2:       return Vb6Type::Integer;
        case VT_I4:       return Vb6Type::Long;
        case VT_R4:       return Vb6Type::Single;
        case VT_R8:       return Vb6Type::Double;
        case VT_CY:       return Vb6Type::Currency;
        case VT_DATE:     return Vb6Type::Date;
        case VT_BSTR:     return Vb6Type::String;
        case VT_DISPATCH: return Vb6Type::Object;
        case VT_ERROR:    return Vb6Type::Error;
        case VT_BOOL:     return Vb6Type::Boolean;
        case VT_VARIANT:  return Vb6Type::Variant;
        case VT_DECIMAL:  return Vb6Type::Decimal;
        case VT_I1:       return Vb6Type::Byte;
        case VT_UI1:      return Vb6Type::Byte;
        case VT_UI2:      return Vb6Type::Integer;
        case VT_UI4:      return Vb6Type::Long;
        case VT_UNKNOWN:  return Vb6Type::Object;
        default:          return Vb6Type::Variant;
    }
}

// ============================================================
// 内部: ELEMDESC → ComParamInfo
// ============================================================

ComParamInfo TypeLibParser::mapElemDesc(void* pElemDesc, const std::string& name,
                                         void* pTypeInfo) {
    if (!pElemDesc) return {name, Vb6Type::Variant, ComParamDir::In};
    ELEMDESC* pED = static_cast<ELEMDESC*>(pElemDesc);

    ComParamInfo param;
    param.name = name;
    param.type = mapTypeDesc(&pED->tdesc, pTypeInfo);

    // 参数方向
    WORD flags = pED->paramdesc.wParamFlags;
    if (flags & PARAMFLAG_FOUT) {
        if (flags & PARAMFLAG_FRETVAL) {
            param.direction = ComParamDir::RetVal;
        } else if (flags & PARAMFLAG_FIN) {
            param.direction = ComParamDir::InOut;
        } else {
            param.direction = ComParamDir::Out;
        }
    } else {
        param.direction = ComParamDir::In;
    }

    param.isOptional = (flags & PARAMFLAG_FOPT) != 0;
    param.hasDefault = (flags & 0x10) != 0;  // PARAMFLAG_FDEFAULT = 0x10 (not in all SDK versions)

    return param;
}

// ============================================================
// 内部: IID → 字符串
// ============================================================

std::string TypeLibParser::iidToString(const uint8_t* iidBytes) {
    // GUID结构: Data1(4), Data2(2), Data3(2), Data4(8)
    const uint32_t* d1 = reinterpret_cast<const uint32_t*>(iidBytes);
    const uint16_t* d2 = reinterpret_cast<const uint16_t*>(iidBytes + 4);
    const uint16_t* d3 = reinterpret_cast<const uint16_t*>(iidBytes + 6);
    const uint8_t* d4 = iidBytes + 8;

    char buf[64];
    snprintf(buf, sizeof(buf), "{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
             (unsigned long)*d1, *d2, *d3,
             d4[0], d4[1], d4[2], d4[3], d4[4], d4[5], d4[6], d4[7]);
    return buf;
}

} // namespace vb6c3