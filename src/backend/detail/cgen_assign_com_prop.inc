// cgen_assign_com_prop.inc - visit(AssignmentStmt&) 函数体片段：参数化属性与 COM 写（P25/P25b / 类内 Property Let / COM SetProp / 返回值解封）
// 由 src/backend/stmt/cgen_assign.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 829~1055 行，逐行未改。

    // ---- P25b: 链式 COM 默认属性索引赋值 ---- (实现提取为
    // tryEmitChainedComWrite, 090ae; AssignmentStmt/SetStmt 共用)
    if (tryEmitChainedComWrite(node.target.get(), node.value.get())) {
        return;
    }

    // P25: 参数化COM属性赋值检测: dic.Item(key) = value → vb6_ComSetPropArg
    if (node.target->kind == ASTNodeKind::IndexOrCallExpr) {
        auto& callTarget = static_cast<IndexOrCallExpr&>(*node.target);
        if (callTarget.callee && callTarget.callee->kind == ASTNodeKind::MemberAccessExpr) {
            auto& maTarget = static_cast<MemberAccessExpr&>(*callTarget.callee);
            // 检查对象是否是COM变量
            if (maTarget.object && maTarget.object->kind == ASTNodeKind::IdentifierExpr) {
                auto& objId = static_cast<IdentifierExpr&>(*maTarget.object);
                std::string objLower = objId.name;
                std::transform(objLower.begin(), objLower.end(), objLower.begin(), ::tolower);
                bool isComVar = knownObjectVars_.count(objLower) || knownVariantVars_.count(objLower);
                // 也检查前期绑定COM变量
                if (!isComVar) {
                    isComVar = knownTypedComVars_.count(objLower) > 0;
                }
                if (isComVar && !callTarget.positional.empty()) {
                    // 生成: vb6_ComSetPropArg(obj, L"Prop", {pack(arg1),...}, argc, pack(value))
                    emitExpr(*maTarget.object);
                    std::string objExpr = lastExpr_;
                    std::string memberName = maTarget.memberName;
                    // 打包索引参数
                    std::vector<std::string> packedIdxArgs;
                    for (size_t i = 0; i < callTarget.positional.size(); i++) {
                        std::string packFn = comPackExpr(*callTarget.positional[i]);
                        emitExpr(*callTarget.positional[i]);
                        { std::string resolved = resolveComMarkerForPack(packFn); if (!resolved.empty()) lastExpr_ = resolved; }
                        packedIdxArgs.push_back(packFn + "(" + lastExpr_ + ")");
                    }
                    int32_t idxArgc = (int32_t)packedIdxArgs.size();
                    std::string idxArgsArray = "(void*[]){";
                    for (int i = 0; i < idxArgc; i++) {
                        if (i > 0) idxArgsArray += ", ";
                        idxArgsArray += packedIdxArgs[i];
                    }
                    idxArgsArray += "}";
                    // 打包值参数并生成调用
                    emitExpr(*node.value);
                    std::string valExpr = std::move(lastExpr_);
                    std::string packFn = comPackExpr(*node.value);
                    c_.emitLine("vb6_ComSetPropArg(" + objExpr + ", L\"" + memberName + "\", " +
                                 idxArgsArray + ", " + std::to_string(idxArgc) + ", " +
                                 packFn + "(" + valExpr + "));  /* COM SetPropArg */");
                    return;
                }
            }
        }
    }

    // Fix 092u: 当前类内**参数化** Property Let/Set 赋值 —
    //   Extend(Array(A, C)) = Split(...)     (cToolsArray.cls 97, Sub test 内隐式 me)
    // 此前落到通用路径: emitExpr(target) 生成裸名 + 补的 value 占位
    // "Extend(&_arr_1, &(vb6_VARIANT){0})", 再拼 " = <RHS>" → C2106 ("=" 左侧必须
    // 是左值, ToolsArray.c 130). 正确形态是按**写方向**形参表生成
    //   vb6_cToolsArray_prop_let_Extend(me, &Vars, &Value);
    if (node.target->kind == ASTNodeKind::IndexOrCallExpr && isClassModule_) {
        auto& plCall092u = static_cast<IndexOrCallExpr&>(*node.target);
        if (plCall092u.callee && plCall092u.callee->kind == ASTNodeKind::IdentifierExpr
            && !plCall092u.positional.empty() && plCall092u.named.empty()) {
            auto& plId092u = static_cast<IdentifierExpr&>(*plCall092u.callee);
            const std::string plLower092u = Symbol::toLower(plId092u.name);
            // 局部变量 / 当前过程名同名 → 不是属性 (同 642 段规则)
            bool plSkip092u = knownLocalVars_.count(plLower092u) > 0;
            if (!plSkip092u && currentProc_) {
                plSkip092u = (Symbol::toLower(currentProc_->name) == plLower092u);
            }
            if (!plSkip092u) {
                std::vector<ParameterInfo> wp092u;
                bool isSet092u = false;
                bool found092u = findClassMemberWriteParams(moduleName_, plId092u.name,
                                                            false, wp092u);
                Symbol* plSym092u = nullptr;
                if (found092u) {
                    plSym092u = symTab_.lookupModuleByKind(plId092u.name,
                                                           SymbolKind::PropertyLet);
                } else {
                    found092u = findClassMemberWriteParams(moduleName_, plId092u.name,
                                                           true, wp092u);
                    if (found092u) {
                        isSet092u = true;
                        plSym092u = symTab_.lookupModuleByKind(plId092u.name,
                                                               SymbolKind::PropertySet);
                    }
                }
                // 形参数须恰好比括号实参多 1 (末参是 value); 且**不含 Optional 形参** —
                // Optional 在 C 侧另有存在标志参数 (cAsyncSocket 2611:
                // Property Let ThunkPrivateData(pThunk, Optional ByVal Index, ByVal lValue)
                // 的 C 签名是 (me, IUnknown**, int32_t, int32_t, int)), 本路径只按符号表
                // 拼参会 C2198 → 这类仍交给原有 prop_get_ 重写路径 (Pattern C/D2) 处理.
                bool hasOptional092u = false;
                for (const auto& pOpt092u : wp092u) {
                    if (pOpt092u.isOptional) { hasOptional092u = true; break; }
                }
                if (found092u && plSym092u && !hasOptional092u
                    && wp092u.size() == plCall092u.positional.size() + 1) {
                    const std::string tmp092u = "_pv" + std::to_string(tempCounter_++) + "_";
                    std::string args092u;
                    int slot092u = 0;
                    auto pushArg092u = [&](Expr* argNode, const ParameterInfo& pi) {
                        emitExpr(*argNode);
                        std::string argExpr = std::move(lastExpr_);
                        std::string one092u;
                        if (pi.type == Vb6Type::Variant) {
                            if (pi.isByVal) {
                                one092u = "vb6_VariantFromValue(" + argExpr + ")";
                            } else {
                                // ByRef Variant 需可寻址: 先落到栈变量再取址
                                // (复合字面量 {Variant值} 触发 C2440)
                                std::string tv = tmp092u + std::to_string(slot092u++);
                                c_.emitLine("vb6_VARIANT " + tv + " = vb6_VariantFromValue("
                                            + argExpr + ");");
                                one092u = "&" + tv;
                            }
                        } else if (pi.type == Vb6Type::String && cExprIsVariant(argExpr)) {
                            one092u = wrapToBSTR(argExpr, *argNode);
                        } else if (!pi.isByVal) {
                            // ByRef 非 Variant 形参: 简单标识符直接取址; 函数结果 /
                            // 字段链等非左值先落栈变量再取址 — 否则 &<非左值> C2102
                            // (cAsyncSocket 2395: &vb6_BSTR_Concat(...)).
                            bool simple092u = !argExpr.empty()
                                && ((argExpr[0] >= 'a' && argExpr[0] <= 'z')
                                    || (argExpr[0] >= 'A' && argExpr[0] <= 'Z')
                                    || argExpr[0] == '_')
                                && argExpr.find('(') == std::string::npos
                                && argExpr.find("->") == std::string::npos
                                && argExpr.find('.') == std::string::npos
                                && argExpr.find(' ') == std::string::npos;
                            if (simple092u) {
                                one092u = "&" + argExpr;
                            } else {
                                std::string tv = tmp092u + std::to_string(slot092u++);
                                c_.emitLine(mapType(pi.type) + " " + tv + " = " + argExpr + ";");
                                one092u = "&" + tv;
                            }
                        } else {
                            one092u = argExpr;
                        }
                        if (!args092u.empty()) args092u += ", ";
                        args092u += one092u;
                    };
                    for (size_t ai = 0; ai < plCall092u.positional.size(); ai++) {
                        pushArg092u(plCall092u.positional[ai].get(), wp092u[ai]);
                    }
                    pushArg092u(node.value.get(), wp092u.back());
                    c_.emitLine(cProcName((isSet092u ? "prop_set_" : "prop_let_")
                                              + plId092u.name,
                                          plSym092u->access, moduleName_)
                                + "((void*)me, " + args092u
                                + ");  /* Property Let/Set (param, own class) */");
                    return;
                }
            }
        }
    }

    emitExpr(*node.target);
    std::string target = std::move(lastExpr_);

    // COM属性赋值检测 (P6.2): obj.Property = value → vb6_ComSetProp(obj, L"Property", pack(value))
    if (isComMarker_) {
        isComMarker_ = false;
        std::string objExpr = std::move(comObjExpr_);
        std::string memberName = std::move(comMemberName_);
        emitExpr(*node.value);
        std::string valExpr = std::move(lastExpr_);
        std::string packFn = comPackExpr(*node.value);
        c_.emitLine("vb6_ComSetProp(" + objExpr + ", L\"" + memberName + "\", " +
                     packFn + "(" + valExpr + "));  /* COM SetProp */");
        return;
    }

    emitExpr(*node.value);
    // COM属性值: 如果右侧是COM属性, 根据目标变量类型解析为适当C类型
    if (isComMarker_) {
        // 推断目标变量类型, 用于COM值解封
        std::string unpackHint;
        if (node.target->kind == ASTNodeKind::IdentifierExpr) {
            auto& idExpr = static_cast<IdentifierExpr&>(*node.target);
            std::string lower = idExpr.name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            // 从已知变量集合推断类型
            if (knownLongVars_.count(lower)) unpackHint = "Long";
            else if (knownDoubleVars_.count(lower)) unpackHint = "Double";
            else if (knownObjectVars_.count(lower)) unpackHint = "Object";
            else if (knownBstrVars_.count(lower)) unpackHint = "BSTR";
            else if (knownVariantVars_.count(lower)) unpackHint = "Variant";
        }
        resolveComValue(unpackHint);
    }
    std::string value = std::move(lastExpr_);

    // P6.12: COM方法调用返回值类型化解封
    // vb6_ComCall 返回 VARIANT* (void*), 赋值给typed变量时需用对应解封函数
    // vb6_ComCall(obj, L"Method", args, argc) → vb6_ComCallInt/Double/BSTR/Object(...)
    if (value.find("vb6_ComCall(") == 0) {
        std::string callArgs = value.substr(strlen("vb6_ComCall"));
        // 推断目标变量类型
        std::string comCallType;
        if (node.target->kind == ASTNodeKind::IdentifierExpr) {
            auto& idExpr = static_cast<IdentifierExpr&>(*node.target);
            std::string lower = idExpr.name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (knownLongVars_.count(lower)) comCallType = "Int";
            else if (knownDoubleVars_.count(lower)) comCallType = "Double";
            else if (knownBstrVars_.count(lower)) comCallType = "BSTR";
            else if (knownObjectVars_.count(lower)) comCallType = "Object";
        }
        if (!comCallType.empty()) {
            value = "vb6_ComCall" + comCallType + callArgs;
        }
    }

    // Fix 010r-15+010r-16 (Pattern A/C/D2/F): LHS 是非左值 COM 调用或 Property Get,
    // 改写为对应的 COM SetProp/SetPropArg 或 prop_let_/prop_set_ 调用.
    // 覆盖模式:
    //   A: vb6_ComCall(obj, L"Item", args, n) = value → vb6_ComSetPropArg(...)
    //   F: vb6_ComGetStringProp(obj, L"Prop") = value → vb6_ComSetProp(...)
    //   C/D2: vb6_X_prop_get_Y(args) = value → vb6_X_prop_let_Y(args, value)
    if (tryRewriteCOMLvalue(target, value, node.value.get(), /*isSet=*/false)) {
        return;
    }
