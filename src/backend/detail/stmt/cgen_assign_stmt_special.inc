// cgen_assign_stmt_special.inc - visit(AssignmentStmt&) 函数体片段：语句特殊形态（Date$/Time$ / Mid$ / LSet-RSet / 控件属性 / 跨模块变量）
// 由 src/backend/stmt/cgen_assign.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 12~405 行，逐行未改。

    if (!node.target || !node.value) return;
    // P22: Date$/Time$ statement interception (Date$ = "12-31-2025" / Time$ = "23:59:59")
    if (node.target->kind == ASTNodeKind::IdentifierExpr) {
        auto& _tgtId22 = static_cast<IdentifierExpr&>(*node.target);
        std::string _tgtLower22 = _tgtId22.name;
        std::transform(_tgtLower22.begin(), _tgtLower22.end(), _tgtLower22.begin(), ::tolower);
        if (_tgtLower22.size() > 1 && _tgtLower22.back() == '$') _tgtLower22.pop_back();
        if (_tgtLower22 == "date") {
            emitExpr(*node.value);
            c_.emitLine("vb6_DateSet(" + wrapToBSTR(lastExpr_, *node.value) + ");  /* Date$ = ... */");
            return;
        }
        if (_tgtLower22 == "time") {
            emitExpr(*node.value);
            c_.emitLine("vb6_TimeSet(" + wrapToBSTR(lastExpr_, *node.value) + ");  /* Time$ = ... */");
            return;
        }
    }
    // Fix 010r-14 (Pattern B): Mid$ statement — `Mid$(var, start[, len]) = value`
    // 当 Mid$ 被词法器吃成单一 Identifier "Mid$" 时, 解析器无法走 TokenKind::Mid 分支,
    // 会落到 parseLabelOrAssignmentOrCall(), 把 LHS 当作普通函数调用 (IndexOrCallExpr).
    // 在 codegen 层面识别此模式并改写为 vb6_MidSet(&var, start, len, value); 调用
    // (与 visit(MidStmt) 等价, 仅适用于 AssignmentStmt fallback 路径).
    if (node.target->kind == ASTNodeKind::IndexOrCallExpr) {
        auto& call = static_cast<IndexOrCallExpr&>(*node.target);
        if (call.callee && call.callee->kind == ASTNodeKind::IdentifierExpr) {
            auto& calleeId = static_cast<IdentifierExpr&>(*call.callee);
            std::string calleeLower = calleeId.name;
            std::transform(calleeLower.begin(), calleeLower.end(), calleeLower.begin(), ::tolower);
            // 兼容 "mid$" 和 "mid" 两种写法
            if (calleeLower == "mid$" || calleeLower == "mid") {
                if (call.positional.size() >= 2 && call.positional.size() <= 3) {
                    // 1. 发出 target (字符串变量)
                    emitExpr(*call.positional[0]);
                    std::string midTarget = std::move(lastExpr_);
                    // 2. 发出 start
                    emitExpr(*call.positional[1]);
                    std::string midStart = std::move(lastExpr_);
                    // 3. 发出 length (可选, 缺省为 0 表示 "到字符串末尾")
                    std::string midLen = "0";
                    if (call.positional.size() >= 3) {
                        emitExpr(*call.positional[2]);
                        midLen = std::move(lastExpr_);
                    }
                    // 4. 发出 value (RHS)
                    emitExpr(*node.value);
                    std::string midValue = std::move(lastExpr_);
                    // Fix 086: 与 visit(MidStmt) 的 Fix 084b 一致 — Variant值
                    // (vb6_VariantArrayGet 等) 需转 BSTR, 否则 C2440
                    if (cExprIsVariant(midValue)) {
                        midValue = "vb6_VariantToString(" + midValue + ")";
                    }
                    c_.emitLine("vb6_MidSet(&" + midTarget + ", " + midStart + ", " + midLen + ", " + midValue + ");  /* Mid$ statement */");
                    return;
                }
            }
        }
    }
    // Fix 110b: 控件数组元素默认属性写入 — VB6 里 `TxtARGB(0) = v` 等价于
    // `TxtARGB(0).Text = v` (TextBox 控件数组元素的默认属性写入).
    // 不处理时 LHS 会被展开为 `vb6_GetControlText(vb6_hwnd_TxtARGB)(0)` →
    // C2064/C2106. 正确形态: vb6_SetControlText(vb6_CtrlArr_GetAt(&vb6_arr_X, i), v).
    if (node.target->kind == ASTNodeKind::IndexOrCallExpr) {
        auto& caP = static_cast<IndexOrCallExpr&>(*node.target);
        if (caP.callee && caP.callee->kind == ASTNodeKind::IdentifierExpr
            && !caP.positional.empty()) {
            auto& caId = static_cast<IdentifierExpr&>(*caP.callee);
            std::string caLower = caId.name;
            std::transform(caLower.begin(), caLower.end(), caLower.begin(), ::tolower);
            if (knownControlArrays_.count(caLower)) {
                auto itCtrl = knownFormControls_.find(caLower);
                if (itCtrl != knownFormControls_.end()) {
                    const char* defProp = getDefaultPropertyName(itCtrl->second);
                    std::string writeFn = defProp
                        ? getControlPropWriteFn(itCtrl->second, defProp) : std::string();
                    if (!writeFn.empty()) {
                        emitExpr(*caP.positional[0]);
                        std::string idxArg = std::move(lastExpr_);
                        emitExpr(*node.value);
                        std::string valExpr = std::move(lastExpr_);
                        if (writeFn.find("SetControlText") != std::string::npos
                            || writeFn.find("SetMenuCaption") != std::string::npos) {
                            valExpr = wrapToBSTR(valExpr, *node.value);
                        }
                        std::string origName = knownFormControlOriginalNames_.count(caLower)
                            ? knownFormControlOriginalNames_[caLower] : caId.name;
                        c_.emitLine(writeFn + "(vb6_CtrlArr_GetAt(&vb6_arr_" + cIdent(origName)
                                    + ", " + idxArg + "), " + valExpr
                                    + ");  /* ctrl array default prop write */");
                        return;
                    }
                }
            }
        }
    }

    // P22: LSet/RSet statement (LSet strVar = expr / RSet strVar = expr)
    if (node.isLSet || node.isRSet) {
        if (node.target->kind == ASTNodeKind::IdentifierExpr) {
            auto& tgtId = static_cast<IdentifierExpr&>(*node.target);
            std::string tgtC = cIdent(tgtId.name);
            // P22-10: UDT LSet — memory copy between UDTs
            std::string tgtLower = tgtId.name;
            std::transform(tgtLower.begin(), tgtLower.end(), tgtLower.begin(), ::tolower);
            auto udtIt = knownUdtVars_.find(tgtLower);
            if (udtIt != knownUdtVars_.end()) {
                // Target is a UDT: LSet/RSet copies raw memory (truncated to target size)
                emitExpr(*node.value);
                std::string srcExpr = lastExpr_;
                std::string udtCType = udtIt->second;
                c_.emitLine("memcpy(&" + tgtC + ", &" + srcExpr + ", sizeof(" + udtCType + "));  /* LSet UDT */");
                return;
            }
            // String LSet/RSet
            emitExpr(*node.value);
            std::string valExpr = wrapToBSTR(lastExpr_, *node.value);
            // 定长字符串使用固定长度而非SysStringLen
            std::string strLen;
            auto fsIt = knownFixedStringLen_.find(tgtLower);
            if (fsIt != knownFixedStringLen_.end()) {
                strLen = fsIt->second;
            } else {
                strLen = "SysStringLen(" + tgtC + ")";
            }
            if (node.isLSet) {
                c_.emitLine("vb6_BSTR_Assign(&" + tgtC + ", vb6_LSet(" + valExpr + ", " + strLen + "));  /* LSet */");
            } else {
                c_.emitLine("vb6_BSTR_Assign(&" + tgtC + ", vb6_RSet(" + valExpr + ", " + strLen + "));  /* RSet */");
            }
            return;
        }
    }
    // P7.5+P7.6: 控件属性写入
    // 情况1: ctrl.Property = value (非数组)
    // 情况2: ctrlArr(idx).Property = value (数组)
    if (node.target->kind == ASTNodeKind::MemberAccessExpr) {
        auto& maExpr = static_cast<MemberAccessExpr&>(*node.target);
        // P18-C: Printer.CurrentX / Printer.CurrentY 赋值
        if (maExpr.object && maExpr.object->kind == ASTNodeKind::IdentifierExpr) {
            auto& objId = static_cast<IdentifierExpr&>(*maExpr.object);
            std::string objName = objId.name;
            std::transform(objName.begin(), objName.end(), objName.begin(), ::tolower);
            std::string memName = maExpr.memberName;
            std::transform(memName.begin(), memName.end(), memName.begin(), ::tolower);
            if (objName == "printer") {
                emitExpr(*node.value);
                if (memName == "currentx") { c_.emitLine("vb6_Printer_SetCurrentX((int32_t)(" + lastExpr_ + "));"); return; }
                if (memName == "currenty") { c_.emitLine("vb6_Printer_SetCurrentY((int32_t)(" + lastExpr_ + "));"); return; }
            }
        }

        // M22: Me.Property = expr — 窗体模块中Me的属性赋值 (如 Me.Caption = myName)
        if (maExpr.object && (maExpr.object->kind == ASTNodeKind::MeExpr ||
                              (maExpr.object->kind == ASTNodeKind::IdentifierExpr &&
                               static_cast<IdentifierExpr&>(*maExpr.object).name.size() == 2 &&
                               (static_cast<IdentifierExpr&>(*maExpr.object).name[0] == 'M' || static_cast<IdentifierExpr&>(*maExpr.object).name[0] == 'm') &&
                               (static_cast<IdentifierExpr&>(*maExpr.object).name[1] == 'e' || static_cast<IdentifierExpr&>(*maExpr.object).name[1] == 'E')))) {
            if (isFormModule_ && !knownFormName_.empty()) {
                std::string memLower = maExpr.memberName;
                std::transform(memLower.begin(), memLower.end(), memLower.begin(), ::tolower);
                std::string formHwnd = "vb6_hwnd_" + moduleName_;
                auto origIt = knownFormControlOriginalNames_.find(knownFormName_);
                if (origIt != knownFormControlOriginalNames_.end()) {
                    formHwnd = "vb6_hwnd_" + cIdent(origIt->second);
                }
                // Form properties: Caption, Text, Visible, etc.
                std::string writeFn = getControlPropWriteFn(FrmControlType::Form, maExpr.memberName);
                if (!writeFn.empty()) {
                    emitExpr(*node.value);
                    std::string valExpr = std::move(lastExpr_);
                    // M22: Text/Caption property writes need BSTR value
                    if (writeFn.find("SetControlText") != std::string::npos ||
                        writeFn.find("SetMenuCaption") != std::string::npos) {
                        valExpr = wrapToBSTR(valExpr, *node.value);
                    }
                    c_.emitLine(writeFn + "(" + formHwnd + ", " + valExpr + ");  /* Me." + maExpr.memberName + " */");
                    return;
                }
            }
        }
        // P7.6: 控件数组属性写入 ctrlArr(idx).Property = value
        if (maExpr.object && maExpr.object->kind == ASTNodeKind::IndexOrCallExpr) {
            auto& idxExpr = static_cast<IndexOrCallExpr&>(*maExpr.object);
            if (idxExpr.callee && idxExpr.callee->kind == ASTNodeKind::IdentifierExpr) {
                auto& arrId = static_cast<IdentifierExpr&>(*idxExpr.callee);
                std::string arrLower = arrId.name;
                std::transform(arrLower.begin(), arrLower.end(), arrLower.begin(), ::tolower);
                if (knownControlArrays_.count(arrLower)) {
                    auto itCtrl = knownFormControls_.find(arrLower);
                    if (itCtrl != knownFormControls_.end()) {
                        std::string writeFn = getControlPropWriteFn(itCtrl->second, maExpr.memberName);
                        if (!writeFn.empty()) {
                            emitExpr(*node.value);
                            std::string valExpr = std::move(lastExpr_);
                            std::string idxArg;
                            if (!idxExpr.positional.empty()) {
                                emitExpr(*idxExpr.positional[0]);
                                idxArg = std::move(lastExpr_);
                                // Re-emit value because we just clobbered lastExpr_
                                emitExpr(*node.value);
                                valExpr = std::move(lastExpr_);
                            } else {
                                idxArg = "0";
                            }
                            c_.emitLine(writeFn + "(vb6_CtrlArr_GetAt(&vb6_arr_" + cIdent(arrId.name) + ", " + idxArg + "), " + valExpr + ");  /* Control Array Property */");
                            return;
                        }
                    }
                }
            }
        }
        // P7.5: 非数组控件属性写入 ctrl.Property = value
        if (maExpr.object && maExpr.object->kind == ASTNodeKind::IdentifierExpr) {
            auto& objId = static_cast<IdentifierExpr&>(*maExpr.object);
            std::string objLower = objId.name;
            std::transform(objLower.begin(), objLower.end(), objLower.begin(), ::tolower);
            // P16: WithEvents控件属性写入
            {
                auto itWECtrl = knownWithEventsCtrlVars_.find(objLower);
                if (itWECtrl != knownWithEventsCtrlVars_.end()) {
                    std::string writeFn = getControlPropWriteFn(itWECtrl->second, maExpr.memberName);
                    if (!writeFn.empty()) {
                        auto itOrig = knownWithEventsCtrlOrigNames_.find(objLower);
                        std::string weVarName = (itOrig != knownWithEventsCtrlOrigNames_.end()) ? itOrig->second : objLower;
                        emitExpr(*node.value);
                        std::string valExpr = std::move(lastExpr_);
                        c_.emitLine(writeFn + "(" + weVarName + ", " + valExpr + ");  /* WithEvents ctrl prop write */");
                        return;
                    }
                }
            }
            auto itCtrl = knownFormControls_.find(objLower);
            if (itCtrl != knownFormControls_.end()) {
                std::string writeFn = getControlPropWriteFn(itCtrl->second, maExpr.memberName);
                if (!writeFn.empty()) {
                    emitExpr(*node.value);
                    // ImageList Picture fix: resolve COM marker on RHS before using value
                    // e.g. Picture2.Picture = ImageList1.ListImages(i).Picture
                    // Bug #3 fix: also detect vb6_ComIface_Picture* function return (e.g. QRCodegenBarcode)
                    bool rhsIsComPicture = false;
                    if (isComMarker_) {
                        if (comMemberName_ == "Picture" || comMemberName_ == "picture") {
                            rhsIsComPicture = true;
                            resolveComValue("Object");
                        } else {
                            resolveComValue();
                        }
                    } else if (lastExprIsComPicture_) {
                        rhsIsComPicture = true;
                        lastExprIsComPicture_ = false;  // consume the flag
                    }
                    std::string valExpr = std::move(lastExpr_);
                    // M22: Text/Caption property writes need BSTR value
                    if (writeFn.find("SetControlText") != std::string::npos ||
                        writeFn.find("SetMenuCaption") != std::string::npos) {
                        valExpr = wrapToBSTR(valExpr, *node.value);
                    }
                    // ImageList Picture fix: use SetControlPictureFromCom for COM IPictureDisp
                    if (rhsIsComPicture && writeFn.find("SetControlPicture") != std::string::npos) {
                        writeFn = "vb6_SetControlPictureFromCom";
                    }
                    c_.emitLine(writeFn + "(" + makeCtrlHwndArg(objLower, itCtrl->second) + ", " + valExpr + ");  /* Control Property */");
                    return;
                }
                diag_.warn(DiagnosticID::CodeGenUnsupportedFeature, SourceLocation{},
                    std::string("P7.5: Unknown control property write '") + objId.name + "." + maExpr.memberName +
                    "' for control type, generating struct field access (may not compile)");
            }
        }
    }

    // M22: 跨模块变量赋值 Module1.myName = expr → vb6_BSTR_Assign(&vb6_Module1_myName, expr)
    if (node.target->kind == ASTNodeKind::MemberAccessExpr) {
        auto& maExpr = static_cast<MemberAccessExpr&>(*node.target);
        if (maExpr.object && maExpr.object->kind == ASTNodeKind::IdentifierExpr) {
            auto& objIdent = static_cast<IdentifierExpr&>(*maExpr.object);
            std::string objLower = objIdent.name;
            std::transform(objLower.begin(), objLower.end(), objLower.begin(), ::tolower);
            // Check if object is NOT a known variable → assume module name
            auto* objSym = symTab_.lookup(objIdent.name);
            if (!objSym) objSym = symTab_.lookupModule(objIdent.name);
            bool isVarName = (objSym && (objSym->kind == SymbolKind::Variable || objSym->kind == SymbolKind::Parameter));
            // M22-fix: 补充检查cgen层跟踪集合，防止过程级UDT/类变量被误判为模块名
            if (!isVarName && knownUdtVars_.count(objLower)) isVarName = true;
            // Fix 086: Variant局部变量 (For Each Ctl In o) 也是合法的赋值目标对象 —
            // 成员写入走COM后期绑定 (vb6_ComSetProp), 而非 Module.X 回退 (C2065)
            if (!isVarName && knownVariantVars_.count(objLower)) isVarName = true;
            if (!isVarName && knownClassVars_.find(objLower) != knownClassVars_.end()) isVarName = true;
            if (!isVarName && knownNewVars_.count(objLower)) isVarName = true;
            if (!isVarName && knownObjectVars_.count(objLower)) isVarName = true;
            if (!isVarName && knownTypedComVars_.count(objLower)) isVarName = true;
            if (!isVarName && knownIfaceVars_.count(objLower)) isVarName = true;

            // Fix 084o-4: 函数名引用返回 UDT 的字段赋值 ("FuncName.Field = expr").
            // VB6 函数体内可用函数名访问返回 UDT 的字段 (如 pvVfsOpen.FileName = STR_BUFFER).
            // 目标展开为 vb6_ret_FuncName.Field (与读取侧 cgen_expr.cpp:203 一致),
            // 而非误当 Module 变量展开为 vb6_FuncName_Field (未声明 + 类型错误).
            if (!isVarName && objSym && objSym->kind == SymbolKind::Function && !objSym->isExternal) {
                std::string curLower084o = currentProc_ ? currentProc_->name : std::string();
                std::transform(curLower084o.begin(), curLower084o.end(), curLower084o.begin(), ::tolower);
                // Fix 084o-5: 函数返回类实例时的字段赋值 ("FuncName.Field = expr",
                // 如 Set ConnInst.Conn = Me.Conn) → vb6_ret_FuncName->Field = expr,
                // 而非误当 Module 变量生成 vb6_FuncName_Field (C2065 未声明).
                bool isUdtRet084o = (currentReturnCType_.rfind("vb6_type_", 0) == 0);
                bool isClsRet084o = !isUdtRet084o
                    && currentReturnCType_.find("vb6_cls_") != std::string::npos;
                if (!curLower084o.empty() && objLower == curLower084o
                    && currentReturnVar_.size() > 8
                    && (isUdtRet084o || isClsRet084o)) {
                    // 查返回 UDT 的字段类型 (与 inferUdtFieldVb6Type 相同的 udtMembers 查询)
                    std::string udtName084o = isUdtRet084o ? currentReturnCType_.substr(9) : std::string();
                    Symbol* udtSym084o = isUdtRet084o ? symTab_.lookupModule(udtName084o) : nullptr;
                    Vb6Type fldType084o = Vb6Type::Unknown;
                    if (udtSym084o && udtSym084o->kind == SymbolKind::UserDefinedType) {
                        std::string memLower084o = maExpr.memberName;
                        std::transform(memLower084o.begin(), memLower084o.end(), memLower084o.begin(), ::tolower);
                        for (auto& mi : udtSym084o->udtMembers) {
                            if (Symbol::toLower(mi.name) == memLower084o) { fldType084o = mi.type; break; }
                        }
                    }
                    std::string fieldAccess084o = currentReturnVar_
                        + (isUdtRet084o ? "." : "->") + cIdent(maExpr.memberName);
                    emitExpr(*node.value);
                    std::string valExpr084o = std::move(lastExpr_);
                    Vb6Type valType084o = inferExprType(*node.value);
                    bool valIsVariant084o = (valType084o == Vb6Type::Variant) || cExprIsVariant(valExpr084o);
                    switch (fldType084o) {
                        case Vb6Type::String:
                            if (valIsVariant084o) valExpr084o = "vb6_VariantToString(" + valExpr084o + ")";
                            c_.emitLine("vb6_BSTR_Assign(&" + fieldAccess084o + ", " + valExpr084o + ");  /* FnRetUdt." + maExpr.memberName + " */");
                            return;
                        case Vb6Type::Long: case Vb6Type::Integer: case Vb6Type::Byte:
                        case Vb6Type::Boolean: case Vb6Type::ULong: case Vb6Type::LongPtr:
                            if (valIsVariant084o) valExpr084o = "vb6_VariantToLong(" + valExpr084o + ")";
                            c_.emitLine(fieldAccess084o + " = " + valExpr084o + ";  /* FnRetUdt." + maExpr.memberName + " */");
                            return;
                        case Vb6Type::Double: case Vb6Type::Single:
                        case Vb6Type::Currency: case Vb6Type::Date:
                            if (valIsVariant084o) valExpr084o = "vb6_VariantToDouble(" + valExpr084o + ")";
                            c_.emitLine(fieldAccess084o + " = " + valExpr084o + ";  /* FnRetUdt." + maExpr.memberName + " */");
                            return;
                        case Vb6Type::Variant:
                            c_.emitLine("vb6_VariantClear(&" + fieldAccess084o + ");");
                            c_.emitLine(fieldAccess084o + " = vb6_VariantFromValue(" + valExpr084o + ");  /* FnRetUdt." + maExpr.memberName + " */");
                            return;
                        default:
                            c_.emitLine(fieldAccess084o + " = " + valExpr084o + ";  /* FnRetUdt." + maExpr.memberName + " */");
                            return;
                    }
                }
                // Fix 086: 函数名持有对象时的成员赋值 (json_ParseObject.CompareMode = 1,
                // json_ParseObject 是返回 Dictionary 的 Function). 此前误判为
                // Module.X → 生成未声明的 vb6_<fn>_<member> (C2065).
                // 此处处于 FnRetUdt 条件块内: objLower==当前过程名 且 kind 是
                // Function/PropertyGet, 仅需按返回C类型分流.
                if (currentReturnCType_.rfind("vb6_cls_", 0) == 0) {
                    emitExpr(*node.value);
                    std::string valExpr = std::move(lastExpr_);
                    c_.emitLine(currentReturnVar_ + "->" + cIdent(maExpr.memberName)
                                + " = " + valExpr + ";  /* FnRetObj." + maExpr.memberName + " */");
                    return;
                }
                // COM对象/void* 函数返回值 → COM后期绑定属性赋值
                emitExpr(*node.value);
                std::string valExpr = std::move(lastExpr_);
                std::string packFn = comPackExpr(*node.value);
                c_.emitLine("vb6_ComSetProp((void*)" + currentReturnVar_ + ", L\""
                            + maExpr.memberName + "\", " + packFn + "(" + valExpr
                            + "));  /* FnRetObj COM SetProp */");
                return;
            }

            // Fix 086: UDT返回属性的字段赋值 (CurLang.Index = 0) — VB6语义是修改
            // 属性返回的临时副本 (赋值被丢弃). 生成 属性调用.字段 的void表达式,
            // 而非 Module.X 回退 (C2065: vb6_CurLang_Index).
            if (!isVarName) {
                Symbol* propSym86 = symTab_.lookupModule(objIdent.name);
                if (propSym86 && propSym86->kind == SymbolKind::PropertyGet
                    && !propSym86->variableTypeName.empty()) {
                    Symbol* udtSym86 = lookupDotted(propSym86->variableTypeName);
                    if (udtSym86 && udtSym86->kind == SymbolKind::UserDefinedType) {
                        emitExpr(*node.value);  // RHS 副作用保留
                        std::string propFn86 = cProcName("prop_get_" + objIdent.name,
                            propSym86->access,
                            propSym86->isExternal ? propSym86->sourceModule : "");
                        std::string propCall86 = propFn86 + "("
                            + (isClassModule_ ? "(void*)me" : "") + ")";
                        c_.emitLine("(void)(" + propCall86 + "." + cIdent(maExpr.memberName)
                                    + ");  /* UDT prop copy assign (VB6 discards) */");
                        return;
                    }
                }
            }

            if (!isVarName && !knownFormControls_.count(objLower)) {
                // Module.varName = expr
                // When module is #included, use unprefixed name
                std::string modName = objIdent.name;
                std::string varName = cIdent(maExpr.memberName);
                std::string modLower = modName;
                std::transform(modLower.begin(), modLower.end(), modLower.begin(), ::tolower);
                bool isIncluded = false;
                for (const auto& extMod : externalModules_) {
                    std::string extLower = extMod;
                    std::transform(extLower.begin(), extLower.end(), extLower.begin(), ::tolower);
                    if (extLower == modLower) { isIncluded = true; break; }
                }
                std::string memberAccess = isIncluded ? varName : ("vb6_" + cIdent(modName) + "_" + varName);
                emitExpr(*node.value);
                std::string valExpr = std::move(lastExpr_);
                std::string memLower = maExpr.memberName;
                std::transform(memLower.begin(), memLower.end(), memLower.begin(), ::tolower);
                // M22: Check if cross-module variable is BSTR type
                // knownBstrVars_ only has current module's BSTR vars, so also check symbol table
                bool isBstrVar = knownBstrVars_.count(memLower) > 0;
                if (!isBstrVar) {
                    Symbol* memSym = symTab_.lookupModule(maExpr.memberName);
                    if (!memSym) memSym = symTab_.lookup(maExpr.memberName);
                    if (memSym && memSym->kind == SymbolKind::Variable && memSym->type == Vb6Type::String) {
                        isBstrVar = true;
                    }
                }
                if (isBstrVar) {
                    c_.emitLine("vb6_BSTR_Assign(&" + memberAccess + ", " + valExpr + ");  /* Module." + maExpr.memberName + " */");
                } else {
                    c_.emitLine(memberAccess + " = " + valExpr + ";  /* Module." + maExpr.memberName + " */");
                }
                return;
            }
        }
    }
