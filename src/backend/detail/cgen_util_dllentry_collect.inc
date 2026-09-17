// cgen_util_dllentry_collect.inc - generateDllEntry 函数体片段：coclass 收集、空类桩、前向声明与 IDispatch 桥接
// 由 src/backend/cgen_util.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 73~397 行，逐行未改。

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
