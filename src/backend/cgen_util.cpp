#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>

namespace vb6c3 {

// --- cgen_util.cpp: ActiveX DLL 入口代码生成 (generateDllEntry) ---

// P6.4+: 默认实例类注册 (见 cgen.hpp 声明). 若 name 是 VB_PredeclaredId=True 的类
// 模块名, 把 objLower 注册到 knownClassVars_ 使其按"类实例"精确解析成员, 返回类
// 规范名供生成 vb6_cls_X_Default() 对象表达式; 非默认实例类返回空串.
std::string CCodeGen::registerDefaultInstanceClass(const std::string& name) {
    std::string cls = defaultInstanceClassName(name);
    if (cls.empty()) return "";
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    knownClassVars_[lower] = cls;
    return cls;
}

std::string CCodeGen::generateDllEntry(const std::string& progId, const std::vector<SymbolTable*>& allSymTabs) {
    CodeEmitter entry;
    dllProgId_ = progId;

    // 收集所有instancing >= PublicNotCreatable的类模块
    struct CoClassInfo {
        std::string moduleName;
        std::string clsidStr;
        std::string progId;
        std::vector<std::string> publicMethodNames;
        std::vector<Symbol*> publicMethodSyms;
        std::vector<std::string> implementsNames;  // P12.1: Implements接口列表
        // P6.6.4: 事件源信息
        std::vector<std::string> eventNames;       // 事件名称列表
        std::string defaultIfaceIid;              // 默认dispinterface IID (用于早绑定QI)
        std::string sourceIfaceIid;               // source dispinterface IID
    };
    std::vector<CoClassInfo> coClasses;

    // P12.1: Hex格式化辅助 (IID常量生成用)
    auto StringFormatHex2 = [](uint8_t v) -> std::string {
        char buf[4]; snprintf(buf, sizeof(buf), "%02X", v); return buf;
    };
    auto StringFormatHex4 = [](uint16_t v) -> std::string {
        char buf[6]; snprintf(buf, sizeof(buf), "%04X", v); return buf;
    };
    auto StringFormatHex8 = [](uint32_t v) -> std::string {
        char buf[10]; snprintf(buf, sizeof(buf), "%08X", v); return buf;
    };

    auto generateClsid = [](const std::string& name) -> std::string {
        uint32_t h1 = 0x811c9dc5, h2 = 0x01000193, h3 = 0xabcd1234, h4 = 0x5678ef01;
        for (char c : name) {
            h1 ^= (uint32_t)(unsigned char)c; h1 *= 0x01000193;
            h2 ^= (uint32_t)(unsigned char)c; h2 *= 0x01000193;
            h3 ^= (uint32_t)(unsigned char)c; h3 *= 0x01000193;
            h4 ^= (uint32_t)(unsigned char)c; h4 *= 0x01000193;
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "{%08X-%04X-%04X-%04X-%04X%08X}",
                 h1, (h2 >> 16) & 0xFFFF, h2 & 0xFFFF | 0x4000,
                 (h3 >> 16) & 0xFFFF | 0x8000,
                 h3 & 0xFFFF, h4);
        return std::string(buf);
    };

    // IID/CLSID generation helpers
    auto generateIid = [](const std::string& name) -> std::string {
        uint32_t h1 = 0xa1b2c3d4, h2 = 0xe5f60718, h3 = 0x9a0b1c2d, h4 = 0x3e4f5061;
        for (char c : name) {
            h1 ^= (uint32_t)(unsigned char)c; h1 *= 0x01000193;
            h2 ^= (uint32_t)(unsigned char)c; h2 *= 0x01000193;
            h3 ^= (uint32_t)(unsigned char)c; h3 *= 0x01000193;
            h4 ^= (uint32_t)(unsigned char)c; h4 *= 0x01000193;
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "{%08X-%04X-%04X-%04X-%04X%08X}",
                 h1, (h2 >> 16) & 0xFFFF, (h2 & 0xFFFF) | 0x4000,
                 (h3 >> 16) & 0xFFFF | 0x8000,
                 h3 & 0xFFFF, h4);
        return std::string(buf);
    };
    // Fix 016b: 构建 className(lower) -> 定义该类的 SymbolTable* 映射.
    // COM 分派表收集每个类的 Public 方法时, 必须只搜索该类自身的符号表,
    // 不能跨 allSymTabs 搜索同名方法 — 否则跨类同名冲突 (如 6 个类的 Mode:
    // cCryptoHMAC/cCryptoHash 是 Function, cDelay/cModbusTransportTCP/... 是
    // PropertyGet/Let) 会导致 cDelay 的 PropertyGet "mode$pg" 泄漏进 cCryptoHMAC
    // 的分派表, 生成不存在的 vb6_cCryptoHMAC_prop_get_Mode → LNK2019.
    // 每个类的本地方法符号只存在于定义该类的模块符号表中, 搜索 ownTab 即可
    // 精确获取该类自己的方法, 完全避免跨类碰撞.
    std::unordered_map<std::string, SymbolTable*> classOwningSymTab;
    for (auto* st : allSymTabs) {
        if (!st->moduleScope()) continue;
        for (const auto& [ck, cs] : st->moduleScope()->symbols()) {
            if (cs->kind == SymbolKind::Class && !cs->isExternal) {
                classOwningSymTab.emplace(Symbol::toLower(cs->name), st);
            }
        }
    }

    // Fix 053: 去重 — 同一类可能因符号表合并而在 moduleScope 中出现多次
    std::set<std::string> seenCoClassNames;
    for (auto& [key, sym] : symTab_.moduleScope()->symbols()) {
        if (sym->kind == SymbolKind::Class && !sym->isInterface) {
            // BUG-4 fix: PublicNotCreatable(2) is visible but NOT externally creatable
            // Only MultiUse(5)/SingleUse(3) should have ClassFactory entries
            if (sym->instancing == VBInstancing::MultiUse || sym->instancing == VBInstancing::SingleUse) {
                std::string lowerName = Symbol::toLower(sym->name);
                if (seenCoClassNames.count(lowerName)) continue;  // 已处理, 跳过
                seenCoClassNames.insert(lowerName);
                CoClassInfo info;
                info.moduleName = sym->name;
                info.clsidStr = sym->comClsidStr.empty() ? generateClsid(progId + "." + sym->name) : sym->comClsidStr;  // P6.8: 优先使用VBP指定的CLSID
                info.progId = progId + "." + sym->name;

                // 使用类的memberNames查找Public方法符号
                // (不能按前缀"ClassName_"搜索, 因为单模块工程中方法名是"SetValue"而非"Calc_SetValue")
                // Fix 016b: 优先搜索该类自身的符号表 (classOwningSymTab), 避免跨类同名碰撞.
                std::string classNameLower = Symbol::toLower(sym->name);
                SymbolTable* ownTab = nullptr;
                auto itOwn = classOwningSymTab.find(classNameLower);
                if (itOwn != classOwningSymTab.end()) ownTab = itOwn->second;

                // Fix 053: 去重 memberNames — Event 和 Sub/Function 可能同名,
                // memberNames 中会有重复条目, 导致 dispatch 函数被生成两次 (C2084)
                std::set<std::string> seenMemberNames;
                // Fix 093a: 事件名集合 (小写) — Event 无实现函数. 若被当作 Public
                // 方法收集 (如 cTimer/cTimers 的 `Event Timer()` 与内置 Timer() 同名,
                // lookupModule 命中内置符号) 会生成 vb6_disp_<cls>_<ev>_invoke 并
                // extern 引用不存在的 vb6_<cls>_<ev> → LNK2019. 事件不进方法表.
                std::set<std::string> eventNamesLower;
                for (const auto& en : sym->eventNames) eventNamesLower.insert(Symbol::toLower(en));
                for (auto& memberName : sym->memberNames) {
                    if (!seenMemberNames.insert(Symbol::toLower(memberName)).second) continue;
                    if (eventNamesLower.count(Symbol::toLower(memberName))) continue;
                    // Try Sub/Function: 优先在类自身符号表搜索 (本类方法, 无碰撞)
                    Symbol* memSym = nullptr;
                    if (ownTab) memSym = ownTab->lookupModule(memberName);
                    if (!memSym) memSym = symTab_.lookupModule(memberName);
                    if (!memSym) memSym = symTab_.lookup(memberName);
                    if (!memSym) {
                        for (auto* st : allSymTabs) {
                            memSym = st->lookupModule(memberName);
                            if (memSym) break;
                        }
                    }
                    // Fix 016b: 验证找到的符号属于当前类 (外部符号查 sourceModule;
                    // 本地符号只需 ownTab 命中即可, 因 ownTab 只含本类定义)
                    if (memSym && (memSym->kind == SymbolKind::Sub || memSym->kind == SymbolKind::Function)
                        && memSym->access == AccessLevel::Public && !memSym->isBuiltin) {
                        bool belongs = false;
                        if (memSym->isExternal) {
                            belongs = (Symbol::toLower(memSym->sourceModule) == classNameLower);
                        } else {
                            belongs = (ownTab && ownTab->lookupModule(memberName) == memSym);
                        }
                        if (belongs) {
                            info.publicMethodNames.push_back(memSym->name);
                            info.publicMethodSyms.push_back(memSym);
                            memSym = nullptr;  // Don't double-count
                        }
                    }
                    // Lookup Property Get/Let/Set: 优先在类自身符号表搜索
                    for (auto kind : {SymbolKind::PropertyGet, SymbolKind::PropertyLet, SymbolKind::PropertySet}) {
                        Symbol* propSym = nullptr;
                        if (ownTab) propSym = ownTab->lookupModuleByKind(memberName, kind);
                        if (!propSym) propSym = symTab_.lookupModuleByKind(memberName, kind);
                        if (!propSym) {
                            for (auto* st : allSymTabs) {
                                propSym = st->lookupModuleByKind(memberName, kind);
                                if (propSym) break;
                            }
                        }
                        if (propSym && propSym->access == AccessLevel::Public) {
                            // Fix 016b: 验证归属, 阻止其他类的同名 Property 泄漏
                            bool belongs = false;
                            if (propSym->isExternal) {
                                belongs = (Symbol::toLower(propSym->sourceModule) == classNameLower);
                            } else {
                                belongs = (ownTab && ownTab->lookupModuleByKind(memberName, kind) == propSym);
                            }
                            if (belongs) {
                                info.publicMethodNames.push_back(propSym->name);
                                info.publicMethodSyms.push_back(propSym);
                            }
                        }
                    }
                }
                // P12.1: 收集 Implements 接口列表
                info.implementsNames = sym->implementsNames;
                // P6.6.4: 收集事件信息
                if (!sym->eventNames.empty()) {
                    info.eventNames = sym->eventNames;
                    // P6.6.6: 优先使用TypeLib builder回写的IID, 保证与TypeLib中一致
                    info.sourceIfaceIid = sym->comSourceIfaceIid.empty()
                        ? generateIid("source:" + progId + "." + sym->name + "Events")
                        : sym->comSourceIfaceIid;
                }
                // P6.6.6: 默认接口IID (由driver TypeLib builder回写)
                info.defaultIfaceIid = sym->comDefaultIfaceIid;
                coClasses.push_back(std::move(info));
            }
        }
    }

    if (coClasses.empty()) {
        // BUG-2 fix: Generate minimal stubs (prevents linker errors for empty ActiveX DLL)
        entry.emitLine("// Generated by vb6c3 (C3.exe) - ActiveX DLL entry point (no public classes)");
        entry.emitLine("#include \"vb6comserver.h\"");
        entry.emitBlank();
        entry.emitLine("const vb6_CoClassDesc g_vb6_coclasses[] = {{0}};");
        entry.emitLine("const int g_vb6_coclassCount = 0;");
        entry.emitBlank();
        entry.emitLine("HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {");
        entry.indent();
        entry.emitLine("return CLASS_E_CLASSNOTAVAILABLE;");
        entry.dedent();
        entry.emitLine("}");
        entry.emitBlank();
        entry.emitLine("HRESULT WINAPI DllCanUnloadNow(void) { return S_OK; }");
        entry.emitLine("HRESULT WINAPI DllRegisterServer(void) { return S_OK; }");
        entry.emitLine("HRESULT WINAPI DllUnregisterServer(void) { return S_OK; }");
        entry.emitBlank();
        return entry.str();
    }

    // 生成 dll_entry.c 内容

    // 头文件注释
    entry.emitLine("// Generated by vb6c3 (C3.exe) - ActiveX DLL entry point");
    entry.emitLine("// Contains COM server exports: DllGetClassObject, DllRegisterServer, etc.");
    entry.emitBlank();

    // 包含必要头文件
    // Fix 010b: 添加vb6rtl.h以提供vb6_VARIANT等类型定义
    // (vb6comserver.h只include windows.h/oleauto.h, 不含vb6_VARIANT)
    // vb6rtl.h使用vb6_前缀避免与Windows VARIANT冲突, 可安全与oleauto.h共存
    entry.emitLine("#include \"vb6rtl.h\"");
    entry.emitLine("#include \"vb6comserver.h\"");
    entry.emitBlank();

    // 前向声明: 类结构体和工厂/销毁函数 (C语言需要struct关键字)
    for (auto& cc : coClasses) {
        std::string clsId = cIdent(cc.moduleName);
        std::string clsStruct = "vb6_cls_" + clsId;
        entry.emitLine("struct " + clsStruct + ";  /* forward decl */");
        entry.emitLine("extern struct " + clsStruct + "* vb6_cls_" + clsId + "_New(void);");
        entry.emitLine("extern void vb6_cls_" + clsId + "_Destroy(struct " + clsStruct + "*);");
        // 类的方法函数前向声明
        for (size_t mi = 0; mi < cc.publicMethodNames.size(); mi++) {
            Symbol* methodSym = cc.publicMethodSyms[mi];
            // Property需要加前缀: prop_get_/prop_let_/prop_set_
            std::string methodCName = cc.publicMethodNames[mi];
            if (methodSym->kind == SymbolKind::PropertyGet) {
                methodCName = "prop_get_" + methodCName;
            } else if (methodSym->kind == SymbolKind::PropertyLet) {
                methodCName = "prop_let_" + methodCName;
            } else if (methodSym->kind == SymbolKind::PropertySet) {
                methodCName = "prop_set_" + methodCName;
            }
            std::string procName = cProcName(methodCName, methodSym->access, cc.moduleName);
            // Fix 052: 前向声明必须与函数定义签名完全一致
            // - ByRef 参数需要添加 * (指针)
            // - Optional 参数需要添加 _has_ 标志
            {
                std::string retType = (methodSym->kind == SymbolKind::Function || methodSym->kind == SymbolKind::PropertyGet)
                    ? mapType(methodSym->type) : "void";
                std::string params = "struct " + clsStruct + "*";
                for (auto& p : methodSym->params) {
                    std::string pType = mapType(p.type);
                    if (!p.isByVal) {
                        // ByRef → C指针 (与 cgen_decl.cpp 函数定义一致)
                        pType += "*";
                    }
                    params += ", " + pType;
                }
                // Optional 参数的 _has_ 标志
                for (auto& p : methodSym->params) {
                    if (p.isOptional && !p.isParamArray) {
                        params += ", int _has_" + cIdent(p.name);
                    }
                }
                entry.emitLine("extern " + retType + " " + procName + "(" + params + ");");
            }
        }
    }
    // vb6_Init/vb6_Exit 前向声明 (定义在vb6rtl.c)
    entry.emitLine("extern void vb6_Init(void);");
    entry.emitLine("extern void vb6_Exit(void);");
    entry.emitBlank();

    // 1. IDispatch方法桥接函数
    for (auto& cc : coClasses) {
        std::string clsId = cIdent(cc.moduleName);
        std::string clsStruct = "vb6_cls_" + clsId;

        for (size_t mi = 0; mi < cc.publicMethodNames.size(); mi++) {
            std::string& methodName = cc.publicMethodNames[mi];
            Symbol* methodSym = cc.publicMethodSyms[mi];
            std::string prefix = cc.moduleName + "_";
            std::string bareName = methodName;
            if (Symbol::toLower(bareName).substr(0, prefix.size()) == Symbol::toLower(prefix)) {
                bareName = bareName.substr(prefix.size());
            }
            std::string invokeSuffixMD;
            if (methodSym->kind == SymbolKind::PropertyGet) invokeSuffixMD = "_get";
            else if (methodSym->kind == SymbolKind::PropertyLet) invokeSuffixMD = "_let";
            else if (methodSym->kind == SymbolKind::PropertySet) invokeSuffixMD = "_set";
            std::string invokeName = "vb6_disp_" + clsId + "_" + cIdent(bareName) + invokeSuffixMD + "_invoke";

            entry.emitLine("static void " + invokeName + "(void* instance, void** args, int32_t argc, void* result) {");
            entry.indent();
            entry.emitLine("struct " + clsStruct + "* me = (struct " + clsStruct + "*)instance;");

            // Fix 052: 构建实参列表 (不含me的类型, 只有表达式)
            // 注意: args[i] 是 VARIANT* (来自DISPPARAMS.rgvarg), 需用VARIANT字段提取值
            std::string callArgs = "me";
            for (int i = 0; i < (int)methodSym->params.size(); i++) {
                callArgs += ", ";
                Vb6Type pType = methodSym->params[i].type;
                if (methodSym->params[i].isByVal) {
                    // 从VARIANT中提取ByVal值
                    if (pType == Vb6Type::Long || pType == Vb6Type::Integer ||
                        pType == Vb6Type::Byte) {
                        callArgs += "((VARIANT*)args[" + std::to_string(i) + "])->lVal";
                    } else if (pType == Vb6Type::Boolean) {
                        callArgs += "(((VARIANT*)args[" + std::to_string(i) + "])->vt == VT_BOOL ? (((VARIANT*)args[" + std::to_string(i) + "])->boolVal == VARIANT_TRUE ? -1 : 0) : ((VARIANT*)args[" + std::to_string(i) + "])->lVal)";
                    } else if (pType == Vb6Type::Double || pType == Vb6Type::Single) {
                        callArgs += "((VARIANT*)args[" + std::to_string(i) + "])->dblVal";
                    } else if (pType == Vb6Type::String) {
                        callArgs += "((VARIANT*)args[" + std::to_string(i) + "])->bstrVal";
                    } else if (pType == Vb6Type::Variant) {
                        // Fix 051: ByVal Variant — 传递整个 VARIANT 结构 (vb6_VARIANT 与 Windows VARIANT 布局兼容)
                        callArgs += "*(vb6_VARIANT*)args[" + std::to_string(i) + "]";
                    } else {
                        callArgs += "((VARIANT*)args[" + std::to_string(i) + "])->lVal";
                    }
                } else {
                    // ByRef: 传递VARIANT中的值指针
                    if (pType == Vb6Type::Long || pType == Vb6Type::Integer ||
                        pType == Vb6Type::Byte) {
                        callArgs += "&((VARIANT*)args[" + std::to_string(i) + "])->lVal";
                    } else if (pType == Vb6Type::Boolean) {
                        callArgs += "&((VARIANT*)args[" + std::to_string(i) + "])->lVal";  /* ByRef Boolean: same as Long, VB6 uses int32_t */
                    } else if (pType == Vb6Type::Double || pType == Vb6Type::Single) {
                        callArgs += "&((VARIANT*)args[" + std::to_string(i) + "])->dblVal";
                    } else if (pType == Vb6Type::Variant) {
                        // Fix 051: ByRef Variant — 传递 VARIANT 指针
                        callArgs += "(vb6_VARIANT*)args[" + std::to_string(i) + "]";
                    } else if (pType == Vb6Type::String) {
                        callArgs += "&((VARIANT*)args[" + std::to_string(i) + "])->bstrVal";
                    } else {
                        callArgs += "(void*)&((VARIANT*)args[" + std::to_string(i) + "])->lVal";
                    }
                }
            }
            // Fix 052: Optional 参数的 _has_ 标志 (COM dispatch 中所有参数都已提供)
            for (auto& p : methodSym->params) {
                if (p.isOptional && !p.isParamArray) {
                    callArgs += ", 1";
                }
            }

            // Property需要加前缀: prop_get_/prop_let_/prop_set_
            std::string methodCName = methodName;
            if (methodSym->kind == SymbolKind::PropertyGet) {
                methodCName = "prop_get_" + methodCName;
            } else if (methodSym->kind == SymbolKind::PropertyLet) {
                methodCName = "prop_let_" + methodCName;
            } else if (methodSym->kind == SymbolKind::PropertySet) {
                methodCName = "prop_set_" + methodCName;
            }
            std::string procName = cProcName(methodCName, methodSym->access, cc.moduleName);

            if (methodSym->kind == SymbolKind::Function || methodSym->kind == SymbolKind::PropertyGet) {
                std::string retType = mapType(methodSym->type);
                entry.emitLine("if (result) {");
                entry.indent();
                entry.emitLine(retType + " _r = " + procName + "(" + callArgs + ");");
                if (methodSym->type == Vb6Type::Long || methodSym->type == Vb6Type::Integer ||
                    methodSym->type == Vb6Type::Byte) {
                    entry.emitLine("((VARIANT*)result)->vt = VT_I4; ((VARIANT*)result)->lVal = (int32_t)_r;");
                } else if (methodSym->type == Vb6Type::Boolean) {
                    entry.emitLine("((VARIANT*)result)->vt = VT_BOOL; ((VARIANT*)result)->boolVal = _r ? VARIANT_TRUE : VARIANT_FALSE;");
                } else if (methodSym->type == Vb6Type::Double || methodSym->type == Vb6Type::Single) {
                    entry.emitLine("((VARIANT*)result)->vt = VT_R8; ((VARIANT*)result)->dblVal = (double)_r;");
                } else if (methodSym->type == Vb6Type::String) {
                    entry.emitLine("((VARIANT*)result)->vt = VT_BSTR; ((VARIANT*)result)->bstrVal = _r;");
                } else if (retType == "vb6_VARIANT") {
                    // Fix 052: 所有映射为 vb6_VARIANT 的返回类型 (Variant/Decimal/UDT等) 都用 memcpy
                    // vb6_VARIANT 结构体不能 cast 为 intptr_t,
                    // 用 memcpy 复制到 Windows VARIANT (布局兼容: vt + padding + union).
                    entry.emitLine("memcpy(result, &_r, sizeof(vb6_VARIANT));");
                } else {
                    entry.emitLine("((VARIANT*)result)->vt = VT_I4; ((VARIANT*)result)->lVal = (int32_t)(intptr_t)_r;");
                }
                entry.dedent();
                entry.emitLine("}");
            } else {
                entry.emitLine(procName + "(" + callArgs + ");");
            }
            entry.dedent();
            entry.emitLine("}");
            entry.emitBlank();
        }
    }

    // 2. IDispatch方法描述表
    for (auto& cc : coClasses) {
        std::string clsId = cIdent(cc.moduleName);
        std::string methodsVar = "g_vb6_disp_" + clsId + "Methods";

        if (cc.publicMethodNames.empty()) {
            continue;  // No public methods - skip method table
        }

        // M29: dispid取自methodSym->comDispid (来自driver TypeLib全局连续分配), nextDispid仅作fallback
        std::unordered_map<std::string, int> dispIdMap;
        int nextDispid = 1;

        entry.emitLine("static const vb6_DispMethodDesc " + methodsVar + "[] = {");
        entry.indent();

        for (size_t mi = 0; mi < cc.publicMethodNames.size(); mi++) {
            std::string& methodName = cc.publicMethodNames[mi];
            Symbol* methodSym = cc.publicMethodSyms[mi];
            std::string prefix = cc.moduleName + "_";
            std::string bareName = methodName;
            if (Symbol::toLower(bareName).substr(0, prefix.size()) == Symbol::toLower(prefix)) {
                bareName = bareName.substr(prefix.size());
            }
            std::string invokeSuffixMD;
            if (methodSym->kind == SymbolKind::PropertyGet) invokeSuffixMD = "_get";
            else if (methodSym->kind == SymbolKind::PropertyLet) invokeSuffixMD = "_let";
            else if (methodSym->kind == SymbolKind::PropertySet) invokeSuffixMD = "_set";
            std::string invokeName = "vb6_disp_" + clsId + "_" + cIdent(bareName) + invokeSuffixMD + "_invoke";

            int invkind = 1;
            if (methodSym->kind == SymbolKind::PropertyGet) {
                invkind = 2;
            } else if (methodSym->kind == SymbolKind::PropertyLet) {
                invkind = 4;
            } else if (methodSym->kind == SymbolKind::PropertySet) {
                invkind = 8;
            }

                        // COM convention: Property Get/Let/Set share the same DISPID
            std::string lowerBareName = Symbol::toLower(bareName);
            // M29: dispid优先取自methodSym->comDispid (driver TypeLib阶段回写), 保证dll_entry.c与TypeLib dispid一致
            //      COM convention: Property Get/Let/Set共享同一dispid (driver已确保同名Property变体共享同一comDispid)
            int dispid = methodSym->comDispid;
            if (dispid == 0) {
                // 安全fallback: 理论上不应发生 (driver TypeLib阶段已经回写comDispid) 仅防意外崩溃
                dispid = nextDispid++;
                dispIdMap[lowerBareName] = dispid;
            } else if (dispIdMap.find(lowerBareName) == dispIdMap.end()) {
                dispIdMap[lowerBareName] = dispid;
            }
std::string wideName = "L\"" + bareName + "\"";

            entry.emitLine("{ " + wideName + ", " + std::to_string((int32_t)dispid) +
                        ", " + std::to_string(invkind) + ", " + invokeName + " },");
        }

        entry.dedent();
        entry.emitLine("};");
        entry.emitBlank();
    }

    // generateIid moved above (before collection loop)

    // 收集所有IID字符串 (interfaceName -> IID字符串)，避免重复
    std::unordered_map<std::string, std::string> iidMap;
    for (auto& cc : coClasses) {
        for (auto& ifaceName : cc.implementsNames) {
            if (iidMap.find(ifaceName) == iidMap.end()) {
                iidMap[ifaceName] = generateIid("iface:" + progId + "." + ifaceName);
            }
        }
    }

    // 生成静态IID常量 (仅在有Implements接口时生成)
    if (!iidMap.empty()) {
        entry.emitLine("// P12.1: Implements interface IIDs");
        for (auto& [ifaceName, iidStr] : iidMap) {
            std::string iidVar = "IID_vb6iface_" + cIdent(ifaceName);
            // 解析IID字符串并生成静态IID常量
            // 格式: {0xAABBCCDD,0xEEFF,0x1122,{0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xAA}}
            uint32_t d1; uint16_t d2, d3; uint8_t d4[8];
            sscanf(iidStr.c_str(), "{%08X-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}",
                   &d1, &d2, &d3, &d4[0], &d4[1], &d4[2], &d4[3], &d4[4], &d4[5], &d4[6], &d4[7]);
            entry.emitLine("static const IID " + iidVar + " = {0x" + 
                StringFormatHex8(d1) + ",0x" + StringFormatHex4(d2) + ",0x" + StringFormatHex4(d3) +
                ",{0x" + StringFormatHex2(d4[0]) + ",0x" + StringFormatHex2(d4[1]) +
                ",0x" + StringFormatHex2(d4[2]) + ",0x" + StringFormatHex2(d4[3]) +
                ",0x" + StringFormatHex2(d4[4]) + ",0x" + StringFormatHex2(d4[5]) +
                ",0x" + StringFormatHex2(d4[6]) + ",0x" + StringFormatHex2(d4[7]) + "}};");
        }
        entry.emitBlank();

        // 生成每个coclass的ifaceIids数组
        for (auto& cc : coClasses) {
            if (cc.implementsNames.empty()) continue;
            std::string clsId = cIdent(cc.moduleName);
            std::string iidsVar = "g_vb6_ifaceIids_" + clsId;
            entry.emitLine("static const IID* const " + iidsVar + "[] = {");
            entry.indent();
            for (auto& ifaceName : cc.implementsNames) {
                entry.emitLine("&IID_vb6iface_" + cIdent(ifaceName) + ",");
            }
            entry.dedent();
            entry.emitLine("};");
            entry.emitBlank();
        }
    }
    // P6.6.4: 生成事件描述表和source IID常量
    bool hasAnyEvents = false;

    // P6.6.6: Default dispinterface IID constants (用于早绑定QI匹配)
    {
        bool hasDefaultIid = false;
        for (auto& cc : coClasses) {
            if (!cc.defaultIfaceIid.empty()) { hasDefaultIid = true; break; }
        }
        if (hasDefaultIid) {
            entry.emitLine("// P6.6.6: Default dispinterface IID constants");
            for (auto& cc : coClasses) {
                if (cc.defaultIfaceIid.empty()) continue;
                std::string clsId = cIdent(cc.moduleName);
                std::string iidVar = "IID_vb6def_" + clsId;
                uint32_t d1; uint16_t d2, d3; uint8_t d4[8];
                sscanf(cc.defaultIfaceIid.c_str(), "{%08X-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}",
                       &d1, &d2, &d3, &d4[0], &d4[1], &d4[2], &d4[3], &d4[4], &d4[5], &d4[6], &d4[7]);
                entry.emitLine("static const IID " + iidVar + " = {0x" +
                    StringFormatHex8(d1) + ",0x" + StringFormatHex4(d2) + ",0x" + StringFormatHex4(d3) +
                    ",{0x" + StringFormatHex2(d4[0]) + ",0x" + StringFormatHex2(d4[1]) +
                    ",0x" + StringFormatHex2(d4[2]) + ",0x" + StringFormatHex2(d4[3]) +
                    ",0x" + StringFormatHex2(d4[4]) + ",0x" + StringFormatHex2(d4[5]) +
                    ",0x" + StringFormatHex2(d4[6]) + ",0x" + StringFormatHex2(d4[7]) + "}};");
            }
            entry.emitBlank();
        }
    }
    for (auto& cc : coClasses) {
        if (!cc.eventNames.empty()) { hasAnyEvents = true; break; }
    }
    if (hasAnyEvents) {
        entry.emitLine("// P6.6.4: Source dispinterface IID constants");
        for (auto& cc : coClasses) {
            if (cc.sourceIfaceIid.empty()) continue;
            std::string clsId = cIdent(cc.moduleName);
            std::string iidVar = "IID_vb6src_" + clsId;
            uint32_t d1; uint16_t d2, d3; uint8_t d4[8];
            sscanf(cc.sourceIfaceIid.c_str(), "{%08X-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}",
                   &d1, &d2, &d3, &d4[0], &d4[1], &d4[2], &d4[3], &d4[4], &d4[5], &d4[6], &d4[7]);
            entry.emitLine("static const IID " + iidVar + " = {0x" +
                StringFormatHex8(d1) + ",0x" + StringFormatHex4(d2) + ",0x" + StringFormatHex4(d3) +
                ",{0x" + StringFormatHex2(d4[0]) + ",0x" + StringFormatHex2(d4[1]) +
                ",0x" + StringFormatHex2(d4[2]) + ",0x" + StringFormatHex2(d4[3]) +
                ",0x" + StringFormatHex2(d4[4]) + ",0x" + StringFormatHex2(d4[5]) +
                ",0x" + StringFormatHex2(d4[6]) + ",0x" + StringFormatHex2(d4[7]) + "}};");
        }
        entry.emitBlank();

        entry.emitLine("// P6.6.4: Event description tables");
        for (auto& cc : coClasses) {
            if (cc.eventNames.empty()) continue;
            std::string clsId = cIdent(cc.moduleName);
            std::string eventsVar = "g_vb6_events_" + clsId;
            entry.emitLine("static const vb6_EventDesc " + eventsVar + "[] = {");
            entry.indent();
            for (size_t ei = 0; ei < cc.eventNames.size(); ei++) {
                int32_t dispid = (int32_t)(ei + 1);  // 事件DISPID从1递增
                entry.emitLine("{ L\"" + cc.eventNames[ei] + "\", " + std::to_string(dispid) + " },");
            }
            entry.dedent();
            entry.emitLine("};");
            entry.emitBlank();
        }
    }

    // 3. 全局coclass描述表
    entry.emitLine("const vb6_CoClassDesc g_vb6_coclasses[] = {");
    entry.indent();
    for (auto& cc : coClasses) {
        std::string clsId = cIdent(cc.moduleName);
        std::string clsStruct = "vb6_cls_" + clsId;
        entry.emitLine("{");
        entry.indent();
        entry.emitLine("\"" + cc.progId + "\",  /* progId */");
        entry.emitLine("\"" + cc.clsidStr + "\",  /* clsidStr */");
        entry.emitLine("\"" + cc.moduleName + "\",  /* classVariable */");
        entry.emitLine("(void*(*)(void))" + clsStruct + "_New,  /* factoryFunc */");
        entry.emitLine("(void(*)(void*))" + clsStruct + "_Destroy,  /* destroyFunc */");
        entry.emitLine("NULL,  /* dispatchVtable */");
        if (cc.publicMethodNames.empty()) {
            entry.emitLine("0,  /* methodCount */");
            entry.emitLine("NULL,  /* methods */");
        } else {
            entry.emitLine(std::to_string(cc.publicMethodNames.size()) + ",  /* methodCount */");
            entry.emitLine("g_vb6_disp_" + clsId + "Methods,  /* methods */");
        }
        // P12.1: Implements interface IIDs
        if (!cc.implementsNames.empty()) {
            entry.emitLine(std::to_string(cc.implementsNames.size()) + ",  /* ifaceCount */");
            entry.emitLine("g_vb6_ifaceIids_" + clsId + ",  /* ifaceIids */");
        } else {
            entry.emitLine("0,  /* ifaceCount */");
            entry.emitLine("NULL,  /* ifaceIids */");
        }
        // P6.6.6: 默认接口IID (早绑定QI用)
        if (!cc.defaultIfaceIid.empty()) {
            entry.emitLine("&IID_vb6def_" + clsId + ",  /* defaultIfaceIid */");
        } else {
            entry.emitLine("NULL,  /* defaultIfaceIid */");
        }
        // P6.6.4: 事件源信息
        if (!cc.sourceIfaceIid.empty()) {
            entry.emitLine("\"" + cc.sourceIfaceIid + "\",  /* sourceIfaceIid */");
            entry.emitLine(std::to_string(cc.eventNames.size()) + ",  /* eventCount */");
            entry.emitLine("g_vb6_events_" + clsId + ",  /* events */");
        } else {
            entry.emitLine("NULL,  /* sourceIfaceIid */");
            entry.emitLine("0,  /* eventCount */");
            entry.emitLine("NULL,  /* events */");
        }
        entry.dedent();
        entry.emitLine("},");
    }
    entry.dedent();
    entry.emitLine("};");
    entry.emitBlank();

    entry.emitLine("const int g_vb6_coclassCount = " + std::to_string(coClasses.size()) + ";");
    entry.emitBlank();

    // 4. DLL导出函数
    entry.emitLine("// === DLL Export Functions ===");
    entry.emitBlank();

    entry.emitLine("HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {");
    entry.indent();
    entry.emitLine("return vb6_GetClassFactory(rclsid, riid, ppv, g_vb6_coclasses, g_vb6_coclassCount);");
    entry.dedent();
    entry.emitLine("}");
    entry.emitBlank();

    entry.emitLine("HRESULT WINAPI DllCanUnloadNow(void) {");
    entry.indent();
    entry.emitLine("return vb6_DllCanUnloadNow();");
    entry.dedent();
    entry.emitLine("}");
    entry.emitBlank();

    entry.emitLine("HRESULT WINAPI DllRegisterServer(void) {");
    entry.indent();
    entry.emitLine("wchar_t dllPath[MAX_PATH];");
    entry.emitLine("HRESULT hr = vb6_GetDllPath(dllPath, MAX_PATH);");
    entry.emitLine("if (FAILED(hr)) return hr;");
    entry.emitLine("for (int i = 0; i < g_vb6_coclassCount; i++) {");
    entry.indent();
    entry.emitLine("hr = vb6_RegisterCoClass(&g_vb6_coclasses[i], dllPath);");
    entry.emitLine("if (FAILED(hr)) return hr;");
    entry.dedent();
    entry.emitLine("}");
    entry.emitLine("// P6.13: Register embedded TypeLib");
    entry.emitLine("vb6_RegisterTypeLib(dllPath);");
    entry.emitLine("return S_OK;");
    entry.dedent();
    entry.emitLine("}");
    entry.emitBlank();

    entry.emitLine("HRESULT WINAPI DllUnregisterServer(void) {");
    entry.indent();
    entry.emitLine("wchar_t dllPath[MAX_PATH];");
    entry.emitLine("vb6_GetDllPath(dllPath, MAX_PATH);");
    entry.emitLine("for (int i = 0; i < g_vb6_coclassCount; i++) {");
    entry.indent();
    entry.emitLine("vb6_UnregisterCoClass(&g_vb6_coclasses[i]);");
    entry.dedent();
    entry.emitLine("}");
    entry.emitLine("// P6.13: Unregister embedded TypeLib");
    entry.emitLine("vb6_UnregisterTypeLib(dllPath);");
    entry.emitLine("return S_OK;");
    entry.dedent();
    entry.emitLine("}");
    entry.emitBlank();

    entry.emitLine("BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {");
    entry.indent();
    entry.emitLine("if (fdwReason == DLL_PROCESS_ATTACH) {");
    entry.indent();
    entry.emitLine("DisableThreadLibraryCalls(hinstDLL);");
    entry.emitLine("vb6_Init();");
    entry.dedent();
    entry.emitLine("} else if (fdwReason == DLL_PROCESS_DETACH) {");
    entry.indent();
    entry.emitLine("vb6_Exit();");
    entry.dedent();
    entry.emitLine("}");
    entry.emitLine("return TRUE;");
    entry.dedent();
    entry.emitLine("}");
    entry.emitBlank();

    return entry.str();
}
} // namespace vb6c3
