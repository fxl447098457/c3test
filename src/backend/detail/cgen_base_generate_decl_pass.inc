// cgen_base_generate_decl_pass.inc - generate 函数体片段：第一遍 = 声明
// 由 src/backend/cgen_base.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 498~596 行，逐行未改。

    // === 第一遍: 声明 (前向声明 → .h, 定义 → .c) ===
    // (Enum和Type定义已在类结构体前生成, 见上方Fix 010)

    // 3. Const
    for (auto& decl : module.declarations) {
        if (decl->kind == ASTNodeKind::ConstDecl) {
            visit(static_cast<ConstDecl&>(*decl));
        }
    }

    // 4. 类模块变量: 只注册tracking set, 不生成声明(已在结构体中)
    //    标准模块/窗体模块: 生成变量声明并注册tracking set
    trackOnly_ = isClassModule_;
    for (auto& decl : module.declarations) {
        if (decl->kind == ASTNodeKind::VariableDecl) {
            visit(static_cast<VariableDecl&>(*decl));
        }
    }
    trackOnly_ = false;

    // P13.23: External COM WithEvents need vb6com.h for vb6_CreateEventSink/ComAdvise/ComUnadvise
    // (knownWithEventsVars_ is populated during VariableDecl visits above)
    if (!knownWithEventsVars_.empty()) {
        for (auto& [varLower, srcClassName] : knownWithEventsVars_) {
            auto* srcClsSym = symTab_.lookup(srcClassName);
            if (srcClsSym && srcClsSym->kind == SymbolKind::ComClass && srcClsSym->comHasSourceIface) {
                c_.emitLine("#include \"vb6com.h\"");
                break;
            }
        }
    }

    // === P7: 窗体模块额外代码 ===
    // 窗体模块需要: WndProc声明、控件句柄变量、控件创建函数
    if (module.isFormModule && frmDesc) {
        emitFormFramework(*frmDesc, module);
    }

    // 5. Declare (外部函数声明)
    for (auto& decl : module.declarations) {
        if (decl->kind == ASTNodeKind::DeclareDecl) {
            visit(static_cast<DeclareDecl&>(*decl));
        }
    }

    // 6. Event声明 (类模块中)
    if (isClassModule_) {
        for (auto& decl : module.declarations) {
            if (decl->kind == ASTNodeKind::EventDecl) {
                visit(static_cast<EventDecl&>(*decl));
            }
        }
    }

    // 7. 过程前向声明 → .h
    // P6.4: 接口类的方法不生成前向声明 (方法是抽象的, 由实现类提供)
    bool currentClassIsInterface = false;
    if (isClassModule_) {
        auto* clsSym = symTab_.lookupModule(module.moduleName);
        if (clsSym && clsSym->kind == SymbolKind::Class && clsSym->isInterface) {
            currentClassIsInterface = true;
        }
    }
    for (auto& decl : module.declarations) {
        if (currentClassIsInterface) continue;  // 接口类: 跳过方法声明
        if (decl->kind == ASTNodeKind::SubDecl) {
            auto& sub = static_cast<SubDecl&>(*decl);
            std::string sig = makeProcSignature(sub);
            // Fix 055: Form事件处理函数不能为static, 因为wndproc用extern引用它们
            bool isFormEventProc = module.isFormModule && sub.name.find("Form_") == 0;
            // Fix 089e: Friend/Public 成员跨模块可调用 → 非 static 声明
            if (sub.access == AccessLevel::Public || sub.access == AccessLevel::Friend || isFormEventProc) {
                h_.emitLine(sig + ";");
            } else {
                h_.emitLine("static " + sig + ";");
            }
        } else if (decl->kind == ASTNodeKind::FunctionDecl) {
            auto& func = static_cast<FunctionDecl&>(*decl);
            std::string sig = makeProcSignature(func);
            // Fix 055: Form事件处理函数不能为static, 因为wndproc用extern引用它们
            bool isFormEventFunc = module.isFormModule && func.name.find("Form_") == 0;
            // Fix 089e: Friend/Public 成员跨模块可调用 → 非 static 声明
            if (func.access == AccessLevel::Public || func.access == AccessLevel::Friend || isFormEventFunc) {
                h_.emitLine(sig + ";");
            } else {
                h_.emitLine("static " + sig + ";");
            }
        } else if (decl->kind == ASTNodeKind::PropertyDecl) {
            auto& prop = static_cast<PropertyDecl&>(*decl);
            std::string sig = makePropertySignature(prop);
            // Fix 089e: Friend Property (Friend Property Get/Let/Set) 跨模块可调用
            if (prop.access == AccessLevel::Public || prop.access == AccessLevel::Friend) {
                h_.emitLine(sig + ";");
            } else {
                h_.emitLine("static " + sig + ";");
            }
        }
    }

