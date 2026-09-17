// driver_codegen_dll_sync.inc - runCodeGeneration 函数体片段：P6.6.6/M29 IID/CLSID/DISPID 符号同步（位于 if (options.isDll) 块内）
// 由 src/driver/driver.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 699~753 行，逐行未改。

        // P6.6.6: Sync comDefaultIfaceIid/comClsidStr from allSymTabs to lastAnalyzer's symbol table
        // (TypeLib builder writes to per-module Symbol objects, but cgen uses lastAnalyzer's merged table)
        for (auto& [key, sym] : lastAnalyzer->symbolTable().moduleScope()->symbols()) {
            if (sym->kind == SymbolKind::Class && !sym->isInterface && sym->comDefaultIfaceIid.empty()) {
                // Search all symbol tables for a matching class with IID info
                for (auto* st : allSymTabs) {
                    auto* srcSym = st->lookupModule(sym->name);
                    if (srcSym && srcSym->kind == SymbolKind::Class && !srcSym->comDefaultIfaceIid.empty()) {
                        sym->comDefaultIfaceIid = srcSym->comDefaultIfaceIid;
                        sym->comDefaultIfaceName = srcSym->comDefaultIfaceName;
                        sym->comDefaultMemberName = srcSym->comDefaultMemberName;
                        sym->comDefaultMemberRealName = srcSym->comDefaultMemberRealName;
                    }
                    if (srcSym && srcSym->kind == SymbolKind::Class && !srcSym->comClsidStr.empty() && sym->comClsidStr.empty()) {
                        sym->comClsidStr = srcSym->comClsidStr;
                    }
                    if (srcSym && srcSym->kind == SymbolKind::Class && !srcSym->comSourceIfaceIid.empty() && sym->comSourceIfaceIid.empty()) {
                        sym->comSourceIfaceIid = srcSym->comSourceIfaceIid;
                        sym->comSourceIfaceName = srcSym->comSourceIfaceName;
                    }
                }
            }
        }

        // M29: Sync comDispid from per-module method symbols to lastAnalyzer's merged table
        // (TypeLib builder writes comDispid to per-module Symbol* in allSymTabs, but cgen's
        //  generateDllEntry reads via lastAnalyzer.symbolTable().lookupModule[ByKind], which
        //  may return a DIFFERENT Symbol* than what driver TypeLib phase wrote to. Same pattern
        //  as the comDefaultIfaceIid sync block above.)
        for (auto& [mKey, mClsSym] : lastAnalyzer->symbolTable().moduleScope()->symbols()) {
            if (!mClsSym || mClsSym->kind != SymbolKind::Class || mClsSym->isInterface) continue;
            if (mClsSym->isExternal) continue;  // 跳过外部注入副本
            for (auto& memberName : mClsSym->memberNames) {
                // Sync each possible method kind independently (Sub/Function are exclusive;
                // Property Get/Let/Set share name but are distinct Symbol objects)
                for (auto mKind : {SymbolKind::Sub, SymbolKind::Function,
                                   SymbolKind::PropertyGet, SymbolKind::PropertyLet,
                                   SymbolKind::PropertySet}) {
                    // Find any Symbol* with comDispid set (the "source of truth" written by driver TypeLib phase)
                    int knownDispid = 0;
                    for (auto* st : allSymTabs) {
                        Symbol* s = st->lookupModuleByKind(memberName, mKind);
                        if (s && !s->isExternal && s->comDispid != 0) { knownDispid = s->comDispid; break; }
                    }
                    if (knownDispid == 0) continue;  // method not present, or driver didn't set dispid
                    // Propagate to ALL matching Symbol* across all symbol tables (only fill zeros)
                    // (cgen's generateDllEntry may read via a DIFFERENT Symbol* than what driver wrote to,
                    //  because lastAnalyzer's merged table can have its own copy. Update every copy.)
                    for (auto* st : allSymTabs) {
                        Symbol* s = st->lookupModuleByKind(memberName, mKind);
                        if (s && !s->isExternal && s->comDispid == 0) s->comDispid = knownDispid;
                    }
                }
            }
        }
