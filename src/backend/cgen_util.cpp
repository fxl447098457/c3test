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
    for (auto& [key, sym] : symTab_.moduleScope()->symbols()) {
        if (sym->kind == SymbolKind::Class && !sym->isInterface) {
            // BUG-4 fix: PublicNotCreatable(2) is visible but NOT externally creatable
            // Only MultiUse(5)/SingleUse(3) should have ClassFactory entries
            if (sym->instancing == VBInstancing::MultiUse || sym->instancing == VBInstancing::SingleUse) {
                CoClassInfo info;
                info.moduleName = sym->name;
                info.clsidStr = sym->comClsidStr.empty() ? generateClsid(progId + "." + sym->name) : sym->comClsidStr;  // P6.8: 优先使用VBP指定的CLSID
                info.progId = progId + "." + sym->name;

                // 使用类的memberNames查找Public方法符号
                // (不能按前缀"ClassName_"搜索, 因为单模块工程中方法名是"SetValue"而非"Calc_SetValue")
                for (auto& memberName : sym->memberNames) {
                    // Try Sub/Function: search in all symbol tables
                    Symbol* memSym = nullptr;
                    memSym = symTab_.lookupModule(memberName);
                    if (!memSym) memSym = symTab_.lookup(memberName);
                    if (!memSym) {
                        for (auto* st : allSymTabs) {
                            memSym = st->lookupModule(memberName);
                            if (memSym) break;
                        }
                    }
                    if (memSym && (memSym->kind == SymbolKind::Sub || memSym->kind == SymbolKind::Function)
                        && memSym->access == AccessLevel::Public) {
                        info.publicMethodNames.push_back(memSym->name);
                        info.publicMethodSyms.push_back(memSym);
                        memSym = nullptr;  // Don't double-count
                    }
                    // Lookup Property Get/Let/Set: search in all symbol tables
                    for (auto kind : {SymbolKind::PropertyGet, SymbolKind::PropertyLet, SymbolKind::PropertySet}) {
                        Symbol* propSym = symTab_.lookupModuleByKind(memberName, kind);
                        if (!propSym) {
                            for (auto* st : allSymTabs) {
                                propSym = st->lookupModuleByKind(memberName, kind);
                                if (propSym) break;
                            }
                        }
                        if (propSym && propSym->access == AccessLevel::Public) {
                            info.publicMethodNames.push_back(propSym->name);
                            info.publicMethodSyms.push_back(propSym);
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
    // 注意: 不include类模块的.h文件, 因为vb6rtl.h中的VARIANT定义
    // 与Windows <oleauto.h>中的VARIANT冲突。改用前向声明。
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
            if (methodSym->kind == SymbolKind::Function || methodSym->kind == SymbolKind::PropertyGet) {
                std::string retType = mapType(methodSym->type);
                std::string params = "struct " + clsStruct + "*";
                for (auto& p : methodSym->params) {
                    params += ", " + mapType(p.type);
                }
                entry.emitLine("extern " + retType + " " + procName + "(" + params + ");");
            } else {
                std::string params = "struct " + clsStruct + "*";
                for (auto& p : methodSym->params) {
                    params += ", " + mapType(p.type);
                }
                entry.emitLine("extern void " + procName + "(" + params + ");");
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

            // 构建实参列表 (不含me的类型, 只有表达式)
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
                    } else {
                        callArgs += "(void*)&((VARIANT*)args[" + std::to_string(i) + "])->lVal";
                    }
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

        // Per-class DISPID assignment (matches TypeLib builder sequential)
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
            int dispid;
            auto dpIt = dispIdMap.find(lowerBareName);
            if (dpIt != dispIdMap.end()) {
                dispid = dpIt->second;
            } else {
                dispid = nextDispid++;
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
            // 查找成员函数/属性的返回类型
            auto* memSym = symTab_.lookupModule(ma.memberName);
            if (memSym) return memSym->type;
            break;
        }
        default:
            break;
    }
    return Vb6Type::Variant;
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
            return "vb6_ComPackVariant"; // vb6_VARIANT → VARIANT
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
    // vb6_ComPackVariant: 保留默认的VariantFromComResult形式
    return "";
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
    if (cExpr.find("vb6_Variant") == 0 || cExpr.find("vb6_CStr") == 0) {
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
    
    // 默认: 尝试用Long包装 (VB6默认整数类型)
    return "vb6_VariantLong(" + cExpr + ")";
}

} // namespace vb6c3

