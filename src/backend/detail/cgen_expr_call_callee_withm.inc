// cgen_expr_call_callee_withm.inc - visit(IndexOrCallExpr&) 函数体片段：Fix 060 WithMemberExpr callee、callee 发射与控件方法拦截
// 由 src/backend/expr/cgen_expr_call.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 1010~1167 行，逐行未改。

    // ---- Fix 060: WithMemberExpr callee with positional args — With block UDT array field ----
    // 处理 With 块内 .Data(index) 模式, 其中 .Data 是 UDT 动态数组字段
    // WithMemberExpr 不同于 MemberAccessExpr, 需要单独处理
    if (node.callee && node.callee->kind == ASTNodeKind::WithMemberExpr
        && !node.positional.empty() && node.named.empty()
        && !withObjectVars_.empty() && !withObjectInfoStack_.empty()) {
        auto& wmExpr = static_cast<WithMemberExpr&>(*node.callee);
        const auto& info = withObjectInfoStack_.back();
        if (info.kind == WithObjKind::Unknown) {
            // UDT With block — 查找 UDT 类型
            const std::string& tempVar = withObjectVars_.back();
            std::string tempLower = Symbol::toLower(tempVar);
            auto it = knownUdtVars_.find(tempLower);
            if (it != knownUdtVars_.end()) {
                const std::string prefix = "vb6_type_";
                const std::string& udtCType = it->second;
                if (udtCType.size() > prefix.size()
                    && udtCType.compare(0, prefix.size(), prefix) == 0) {
                    std::string udtName = udtCType.substr(prefix.size());
                    Symbol* udtSym = symTab_.lookupModule(udtName);
                    if (udtSym && udtSym->kind == SymbolKind::UserDefinedType) {
                        std::string memLower = Symbol::toLower(wmExpr.memberName);
                        for (auto& mi : udtSym->udtMembers) {
                            if (Symbol::toLower(mi.name) == memLower) {
                                bool isHandled = false;
                                if (mi.arraySize > 0) {
                                    // Pattern A: 固定大小数组字段 → tempVar->member[idx]
                                    // Fix 081j-2: With 块临时变量是指针，用 -> 访问成员
                                    emitExpr(*node.positional[0]);
                                    std::string idx = std::move(lastExpr_);
                                    lastExpr_ = tempVar + "->" + cIdent(mi.name) + "[" + idx + "]";
                                    isHandled = true;
                                } else if (mi.isArrayDynamic) {
                                    // Pattern B: 动态数组字段 → VB6_SA_AT(elemType, tempVar->member, idx)
                                    // Fix 081j-2: With 块临时变量是指针，用 -> 访问成员
                                    emitExpr(*node.positional[0]);
                                    std::string idx = std::move(lastExpr_);
                                    // Fix 092s: 同 3626 — UDT 元素类型成员数组
                                    std::string elemCType = (mi.type == Vb6Type::UserDefinedType
                                                             && !mi.typeRefName.empty())
                                        ? ("vb6_type_" + cIdent(mi.typeRefName))
                                        : mapSaElemCType(mi.type);
                                    lastExpr_ = "VB6_SA_AT(" + elemCType + ", "
                                              + tempVar + "->" + cIdent(mi.name) + ", " + idx + ")";
                                    isHandled = true;
                                }
                                if (isHandled) return;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    // M22: 设置asCallCallee_标志, 让IdentifierExpr知道当前是函数调用callee上下文
    // 这确保递归调用时(如 Factorial(n-1))返回函数名而非返回值变量
    // Fix 015: 加 save/restore. 原代码硬编码 `asCallCallee_=false` 会丢失嵌套 callee
    // 上下文 (如 CallStmt→MemberAccessExpr.Fix015→IndexOrCallExpr 链中, 内层 IndexOrCallExpr
    // 反复重置成 false, 外层 Fix 015 看到的是 false 而非 CallStmt 设置的 true).
    // 同时清空 pendingChainObj_ 防止跨调用泄漏 (Fix 015 在 callee emission 时设置).
    pendingChainObj_.clear();
    bool savedAsCallCallee = asCallCallee_;
    asCallCallee_ = true;
    emitExpr(*node.callee);
    asCallCallee_ = savedAsCallCallee;
    std::string callee = std::move(lastExpr_);

    // Bug #3 fix: 清除COM Picture标志 — 每次新的IndexOrCallExpr重置
    // 后续根据被调用函数的返回类型重新设置
    lastExprIsComPicture_ = false;

    // Fix 032: args (positional + named + early-COM 早期路径) 是 值上下文, 不可能是
    // callee 上下文. 外层 CallStmt(2159-2162) 或嵌套 IndexOrCallExpr 可能把
    // asCallCallee_ 留在 true 状态. 若不强制 false, 后续所有 emitExpr(子表达式)
    // 作为参数发联时, 会被 IdentifierExpr 的自引用检查 (line ~199-213) 误判为
    // callee 上下文, 导致 Property Get / Function 返回值变量名引用错误地返回
    // 过程名 (如 vb6_cTlsReMaster_LocalHostName 而非 vb6_ret_LocalHostName).
    // RAII: 函数退出时 (任何 return 或自然走到末尾) 自动恢复为 savedAsCallCallee.
    struct CallCalleeValueScope {
        bool& ref;
        bool saved;
        CallCalleeValueScope(bool& r, bool s) : ref(r), saved(s) { ref = false; }
        ~CallCalleeValueScope() { ref = saved; }
    } _argsValueScope{asCallCallee_, savedAsCallCallee};
    (void)_argsValueScope;  // suppress unused-warning

    // --- P7.9: WebBrowser控件方法调用 ---
    // Navigate/GoBack/GoForward/Refresh via isComMarker_ flag set by MemberAccessExpr
    if (isComMarker_) {
        auto itCtrl = knownFormControls_.find(comObjExpr_);
        if (itCtrl != knownFormControls_.end() && itCtrl->second == FrmControlType::WebBrowser) {
            isComMarker_ = false;
            std::string method = std::move(comMemberName_);
            std::string ctrlName = cIdent(knownFormControlOriginalNames_.count(comObjExpr_) ? knownFormControlOriginalNames_[comObjExpr_] : comObjExpr_);
            comObjExpr_.clear();
            comMemberName_.clear();
            if (method == "navigate") {
                std::string urlArg = "0";
                if (!node.positional.empty()) {
                    emitExpr(*node.positional[0]);
                    urlArg = std::move(lastExpr_);
                }
                lastExpr_ = "(void)vb6_WebViewNavigate((void*)vb6_hwnd_" + ctrlName + ", " + urlArg + ")";
                return;
            } else if (method == "goback" || method == "goforward" || method == "refresh") {
                // Simplified: not yet implemented
                lastExpr_ = "(void)0";
                return;
            }
        }
    }

    // P13.3: ListBox/ComboBox methods (AddItem/RemoveItem/Clear/List) via isComMarker_ flag
    if (isComMarker_) {
        auto itCtrl = knownFormControls_.find(comObjExpr_);
        if (itCtrl != knownFormControls_.end() &&
            (itCtrl->second == FrmControlType::ListBox || itCtrl->second == FrmControlType::ComboBox)) {
            isComMarker_ = false;
            std::string method = std::move(comMemberName_);
            std::string ctrlName = cIdent(knownFormControlOriginalNames_.count(comObjExpr_) ? knownFormControlOriginalNames_[comObjExpr_] : comObjExpr_);
            comObjExpr_.clear();
            comMemberName_.clear();
            if (method == "additem") {
                std::string itemArg = "0";
                if (!node.positional.empty()) {
                    emitExpr(*node.positional[0]);
                    itemArg = std::move(lastExpr_);
                }
                c_.emitLine("vb6_AddItem((void*)vb6_hwnd_" + ctrlName + ", " + itemArg + ");  /* ListBox.AddItem */");
                lastExpr_ = "0";
                return;
            } else if (method == "removeitem") {
                std::string idxArg = "0";
                if (!node.positional.empty()) {
                    emitExpr(*node.positional[0]);
                    idxArg = std::move(lastExpr_);
                }
                c_.emitLine("vb6_RemoveItem((void*)vb6_hwnd_" + ctrlName + ", " + idxArg + ");  /* ListBox.RemoveItem */");
                lastExpr_ = "0";
                return;
            } else if (method == "clear") {
                c_.emitLine("vb6_ClearList((void*)vb6_hwnd_" + ctrlName + ");  /* ListBox.Clear */");
                lastExpr_ = "0";
                return;
            } else if (method == "list") {
                // List(idx) property read - List1.List(0)
                std::string idxArg = "0";
                if (!node.positional.empty()) {
                    emitExpr(*node.positional[0]);
                    idxArg = std::move(lastExpr_);
                }
                lastExpr_ = "vb6_GetListItem((void*)vb6_hwnd_" + ctrlName + ", " + idxArg + ")";
                return;
            }
        }
    }