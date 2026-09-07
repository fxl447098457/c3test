#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>

namespace vb6c3 {

// --- cgen_util.cpp: DLL入口 + 类型推断 + AST辅助 + COM辅助 + 控件属性映射 + Variant包装 ---

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
                for (auto& memberName : sym->memberNames) {
                    if (!seenMemberNames.insert(Symbol::toLower(memberName)).second) continue;
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
                        && memSym->access == AccessLevel::Public) {
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

// ============================================================
void CCodeGen::visit(SimpleTypeRef& node) {}
void CCodeGen::visit(ArrayTypeRef& node) {}
void CCodeGen::visit(FixedStringTypeRef& node) {}

// ============================================================
// 模块 visit (generate()已按类别分派, 此处为空)
// ============================================================

void CCodeGen::visit(Module& node) {}

// ============================================================
// 表达式类型推断 (简化版, 用于Select Case等场景)
// ============================================================

Vb6Type CCodeGen::inferExprType(Expr& expr) const {
    switch (expr.kind) {
        case ASTNodeKind::IdentifierExpr: {
            auto& id = static_cast<IdentifierExpr&>(expr);
            // 优先检查已知的变量类型集合 (局部变量在符号表中作用域可能不可达)
            std::string lower = id.name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (knownBstrVars_.count(lower)) return Vb6Type::String;
            if (knownDoubleVars_.count(lower)) return Vb6Type::Double;
            if (knownLongVars_.count(lower)) return Vb6Type::Long;
            if (knownLongPtrVars_.count(lower)) return Vb6Type::LongPtr;
            if (knownVariantVars_.count(lower)) return Vb6Type::Variant;
            // 检查符号表
            auto* sym = symTab_.lookup(id.name);
            if (!sym) sym = symTab_.lookupModule(id.name);
            if (sym) return sym->type;
            break;
        }
        case ASTNodeKind::LiteralExpr: {
            auto& lit = static_cast<LiteralExpr&>(expr);
            if (lit.literalKind == LiteralKind::String) return Vb6Type::String;
            if (lit.literalKind == LiteralKind::Double || lit.literalKind == LiteralKind::Single
                || lit.literalKind == LiteralKind::Currency || lit.literalKind == LiteralKind::Decimal)
                return Vb6Type::Double;
            if (lit.literalKind == LiteralKind::Boolean) return Vb6Type::Boolean;
            if (lit.literalKind == LiteralKind::Date) return Vb6Type::Date;
            return Vb6Type::Long;
        }
        case ASTNodeKind::BinaryExpr: {
            auto& bin = static_cast<BinaryExpr&>(expr);
            // 字符串连接运算符 → String
            if (bin.op == BinaryOp::Concat) return Vb6Type::String;
            // 比较运算符 → Boolean
            if (bin.op == BinaryOp::Eq || bin.op == BinaryOp::Neq ||
                bin.op == BinaryOp::Lt || bin.op == BinaryOp::Gt ||
                bin.op == BinaryOp::Le || bin.op == BinaryOp::Ge ||
                bin.op == BinaryOp::Like || bin.op == BinaryOp::Is)
                return Vb6Type::Boolean;
            // 逻辑运算符 → Boolean (VB6中)
            if (bin.op == BinaryOp::And || bin.op == BinaryOp::Or || bin.op == BinaryOp::Xor)
                return Vb6Type::Boolean;
            // 浮点除法 → Double
            if (bin.op == BinaryOp::Div) return Vb6Type::Double;

            // 算术运算符: 提升左右类型
            {
                Vb6Type lt = inferExprType(*bin.left);
                Vb6Type rt = inferExprType(*bin.right);
                return TypeSystem::promote(lt, rt);
            }
        }
        case ASTNodeKind::UnaryExpr: {
            auto& un = static_cast<UnaryExpr&>(expr);
            if (un.op == UnaryOp::Not) return Vb6Type::Boolean;
            return inferExprType(*un.operand);
        }
        case ASTNodeKind::IndexOrCallExpr: {
            // 函数调用: 返回函数返回类型
            auto& call = static_cast<IndexOrCallExpr&>(expr);
            if (call.callee && call.callee->kind == ASTNodeKind::IdentifierExpr) {
                auto& id = static_cast<IdentifierExpr&>(*call.callee);
                auto* sym = symTab_.lookup(id.name);
                if (!sym) sym = symTab_.lookupModule(id.name);
                if (sym) return sym->type;
            }
            // P26: 类实例方法调用 a.Method(args) → callee是MemberAccessExpr
            // 需要查询成员函数的返回类型, 而非直接fallback到Variant
            if (call.callee && call.callee->kind == ASTNodeKind::MemberAccessExpr) {
                return inferExprType(*call.callee);
            }
            break;
        }
        case ASTNodeKind::MemberAccessExpr: {
            auto& ma = static_cast<MemberAccessExpr&>(expr);
            // P24-12: Err对象特殊处理
            if (ma.object && ma.object->kind == ASTNodeKind::IdentifierExpr) {
                auto& objId = static_cast<IdentifierExpr&>(*ma.object);
                std::string objLower = objId.name;
                std::transform(objLower.begin(), objLower.end(), objLower.begin(), ::tolower);
                if (objLower == "err") {
                    std::string memLower = ma.memberName;
                    std::transform(memLower.begin(), memLower.end(), memLower.begin(), ::tolower);
                    if (memLower == "number") return Vb6Type::Long;
                    if (memLower == "description" || memLower == "source") return Vb6Type::String;
                    if (memLower == "helppath" || memLower == "helpfile" || memLower == "helpcontext") return Vb6Type::String;
                    if (memLower == "lastdllerror") return Vb6Type::Long;
                }
            }
            // Fix 081i: UDT字段访问 — 先查找UDT成员类型，避免lookupModule
            // 匹配到内置函数(如Left→String)导致类型推断错误
            {
                std::string udtCType = inferUdtTypeOfExpr(*ma.object);
                if (!udtCType.empty()) {
                    const std::string prefix = "vb6_type_";
                    if (udtCType.size() > prefix.size()
                        && udtCType.compare(0, prefix.size(), prefix) == 0) {
                        std::string udtName = udtCType.substr(prefix.size());
                        Symbol* udtSym = symTab_.lookupModule(udtName);
                        if (udtSym && udtSym->kind == SymbolKind::UserDefinedType) {
                            std::string memLower = ma.memberName;
                            std::transform(memLower.begin(), memLower.end(), memLower.begin(), ::tolower);
                            for (auto& mi : udtSym->udtMembers) {
                                std::string miLower = mi.name;
                                std::transform(miLower.begin(), miLower.end(), miLower.begin(), ::tolower);
                                if (miLower == memLower) {
                                    return mi.type;  // 找到UDT字段，返回其Vb6Type
                                }
                            }
                            // Fix 084o-2: UDT 已确认但字段未找到 → 字段类型未知, 返回 Unknown.
                            // 不要回退 lookupModule(memberName): 成员名是字段名而非模块符号,
                            // 可能误匹配模块级同名符号 (如 SourceFile 变量) 导致类型误判为 String.
                            // Fix 090ab: 也不应返回 Variant — 否则 UDT 字段 (如 COMSTAT.fBitFields,
                            // 跨模块 Type 符号缺失) 被 isDefinitelyVariantExpr 误判为 Variant,
                            // 实参生成时套 vb6_VariantToLong(int32 字段) → C2440.
                            return Vb6Type::Unknown;
                        }
                    }
                    // Fix 090ab: UDT C 类型已知 (knownUdtVars_) 但符号不可解析 (跨模块
                    // Public Type 在符号表注册表不可达) → 字段类型未知, 保守返回 Unknown,
                    // 禁止回退到模块级同名符号或默认 Variant (否则 COMSTAT.fBitFields 等
                    // 会被误判为 Variant 而错误套用 vb6_VariantToLong → C2440).
                    return Vb6Type::Unknown;
                }
            }
            // 查找成员函数/属性的返回类型
            auto* memSym = symTab_.lookupModule(ma.memberName);
            if (memSym) return memSym->type;
            break;
        }
        // Fix 084o-2: With 块内 UDT 字段访问 (.SourceFile 等) 的类型推断.
        // WithMemberExpr 由 With 语句展开 (_vb6_with_N->field), 必须按 UDT 字段查
        // udtMembers, 且不能回退 lookupModule(memberName) — 与 MemberAccessExpr 同理.
        case ASTNodeKind::WithMemberExpr: {
            auto& wm = static_cast<WithMemberExpr&>(expr);
            if (withObjectInfoStack_.empty() || withObjectVars_.empty()) return Vb6Type::Variant;
            const auto& winfo = withObjectInfoStack_.back();
            if (winfo.kind != WithObjKind::Unknown) {
                // 非 UDT With (类实例/COM 对象): memberName 是属性/方法 → 查成员符号
                auto* memSym2 = symTab_.lookupModule(wm.memberName);
                if (memSym2) return memSym2->type;
                return Vb6Type::Variant;
            }
            std::string tempLower = Symbol::toLower(withObjectVars_.back());
            auto it = knownUdtVars_.find(tempLower);
            if (it == knownUdtVars_.end()) return Vb6Type::Variant;
            const std::string prefix = "vb6_type_";
            const std::string& udtCType = it->second;
            if (udtCType.size() <= prefix.size()
                || udtCType.compare(0, prefix.size(), prefix) != 0) return Vb6Type::Variant;
            std::string udtName = udtCType.substr(prefix.size());
            Symbol* udtSym = symTab_.lookupModule(udtName);
            if (udtSym && udtSym->kind == SymbolKind::UserDefinedType) {
                std::string memLower = Symbol::toLower(wm.memberName);
                for (auto& mi : udtSym->udtMembers) {
                    if (Symbol::toLower(mi.name) == memLower) return mi.type;
                }
            }
            return Vb6Type::Variant;
        }
        default:
            break;
    }
    return Vb6Type::Variant;
}

// Fix 029: 严格 Variant 推断. 仅当表达式明确为 Variant 时返回 true.
// 与 inferExprType 的差异: 内置函数 (sym==null) 与 UDT 字段访问 (lookupModule 失败) 等
// 通过 fallback 返回 Variant 的情形, 此处视为非 Variant, 避免对 int/BSTR 等实参误包装.
bool CCodeGen::isDefinitelyVariantExpr(Expr& expr, bool* isArrOut) const {
    if (isArrOut) *isArrOut = false;
    uint16_t variantArrRaw = static_cast<uint16_t>(Vb6Type::Variant)
                           | static_cast<uint16_t>(Vb6Type::Array);

    // 多态内置函数 denylist: symTab 注册为 Variant, 但 codegen 按上下文
    // 发出类型化版本 (vb6_IIfBSTR/Long/Double, Choose 嵌套三元, Switch 嵌套三元),
    // 实际 C 返回类型不是 vb6_VARIANT. 视为非 Variant 以避免错误包装.
    auto isPolymorphicBuiltin = [](const std::string& name) -> bool {
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        return lower == "iif" || lower == "choose" || lower == "switch";
    };

    auto checkSym = [&](Symbol* sym) -> bool {
        if (!sym) return false;
        if (sym->type == Vb6Type::Variant) return true;
        if (static_cast<uint16_t>(sym->type) == variantArrRaw) {
            if (isArrOut) *isArrOut = true;
            return true;
        }
        return false;
    };

    switch (expr.kind) {
        case ASTNodeKind::IdentifierExpr: {
            auto& id = static_cast<IdentifierExpr&>(expr);
            std::string lower = id.name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            // 已知数组变量: C 类型已是 vb6_SafeArray1D* (元素为具体类型或 VARIANT),
            // 不是 vb6_VARIANT. 防止 symTab_ lookupModule 回退命中其他模块同名
            // Variant 符号, 导致 UBound(arr) 被错误包装成 vb6_VariantToSafeArray1D(arr)
            // (C2440: 无法从 vb6_SafeArray1D* 转换为 vb6_VARIANT, 如 cDataBase WhereIn).
            if (knownArrays_.count(lower)) {
                return false;
            }
            // 已知 Variant 局部变量集合
            if (knownVariantVars_.count(lower)) {
                // 无法区分 Variant 与 Variant(), 视作普通 Variant
                return true;
            }
            // Fix 049b: 如果已知为非 Variant 具体类型 (BSTR/Long/Double),
            // 不应回退到符号表查找 (可能命中其他模块的同名 Variant 符号)
            if (knownBstrVars_.count(lower) || knownLongVars_.count(lower)
                || knownDoubleVars_.count(lower)) {
                return false;
            }
            // 符号表查询
            auto* sym = symTab_.lookup(id.name);
            if (!sym) sym = symTab_.lookupModule(id.name);
            return checkSym(sym);
        }
        case ASTNodeKind::IndexOrCallExpr: {
            auto& call = static_cast<IndexOrCallExpr&>(expr);
            if (call.callee && call.callee->kind == ASTNodeKind::IdentifierExpr) {
                auto& cid = static_cast<IdentifierExpr&>(*call.callee);
                // 多态内置函数: 实际返回类型与 symTab 注册不同, 不视为 Variant
                if (isPolymorphicBuiltin(cid.name)) return false;
                auto* sym = symTab_.lookup(cid.name);
                if (!sym) sym = symTab_.lookupModule(cid.name);
                // 关键差异: sym==null (内置函数) 视为非 Variant
                return checkSym(sym);
            }
            // 类方法调用 a.Method(): 递归推断 callee 类型
            if (call.callee && call.callee->kind == ASTNodeKind::MemberAccessExpr) {
                bool arr = false;
                bool v = isDefinitelyVariantExpr(*call.callee, &arr);
                if (v && isArrOut) *isArrOut = arr;
                return v;
            }
            return false;
        }
        case ASTNodeKind::MemberAccessExpr: {
            auto& ma = static_cast<MemberAccessExpr&>(expr);
            // Err 对象特殊处理 (与 inferExprType 一致)
            if (ma.object && ma.object->kind == ASTNodeKind::IdentifierExpr) {
                auto& objId = static_cast<IdentifierExpr&>(*ma.object);
                std::string objLower = objId.name;
                std::transform(objLower.begin(), objLower.end(), objLower.begin(), ::tolower);
                if (objLower == "err") {
                    // Err.* 的具体类型见 inferExprType, 均为具体类型 (Long/String), 非 Variant
                    return false;
                }
            }
            // Fix 090l: UDT 字段访问 — 字段声明 As Variant (如 cZipArchive
            // ZipVfsType.BufferArray / SourceFileInfo) 是明确 Variant 表达式.
            // 此前 memSym==null (UDT 字段) 被一律视为非 Variant, 导致
            // UBound(vb6_ret_X.BufferArray) 等不包装 vb6_VariantToSafeArray1D →
            // C2440 (无法从 vb6_VARIANT 转 vb6_SafeArray1D*).
            if (!inferUdtTypeOfExpr(*ma.object).empty()) {
                return inferExprType(expr) == Vb6Type::Variant;
            }
            // 符号表查找成员 (Property/Function)
            auto* memSym = symTab_.lookupModule(ma.memberName);
            // 关键差异: memSym==null (UDT 字段访问或外部类成员) 视为非 Variant
            return checkSym(memSym);
        }
        default:
            // 其他表达式 (BinaryExpr/UnaryExpr/LiteralExpr 等) 不会明确返回 Variant,
            // 除非其子表达式明确为 Variant. 此处不递归, 保持严格性.
            return false;
    }
}

// ============================================================
// Fix 038b-1: 基于 C 表达式字符串的 Variant 检测
// ============================================================
// 补充 isDefinitelyVariantExpr 的 AST 级检测. 当 codegen 生成的 C 表达式
// 包含已知返回 vb6_VARIANT 的函数调用时, 判定为 Variant.
// 仅检查顶层表达式 (去除前导括号/空白后), 避免对子表达式误判.

bool CCodeGen::cExprIsVariant(const std::string& cExpr) {
    // 去除前导空白和括号
    size_t start = 0;
    while (start < cExpr.size()) {
        char c = cExpr[start];
        if (c == '(' || c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            start++;
        } else {
            break;
        }
    }
    if (start >= cExpr.size()) return false;

    // 已知返回 vb6_VARIANT 的函数前缀
    static const std::vector<std::string> variantPrefixes = {
        "vb6_VariantArrayGet(",
        "vb6_VariantFromComResult(",
        "vb6_VariantFromStackVARIANT(",
        "vb6_VariantFromValue(",
        "vb6_VariantEmpty(",
        "vb6_VariantLong(",
        "vb6_VariantDouble(",
        "vb6_VariantString(",
        "vb6_VariantBool(",
        "vb6_VariantArray(",
        "vb6_VariantObject(",
        "vb6_VariantNull(",
        "vb6_VariantNothing(",
        "vb6_VariantFromI2(",
        "vb6_VariantFromI4(",
        "vb6_VariantFromR4(",
        "vb6_VariantFromR8(",
        "vb6_VariantFromBSTR(",
        "vb6_VariantFromBool(",
        "vb6_VariantFromDate(",
        "vb6_VariantFromUI1(",
        "vb6_VariantFromSafeArray(",
        "vb6_DispCallByVtbl(",  // Fix 068: DispCallByVtbl returns Variant
    };
    for (const auto& prefix : variantPrefixes) {
        if (cExpr.compare(start, prefix.size(), prefix) == 0) return true;
    }

    // Fix 040b: VB6_SA_AT(vb6_VARIANT, arr, idx) expands to an array element
    // of type vb6_VARIANT — also a Variant expression.
    if (cExpr.compare(start, 21, "VB6_SA_AT(vb6_VARIANT") == 0) return true;

    // Fix 045: 检查项目函数是否返回 Variant — 通过 driver.cpp 预扫描构建的
    // C 函数名集合. 提取 C 表达式中的函数名 (从 start 到第一个 '(') 并查集合.
    if (variantReturnFuncs_) {
        size_t parenPos = cExpr.find('(', start);
        if (parenPos != std::string::npos) {
            std::string funcName = cExpr.substr(start, parenPos - start);
            if (variantReturnFuncs_->count(funcName)) return true;
        }
    }

    // 检查 (&(vb6_VARIANT){...}) 复合字面量 — 也是 VARIANT 类型
    // 但这种形式通常作为 ByRef 参数传递, 不需要再转换, 故不检测.

    return false;
}

// ============================================================
// Fix 038b-5: 运行时函数参数 C 类型查找表
// ============================================================
// 当 calleeParams 为空 (运行时/内置函数) 时, 通过函数名和参数索引查找
// 期望的 C 类型. 返回空字符串表示未知.

std::string CCodeGen::getRuntimeParamCType(const std::string& funcName, size_t paramIdx) {
    static const std::unordered_map<std::string, std::vector<std::string>> table = {
        // Array 创建/设置
        {"vb6_ArraySetLong",    {"vb6_SafeArray1D*", "int32_t", "int32_t"}},
        {"vb6_ArraySetBSTR",    {"vb6_SafeArray1D*", "int32_t", "BSTR"}},
        {"vb6_ArraySetDouble",  {"vb6_SafeArray1D*", "int32_t", "double"}},
        {"vb6_ArraySetVariant", {"vb6_SafeArray1D*", "int32_t", "vb6_VARIANT"}},
        {"vb6_ArrayGetLong",    {"vb6_SafeArray1D*", "int32_t"}},
        {"vb6_ArrayGetBSTR",    {"vb6_SafeArray1D*", "int32_t"}},
        {"vb6_ArrayGetDouble",  {"vb6_SafeArray1D*", "int32_t"}},
        {"vb6_ArrayGetVariant", {"vb6_SafeArray1D*", "int32_t"}},
        // BSTR 操作
        {"vb6_BSTR_Assign",     {"BSTR*", "BSTR"}},
        {"vb6_BSTR_Concat",     {"BSTR", "BSTR"}},
        {"vb6_BSTR_ConcatFree", {"BSTR", "BSTR"}},
        {"vb6_BSTR_FromStr",    {"const wchar_t*"}},
        {"vb6_BSTR_Empty",      {}},
        {"vb6_BSTR_ToANSI",     {"BSTR"}},
        {"vb6_BSTR_Free",       {"BSTR*"}},
        {"vb6_BSTR_Clone",      {"BSTR"}},
        // 字符串比较/操作
        {"vb6_StrCmp",          {"BSTR", "BSTR"}},
        {"vb6_StrComp",         {"BSTR", "BSTR", "int32_t"}},
        {"vb6_Val",             {"BSTR"}},
        {"vb6_Trim",            {"BSTR"}},
        {"vb6_LTrim",           {"BSTR"}},
        {"vb6_RTrim",           {"BSTR"}},
        {"vb6_Left",            {"BSTR", "int32_t"}},
        {"vb6_Right",           {"BSTR", "int32_t"}},
        {"vb6_Mid",             {"BSTR", "int32_t", "int32_t"}},
        {"vb6_Len",             {"BSTR"}},
        {"vb6_LenB",            {"BSTR"}},
        {"vb6_InStr",           {"BSTR", "BSTR"}},
        {"vb6_Replace",         {"BSTR", "BSTR", "BSTR"}},
        {"vb6_Split",           {"BSTR", "BSTR"}},
        {"vb6_Join",            {"vb6_SafeArray1D*", "BSTR"}},
        {"vb6_UCase",           {"BSTR"}},
        {"vb6_LCase",           {"BSTR"}},
        {"vb6_Space",           {"int32_t"}},
        {"vb6_String",          {"int32_t", "int32_t"}},
        {"vb6_Chr",             {"int32_t"}},
        {"vb6_Asc",             {"BSTR"}},
        {"vb6_Hex",             {"int32_t"}},
        {"vb6_Oct",             {"int32_t"}},
        // 类型转换
        {"vb6_CStr",            {"vb6_VARIANT"}},
        {"vb6_CLng",            {"double"}},
        {"vb6_CInt",            {"double"}},
        {"vb6_CDbl",            {"double"}},
        {"vb6_CSng",            {"double"}},
        {"vb6_CBool",           {"vb6_VARIANT"}},
        {"vb6_CByte",           {"vb6_VARIANT"}},
        {"vb6_CDate",           {"vb6_VARIANT"}},
        {"vb6_CCur",            {"vb6_VARIANT"}},
        // 数组操作
        {"vb6_UBound",          {"vb6_SafeArray1D*", "int32_t"}},
        {"vb6_LBound",          {"vb6_SafeArray1D*", "int32_t"}},
        {"vb6_ArrayCreate",     {"int32_t"}},
        // 消息框
        {"vb6_MsgBox",          {"BSTR"}},
        {"vb6_MsgBox1",         {"BSTR"}},
        // 错误处理
        {"vb6_ErrRaise",        {"int32_t", "BSTR", "BSTR"}},
        {"vb6_ErrNumber",       {}},
        {"vb6_ErrDescription",  {}},
        {"vb6_ErrSource",       {}},
        {"vb6_ErrClear",        {}},
        // IsMissing
        {"vb6_IsMissing",       {"vb6_VARIANT*"}},
        // Variant 提取
        {"vb6_VariantToLong",      {"vb6_VARIANT"}},
        {"vb6_VariantToDouble",    {"vb6_VARIANT"}},
        {"vb6_VariantToString",    {"vb6_VARIANT"}},
        {"vb6_VariantToBool",      {"vb6_VARIANT"}},
        {"vb6_VariantToSafeArray1D", {"vb6_VARIANT"}},
        {"vb6_VariantToObjectVal", {"vb6_VARIANT"}},
        // Fix 046: IIf family + Variant-aware conversion functions
        {"vb6_IIfBSTR",         {"int32_t", "BSTR", "BSTR"}},
        {"vb6_IIfLong",         {"int32_t", "int32_t", "int32_t"}},
        {"vb6_IIfDouble",       {"int32_t", "double", "double"}},
        {"vb6_IIfVariant",      {"int32_t", "vb6_VARIANT", "vb6_VARIANT"}},
        {"vb6_CLngV",           {"vb6_VARIANT"}},
        {"vb6_CIntV",           {"vb6_VARIANT"}},
        {"vb6_IntDiv",          {"int32_t", "int32_t"}},
        // Debug
        {"vb6_DebugPrint",      {"BSTR"}},
        {"vb6_DebugWriteLong",  {"int32_t"}},
        // 对象操作
        {"vb6_StrPtr",          {"BSTR"}},   // Fix 084o-7: StrPtr(Variant) → vb6_VariantToString 先行
        {"vb6_ObjPtr",          {"uintptr_t"}},
        {"vb6_ReleaseObject",   {"void**"}},
        {"vb6_NewObject",       {"const wchar_t*"}},
        {"vb6_CallByName",      {"void*", "BSTR", "int32_t"}},
    };
    auto it = table.find(funcName);
    if (it != table.end() && paramIdx < it->second.size()) {
        return it->second[paramIdx];
    }
    return "";
}

// ============================================================
// AST辅助: 检测语句列表中是否包含GoSubStmt
// ============================================================

bool CCodeGen::hasGoSubInStmts(StmtList& stmts) const {
    for (auto& stmt : stmts) {
        if (!stmt) continue;
        if (stmt->kind == ASTNodeKind::GoSubStmt) return true;
if (stmt->kind == ASTNodeKind::OnGoSubStmt) return true;  // P17.4
        // 递归检查复合语句
        if (stmt->kind == ASTNodeKind::IfStmt) {
            auto& ifStmt = static_cast<IfStmt&>(*stmt);
            if (hasGoSubInStmts(ifStmt.thenBody)) return true;
            if (hasGoSubInStmts(ifStmt.elseBody)) return true;
            for (auto& elif : ifStmt.elseIfs) {
                if (hasGoSubInStmts(elif->body)) return true;
            }
        } else if (stmt->kind == ASTNodeKind::ForStmt) {
            if (hasGoSubInStmts(static_cast<ForStmt&>(*stmt).body)) return true;
        } else if (stmt->kind == ASTNodeKind::ForEachStmt) {
            // Fix 043a: ForEachStmt was missing — GoSub inside For Each was not detected
            if (hasGoSubInStmts(static_cast<ForEachStmt&>(*stmt).body)) return true;
        } else if (stmt->kind == ASTNodeKind::DoLoopStmt) {
            if (hasGoSubInStmts(static_cast<DoLoopStmt&>(*stmt).body)) return true;
        } else if (stmt->kind == ASTNodeKind::WhileWendStmt) {
            if (hasGoSubInStmts(static_cast<WhileWendStmt&>(*stmt).body)) return true;
        } else if (stmt->kind == ASTNodeKind::SelectCaseStmt) {
            auto& sel = static_cast<SelectCaseStmt&>(*stmt);
            for (auto& c : sel.cases) {
                if (hasGoSubInStmts(c->body)) return true;
            }
            if (hasGoSubInStmts(sel.elseCase)) return true;
        }
    }
    return false;
}

// Bug #1 fix (082h): 预扫描语句列表, 收集UBound/LBound(arr, N>1)的数组名
// 到 knownNDArraysInProc_, 这样后续 UBound(arr, 1) 也能正确使用ND版本
void CCodeGen::scanNDArraysInExpr(Expr& expr) {
    if (expr.kind == ASTNodeKind::IndexOrCallExpr) {
        auto& call = static_cast<IndexOrCallExpr&>(expr);
        // Check if this is UBound/LBound with dimension > 1
        if (call.callee && call.callee->kind == ASTNodeKind::IdentifierExpr) {
            std::string name = static_cast<IdentifierExpr&>(*call.callee).name;
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            if ((name == "ubound" || name == "lbound") && call.positional.size() >= 2) {
                // Check if dimension arg > 1 (must be a literal integer)
                auto& dimArg = call.positional[1];
                if (dimArg && dimArg->kind == ASTNodeKind::LiteralExpr) {
                    auto& lit = static_cast<LiteralExpr&>(*dimArg);
                    if (lit.literalKind == LiteralKind::Integer && lit.intValue > 1) {
                        // Register the array name
                        auto& arrArg = call.positional[0];
                        if (arrArg && arrArg->kind == ASTNodeKind::IdentifierExpr) {
                            std::string arrName = static_cast<IdentifierExpr&>(*arrArg).name;
                            std::transform(arrName.begin(), arrName.end(), arrName.begin(), ::tolower);
                            knownNDArraysInProc_.insert(arrName);
                        }
                    }
                }
            }
        }
        // Also scan sub-expressions in call args and callee
        if (call.callee) scanNDArraysInExpr(*call.callee);
        for (auto& arg : call.positional) {
            if (arg) scanNDArraysInExpr(*arg);
        }
    } else if (expr.kind == ASTNodeKind::BinaryExpr) {
        auto& bin = static_cast<BinaryExpr&>(expr);
        if (bin.left) scanNDArraysInExpr(*bin.left);
        if (bin.right) scanNDArraysInExpr(*bin.right);
    } else if (expr.kind == ASTNodeKind::UnaryExpr) {
        auto& un = static_cast<UnaryExpr&>(expr);
        if (un.operand) scanNDArraysInExpr(*un.operand);
    } else if (expr.kind == ASTNodeKind::MemberAccessExpr) {
        auto& ma = static_cast<MemberAccessExpr&>(expr);
        if (ma.object) scanNDArraysInExpr(*ma.object);
    } else if (expr.kind == ASTNodeKind::LiteralExpr) {
        // No sub-expressions
    } else if (expr.kind == ASTNodeKind::IdentifierExpr) {
        // No sub-expressions
    } else if (expr.kind == ASTNodeKind::WithMemberExpr) {
        // No sub-expressions to scan
    }
}

void CCodeGen::scanNDArraysInStmts(StmtList& stmts) {
    for (auto& stmt : stmts) {
        if (!stmt) continue;
        // Scan expressions in statements
        if (stmt->kind == ASTNodeKind::AssignmentStmt) {
            auto& assign = static_cast<AssignmentStmt&>(*stmt);
            if (assign.value) scanNDArraysInExpr(*assign.value);
        } else if (stmt->kind == ASTNodeKind::CallStmt) {
            auto& call = static_cast<CallStmt&>(*stmt);
            if (call.callee) scanNDArraysInExpr(*call.callee);
        } else if (stmt->kind == ASTNodeKind::ForStmt) {
            auto& forStmt = static_cast<ForStmt&>(*stmt);
            if (forStmt.start) scanNDArraysInExpr(*forStmt.start);
            if (forStmt.end) scanNDArraysInExpr(*forStmt.end);
            if (forStmt.step) scanNDArraysInExpr(*forStmt.step);
            scanNDArraysInStmts(forStmt.body);
        } else if (stmt->kind == ASTNodeKind::ForEachStmt) {
            auto& fe = static_cast<ForEachStmt&>(*stmt);
            if (fe.collection) scanNDArraysInExpr(*fe.collection);
            scanNDArraysInStmts(fe.body);
        } else if (stmt->kind == ASTNodeKind::DoLoopStmt) {
            auto& dl = static_cast<DoLoopStmt&>(*stmt);
            if (dl.condition) scanNDArraysInExpr(*dl.condition);
            scanNDArraysInStmts(dl.body);
        } else if (stmt->kind == ASTNodeKind::WhileWendStmt) {
            auto& ww = static_cast<WhileWendStmt&>(*stmt);
            if (ww.condition) scanNDArraysInExpr(*ww.condition);
            scanNDArraysInStmts(ww.body);
        } else if (stmt->kind == ASTNodeKind::IfStmt) {
            auto& ifStmt = static_cast<IfStmt&>(*stmt);
            if (ifStmt.condition) scanNDArraysInExpr(*ifStmt.condition);
            scanNDArraysInStmts(ifStmt.thenBody);
            scanNDArraysInStmts(ifStmt.elseBody);
            for (auto& elif : ifStmt.elseIfs) {
                if (elif && elif->condition) scanNDArraysInExpr(*elif->condition);
                if (elif) scanNDArraysInStmts(elif->body);
            }
        } else if (stmt->kind == ASTNodeKind::SelectCaseStmt) {
            auto& sel = static_cast<SelectCaseStmt&>(*stmt);
            if (sel.testExpr) scanNDArraysInExpr(*sel.testExpr);
            for (auto& c : sel.cases) {
                if (c) scanNDArraysInStmts(c->body);
            }
            scanNDArraysInStmts(sel.elseCase);
        } else if (stmt->kind == ASTNodeKind::SetStmt) {
            auto& set = static_cast<SetStmt&>(*stmt);
            if (set.value) scanNDArraysInExpr(*set.value);
        } else if (stmt->kind == ASTNodeKind::LetStmt) {
            auto& let = static_cast<LetStmt&>(*stmt);
            if (let.value) scanNDArraysInExpr(*let.value);
        } else if (stmt->kind == ASTNodeKind::WithStmt) {
            auto& with = static_cast<WithStmt&>(*stmt);
            if (with.object) scanNDArraysInExpr(*with.object);
            scanNDArraysInStmts(with.body);
        } else if (stmt->kind == ASTNodeKind::Block) {
            auto& block = static_cast<Block&>(*stmt);
            scanNDArraysInStmts(block.stmts);
        }
    }
}

// P14.1.2: 检测语句列表中是否包含Resume/Resume Next语句
bool CCodeGen::hasResumeInStmts(StmtList& stmts) const {
    for (auto& stmt : stmts) {
        if (!stmt) continue;
        if (stmt->kind == ASTNodeKind::ResumeStmt) {
            auto& resume = static_cast<ResumeStmt&>(*stmt);
            if (resume.resumeKind == ResumeKind::ResumeHere || resume.resumeKind == ResumeKind::ResumeNext)
                return true;
        }
        // 递归检查复合语句
        if (stmt->kind == ASTNodeKind::IfStmt) {
            auto& ifStmt = static_cast<IfStmt&>(*stmt);
            if (hasResumeInStmts(ifStmt.thenBody)) return true;
            if (hasResumeInStmts(ifStmt.elseBody)) return true;
            for (auto& elif : ifStmt.elseIfs) {
                if (hasResumeInStmts(elif->body)) return true;
            }
        } else if (stmt->kind == ASTNodeKind::ForStmt) {
            if (hasResumeInStmts(static_cast<ForStmt&>(*stmt).body)) return true;
        } else if (stmt->kind == ASTNodeKind::ForEachStmt) {
            // Fix 043a: ForEachStmt was missing — Resume inside For Each was not detected
            if (hasResumeInStmts(static_cast<ForEachStmt&>(*stmt).body)) return true;
        } else if (stmt->kind == ASTNodeKind::DoLoopStmt) {
            if (hasResumeInStmts(static_cast<DoLoopStmt&>(*stmt).body)) return true;
        } else if (stmt->kind == ASTNodeKind::WhileWendStmt) {
            if (hasResumeInStmts(static_cast<WhileWendStmt&>(*stmt).body)) return true;
        } else if (stmt->kind == ASTNodeKind::SelectCaseStmt) {
            auto& sel = static_cast<SelectCaseStmt&>(*stmt);
            for (auto& c : sel.cases) {
                if (hasResumeInStmts(c->body)) return true;
            }
            if (hasResumeInStmts(sel.elseCase)) return true;
        }
    }
    return false;
}

// ============================================================
// P12.3: 检测语句列表中是否包含OnErrorStmt
// ============================================================

bool CCodeGen::hasOnErrorInStmts(StmtList& stmts) const {
    for (auto& stmt : stmts) {
        if (!stmt) continue;
        if (stmt->kind == ASTNodeKind::OnErrorStmt || stmt->kind == ASTNodeKind::ResumeStmt) return true;
        // 递归检查复合语句
        if (stmt->kind == ASTNodeKind::IfStmt) {
            auto& ifStmt = static_cast<IfStmt&>(*stmt);
            if (hasOnErrorInStmts(ifStmt.thenBody)) return true;
            if (hasOnErrorInStmts(ifStmt.elseBody)) return true;
            for (auto& elif : ifStmt.elseIfs) {
                if (hasOnErrorInStmts(elif->body)) return true;
            }
        } else if (stmt->kind == ASTNodeKind::ForStmt) {
            if (hasOnErrorInStmts(static_cast<ForStmt&>(*stmt).body)) return true;
        } else if (stmt->kind == ASTNodeKind::ForEachStmt) {
            // Fix 043a: ForEachStmt was missing — OnError inside For Each was not detected
            if (hasOnErrorInStmts(static_cast<ForEachStmt&>(*stmt).body)) return true;
        } else if (stmt->kind == ASTNodeKind::DoLoopStmt) {
            if (hasOnErrorInStmts(static_cast<DoLoopStmt&>(*stmt).body)) return true;
        } else if (stmt->kind == ASTNodeKind::WhileWendStmt) {
            if (hasOnErrorInStmts(static_cast<WhileWendStmt&>(*stmt).body)) return true;
        } else if (stmt->kind == ASTNodeKind::SelectCaseStmt) {
            auto& sel = static_cast<SelectCaseStmt&>(*stmt);
            for (auto& c : sel.cases) {
                if (hasOnErrorInStmts(c->body)) return true;
            }
            if (hasOnErrorInStmts(sel.elseCase)) return true;
        }
    }
    return false;
}


// ============================================================
// COM辅助 (P6.2)
// ============================================================

std::string CCodeGen::resolveComValue(const std::string& unpackType) {
    // P24-07: 早期绑定 — 利用签名returnType选择正确的解包函数
    if (!isComMarker_) return lastExpr_;

    std::string objExpr = std::move(comObjExpr_);
    std::string memberName = std::move(comMemberName_);

    // P24-07: 早期绑定推断 — 利用TypeLib签名的returnType决策
    if (isEarlyBoundCom_ && earlyBoundSym_) {
        isEarlyBoundCom_ = false;
        const Symbol* comSym = earlyBoundSym_;
        earlyBoundSym_ = nullptr;
        std::string memLower = memberName;
        std::transform(memLower.begin(), memLower.end(), memLower.begin(), ::tolower);
        auto it = comSym->comMethods.find(memLower);
        if (it != comSym->comMethods.end()) {
            const auto& sig = it->second;
            std::string returnType = mapType(sig.returnType);
            std::string getPropArgs = objExpr + ", L\"" + memberName + "\"";
            if (returnType == "BSTR") {
                lastExpr_ = "vb6_ComGetStringProp(" + getPropArgs + ")";
            } else if (returnType == "int32_t" || returnType == "int16_t") {
                lastExpr_ = "vb6_ComGetIntProp(" + getPropArgs + ")";
            } else if (returnType == "double" || returnType == "float") {
                lastExpr_ = "vb6_ComGetDoubleProp(" + getPropArgs + ")";
            } else if (returnType == "void*") {
                lastExpr_ = "vb6_ComGetObjectProp(" + getPropArgs + ")";
            } else {
                // P24-07: 未知返回类型(如Enum→UserDefinedType) → 按目标变量类型选择
                if (unpackType == "BSTR") {
                    lastExpr_ = "vb6_ComGetStringProp(" + getPropArgs + ")";
                } else if (unpackType == "Int" || unpackType == "Long" || unpackType == "Boolean") {
                    lastExpr_ = "vb6_ComGetIntProp(" + getPropArgs + ")";
                } else if (unpackType == "Double" || unpackType == "Single") {
                    lastExpr_ = "vb6_ComGetDoubleProp(" + getPropArgs + ")";
                } else if (unpackType == "Object") {
                    lastExpr_ = "vb6_ComGetObjectProp(" + getPropArgs + ")";
                } else {
                    lastExpr_ = "vb6_VariantFromComResult(vb6_ComCall(" + objExpr + ", L\"" + memberName + "\", NULL, 0))";
                }
            }
            isComMarker_ = false;
            return lastExpr_;
        }
    }
    isEarlyBoundCom_ = false;
    isComMarker_ = false;

    std::string getPropArgs = objExpr + ", L\"" + memberName + "\"";

    if (unpackType == "BSTR") {
        lastExpr_ = "vb6_ComGetStringProp(" + getPropArgs + ")";
    } else if (unpackType == "Int" || unpackType == "Long" || unpackType == "Boolean") {
        lastExpr_ = "vb6_ComGetIntProp(" + getPropArgs + ")";
    } else if (unpackType == "Double" || unpackType == "Single") {
        lastExpr_ = "vb6_ComGetDoubleProp(" + getPropArgs + ")";
    } else if (unpackType == "Object") {
        lastExpr_ = "vb6_ComGetObjectProp(" + getPropArgs + ")";
    } else if (unpackType == "Variant") {
        // P24-02: COM属性返回原生VARIANT(如dic.Keys/dic.Items返回SAFEARRAY)
        lastExpr_ = "vb6_VariantFromComResult(vb6_ComCall(" + objExpr + ", L\"" + memberName + "\", NULL, 0))";
    } else {
        // 默认: BSTR解封 (最通用, COM VARIANT → BSTR自动转换)
        lastExpr_ = "vb6_ComGetStringProp(" + getPropArgs + ")";
    }
    return lastExpr_;
}

std::string CCodeGen::comPackExpr(Expr& expr) {
    // 根据表达式类型推断应该用的VARIANT封装函数
    Vb6Type vt = inferExprType(expr);
    switch (vt) {
        case Vb6Type::String:
            return "vb6_ComPackBSTR";  // BSTR → VARIANT
        case Vb6Type::Integer:
        case Vb6Type::Long:
            return "vb6_ComPackInt";   // int32_t → VARIANT
        case Vb6Type::Boolean:
            return "vb6_ComPackBool";  // VB6 Boolean → VARIANT VT_BOOL
        case Vb6Type::Single:
        case Vb6Type::Double:
            return "vb6_ComPackDouble"; // double → VARIANT
        case Vb6Type::Object:
            return "vb6_ComPackObject"; // void* → VARIANT
        case Vb6Type::Variant:
            return "vb6_ComPackValue";  // Fix 030: 通用打包宏 — 路由任意 C 类型实参 (inferExprType 回退 Variant 时安全)
        default:
            // Variant/未知: 尝试用BSTR封装 (运行时会处理转换)
            // 更安全的做法: 检查已知变量类型
            if (expr.kind == ASTNodeKind::IdentifierExpr) {
                auto& id = static_cast<IdentifierExpr&>(expr);
                std::string lower = id.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if (knownObjectVars_.count(lower)) return "vb6_ComPackObject";
                if (knownBstrVars_.count(lower)) return "vb6_ComPackBSTR";
                if (knownDoubleVars_.count(lower)) return "vb6_ComPackDouble";
                if (knownLongVars_.count(lower)) return "vb6_ComPackInt";
                if (knownVariantVars_.count(lower)) return "vb6_ComPackVariant";
            }
            if (expr.kind == ASTNodeKind::MemberAccessExpr) {
                auto& ma = static_cast<MemberAccessExpr&>(expr);
                if (ma.object && ma.object->kind == ASTNodeKind::IdentifierExpr) {
                    auto& objId = static_cast<IdentifierExpr&>(*ma.object);
                    std::string objLower = objId.name;
                    std::transform(objLower.begin(), objLower.end(), objLower.begin(), ::tolower);
                    if (knownVariantVars_.count(objLower)) return "vb6_ComPackVariant";
                }
            }
            return "vb6_ComPackInt";  // 默认整数封装
    }
}

// P25: 解析COM标记为类型化属性取值, 用于COM调用参数打包
// 当isComMarker_为true时, 根据packFnHint选择对应类型的COM属性取值函数
// 如果isComMarker_为false, 返回空串
std::string CCodeGen::resolveComMarkerForPack(const std::string& packFnHint) {
    // P26: vb6_ComPackVariant / Fix 030: vb6_ComPackValue 需要把 COM 调用返回的
    // VARIANT* 转成 vb6_VARIANT (即使 isComMarker_ 已被消费, lastExpr_ 仍可能是 COM 调用结果).
    // vb6_ComPackValue(vb6_VariantFromComResult(ComCall)) 经 _Generic VariantIdentity 路径
    // 等价于 vb6_ComPackVariant(vb6_VariantFromComResult(ComCall)).
    if (packFnHint == "vb6_ComPackVariant" || packFnHint == "vb6_ComPackValue") {
        if (lastExpr_.find("vb6_ComCall(") == 0 ||
            lastExpr_.find("vb6_ComGetProp(") == 0 ||
            lastExpr_.find("vb6_ComGetObjectProp(") == 0 ||
            lastExpr_.find("vb6_ComCallObject(") == 0) {
            return "vb6_VariantFromComResult(" + lastExpr_ + ")";
        }
    }
    if (!isComMarker_) return "";
    isComMarker_ = false;
    std::string objExpr = std::move(comObjExpr_);
    std::string memName = std::move(comMemberName_);

    // 前期绑定: 利用签名确定返回类型
    if (isEarlyBoundCom_ && earlyBoundSym_) {
        isEarlyBoundCom_ = false;
        const Symbol* comSym = earlyBoundSym_;
        earlyBoundSym_ = nullptr;
        std::string memLower = memName;
        std::transform(memLower.begin(), memLower.end(), memLower.begin(), ::tolower);
        auto it = comSym->comMethods.find(memLower);
        if (it != comSym->comMethods.end() && it->second.isPropertyGet) {
            const auto& sig = it->second;
            std::string returnType = mapType(sig.returnType);
            if (returnType == "int32_t" || returnType == "int16_t") {
                return "vb6_ComGetIntProp(" + objExpr + ", L\"" + memName + "\")";
            } else if (returnType == "BSTR") {
                return "vb6_ComGetStringProp(" + objExpr + ", L\"" + memName + "\")";
            } else if (returnType == "double" || returnType == "float") {
                return "vb6_ComGetDoubleProp(" + objExpr + ", L\"" + memName + "\")";
            } else if (returnType == "void*") {
                return "vb6_ComGetObjectProp(" + objExpr + ", L\"" + memName + "\")";
            }
        }
    }

    // 后期绑定: 根据packFnHint推断所需的属性取值函数
    // packFnHint由comPackExpr根据上下文确定, 代表参数期望的C类型
    if (packFnHint == "vb6_ComPackObject") {
        return "vb6_ComGetObjectProp(" + objExpr + ", L\"" + memName + "\")";
    } else if (packFnHint == "vb6_ComPackBSTR" || packFnHint.empty()) {
        return "vb6_ComGetStringProp(" + objExpr + ", L\"" + memName + "\")";
    } else if (packFnHint == "vb6_ComPackInt") {
        return "vb6_ComGetIntProp(" + objExpr + ", L\"" + memName + "\")";
    } else if (packFnHint == "vb6_ComPackDouble") {
        return "vb6_ComGetDoubleProp(" + objExpr + ", L\"" + memName + "\")";
    } else if (packFnHint == "vb6_ComPackBool") {
        return "vb6_ComGetIntProp(" + objExpr + ", L\"" + memName + "\")";
    }
    // vb6_ComPackVariant / Fix 030 vb6_ComPackValue: 需要把 COM 返回的 VARIANT* 转成 vb6_VARIANT
    if (packFnHint == "vb6_ComPackVariant" || packFnHint == "vb6_ComPackValue") {
        if (lastExpr_.find("vb6_ComCall(") == 0 ||
            lastExpr_.find("vb6_ComGetProp(") == 0 ||
            lastExpr_.find("vb6_ComGetObjectProp(") == 0 ||
            lastExpr_.find("vb6_ComCallObject(") == 0) {
            return "vb6_VariantFromComResult(" + lastExpr_ + ")";
        }
    }
    return "";
}

// ============================================================
// Fix 010r-16: COM/Property-Get 左值重写辅助函数实现
// ============================================================
bool CCodeGen::tryRewriteCOMLvalue(const std::string& target, const std::string& value,
                                   Expr* valueExpr, bool isSet) {
    if (target.empty()) return false;

    // 工具 lambda: 在 target 中查找首个顶层括号 (跳过嵌套括号), 返回 '(' 与 ')'
    // 的位置; 找不到返回 npos.
    auto findCallParens = [](const std::string& s, size_t startAt) -> std::pair<size_t, size_t> {
        size_t openPos = std::string::npos;
        for (size_t i = startAt; i < s.size(); ++i) {
            if (s[i] == '(') { openPos = i; break; }
        }
        if (openPos == std::string::npos) return {std::string::npos, std::string::npos};
        int depth = 0;
        for (size_t i = openPos; i < s.size(); ++i) {
            if (s[i] == '(') depth++;
            else if (s[i] == ')') {
                depth--;
                if (depth == 0) return {openPos, i};
            }
        }
        return {std::string::npos, std::string::npos};
    };
    // 工具 lambda: 按顶层逗号分割调用实参 (跳过括号/方括号/花括号内的逗号), 每段 trim.
    auto splitTopLevelArgs = [](const std::string& s) -> std::vector<std::string> {
        std::vector<std::string> out;
        if (s.empty()) return out;
        int depth = 0;
        size_t start = 0;
        for (size_t i = 0; i < s.size(); ++i) {
            char ch = s[i];
            if (ch == '(' || ch == '[' || ch == '{') { depth++; }
            else if (ch == ')' || ch == ']' || ch == '}') { if (depth > 0) depth--; }
            else if (ch == ',' && depth == 0) {
                out.push_back(s.substr(start, i - start));
                start = i + 1;
            }
        }
        out.push_back(s.substr(start));
        for (auto& seg : out) {
            size_t b = seg.find_first_not_of(" \t");
            if (b == std::string::npos) { seg.clear(); continue; }
            size_t e = seg.find_last_not_of(" \t");
            seg = seg.substr(b, e - b + 1);
        }
        return out;
    };

    // ---- Pattern A: vb6_ComCall(obj, L"Item", args, n) = value ----
    // vb6_ComCall 返回 VARIANT*, 不是左值. 改走 vb6_ComSetPropArg (内部
    // DISPATCH_PROPERTYPUT|PUTREF).
    if (target.find("vb6_ComCall") == 0) {
        auto parens = findCallParens(target, 0);
        if (parens.first != std::string::npos && parens.second != std::string::npos) {
            std::string callArgs = target.substr(parens.first + 1,
                                                 parens.second - parens.first - 1);
            std::string packFn = valueExpr ? comPackExpr(*valueExpr) : "vb6_ComPackVariant";
            std::string tag = isSet ? "Set" : "Let";
            c_.emitLine("vb6_ComSetPropArg(" + callArgs + ", " + packFn + "(" + value +
                        "));  /* COM Item assignment (Pattern A, " + tag + ") */");
            return true;
        }
    }

    // ---- Pattern F: vb6_ComGetStringProp(obj, L"Prop") = value ----
    // With-block ClassInstance member: 当 Property 在跨模块符号表里找不到时,
    // codegen 会走 "vb6_ComGetStringProp(_vb6_with_X, L\"Prop\")" 路径; 但读路径
    // 产生的 wchar_t* 不是左值, 赋值触发 C2106. 改走 vb6_ComSetProp.
    // 另外也匹配 vb6_ComGetIntProp / vb6_ComGetDoubleProp / vb6_ComGetObjectProp.
    const std::vector<std::string> comGetFns = {
        "vb6_ComGetStringProp", "vb6_ComGetIntProp", "vb6_ComGetDoubleProp",
        "vb6_ComGetObjectProp", "vb6_ComGetProp"
    };
    for (const auto& fnName : comGetFns) {
        if (target.find(fnName) == 0) {
            auto parens = findCallParens(target, 0);
            if (parens.first != std::string::npos && parens.second != std::string::npos) {
                std::string callArgs = target.substr(parens.first + 1,
                                                     parens.second - parens.first - 1);
                // isSet=true => 对象引用语义, 用 PackObject (DISPATCH_PROPERTYPUTREF)
                // isSet=false => Let 语义, 用 comPackExpr 推断
                std::string packFn = isSet ? "vb6_ComPackObject"
                                  : (valueExpr ? comPackExpr(*valueExpr) : "vb6_ComPackVariant");
                std::string tag = isSet ? "Set" : "Let";
                c_.emitLine("vb6_ComSetProp(" + callArgs + ", " + packFn + "(" + value +
                            "));  /* COM prop assignment (Pattern F, " + tag + ") */");
                return true;
            }
        }
    }

    // ---- Pattern C/D2: vb6_X_prop_get_Y(args) = value ----
    // Property Get 用作 LHS. 改写为 prop_let_Y(args, value) (Let) 或
    // prop_set_Y(args, value) (Set).
    // Fix 084i: 也匹配 prop_set_/prop_let_ 前缀 — 当属性只有 PropertySet
    // (无 Get) 时 emitExpr 直接生成 vb6_cX_prop_set_Y(obj) 单参数形式,
    // 追加 value 参数改写成完整调用, 避免 prop_set_(obj) = value 左值错误.
    {
        const char* verbs[] = {"prop_get_", "prop_set_", "prop_let_"};
        size_t pgPos = std::string::npos;
        std::string matchedVerb;
        for (const char* v : verbs) {
            size_t p = target.find(v);
            if (p != std::string::npos) { pgPos = p; matchedVerb = v; break; }
        }
        if (pgPos != std::string::npos) {
            auto parens = findCallParens(target, pgPos);
            if (parens.first != std::string::npos && parens.second != std::string::npos) {
                std::string prefix = target.substr(0, pgPos);              // vb6_cX_
                std::string afterPg = target.substr(pgPos + matchedVerb.size(),
                                                    parens.first - (pgPos + matchedVerb.size()));
                std::string argsStr = target.substr(parens.first + 1,
                                                    parens.second - parens.first - 1);
                std::string newVerbs = (matchedVerb != "prop_get_") ? matchedVerb
                                      : (isSet ? "prop_set_" : "prop_let_");
                // Fix 090w: Property Let 末参 As Variant (cJson.Item Dat As Variant
                // ByRef / cCsv.Value ByVal Dat As Variant) — 值实参 (vb6_Now() double
                // 666 int / BSTR) 需打包成 vb6_VARIANT, 否则 Pattern C/D2 裸拼 →
                // C2440 "double/int/BSTR → vb6_VARIANT(/ *)". 仅当改写目标是
                // prop_let_ (Let 语义) 且末形参解析为 Variant 时打包.
                std::string valArg = value;
                if (!isSet && newVerbs.find("prop_set_") == std::string::npos
                    && prefix.rfind("vb6_", 0) == 0 && !afterPg.empty()
                    && afterPg.find('(') == std::string::npos) {
                    std::string cls90w = prefix.substr(4);  // 去 "vb6_" → "cCsv_"
                    if (!cls90w.empty() && cls90w.back() == '_') cls90w.pop_back();
                    std::vector<ParameterInfo> letParams90w;
                    bool letBuiltin90w = false;
                    if (findClassMemberCallParams(cls90w, afterPg, letParams90w, letBuiltin90w)
                        && !letParams90w.empty()) {
                        const auto& lastP90w = letParams90w.back();
                        if (lastP90w.type == Vb6Type::Variant) {
                            if (lastP90w.isByVal) {
                                valArg = "vb6_VariantFromValue(" + value + ")";
                            } else {
                                valArg = "(&(vb6_VARIANT){vb6_VariantFromValue(" + value + ")})";
                            }
                        }
                    }
                }
                // Fix 090ad: 只写属性 (无 Get, 如 Dictionary.key(OldKey)=NewKey) 的 LHS
                // target — emitExpr 按 prop_let_/prop_set_ 完整调用生成时 (读上下文
                // fallback 到 Let/Set), 对缺失的 value 形参也 pad 了默认值
                // (vb6_BSTR_Empty()), 例如 prop_let_key(me->m_Dict, OldKey,
                // vb6_BSTR_Empty()). 若括号实参顶层段数 == 业务形参数+1 (对象+全部
                // 形参含 value), 末段即被 pad 的 value 位 → 丢弃, 由真实 RHS value
                // 拼接补齐, 否则 C2197 参数太多 (Dictionary.c 110/162).
                std::string finalArgs = argsStr;
                if (matchedVerb != "prop_get_" && !argsStr.empty()
                    && prefix.rfind("vb6_", 0) == 0 && !afterPg.empty()
                    && afterPg.find('(') == std::string::npos) {
                    std::string clsCd2 = prefix.substr(4);  // 去 "vb6_" → "cXx_"
                    if (!clsCd2.empty() && clsCd2.back() == '_') clsCd2.pop_back();
                    if (!clsCd2.empty()) {
                        std::vector<ParameterInfo> paramsCd2;
                        bool builtinCd2 = false;
                        if (findClassMemberCallParams(clsCd2, afterPg, paramsCd2, builtinCd2)
                            && !paramsCd2.empty()) {
                            std::vector<std::string> topArgs = splitTopLevelArgs(argsStr);
                            if (topArgs.size() == paramsCd2.size() + 1 && topArgs.size() > 1) {
                                topArgs.pop_back();  // 移除被 pad 的 value 默认值
                                finalArgs.clear();
                                for (size_t i = 0; i < topArgs.size(); i++) {
                                    if (i) finalArgs += ", ";
                                    finalArgs += topArgs[i];
                                }
                            }
                        }
                    }
                }
                std::string newCall;
                if (finalArgs.empty()) {
                    newCall = prefix + newVerbs + afterPg + "(" + valArg + ")";
                } else {
                    newCall = prefix + newVerbs + afterPg + "(" + finalArgs + ", " + valArg + ")";
                }
                std::string tag = isSet ? "Set" : "Let";
                c_.emitLine(newCall + ";  /* Property " + tag + " via prop_get_ rewrite (Pattern C/D2) */");
                return true;
            }
        }
    }

    return false;
}

// ============================================================
// Fix 011r-1: 类实例成员调用解析 (跨模块符号表的精确类查找)
// ============================================================
std::string CCodeGen::resolveClassMemberCall(const std::string& className,
                                               const std::string& memberName) const {
    if (className.empty()) return "";
    if (!symTab_.moduleScope()) return "";

    const std::string memberLower = Symbol::toLower(memberName);
    // Fix 011r-1b: VB6 case-insensitive — compare class name ignoring case
    // (用户源代码可能写 "cWinsock", 但struct定义用的是文件名大小写 "cWinSock")
    const std::string classNameLower = Symbol::toLower(className);
    // 跟踪规范类名 (取自符号表, 用于C输出大小写一致)
    std::string canonicalClassName;

    // 遍历模块级符号, 收集属于指定类且名字匹配的所有方法/属性符号
    // 优先 Get (读上下文最常见), 其次 Sub/Function, 再 Property Let, 最后 Set
    const Symbol* foundGet   = nullptr;
    const Symbol* foundLet   = nullptr;
    const Symbol* foundSet   = nullptr;
    const Symbol* foundSubFn = nullptr;

    for (const auto& [key, sym] : symTab_.moduleScope()->symbols()) {
        if (sym->lowerName != memberLower) continue;

        // 类匹配判定 (case-insensitive: VB6 case-insensitive)
        bool matches = false;
        if (sym->isExternal) {
            if (Symbol::toLower(sym->sourceModule) == classNameLower) {
                matches = true;
                if (canonicalClassName.empty()) canonicalClassName = sym->sourceModule;
            }
        } else if (isClassModule_ && Symbol::toLower(moduleName_) == classNameLower) {  // Fix 013: moduleName_ = VB_Name
            // 当类模块编译自身时, 同模块类的方法符号 isExternal=false
            matches = true;
            if (canonicalClassName.empty()) canonicalClassName = moduleName_;
        }
        if (!matches) continue;

        switch (sym->kind) {
            case SymbolKind::PropertyGet:  foundGet   = sym.get(); break;
            case SymbolKind::PropertyLet:  foundLet   = sym.get(); break;
            case SymbolKind::PropertySet:  foundSet   = sym.get(); break;
            case SymbolKind::Sub:
            case SymbolKind::Function:     foundSubFn = sym.get(); break;
            default: break;  // Variable / Constant / Class / EnumType... 跳过
        }
    }

    // Fix 089g: 跨模块同名成员 storageKey 抢占 — 多个类有同名 Property Get 时
    // (如 cWebSocketClient / cWebSocketServerClient 都有 State), driver.cpp 的
    // globalPublicSyms 按 storageKey (state$pg) 只保留先分析模块的符号. 目标类的
    // Get 外部符号可能被别类抢占而 Let ($pl) 正常注入 → 上面的循环只找到 Let,
    // 读上下文 (oClient.State = 1) 误发 prop_let_State → void 返回/C2198.
    // 修正: Get 缺失但读到 Let/Set 时, 查 Class 符号的 memberProcKinds — semantic
    // 按读上下文优先级 (Get > Function > Sub > Let > Set) 写入, 可确定该成员在读
    // 上下文的真实形式. 若读形式是 PropertyGet/Function/Sub, 按读形式重定向,
    // 避免生成 prop_let_/prop_set_ 调用.
    if (!foundGet) {
        const Symbol* classSymFix = nullptr;
        std::string classCanonFix;
        for (const auto& [ckey2, csym2] : symTab_.moduleScope()->symbols()) {
            if (csym2->kind != SymbolKind::Class) continue;
            if (csym2->isExternal) {
                if (Symbol::toLower(csym2->sourceModule) == classNameLower) {
                    classSymFix = csym2.get();
                    classCanonFix = csym2->sourceModule;
                    break;
                }
            } else if (isClassModule_ && Symbol::toLower(moduleName_) == classNameLower) {
                classSymFix = csym2.get();
                classCanonFix = moduleName_;
                break;
            }
        }
        if (classSymFix) {
            auto itKindFix = classSymFix->memberProcKinds.find(memberLower);
            if (itKindFix != classSymFix->memberProcKinds.end()) {
                std::string canonFix = classCanonFix.empty() ? canonicalClassName : classCanonFix;
                switch (itKindFix->second) {
                    case ProcKind::PropertyGet:
                        return "vb6_" + cIdent(canonFix) + "_prop_get_" + cIdent(memberName);
                    case ProcKind::Function:
                    case ProcKind::Sub:
                        return "vb6_" + cIdent(canonFix) + "_" + cIdent(memberName);
                    default:
                        break;  // PropertyLet/PropertySet 为主 — 维持 scope 选择
                }
            }
        }
    }

    // 选择优先级: Get > Sub/Function > Let > Set
    // (本helper用于读上下文; 写上下文由tryRewriteCOMLvalue把Get改写成Let/Set)
    const Symbol* chosen = foundGet;
    if (!chosen) chosen = foundSubFn;
    if (!chosen) chosen = foundLet;
    if (!chosen) chosen = foundSet;

    if (!chosen) {
        // Fix 014: 跨模块类成员符号缺失回退 (Class symbol fallback)
        // 当 cTlsSocket.Create 与其他类的 Public 方法同名时 (cPassword.Create / cAsyncSocket.Create),
        // driver.cpp 的 globalPublicSyms 以 storageKey (lowerName) 去重, 只保留首个注入,
        // 导致消费模块 (cTlsReMaster) 的作用域中外部 "create" 符号 sourceModule 不匹配 className.
        // scope iteration 找不到 sourceModule==className 的方法符号 → chosen 为 null.
        // 回退: 直接查找目标类的 Class 符号 (每个类的 Class 符号 storageKey=lowerName 唯一,
        // 不会被同名方法冲突覆盖), 验证 memberName 是否在该类 memberNames 中,
        // 命中则按 Sub/Function 风格发出 vb6_<className>_<memberName> (无前缀).
        // 限制: Property 变体需要 prop_get_/let_/set_ 前缀, 此回退假设 Sub/Function;
        //       若目标成员确实是 Property, 链接器会失败 — 届时再加 Property 分支.
        const Symbol* classSym = nullptr;
        std::string classCanonName;
        for (const auto& [ckey, csym] : symTab_.moduleScope()->symbols()) {
            if (csym->kind != SymbolKind::Class) continue;
            if (csym->isExternal) {
                if (Symbol::toLower(csym->sourceModule) == classNameLower) {
                    classSym = csym.get();
                    classCanonName = csym->sourceModule;
                    break;
                }
            } else if (isClassModule_ && Symbol::toLower(moduleName_) == classNameLower) {
                // 同模块 (本消费模块本身就是该类)
                classSym = csym.get();
                classCanonName = moduleName_;
                break;
            }
        }
        if (classSym) {
            for (const auto& mn : classSym->memberNames) {
                if (Symbol::toLower(mn) == memberLower) {
                    // Fix 016: 用 Class 符号的 memberProcKinds 直接判断前缀, 替代
                    // Fix 014b 的 "$pg 外部符号启发式" (后者在跨类同名混合场景下误判:
                    // 如 cCryptoHMAC.Mode 是 Function 而 cDelay.Mode 是 PropertyGet,
                    // 6 个类的 Mode 共同以 storageKey="mode" 去重, 消费模块作用域中
                    // "mode$pg" 外部符号被某个类率先占据, 启发式将所有类的 Mode 都
                    // 发出 prop_get_Mode — 对 Function 类生成不存在的函数名 → LNK2019).
                    // memberProcKinds 按 className 索引, 互不影响; 其值由
                    // semantic_analyzer.cpp 在创建 Class 符号时按读上下文优先级
                    // (Get > Function > Sub > Let > Set) 填充.
                    std::string prefix;
                    auto itKind = classSym->memberProcKinds.find(memberLower);
                    if (itKind != classSym->memberProcKinds.end()) {
                        switch (itKind->second) {
                            case ProcKind::PropertyGet: prefix = "prop_get_"; break;
                            case ProcKind::PropertyLet: prefix = "prop_let_"; break;
                            case ProcKind::PropertySet: prefix = "prop_set_"; break;
                            default: break;  // Function / Sub: 无前缀
                        }
                    }
                    // 若 memberProcKinds 缺失 (理论上不应发生, 旧 Class 符号兼容),
                    // 回退到原启发式: 作用域中存在 "<memberLower>$pg" 即判为 PropertyGet.
                    if (prefix.empty() && itKind == classSym->memberProcKinds.end()) {
                        std::string pgKey = memberLower + "$pg";
                        if (symTab_.moduleScope()->symbols().find(pgKey)
                            != symTab_.moduleScope()->symbols().end()) {
                            prefix = "prop_get_";
                        }
                    }
                    return "vb6_" + cIdent(classCanonName) + "_" + prefix + cIdent(memberName);
                }
            }
        }
        return "";  // 非方法/属性 → 视为数据字段访问
    }

    std::string prefix;
    switch (chosen->kind) {
        case SymbolKind::PropertyGet: prefix = "prop_get_"; break;
        case SymbolKind::PropertyLet: prefix = "prop_let_"; break;
        case SymbolKind::PropertySet: prefix = "prop_set_"; break;
        default: break;  // Sub/Function: 无前缀
    }

    // 强制使用规范类名 (来自符号表, 与类定义struct名一致),
    // 避免用户源代码大小写差异导致生成的函数名与定义不匹配
    return "vb6_" + cIdent(canonicalClassName) + "_" + prefix + cIdent(memberName);
}

// ============================================================
// Fix 033: 类感知方法/属性参数查找 (Phase A + Phase B 回退)
// ============================================================
// IndexOrCallExpr calleeParams 解析专用 — 与 resolveClassMemberCall 相同的
// 类匹配规则, 但返回 ParameterInfo 列表本身 (而非函数名).
// Phase A 失败的根本原因: driver.cpp globalPublicSyms 按 storageKey 去重,
// 跨模块同名方法 (如 N 个类的 Create) 在消费模块中仅保留首个注册者的 external
// 符号 → sourceModule 不匹配 className → Phase A 遍历不到目标类符号.
// 此时 Phase B 通过 Class 符号自身的 memberParams 表 (semantic_analyzer 在
// 类扫描阶段已填好, 按 Get > Function > Sub > Let > Set 优先级存参数) 取回.
bool CCodeGen::findClassMemberCallParams(const std::string& className,
                                         const std::string& memberName,
                                         std::vector<ParameterInfo>& outParams,
                                         bool& outIsBuiltin) const {
    outParams.clear();
    outIsBuiltin = false;
    if (className.empty() || !symTab_.moduleScope()) return false;

    const std::string memberLower = Symbol::toLower(memberName);
    const std::string classLower  = Symbol::toLower(className);

    // ---- Phase A: 与 resolveClassMemberCall 同迭代规则 ----
    // 找出模块作用域中所有与目标 className / memberName 匹配的 Sub/Function/Property
    // 符号, 按 Get > Function > Sub > Let > Set 优先级选择最优先者.
    // Note: 同模块类 (!isExternal) 由 isClassModule_ && moduleName_ 匹配; 跨模块类
    // 由 isExternal && sourceModule 匹配.
    const Symbol* foundGet   = nullptr;
    const Symbol* foundLet   = nullptr;
    const Symbol* foundSet   = nullptr;
    const Symbol* foundSubFn = nullptr;
    for (const auto& [key, sym] : symTab_.moduleScope()->symbols()) {
        if (sym->lowerName != memberLower) continue;

        bool matches = false;
        if (sym->isExternal) {
            if (Symbol::toLower(sym->sourceModule) == classLower) matches = true;
        } else if (isClassModule_ && Symbol::toLower(moduleName_) == classLower) {
            matches = true;
        }
        if (!matches) continue;

        switch (sym->kind) {
            case SymbolKind::PropertyGet:  foundGet   = sym.get(); break;
            case SymbolKind::PropertyLet:  foundLet   = sym.get(); break;
            case SymbolKind::PropertySet:  foundSet   = sym.get(); break;
            case SymbolKind::Sub:
            case SymbolKind::Function:     foundSubFn = sym.get(); break;
            default: break;  // Variable / Constant / Class / EnumType / DeclareSub/DeclareFunc 跳过
        }
    }

    const Symbol* chosen = foundGet;
    if (!chosen) chosen = foundSubFn;
    if (!chosen) chosen = foundLet;
    if (!chosen) chosen = foundSet;

    if (chosen
        && (chosen->kind == SymbolKind::Sub || chosen->kind == SymbolKind::Function
            || chosen->kind == SymbolKind::PropertyGet
            || chosen->kind == SymbolKind::PropertyLet
            || chosen->kind == SymbolKind::PropertySet)) {
        outParams = chosen->params;
        outIsBuiltin = chosen->isBuiltin;
        return true;
    }

    // ---- Phase B: 类符号自身 memberParams 表回退 ----
    // Note: 按 className 找到 Class 符号 (跨模块 external Class 符号已由 driver.cpp
    // 从 producing 模块拷贝 memberParams 表); 在该表上 lookup memberLower.
    // memberProcKinds 表 (优先级一致) 同步检查: 若有则进一步确认该成员在该类存在;
    // memberParams 表可能因 declaration 缺失 params 而存在性小于 memberProcKinds,
    // 保险起见以 memberParams 命中作为唯一成功判据.
    for (const auto& [ckey, csym] : symTab_.moduleScope()->symbols()) {
        if (csym->kind != SymbolKind::Class) continue;
        bool classMatches = false;
        if (csym->isExternal) {
            if (Symbol::toLower(csym->sourceModule) == classLower) classMatches = true;
        } else if (isClassModule_ && Symbol::toLower(moduleName_) == classLower) {
            classMatches = true;
        }
        if (!classMatches) continue;

        auto it = csym->memberParams.find(memberLower);
        if (it != csym->memberParams.end()) {
            outParams = it->second;
            outIsBuiltin = false;  // memberParams 仅记录用户类方法, 不是 RTL builtin
            return true;
        }
        // 同 className 的 Class 符号在消费模块中可能注册了多个外部副本, 但都来自同一
        // producing 模块的 Class 符号 memberParams, 命中任一即可. 未命中时继续遍历
        // 后续同名 Class 符号 (理论上不应出现, 留作防御性).
    }

    return false;
}

// ============================================================
// Fix 015: Method chaining 解析辅助
// ============================================================

std::string CCodeGen::canonicalClassName(const std::string& typeName) const {
    if (typeName.empty()) return "";
    if (!symTab_.moduleScope()) return typeName;
    std::string lower = Symbol::toLower(typeName);
    // 在模块作用域符号表中查找 Class 符号 (含本模块与 extern 注入),
    // 返回符号记录的规范名 (clsSym->name 或 sourceModule) —
    // 这与 struct 定义 vb6_cls_<name> 中使用的大小写一致.
    for (const auto& [key, sym] : symTab_.moduleScope()->symbols()) {
        if (sym->kind != SymbolKind::Class) continue;
        if (sym->isExternal) {
            if (Symbol::toLower(sym->sourceModule) == lower) return sym->sourceModule;
        }
        if (Symbol::toLower(sym->name) == lower) return sym->name;
    }
    return typeName;  // 未找到 Class 符号 — 用源码大小写返回 (caller 自行承担)
}

std::string CCodeGen::getClassMethodReturnType(const std::string& className,
                                               const std::string& memberName) const {
    if (!symTab_.moduleScope()) return "";
    const std::string memberLower = Symbol::toLower(memberName);
    const std::string classLower  = Symbol::toLower(className);

    // 遍历模块级符号查找属于 className 的 Function/PropertyGet
    // (与 resolveClassMemberCall 相同的匹配规则)
    for (const auto& [key, sym] : symTab_.moduleScope()->symbols()) {
        if (sym->lowerName != memberLower) continue;
        bool matches = false;
        if (sym->isExternal) {
            if (Symbol::toLower(sym->sourceModule) == classLower) matches = true;
        } else if (isClassModule_ && Symbol::toLower(moduleName_) == classLower) {
            matches = true;
        }
        if (!matches) continue;

        if (sym->kind == SymbolKind::Function || sym->kind == SymbolKind::PropertyGet) {
            // 仅 Function/PropertyGet 有返回值
            if (sym->type == Vb6Type::Object && !sym->variableTypeName.empty()) {
                // Fix 015 semantic_analyzer 已在该 Function 的 variableTypeName 记录返回类名
                return canonicalClassName(sym->variableTypeName);
            }
            // Fix 088e: 符号匹配但注入的 type 记录不完整 (PropertyGet 返回类实例
            // 如 RecvBuffer As cByteBuffer, 注入符号 type 未标 Object/变量类型名空)
            // 时, 不再提前 return "" — 落入 Phase B 用 Class 符号的 memberReturnTypes
            // (语义层原始记录, 不受注入影响) 兜底. 否则 Client.RecvBuffer.Size 等
            // 链推断断 → 生成 prop_get(...).Size (C2039: Size 不是 vb6_cls_cByteBuffer
            // 的成员).
            continue;
        }
        // Property Let/Set / Sub 无返回值, 跳过
    }

    // Fix 015: storageKey 冲突回退. 当多名 Public 方法在不同类中重名 (如 cCryptoHMAC.DataString
    // 与 cCryptoHash.DataString, cCryptoHMAC.Mode/cCryptoHash.Mode/cDelay.Mode/...), driver.cpp 的
    // globalPublicSyms 按 storageKey(lowerName) 去重, 消费模块作用域中该名字的外部符号
    // 只保留首个注册者的 sourceModule — 与 className 不匹配 → 上面的循环找不到 → 返回 "".
    // 类似 resolveClassMemberCall 的 Fix 014b 回退, 这里从 Class 符号自身的 memberReturnTypes
    // 表(以 className 索引, 不受 storageKey 冲突影响)读取成员返回类型名. 再校验该返回类型
    // 在当前作用域确实存在对应 Class 符号 (排除 UDT/Enum/String 等非类命名类型 — 链应终止).
    const Symbol* classSym = nullptr;
    for (const auto& [ckey, csym] : symTab_.moduleScope()->symbols()) {
        if (csym->kind != SymbolKind::Class) continue;
        if (csym->isExternal) {
            if (Symbol::toLower(csym->sourceModule) == classLower) {
                classSym = csym.get();
                break;
            }
        } else if (isClassModule_ && Symbol::toLower(moduleName_) == classLower) {
            // 类模块编译自身: 同模块的 Class 符号 isExternal=false
            classSym = csym.get();
            break;
        }
    }
    if (classSym) {
        auto it = classSym->memberReturnTypes.find(memberLower);
        if (it != classSym->memberReturnTypes.end()) {
            const std::string& rawRetName = it->second;
            const std::string retLower = Symbol::toLower(rawRetName);
            // 校验 rawRetName 对应当前作用域中真实存在的 Class 符号 (canonical 同时取大小写)
            for (const auto& [k2, s2] : symTab_.moduleScope()->symbols()) {
                if (s2->kind != SymbolKind::Class) continue;
                if (s2->isExternal) {
                    if (Symbol::toLower(s2->sourceModule) == retLower) {
                        return s2->sourceModule;
                    }
                } else if (Symbol::toLower(s2->name) == retLower) {
                    return s2->name;
                }
            }
            // 返回类型不是类(可能是 UDT/Enum/String/接口等) → 链终止, 不应当继续链式调用
            return "";
        }
    }
    return "";
}

std::string CCodeGen::inferClassTypeOfExpr(const ASTNode& expr) const {
    switch (expr.kind) {
        case ASTNodeKind::IdentifierExpr: {
            // base case: 变量 → knownClassVars_
            auto& id = static_cast<const IdentifierExpr&>(expr);
            std::string lower = Symbol::toLower(id.name);
            auto it = knownClassVars_.find(lower);
            if (it != knownClassVars_.end()) return it->second;
            // Fix 084g: 局部变量/参数声明为 As ClassName (如 Dim Response As cHttpServerResponse)
            // 不在 knownClassVars_ (跨模块类变量表) 中, 从符号表 variableTypeName 推断类名
            const Symbol* sym = symTab_.lookup(id.name);
            if (sym && (sym->kind == SymbolKind::Variable || sym->kind == SymbolKind::Parameter)
                && !sym->variableTypeName.empty()) {
                const Symbol* clsSym = symTab_.lookup(sym->variableTypeName);
                if (clsSym && clsSym->kind == SymbolKind::Class) {
                    return sym->variableTypeName;
                }
            }
            // Fix 084z-2: 当前模块的属性 (Property Get) 返回类 — 属性符号在模块
            // 作用域符号表中 (如 cTlsRemaster.pvSocket() As cTlsSocket). 无参属性
            // 可推断类实例, 用于 pvSocket.SyncReceiveArray(...) 的方法/形参签名解析
            // (否则 Fix 033 把属性名当模块名, 回退 lookupModule 命中 storageKey
            // 同名冲突的错误类 cWinsock.SyncReceiveArray → C2197 参数过多).
            if (sym && sym->kind == SymbolKind::PropertyGet && !sym->variableTypeName.empty()) {
                const Symbol* clsSym = symTab_.lookup(sym->variableTypeName);
                if (clsSym && clsSym->kind == SymbolKind::Class) {
                    return sym->variableTypeName;
                }
            }
            return "";
        }
        case ASTNodeKind::IndexOrCallExpr: {
            // recursive case: 类方法调用 obj.Method(args) → 返回类
            auto& call = static_cast<const IndexOrCallExpr&>(expr);
            if (!call.callee) return "";
            if (call.callee->kind == ASTNodeKind::IdentifierExpr) {
                // Fix 085c: 模块内裸函数调用返回类实例 (如 cAsyncSocket 内
                // pvToSocket(idx) As cAsyncSocket), 后续 .frNotifyGetHostByName(...)
                // 链式调用需要知道返回类以拆成 vb6_cAsyncSocket_frNotify...(this,...).
                // 此前仅支持 MemberAccessExpr/WithMemberExpr callee, 裸函数推断断链 →
                // 生成 (ret).Method(...) 非法字段访问 (C2039: 不是 vb6_cls_X 的成员).
                auto& id = static_cast<const IdentifierExpr&>(*call.callee);
                const Symbol* fn = symTab_.lookupModule(id.name);
                if (fn && fn->kind == SymbolKind::Function
                    && !fn->variableTypeName.empty()) {
                    const Symbol* clsSym = symTab_.lookupModule(fn->variableTypeName);
                    if (clsSym && clsSym->kind == SymbolKind::Class) return fn->variableTypeName;
                }
                return "";
            }
            if (call.callee->kind != ASTNodeKind::MemberAccessExpr
                && call.callee->kind != ASTNodeKind::WithMemberExpr) return "";
            // Fix 085b: With 块内方法链 .Data(...).CalculateCRC16(...) — callee 是
            // WithMemberExpr, 基类是 With 栈顶对象类, 不能按 MemberAccessExpr 解析
            // (否则 With 链方法在推断中断链, 生成 (ret).Method(...) 非法字段调用).
            if (call.callee->kind == ASTNodeKind::WithMemberExpr) {
                auto& wm = static_cast<const WithMemberExpr&>(*call.callee);
                if (withObjectInfoStack_.empty()) return "";
                const auto& winfo = withObjectInfoStack_.back();
                if (winfo.kind != WithObjKind::ClassInstance || winfo.className.empty()) return "";
                return getClassMethodReturnType(winfo.className, wm.memberName);
            }
            auto& ma = static_cast<const MemberAccessExpr&>(*call.callee);
            if (!ma.object) return "";
            // 递归推断对象表达式的类名
            std::string baseClassName = inferClassTypeOfExpr(*ma.object);
            if (baseClassName.empty()) return "";
            // 查找该方法的返回类型
            return getClassMethodReturnType(baseClassName, ma.memberName);
        }
        // Fix 037: MeExpr → 类模块内 me 即当前类
        case ASTNodeKind::MeExpr: {
            if (isClassModule_) return moduleName_;
            return "";
        }
        // Fix 037b: MemberAccessExpr → 递归推断 object 的类类型, 再从
        // classTypedFieldMap_ 查找字段的类类型 (仅项目类字段, 非 COM).
        // 用于链式访问 ctx.Request.QueryString(idx) 中 Request 的类型推断:
        //   ctx (cHttpServerContext) → Request (cHttpServerRequest) → QueryString (COM:Dictionary)
        case ASTNodeKind::MemberAccessExpr: {
            auto& ma = static_cast<const MemberAccessExpr&>(expr);
            if (!ma.object) return "";
            std::string baseClassName = inferClassTypeOfExpr(*ma.object);
            if (baseClassName.empty()) return "";
            if (classTypedFieldMap_) {
                auto it = classTypedFieldMap_->find(baseClassName);
                if (it != classTypedFieldMap_->end()) {
                    std::string memLower = Symbol::toLower(ma.memberName);
                    auto itF = it->second.find(memLower);
                    if (itF != it->second.end()) {
                        // 仅返回项目类字段类型 (COM: 前缀的不是项目类)
                        if (itF->second.compare(0, 4, "COM:") == 0) return "";
                        return itF->second;
                    }
                }
            }
            // Fix 084z: 属性 Get 回退 — 成员是属性(返回类实例)而非数据字段时
            // (如 pvSocket 是 cTlsReMaster 的 Property Get, 不在字段表中),
            // 用 getClassMethodReturnType 推断返回类. 否则调用方类型推断失败
            // → fallback 到错误类解析参数, 生成 C2197/C2198 (参数过多/过少:
            // SyncReceiveArray 声明6参却按 cWinsock 的12参展开, Connect 声明
            // 11参却按 cWinsock 的5参展开).
            return getClassMethodReturnType(baseClassName, ma.memberName);
        }
        // Fix 037: WithMemberExpr → 当前 With 块 tempVar 的类类型 (仅 ClassInstance kind)
        case ASTNodeKind::WithMemberExpr: {
            if (withObjectInfoStack_.empty() || withObjectVars_.empty()) return "";
            const auto& info = withObjectInfoStack_.back();
            if (info.kind == WithObjKind::ClassInstance && !info.className.empty()) {
                // Fix 090s: .X 若是 With 目标类的 typed 项目类字段 (如
                // With HttpSvr: .Router.Reg → .Router 字段 As cHttpServerRouter),
                // 返回字段的类, 供外层 .Reg/.Encode 等成员/方法按字段类解析
                // (findClassMemberCallParams/resolveClassMemberCall 用对类, 否则
                // 形参表空 → .Router.Reg "Test" 实参全丢 C2198). 与 MemberAccessExpr
                // 分支 (classTypedFieldMap_ 查询) 对齐.
                auto& wmRef = static_cast<const WithMemberExpr&>(expr);
                if (classTypedFieldMap_) {
                    auto it = classTypedFieldMap_->find(info.className);
                    if (it != classTypedFieldMap_->end()) {
                        std::string memLower = Symbol::toLower(wmRef.memberName);
                        auto itF = it->second.find(memLower);
                        if (itF != it->second.end()) {
                            // COM:/void* 字段 (COM: 前缀) 不是项目类 → 返回空让外层
                            // 走 COM dispatch; 项目类字段返回类名
                            if (itF->second.compare(0, 4, "COM:") == 0) return "";
                            return itF->second;
                        }
                    }
                }
                // .X 非数据字段 (方法/属性等) → 维持原行为: With 目标类自身
                // (Fix 085b 在调用链推断处对 callee=WithMemberExpr 已按方法返回类
                // 特判, 此处不做方法返回类型推断以免误伤 String/Long 属性场景)
                return info.className;
            }
            return "";
        }
        default:
            return "";
    }
}

// Fix 037: 递归推断表达式的 UDT C 类型标识符 (如 "vb6_type_UcsBuffer").
// 支持 IdentifierExpr (knownUdtVars_ 直查) 和 MemberAccessExpr (嵌套 UDT 字段递归).
// 返回空串表示非 UDT 表达式.
std::string CCodeGen::inferUdtTypeOfExpr(const ASTNode& expr) const {
    switch (expr.kind) {
        case ASTNodeKind::IdentifierExpr: {
            auto& id = static_cast<const IdentifierExpr&>(expr);
            std::string lower = Symbol::toLower(id.name);
            auto it = knownUdtVars_.find(lower);
            if (it != knownUdtVars_.end()) return it->second;
            // Fix 090j: 函数体内函数名标识符 = 本函数返回对象 (VB6: Function
            // pvVfsOpen As ZipVfsType 内写 pvVfsOpen.BufferArray, 即返回 UDT 的
            // 字段). 与发射层 Fix 084z-4/088d (函数名→类返回对象) 及注册层
            // Fix 090i (vb6_ret_X → knownUdtVars_) 对称: 推断层必须把 "函数名"
            // 映射到返回 UDT 类型, 否则字段类型推断落空 → 字段整体按 Unknown:
            // Variant 字段被当函数 (SourceFileInfo(3) → C2064)、LongPtr 字段被
            // Variant 化 (BufferPtr = BufferBase → VariantToLong → C2440),
            // As Any 实参把 Variant 字段强转指针 (C2440) — cZipArchive VFS 簇.
            if (currentProc_ && !currentReturnCType_.empty()
                && currentReturnCType_.rfind("vb6_type_", 0) == 0
                && lower == Symbol::toLower(currentProc_->name)) {
                return currentReturnCType_;
            }
            return "";
        }
        // Fix 081i: IndexOrCallExpr — UDT数组元素访问 arr(idx).field
        // 查 arrayUdtElemTypes_ 获取数组元素UDT类型
        case ASTNodeKind::IndexOrCallExpr: {
            auto& call = static_cast<const IndexOrCallExpr&>(expr);
            if (call.callee && call.callee->kind == ASTNodeKind::IdentifierExpr) {
                auto& id = static_cast<const IdentifierExpr&>(*call.callee);
                std::string lower = Symbol::toLower(id.name);
                auto it = arrayUdtElemTypes_.find(lower);
                if (it != arrayUdtElemTypes_.end()) return it->second;
            }
            return "";
        }
        case ASTNodeKind::MemberAccessExpr: {
            auto& ma = static_cast<const MemberAccessExpr&>(expr);
            if (!ma.object) return "";
            // 递归推断父对象的 UDT 类型
            std::string parentUdtCType = inferUdtTypeOfExpr(*ma.object);
            if (parentUdtCType.empty()) return "";
            // parentUdtCType 形如 "vb6_type_UcsBuffer", 剥前缀得到 UDT 名
            const std::string prefix = "vb6_type_";
            if (parentUdtCType.size() <= prefix.size()
                || parentUdtCType.compare(0, prefix.size(), prefix) != 0) return "";
            std::string udtName = parentUdtCType.substr(prefix.size());
            Symbol* udtSym = symTab_.lookupModule(udtName);
            if (!udtSym || udtSym->kind != SymbolKind::UserDefinedType) return "";
            std::string memLower = Symbol::toLower(ma.memberName);
            for (auto& mi : udtSym->udtMembers) {
                if (Symbol::toLower(mi.name) == memLower) {
                    // 若该成员本身是 UDT (typeRefName 非空且能查到 UserDefinedType 符号)
                    if (!mi.typeRefName.empty()) {
                        Symbol* refSym = symTab_.lookupModule(mi.typeRefName);
                        if (refSym && refSym->kind == SymbolKind::UserDefinedType) {
                            return "vb6_type_" + cIdent(mi.typeRefName);
                        }
                    }
                    return "";  // 成员是标量/数组, 不是嵌套 UDT
                }
            }
            return "";
        }
        // Fix 037: WithMemberExpr → 当前 With 块 tempVar 的 UDT 类型递归.
        // With 块临时变量 (_vb6_with_N) 已被 cgen_stmt.cpp 注册到 knownUdtVars_
        // (仅 WithObjKind::Unknown — UDT — 才注册). 若 tempVar 不是 UDT (ClassInstance/
        // COMObject 等其他 kind), 此处返回空串.
        // 递归: .member 即 With 块 UDT 的某字段; 若该字段本身是嵌套 UDT (typeRefName
        // 在符号表中查到 UserDefinedType), 返回 "vb6_type_<memberUdtName>".
        // 例: With uCtx (UcsTlsContext) 内的 .DecrBuffer (UcsBuffer) → 返回
        // "vb6_type_UcsBuffer"; 让外层 .DecrBuffer.Data(0) 的 IndexOrCallExpr 能
        // 在 UcsBuffer 的 udtMembers 中找到 Data (动态数组成员) 并生成 VB6_SA_AT.
        case ASTNodeKind::WithMemberExpr: {
            if (withObjectInfoStack_.empty() || withObjectVars_.empty()) return "";
            const auto& info = withObjectInfoStack_.back();
            if (info.kind != WithObjKind::Unknown) return "";  // 仅 UDT
            const std::string& tempVar = withObjectVars_.back();
            std::string tempLower = Symbol::toLower(tempVar);
            auto it = knownUdtVars_.find(tempLower);
            if (it == knownUdtVars_.end()) return "";
            const std::string parentUdtCType = it->second;
            const std::string prefix = "vb6_type_";
            if (parentUdtCType.size() <= prefix.size()
                || parentUdtCType.compare(0, prefix.size(), prefix) != 0) return "";
            std::string udtName = parentUdtCType.substr(prefix.size());
            Symbol* udtSym = symTab_.lookupModule(udtName);
            if (!udtSym || udtSym->kind != SymbolKind::UserDefinedType) return "";
            auto& wm = static_cast<const WithMemberExpr&>(expr);
            std::string memLower = Symbol::toLower(wm.memberName);
            for (auto& mi : udtSym->udtMembers) {
                if (Symbol::toLower(mi.name) == memLower) {
                    if (!mi.typeRefName.empty()) {
                        Symbol* refSym = symTab_.lookupModule(mi.typeRefName);
                        if (refSym && refSym->kind == SymbolKind::UserDefinedType) {
                            return "vb6_type_" + cIdent(mi.typeRefName);
                        }
                    }
                    return "";
                }
            }
            return "";
        }
        default:
            return "";
    }
}

// Fix 084n: 推断 target 是否为 UDT 字段链, 是则返回字段 Vb6Type (含 Array 标志), 否则 Unknown.
// 供赋值语句 (cgen_stmt) 将 Variant RHS 转换为目标字段类型.
Vb6Type CCodeGen::inferUdtFieldVb6Type(const ASTNode* target) const {
    if (!target) return Vb6Type::Unknown;
    std::string memName;
    if (target->kind == ASTNodeKind::MemberAccessExpr) {
        auto& ma = static_cast<const MemberAccessExpr&>(*target);
        if (inferUdtTypeOfExpr(*ma.object).empty()) return Vb6Type::Unknown;
        memName = ma.memberName;
    } else if (target->kind == ASTNodeKind::WithMemberExpr) {
        if (withObjectInfoStack_.empty() || withObjectVars_.empty()) return Vb6Type::Unknown;
        const auto& info = withObjectInfoStack_.back();
        if (info.kind != WithObjKind::Unknown) return Vb6Type::Unknown;  // 仅 UDT
        auto& wm = static_cast<const WithMemberExpr&>(*target);
        memName = wm.memberName;
    } else {
        return Vb6Type::Unknown;
    }
    if (memName.empty()) return Vb6Type::Unknown;

    // 解析对象 UDT C 类型
    std::string udtCType;
    if (target->kind == ASTNodeKind::MemberAccessExpr) {
        auto& ma = static_cast<const MemberAccessExpr&>(*target);
        udtCType = inferUdtTypeOfExpr(*ma.object);
    } else {
        const std::string& tempVar = withObjectVars_.back();
        auto it = knownUdtVars_.find(Symbol::toLower(tempVar));
        if (it == knownUdtVars_.end()) return Vb6Type::Unknown;
        udtCType = it->second;
    }
    const std::string prefix = "vb6_type_";
    if (udtCType.size() <= prefix.size() || udtCType.compare(0, prefix.size(), prefix) != 0)
        return Vb6Type::Unknown;
    std::string udtName = udtCType.substr(prefix.size());
    Symbol* udtSym = symTab_.lookupModule(udtName);
    if (!udtSym || udtSym->kind != SymbolKind::UserDefinedType) return Vb6Type::Unknown;
    std::string memLower = Symbol::toLower(memName);
    for (auto& mi : udtSym->udtMembers) {
        if (Symbol::toLower(mi.name) == memLower) return mi.type;
    }
    return Vb6Type::Unknown;
}

// ============================================================
// Fix 085: UDT 对象字段类型推断
// ============================================================
std::string CCodeGen::udtFieldObjCType(const std::string& udtCType,
                                       const std::string& memberLower) const {
    const std::string prefix = "vb6_type_";
    if (udtCType.size() <= prefix.size() || udtCType.compare(0, prefix.size(), prefix) != 0)
        return "";
    std::string udtName = udtCType.substr(prefix.size());
    Symbol* udtSym = symTab_.lookupModule(udtName);
    if (!udtSym || udtSym->kind != SymbolKind::UserDefinedType) return "";
    std::string memLower = Symbol::toLower(memberLower);
    for (const auto& mi : udtSym->udtMembers) {
        if (Symbol::toLower(mi.name) != memLower) continue;
        if (mi.type == Vb6Type::UserDefinedType) {
            // 嵌套 UDT 字段 (非对象) — 返回其 UDT C 类型供链式推断
            return mi.typeRefName.empty() ? "" : "vb6_type_" + cIdent(mi.typeRefName);
        }
        if (mi.type == Vb6Type::Object) {
            if (!mi.typeRefName.empty()) {
                // VBA. 前缀剥离 (如 VBA.Collection → Collection)
                std::string tn = mi.typeRefName;
                if (tn.size() > 4 && tn.compare(0, 4, "VBA.") == 0) tn = tn.substr(4);
                Symbol* refSym = symTab_.lookupModule(tn);
                if (refSym && refSym->kind == SymbolKind::Class) {
                    // 项目类对象字段 → 类方法/属性调度 (early bound).
                    // 注意: C 层类类型名须用类的规范模块名 (sourceModule), 而非 UDT
                    // 字段中的引用名 (如 tZipFileItem.SourceArchive As "ZipArchive" 实际
                    // 对应 vb6_cls_cZipArchive* — 引用名可能与模块名不同, 用了引用名
                    // 会让 resolveClassMemberCall 查不到成员 (类符号按模块名登记)).
                    // sourceModule 仅在跨模块注入时填写; 类自身符号(当前类模块
                    // 编译中)为空, 此时规范名即当前模块名 moduleName_.
                    std::string clsCanon = !refSym->sourceModule.empty()
                                               ? refSym->sourceModule
                                               : moduleName_;
                    return "vb6_cls_" + cIdent(clsCanon) + "*";
                }
            }
            // Collection/COM/接口 等对象字段 → COM dispatch
            return "void*";
        }
        // 标量/字符串/数组等非对象字段
        return "";
    }
    return "";
}

std::string CCodeGen::appendUdtObjFieldMarker(const std::string& objExpr,
                                              const std::string& udtCType,
                                              const std::string& member,
                                              const std::string& accessOp) const {
    std::string fieldCType = udtFieldObjCType(udtCType, Symbol::toLower(member));
    std::string fieldAccess = objExpr + accessOp + cIdent(member);
    // 仅对象字段 (项目类 vb6_cls_* / Collection·COM void*) 才追加标记;
    // 嵌套 UDT (vb6_type_*) 与标量/字符串等原样返回 — 嵌套 UDT 继续由
    // inferUdtTypeOfExpr / 普通字段拼接处理, 标记残留会干扰函数参数等上下文.
    if (fieldCType != "void*" && fieldCType.rfind("vb6_cls_", 0) != 0) return fieldAccess;
    return fieldAccess + "  /* udt objfield " + fieldCType + " */";
}


// Fix 084o: 需要 int32_t 上下文中的 Variant 表达式 → vb6_VariantToLong 包装
std::string CCodeGen::toLongIfVariant(const std::string& cExpr, const Expr* astExpr) {
    if (cExprIsVariant(cExpr)) return "vb6_VariantToLong(" + cExpr + ")";
    if (astExpr && astExpr->kind == ASTNodeKind::IdentifierExpr) {
        auto& ident = static_cast<IdentifierExpr&>(const_cast<Expr&>(*astExpr));
        std::string lower = ident.name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (knownVariantVars_.count(lower)) return "vb6_VariantToLong(" + cExpr + ")";
    }
    return cExpr;
}

// ============================================================
// P7.5: 控件属性 → RTL读取函数名映射
// ============================================================

std::string CCodeGen::getControlPropReadFn(FrmControlType ctrlType, const std::string& propName) const {
    std::string propLower = propName;
    std::transform(propLower.begin(), propLower.end(), propLower.begin(), ::tolower);

        // P11.8: Common properties for all visible controls (checked before switch)
    if (propLower == "left") return "vb6_GetControlLeft";
    if (propLower == "top") return "vb6_GetControlTop";
    if (propLower == "width") return "vb6_GetControlWidth";
    if (propLower == "height") return "vb6_GetControlHeight";
    if (propLower == "hwnd") return "vb6_GetControlHwnd";
    // P13.1: Font properties (all visible controls with text)
    if (propLower == "fontname") return "vb6_GetControlFontName";
    if (propLower == "fontsize") return "vb6_GetControlFontSize";
    if (propLower == "fontbold") return "vb6_GetControlFontBold";
    if (propLower == "fontitalic") return "vb6_GetControlFontItalic";
    if (propLower == "fontunderline") return "vb6_GetControlFontUnderline";
    if (propLower == "fontstrikethrough") return "vb6_GetControlFontStrikethrough";
    // P13.2: Color properties (all visible controls)
    if (propLower == "forecolor") return "vb6_GetControlForeColor";
    if (propLower == "backcolor") return "vb6_GetControlBackColor";
    // P13.5: Alignment (all text controls)
    if (propLower == "alignment") return "vb6_GetAlignment";
    // P13.6: TabIndex/TabStop (all visible controls)
    if (propLower == "tabindex") return "vb6_GetTabIndex";
    if (propLower == "tabstop") return "vb6_GetTabStop";
    if (propLower == "causesvalidation") return "vb6_GetCausesValidation";
    // P13.8: ToolTipText (all visible controls)
    if (propLower == "tooltiptext") return "vb6_GetToolTipText";
    // P13.9: Tag (all controls)
    if (propLower == "tag") return "vb6_GetControlTag";
    // P13.7: MousePointer/MouseIcon (all visible controls)
    if (propLower == "mousepointer") return "vb6_GetMousePointer";
    if (propLower == "mouseicon") return "vb6_GetMouseIcon";
    // P13.10: BorderStyle (all visible controls)
    if (propLower == "borderstyle") return "vb6_GetBorderStyle";

    switch (ctrlType) {
    case FrmControlType::TextBox:
        if (propLower == "text") return "vb6_GetControlText";
        if (propLower == "multiline") return "vb6_GetMultiLine";
        if (propLower == "scrollbars") return "vb6_GetScrollBars";
        if (propLower == "maxlength") return "vb6_GetMaxLength";
        if (propLower == "passwordchar") return "vb6_GetPasswordChar";
        if (propLower == "locked") return "vb6_GetLocked";
        if (propLower == "selstart") return "vb6_GetSelStart";
        if (propLower == "sellength") return "vb6_GetSelLength";
        if (propLower == "seltext") return "vb6_GetSelText";
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        break;
    case FrmControlType::ListBox:
    case FrmControlType::ComboBox:
        if (propLower == "text") return "vb6_GetControlText";
        if (propLower == "listcount") return "vb6_GetListCount";
        if (propLower == "listindex") return "vb6_GetListIndex";
        if (propLower == "list") return "vb6_GetListItem";
        if (propLower == "selected") return "vb6_GetSelected";
        if (propLower == "itemdata") return "vb6_GetItemData";
        if (propLower == "newindex") return "vb6_GetNewIndex";
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        break;
    case FrmControlType::Frame:
        if (propLower == "caption") return "vb6_GetControlText";
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        break;
    case FrmControlType::Label:
        if (propLower == "caption") return "vb6_GetControlText";
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        break;
    case FrmControlType::CommandButton:
        if (propLower == "caption") return "vb6_GetControlText";
        if (propLower == "default") return "vb6_GetDefaultButton";
        if (propLower == "cancel") return "vb6_GetCancelButton";
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        break;
    case FrmControlType::CheckBox:
    case FrmControlType::OptionButton:
        if (propLower == "value") return "vb6_GetCheckValue";
        if (propLower == "caption") return "vb6_GetControlText";
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        break;
    case FrmControlType::Form:
        if (propLower == "caption") return "vb6_GetControlText";
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        // Fix 056: Form-specific properties
        if (propLower == "windowstate") return "vb6_GetWindowState";
        if (propLower == "scalewidth") return "vb6_GetScaleWidth";
        if (propLower == "scaleheight") return "vb6_GetScaleHeight";
        break;
    case FrmControlType::WebBrowser:
        if (propLower == "url" || propLower == "locationurl") return "vb6_WebViewGetUrl";
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        break;
    case FrmControlType::HScrollBar:
    case FrmControlType::VScrollBar:
        if (propLower == "value") return "vb6_GetScrollValue";
        if (propLower == "min") return "vb6_GetScrollMin";
        if (propLower == "max") return "vb6_GetScrollMax";
        if (propLower == "largechange") return "vb6_GetLargeChange";
        if (propLower == "smallchange") return "vb6_GetSmallChange";
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        break;
    case FrmControlType::Timer:
        if (propLower == "interval") return "vb6_GetTimerInterval";
        if (propLower == "enabled") return "vb6_GetTimerEnabled";
        break;
    case FrmControlType::PictureBox:
        if (propLower == "caption") return "vb6_GetControlText";
        if (propLower == "picture") return "vb6_GetControlPicture";
        if (propLower == "autosize") return "vb6_GetPictureAutoSize";
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        break;
    case FrmControlType::Image:
        if (propLower == "picture") return "vb6_GetControlPicture";
        if (propLower == "stretch") return "vb6_GetImageStretch";  // P17.2
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        break;
    case FrmControlType::Menu:  // P20-36
        if (propLower == "caption") return "vb6_GetMenuCaption";
        if (propLower == "checked") return "vb6_GetMenuChecked";
        if (propLower == "enabled") return "vb6_GetMenuEnabled";
        if (propLower == "visible") return "vb6_GetMenuVisible";
        break;
    case FrmControlType::Shape:  // P20-35
        if (propLower == "shape") return "vb6_GetShapeType";
        if (propLower == "borderwidth") return "vb6_GetShapeBorderWidth";
        if (propLower == "borderstyle") return "vb6_GetShapeBorderStyle";
        if (propLower == "fillstyle") return "vb6_GetShapeFillStyle";
        if (propLower == "bordercolor") return "vb6_GetShapeBorderColor";
        if (propLower == "fillcolor") return "vb6_GetShapeFillColor";
        if (propLower == "visible") return "vb6_GetControlVisible";
        break;
    case FrmControlType::Line:  // P20-35
        if (propLower == "x1") return "vb6_GetLineX1";
        if (propLower == "y1") return "vb6_GetLineY1";
        if (propLower == "x2") return "vb6_GetLineX2";
        if (propLower == "y2") return "vb6_GetLineY2";
        if (propLower == "borderwidth") return "vb6_GetLineBorderWidth";
        if (propLower == "borderstyle") return "vb6_GetLineBorderStyle";
        if (propLower == "bordercolor") return "vb6_GetLineColor";
        if (propLower == "visible") return "vb6_GetControlVisible";
        break;
    default:
        // 所有可见控件通用属性
        if (propLower == "visible") return "vb6_GetControlVisible";
        if (propLower == "enabled") return "vb6_GetControlEnabled";
        break;
    }
    return "";  // 未知属性
}

std::string CCodeGen::getControlPropWriteFn(FrmControlType ctrlType, const std::string& propName) const {
    std::string propLower = propName;
    std::transform(propLower.begin(), propLower.end(), propLower.begin(), ::tolower);

        // P11.8: Common properties for all visible controls (checked before switch)
    if (propLower == "left") return "vb6_SetControlLeft";
    if (propLower == "top") return "vb6_SetControlTop";
    if (propLower == "width") return "vb6_SetControlWidth";
    if (propLower == "height") return "vb6_SetControlHeight";
    // P13.1: Font properties (all visible controls with text)
    if (propLower == "fontname") return "vb6_SetControlFontName";
    if (propLower == "fontsize") return "vb6_SetControlFontSize";
    if (propLower == "fontbold") return "vb6_SetControlFontBold";
    if (propLower == "fontitalic") return "vb6_SetControlFontItalic";
    if (propLower == "fontunderline") return "vb6_SetControlFontUnderline";
    if (propLower == "fontstrikethrough") return "vb6_SetControlFontStrikethrough";
    // P13.2: Color properties (all visible controls)
    if (propLower == "forecolor") return "vb6_SetControlForeColor";
    if (propLower == "backcolor") return "vb6_SetControlBackColor";
    // P13.5: Alignment (all text controls)
    if (propLower == "alignment") return "vb6_SetAlignment";
    // P13.6: TabIndex/TabStop (all visible controls)
    if (propLower == "tabindex") return "vb6_SetTabIndex";
    if (propLower == "tabstop") return "vb6_SetTabStop";
    if (propLower == "causesvalidation") return "vb6_SetCausesValidation";
    // P13.8: ToolTipText (all visible controls)
    if (propLower == "tooltiptext") return "vb6_SetToolTipText";
    // P13.9: Tag (all controls)
    if (propLower == "tag") return "vb6_SetControlTag";
    // P13.7: MousePointer/MouseIcon (all visible controls)
    if (propLower == "mousepointer") return "vb6_SetMousePointer";
    if (propLower == "mouseicon") return "vb6_SetMouseIcon";
    // P13.10: BorderStyle (all visible controls)
    if (propLower == "borderstyle") return "vb6_SetBorderStyle";

    switch (ctrlType) {
    case FrmControlType::TextBox:
        if (propLower == "text") return "vb6_SetControlText";
        if (propLower == "multiline") return "vb6_SetMultiLine";
        if (propLower == "scrollbars") return "vb6_SetScrollBars";
        if (propLower == "maxlength") return "vb6_SetMaxLength";
        if (propLower == "passwordchar") return "vb6_SetPasswordChar";
        if (propLower == "locked") return "vb6_SetLocked";
        if (propLower == "selstart") return "vb6_SetSelStart";
        if (propLower == "sellength") return "vb6_SetSelLength";
        if (propLower == "seltext") return "vb6_SetSelText";
        if (propLower == "visible") return "vb6_SetControlVisible";
        if (propLower == "enabled") return "vb6_SetControlEnabled";
        break;
    case FrmControlType::ListBox:
    case FrmControlType::ComboBox:
        if (propLower == "text") return "vb6_SetControlText";
        if (propLower == "listindex") return "vb6_SetListIndex";
        if (propLower == "list") return "vb6_SetListItem";
        if (propLower == "selected") return "vb6_SetSelected";
        if (propLower == "itemdata") return "vb6_SetItemData";
        if (propLower == "visible") return "vb6_SetControlVisible";
        if (propLower == "enabled") return "vb6_SetControlEnabled";
        break;
    case FrmControlType::Frame:
        if (propLower == "caption") return "vb6_SetControlText";
        if (propLower == "visible") return "vb6_SetControlVisible";
        if (propLower == "enabled") return "vb6_SetControlEnabled";
        break;
    case FrmControlType::Label:
        if (propLower == "caption") return "vb6_SetControlText";
        if (propLower == "visible") return "vb6_SetControlVisible";
        if (propLower == "enabled") return "vb6_SetControlEnabled";
        break;
    case FrmControlType::CommandButton:
        if (propLower == "caption") return "vb6_SetControlText";
        if (propLower == "default") return "vb6_SetDefaultButton";
        if (propLower == "cancel") return "vb6_SetCancelButton";
        if (propLower == "visible") return "vb6_SetControlVisible";
        if (propLower == "enabled") return "vb6_SetControlEnabled";
        break;
    case FrmControlType::CheckBox:
    case FrmControlType::OptionButton:
        if (propLower == "value") return "vb6_SetCheckValue";
        if (propLower == "caption") return "vb6_SetControlText";
        if (propLower == "visible") return "vb6_SetControlVisible";
        if (propLower == "enabled") return "vb6_SetControlEnabled";
        break;
    case FrmControlType::Form:
        if (propLower == "caption") return "vb6_SetControlText";
        if (propLower == "visible") return "vb6_SetControlVisible";
        if (propLower == "enabled") return "vb6_SetControlEnabled";
        break;
    case FrmControlType::WebBrowser:
        if (propLower == "visible") return "vb6_SetControlVisible";
        if (propLower == "enabled") return "vb6_SetControlEnabled";
        break;
    case FrmControlType::HScrollBar:
    case FrmControlType::VScrollBar:
        if (propLower == "value") return "vb6_SetScrollValue";
        if (propLower == "min") return "vb6_SetScrollMin";
        if (propLower == "max") return "vb6_SetScrollMax";
        if (propLower == "largechange") return "vb6_SetLargeChange";
        if (propLower == "smallchange") return "vb6_SetSmallChange";
        if (propLower == "visible") return "vb6_SetControlVisible";
        if (propLower == "enabled") return "vb6_SetControlEnabled";
        break;
    case FrmControlType::Timer:
        if (propLower == "interval") return "vb6_SetTimerInterval";
        if (propLower == "enabled") return "vb6_SetTimerEnabled";
        break;
    case FrmControlType::PictureBox:
        if (propLower == "caption") return "vb6_SetControlText";
        if (propLower == "picture") return "vb6_SetControlPicture";
        if (propLower == "autosize") return "vb6_SetPictureAutoSize";
        if (propLower == "visible") return "vb6_SetControlVisible";
        if (propLower == "enabled") return "vb6_SetControlEnabled";
        break;
    case FrmControlType::Image:
        if (propLower == "picture") return "vb6_SetControlPicture";
        if (propLower == "stretch") return "vb6_SetImageStretch";  // P17.2
        if (propLower == "visible") return "vb6_SetControlVisible";
        if (propLower == "enabled") return "vb6_SetControlEnabled";
        break;
    case FrmControlType::Menu:  // P20-36
        if (propLower == "caption") return "vb6_SetMenuCaption";
        if (propLower == "checked") return "vb6_SetMenuChecked";
        if (propLower == "enabled") return "vb6_SetMenuEnabled";
        if (propLower == "visible") return "vb6_SetMenuVisible";
        break;
    case FrmControlType::Shape:  // P20-35
        if (propLower == "shape") return "vb6_SetShapeType";
        if (propLower == "borderwidth") return "vb6_SetShapeBorderWidth";
        if (propLower == "borderstyle") return "vb6_SetShapeBorderStyle";
        if (propLower == "fillstyle") return "vb6_SetShapeFillStyle";
        if (propLower == "bordercolor") return "vb6_SetShapeBorderColor";
        if (propLower == "fillcolor") return "vb6_SetShapeFillColor";
        if (propLower == "visible") return "vb6_SetControlVisible";
        break;
    case FrmControlType::Line:  // P20-35
        if (propLower == "x1") return "vb6_SetLineX1";
        if (propLower == "y1") return "vb6_SetLineY1";
        if (propLower == "x2") return "vb6_SetLineX2";
        if (propLower == "y2") return "vb6_SetLineY2";
        if (propLower == "borderwidth") return "vb6_SetLineBorderWidth";
        if (propLower == "borderstyle") return "vb6_SetLineBorderStyle";
        if (propLower == "bordercolor") return "vb6_SetLineColor";
        if (propLower == "visible") return "vb6_SetControlVisible";
        break;
    default:
        if (propLower == "visible") return "vb6_SetControlVisible";
        if (propLower == "enabled") return "vb6_SetControlEnabled";
        break;
    }
    return "";  // 未知属性
}
// P20-36: 生成控件属性访问的HWND参数 (Menu控件用GetMenu+menuId)
std::string CCodeGen::makeCtrlHwndArg(const std::string& ctrlNameLower, FrmControlType ctrlType) const {
    if (ctrlType == FrmControlType::Menu) {
        auto menuIt = knownMenuIds_.find(ctrlNameLower);
        if (menuIt != knownMenuIds_.end()) {
            return "(void*)GetMenu((HWND)" + knownMenuFormHwnd_ + "), " + std::to_string(menuIt->second);
        }
    }
    auto origIt = knownFormControlOriginalNames_.find(ctrlNameLower);
    std::string origName = (origIt != knownFormControlOriginalNames_.end()) ? origIt->second : ctrlNameLower;
    return "vb6_hwnd_" + cIdent(origName);
}
const char* CCodeGen::getDefaultPropertyName(FrmControlType ctrlType) {
    switch (ctrlType) {
    case FrmControlType::TextBox:      return "Text";
    case FrmControlType::Label:        return "Caption";
    case FrmControlType::CommandButton: return "Caption";
    case FrmControlType::CheckBox:     return "Value";
    case FrmControlType::OptionButton: return "Value";
    case FrmControlType::ListBox:      return "Text";
    case FrmControlType::ComboBox:     return "Text";
    case FrmControlType::Frame:        return "Caption";
    case FrmControlType::Form:         return "Caption";
    case FrmControlType::MDIForm:      return "Caption";
    case FrmControlType::PictureBox:   return "Picture";  // P17.2
    case FrmControlType::Image:        return "Picture";  // P17.2
    case FrmControlType::HScrollBar:   return "Value";  // P20-41
    case FrmControlType::VScrollBar:   return "Value";  // P20-41
    default:                           return nullptr;
    }
}



// ============================================================
// P8.4: Variant值包装 - 根据表达式类型推断Variant构造函数
// ============================================================

std::string CCodeGen::wrapVariantValue(ASTNode* valueNode, const std::string& cExpr) const {
    if (!valueNode) return "vb6_VariantEmpty()";
    
    // 特殊情况: Null字面量
    if (valueNode->kind == ASTNodeKind::LiteralExpr) {
        auto& lit = static_cast<LiteralExpr&>(*valueNode);
        if (lit.literalKind == LiteralKind::Null) return "vb6_VariantNull()";
        if (lit.literalKind == LiteralKind::Empty) return "vb6_VariantEmpty()";
        if (lit.literalKind == LiteralKind::Boolean) {
            return std::string("vb6_VariantBool(") + cExpr + ")";
        }
    }
    
    // 使用inferExprType推断表达式类型
    if (valueNode->kind == ASTNodeKind::BinaryExpr ||
        valueNode->kind == ASTNodeKind::UnaryExpr ||
        valueNode->kind == ASTNodeKind::LiteralExpr ||
        valueNode->kind == ASTNodeKind::IdentifierExpr ||
        valueNode->kind == ASTNodeKind::IndexOrCallExpr ||
        valueNode->kind == ASTNodeKind::MemberAccessExpr) {
        Vb6Type vtype = inferExprType(static_cast<Expr&>(*valueNode));
        switch (vtype) {
            case Vb6Type::String:    return "vb6_VariantString(" + cExpr + ")";
            case Vb6Type::Long:
            case Vb6Type::Integer:   return "vb6_VariantLong(" + cExpr + ")";
            case Vb6Type::Double:
            case Vb6Type::Single:    return "vb6_VariantDouble(" + cExpr + ")";
            case Vb6Type::Boolean:   return "vb6_VariantBool(" + cExpr + ")";
            case Vb6Type::Byte:      return "vb6_VariantLong(" + cExpr + ")";
            case Vb6Type::Date:      return "vb6_VariantDouble(" + cExpr + ")";
            case Vb6Type::Currency:  return "vb6_VariantDouble(" + cExpr + ")";
            default: break;
        }
    }
    
    // 如果右侧已经是vb6_VARIANT类型(如函数返回Variant), 直接赋值
    // Fix 051: 排除 vb6_VariantTo* 函数 (如 vb6_VariantToObjectVal 返回 void*,
    // vb6_VariantToString 返回 BSTR), 这些不是 vb6_VARIANT 类型, 需要包装.
    if ((cExpr.find("vb6_Variant") == 0 && cExpr.find("vb6_VariantTo") != 0)
        || cExpr.find("vb6_CStr") == 0) {
        return cExpr;
    }
    
    // COM后期绑定调用: vb6_ComCall返回VARIANT*, 需转为vb6_VARIANT
    if (cExpr.find("vb6_ComCall(") == 0) {
        return "vb6_VariantFromComResult(" + cExpr + ")";
    }
    
    // Array()临时变量: _arr_N is vb6_SafeArray1D*, 包装为Variant持有数组
    if (cExpr.find("_arr_") == 0) {
        return "vb6_VariantArray(" + cExpr + ")";
    }
    
    // Fix 025: 默认改用 _Generic 多态宏 vb6_VariantFromValue, 让编译器按实参 C 类型
    // 自动选择 Variant 构造函数。覆盖标量/BSTR/void*/class ptr/vb6_SafeArray1D* 等所有
    // 已注册的 _Generic 选择器, 不再粗暴回退到 VariantLong (会把指针/BSTR 当 int 截断)。
    return "vb6_VariantFromValue(" + cExpr + ")";
}

} // namespace vb6c3

