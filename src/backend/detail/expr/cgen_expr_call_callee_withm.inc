// cgen_expr_call_callee_withm.inc - visit(IndexOrCallExpr&) 函数体片段：Fix 060 WithMemberExpr callee、callee 发射与控件方法拦截
// 由 src/backend/expr/cgen_expr_call.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 1010~1167 行，逐行未改。

    // ---- Fix 158c: WithMemberExpr callee 带 **空括号** — With 块 UDT 动态/固定
    // 数组成员整体引用 .member() → 裸数组表达式 tempVar->member (不带调用括号).
    // 与下方 Fix 060 Pattern B 相同判定, 但不消费 positional (空).
    // 否则 fallback 把 .member() 当函数调用 emit 成 tempVar->member() → C2064
    // (VBFlexGrid.ctl ClearGrid: With VBFlexGridCells.Rows(iRow):
    //     .Cols() = VBFlexGridDefaultCols.Cols() — LHS/RHS 都是整体数组引用).
    if (node.callee && node.callee->kind == ASTNodeKind::WithMemberExpr
        && node.positional.empty() && node.named.empty()
        && !withObjectVars_.empty() && !withObjectInfoStack_.empty()) {
        auto& wmExpr158 = static_cast<WithMemberExpr&>(*node.callee);
        const auto& info158 = withObjectInfoStack_.back();
        if (info158.kind == WithObjKind::Unknown) {
            const std::string& tempVar158 = withObjectVars_.back();
            std::string tempLower158 = Symbol::toLower(tempVar158);
            auto it158 = knownUdtVars_.find(tempLower158);
            if (it158 != knownUdtVars_.end()) {
                const std::string prefix158c = "vb6_type_";
                const std::string& udtCType158 = it158->second;
                if (udtCType158.size() > prefix158c.size()
                    && udtCType158.compare(0, prefix158c.size(), prefix158c) == 0) {
                    std::string udtName158 = udtCType158.substr(prefix158c.size());
                    Symbol* udtSym158 = symTab_.lookupModule(udtName158);
                    if (udtSym158 && udtSym158->kind == SymbolKind::UserDefinedType) {
                        std::string memLower158 = Symbol::toLower(wmExpr158.memberName);
                        for (auto& mi158 : udtSym158->udtMembers) {
                            if (Symbol::toLower(mi158.name) == memLower158) {
                                if (mi158.isArrayDynamic || mi158.arraySize > 0) {
                                    lastExpr_ = tempVar158 + "->" + cIdent(mi158.name);
                                    return;
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

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
        } else if (info.kind == WithObjKind::FormControl
                   && (info.ctrlType == FrmControlType::ListBox
                       || info.ctrlType == FrmControlType::ComboBox)) {
            // Fix 152e: With 块内 ListBox/ComboBox 的**可索引属性读取** —
            // `.ItemData(i)` / `.List(i)` / `.Selected(i)` (callee 是
            // WithMemberExpr 且带索引实参)。不拦截时走通用路径: 先 visit
            // (WithMemberExpr) 发 `vb6_GetItemData(tempVar)` (cgen_expr_with.cpp
            // 的 FormControl 读只喂 hwnd 单参, RTL 需 (void* hwnd, int idx) 两参)
            // → C2198 "太少实参", 再把索引当函数调用拼 `(i)` → C2064.
            // 与 P13.3b (MemberAccessExpr 同位) 一致: 直接生成 fn(hwnd, idx).
            const std::string& tempVar = withObjectVars_.back();
            std::string memLower152 = Symbol::toLower(wmExpr.memberName);
            const char* fn152 = nullptr;
            if (memLower152 == "list") fn152 = "vb6_GetListItem";
            else if (memLower152 == "selected") fn152 = "vb6_GetSelected";
            else if (memLower152 == "itemdata") fn152 = "vb6_GetItemData";
            if (fn152) {
                emitExpr(*node.positional[0]);
                std::string idx152 = std::move(lastExpr_);
                lastExpr_ = std::string(fn152) + "((void*)" + tempVar + ", " + idx152
                          + ")  /* With indexed ctrl prop */";
                return;
            }
            // Fix 152f: With 块内 ListBox/ComboBox 的**方法调用** —
            // `.AddItem(item[, index])` / `.RemoveItem(index)`. 这两者既不在
            // getControlPropReadFn (非属性) 也不在通用路径里映射, 原会退化成
            // `tempVar.AddItem` → HWND__.AddItem → C2039 "不是 HWND__ 的成员"
            // (VBFlexGrid demo 里 219 处, 全部 .AddItem)。直接映射到 RTL。
            if (memLower152 == "additem") {
                emitExpr(*node.positional[0]);
                std::string item152 = std::move(lastExpr_);
                lastExpr_ = "vb6_AddItem((void*)" + tempVar + ", "
                          + wrapToBSTR(item152, *node.positional[0])
                          + ")  /* With ctrl .AddItem */";
                return;
            }
            if (memLower152 == "removeitem") {
                emitExpr(*node.positional[0]);
                std::string idx152r = std::move(lastExpr_);
                lastExpr_ = "vb6_RemoveItem((void*)" + tempVar + ", " + idx152r
                          + ")  /* With ctrl .RemoveItem */";
                return;
            }
        }
    }

    // Fix 110b: 控件数组元素默认属性读取 — VB6 里 `TxtARGB(0)` 等价于
    // `TxtARGB(0).Text` (TextBox 控件数组元素的默认属性).
    // 不处理时 callee 的 IdentifierExpr 会先展开成默认属性读取
    // `vb6_GetControlText(vb6_hwnd_TxtARGB)`, 再被拼上 `(0)` →
    // `vb6_GetControlText(vb6_hwnd_TxtARGB)(0)` (C2064 / C2106).
    if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr
        && !node.positional.empty()) {
        auto& caId110 = static_cast<IdentifierExpr&>(*node.callee);
        std::string caLower110 = caId110.name;
        std::transform(caLower110.begin(), caLower110.end(), caLower110.begin(), ::tolower);
        if (knownControlArrays_.count(caLower110)) {
            auto itCtrl110 = knownFormControls_.find(caLower110);
            if (itCtrl110 != knownFormControls_.end()) {
                const char* defProp110 = getDefaultPropertyName(itCtrl110->second);
                std::string readFn110 = defProp110
                    ? getControlPropReadFn(itCtrl110->second, defProp110) : std::string();
                if (!readFn110.empty()) {
                    emitExpr(*node.positional[0]);
                    std::string idxArg110 = std::move(lastExpr_);
                    std::string orig110 = knownFormControlOriginalNames_.count(caLower110)
                        ? knownFormControlOriginalNames_[caLower110] : caId110.name;
                    lastExpr_ = readFn110 + "(vb6_CtrlArr_GetAt(&vb6_arr_" + cIdent(orig110)
                              + ", " + idxArg110 + "))  /* ctrl array default prop */";
                    return;
                }
            }
        }
    }

    // P13.3b: 控件可索引属性读取 — cmbBalloonTooltip.List(0) / .Selected(0) / .ItemData(0).
    // callee 是 MemberAccessExpr(控件, 属性) 且带索引实参时, RTL 读函数需要 hwnd + idx
    // 两个实参; 若走通用路径, 先发 vb6_GetListItem(vb6_hwnd_X) 再拼 (0) →
    // vb6_GetListItem(hwnd)(0) (C2064 把索引当函数调用). 与 Fix 110b 控件数组
    // 默认属性读取同位拦截: 直接生成 vb6_GetListItem((void*)vb6_hwnd_X, idx).
    if (node.callee && node.callee->kind == ASTNodeKind::MemberAccessExpr
        && !node.positional.empty()) {
        auto& maC131 = static_cast<MemberAccessExpr&>(*node.callee);
        if (maC131.object && maC131.object->kind == ASTNodeKind::IdentifierExpr) {
            auto& mcId131 = static_cast<IdentifierExpr&>(*maC131.object);
            std::string mcLower131 = mcId131.name;
            std::transform(mcLower131.begin(), mcLower131.end(), mcLower131.begin(), ::tolower);
            auto itCtrl131 = knownFormControls_.find(mcLower131);
            // C29-1b: 文件系统三控件的原生窗口就是 LISTBOX / COMBOBOX, 可索引属性
            // 与 ListBox 完全同一批 RTL 读函数, 所以拦截面必须一起放宽 —— 否则
            // dirList.List(0) 落到成员读函数那条单参路径, 发成 vb6_GetListItem(hwnd)(0)。
            if (itCtrl131 != knownFormControls_.end()
                && (itCtrl131->second == FrmControlType::ListBox
                    || itCtrl131->second == FrmControlType::ComboBox
                    || itCtrl131->second == FrmControlType::DirListBox
                    || itCtrl131->second == FrmControlType::FileListBox
                    || itCtrl131->second == FrmControlType::DriveListBox)) {
                std::string prop131 = maC131.memberName;
                std::transform(prop131.begin(), prop131.end(), prop131.begin(), ::tolower);
                const char* fn131 = nullptr;
                if (prop131 == "list") fn131 = "vb6_GetListItem";
                else if (prop131 == "selected") fn131 = "vb6_GetSelected";
                else if (prop131 == "itemdata") fn131 = "vb6_GetItemData";
                if (fn131) {
                    std::string orig131 = knownFormControlOriginalNames_.count(mcLower131)
                        ? knownFormControlOriginalNames_[mcLower131] : mcId131.name;
                    emitExpr(*node.positional[0]);
                    std::string idx131 = std::move(lastExpr_);
                    lastExpr_ = std::string(fn131) + "((void*)vb6_hwnd_" + cIdent(orig131)
                              + ", " + idx131 + ")  /* indexed ctrl prop */";
                    return;
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
                    // Fix 143: 实参是 COM 属性读 (cboThemes.AddItem iTheme.Name) —
                    // 必须在此消费 COM marker, 否则 lastExpr_ 停在对象表达式上
                    // (vb6_AddItem(hwnd, iTheme) 把 IDispatch 指针当字符串塞进列表,
                    // 条目显示成 "???" 乱码), 且残留 marker 会被上层当成独立语句
                    // 发射 (vb6_ComVarFree(ComCall(iTheme,"Name"))) → 结果丢弃.
                    if (isComMarker_) {
                        // 直接生成字符串属性读 (AddItem 形参是字符串);
                        // resolveComValue 会按属性类型给出 BSTR/vb6_VARIANT 两种形态,
                        // 这里要的是 BSTR, 故显式用 vb6_ComGetStringProp.
                        isComMarker_ = false;
                        isEarlyBoundCom_ = false;
                        earlyBoundSym_ = nullptr;
                        std::string objM = std::move(comObjExpr_);
                        std::string memM = std::move(comMemberName_);
                        comObjExpr_.clear();
                        comMemberName_.clear();
                        itemArg = "vb6_ComGetStringProp(" + objM + ", L\"" + memM + "\")";
                    } else {
                        itemArg = std::move(lastExpr_);
                    }
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

    // D6 / C29-9: CommonDialog.Show* → 原生 comdlg32 入口（成员侧打的是小写控件名）。
    // 必须在这里消费掉标记，否则下面的通用 COM 绑定会编成运行期 no-op 的 vb6_ComCall。
    if (isComMarker_) {
        auto itCtrlCd = knownFormControls_.find(comObjExpr_);
        if (itCtrlCd != knownFormControls_.end() &&
            itCtrlCd->second == FrmControlType::CommonDialog) {
            std::string mCd = comMemberName_;
            if (mCd == "showopen" || mCd == "showsave" || mCd == "showcolor"
                || mCd == "showfont" || mCd == "showprinter" || mCd == "showabout") {
                isComMarker_ = false;
                isEarlyBoundCom_ = false;
                earlyBoundSym_ = nullptr;
                std::string hwndCd = cIdent(knownFormControlOriginalNames_.count(comObjExpr_)
                    ? knownFormControlOriginalNames_[comObjExpr_] : comObjExpr_);
                comObjExpr_.clear();
                comMemberName_.clear();
                std::string fnCd = "vb6_CdShow"
                    + std::string(1, (char)::toupper((unsigned char)mCd[4])) + mCd.substr(5);
                c_.emitLine("{" + fnCd + "((void*)vb6_hwnd_" + hwndCd + ");}"
                            "  /* CommonDialog." + mCd + " (原生 comdlg32) */");
                lastExpr_ = "0";
                return;
            }
        }
    }

    // Fix 185: PictureBox.Print / PictureBox.Cls → RTL 控件绘制入口。
    // 成员侧 (cgen_expr_member_form_builtin.inc) 把 comObjExpr_ 存成小写控件名，
    // 所以这里查 knownFormControls_ 拿得到类型。本片段 include 在
    // cgen_expr_call_com_bind.inc **之前**（见 cgen_expr_call.cpp:42-43），必须在此
    // 消费掉标记，否则通用 COM 绑定会把它编成运行期 no-op 的 vb6_ComCall。
    if (isComMarker_) {
        auto itCtrl = knownFormControls_.find(comObjExpr_);
        if (itCtrl != knownFormControls_.end() &&
            itCtrl->second == FrmControlType::PictureBox &&
            (comMemberName_ == "print" || comMemberName_ == "cls")) {
            isComMarker_ = false;
            isEarlyBoundCom_ = false;
            earlyBoundSym_ = nullptr;
            std::string method = std::move(comMemberName_);
            std::string ctrlName = cIdent(knownFormControlOriginalNames_.count(comObjExpr_) ? knownFormControlOriginalNames_[comObjExpr_] : comObjExpr_);
            comObjExpr_.clear();
            comMemberName_.clear();
            if (method == "print") {
                std::string textArg = "0";  // 裸 Print = 空行
                if (!node.positional.empty()) {
                    emitExpr(*node.positional[0]);
                    // 同 Fix 143: 实参自身是 COM 属性读时必须就地消费，
                    // 否则把对象指针当字符串画、残留标记还会被上层当独立语句发射。
                    if (isComMarker_) {
                        isComMarker_ = false;
                        isEarlyBoundCom_ = false;
                        earlyBoundSym_ = nullptr;
                        std::string objM = std::move(comObjExpr_);
                        std::string memM = std::move(comMemberName_);
                        comObjExpr_.clear();
                        comMemberName_.clear();
                        textArg = "vb6_ComGetStringProp(" + objM + ", L\"" + memM + "\")";
                    } else {
                        textArg = std::move(lastExpr_);
                    }
                }
                c_.emitLine("vb6_ControlPrint((void*)vb6_hwnd_" + ctrlName + ", " + textArg + ");  /* PictureBox.Print */");
                lastExpr_ = "0";
                return;
            }
            c_.emitLine("vb6_ControlCls((void*)vb6_hwnd_" + ctrlName + ");  /* PictureBox.Cls */");
            lastExpr_ = "0";
            return;
        }
    }