// cgen_util_dllentry_tables.inc - generateDllEntry 函数体片段：方法描述表、IID/事件常量表与 coclass 描述表
// 由 src/backend/cgen_util.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 398~623 行，逐行未改。

    // 2. IDispatch方法描述表
    for (auto& cc : coClasses) {
        std::string clsId = cIdent(cc.moduleName);
        std::string methodsVar = "g_vb6_disp_" + clsId + "Methods";

        // Fix 099: 有 Public 字段 (无方法) 时也要生成表 —— 字段被暴露为 Property Get/Let
        if (cc.publicMethodNames.empty() && cc.publicFields.empty()) {
            continue;  // No public methods/fields - skip method table
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

        // Fix 099: Public 字段条目 —— 每字段两条 (invkind 2=PropertyGet / 4=PropertyPut),
        // 共享同一 dispid (COM 约定: 同名 Property 的 Get/Let 同 dispid, TypeLib 侧亦然).
        for (size_t fi = 0; fi < cc.publicFields.size(); fi++) {
            const std::string& fname = cc.publicFields[fi];
            int32_t fdispid = (fi < cc.publicFieldDispids.size()) ? cc.publicFieldDispids[fi] : 0;
            if (fdispid == 0) {
                // 防御: TypeLib 阶段未回写 (理论上不应发生). 用高位段避开方法区
                // 的连续 dispid (TypeLib 从 1 开始分配), 否则 Invoke 按 dispid
                // 线性匹配会命中错误的表项.
                fdispid = 9000 + (int32_t)fi;
            }
            std::string fInvoke = "vb6_disp_" + clsId + "_" + cIdent(fname);
            entry.emitLine("{ L\"" + fname + "\", " + std::to_string(fdispid) +
                           ", 2, " + fInvoke + "_get_invoke },");
            entry.emitLine("{ L\"" + fname + "\", " + std::to_string(fdispid) +
                           ", 4, " + fInvoke + "_let_invoke },");
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
        // Fix 099: Public 字段占 2 条 (Get + Let), methodCount 要把它们算进去 —
        // GetIDsOfNames / Invoke 都是遍历 methods[0..methodCount) 匹配的.
        if (cc.publicMethodNames.empty() && cc.publicFields.empty()) {
            entry.emitLine("0,  /* methodCount */");
            entry.emitLine("NULL,  /* methods */");
        } else {
            size_t totalMethodEntries = cc.publicMethodNames.size() + cc.publicFields.size() * 2;
            entry.emitLine(std::to_string(totalMethodEntries) + ",  /* methodCount */");
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
