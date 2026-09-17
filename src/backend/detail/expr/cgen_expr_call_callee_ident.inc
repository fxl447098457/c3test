// cgen_expr_call_callee_ident.inc - visit(IndexOrCallExpr&) 函数体片段：Fix 037 IdentifierExpr callee（Pattern F / F2 / G）
// 由 src/backend/expr/cgen_expr_call.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 621~805 行，逐行未改。

    // ---- Fix 037: IdentifierExpr callee — implicit Me.field / ByRef Object param / local Object var ----
    // 处理 name(idx) 模式 (无显式 obj. 前缀), name 可能是:
    //   Pattern G: 类模块隐式 Me.field (void*/Variant) — VB6 在类模块内省略 Me. 时
    //              codegen 会 emit me->field, 但 field 是数据字段不是函数 → C2064
    //              → vb6_ComCall(me->field, L"Item", ...) / vb6_VariantArrayGet(&me->field, idx)
    //   Pattern F: ByRef Object 参数 (*name)(idx) — VB6 Items(idx) 其中 Items 是 ByRef Object
    //              参数, C 中是 void** → (*Items) 是 void*, 不可调用 → C2064
    //              → vb6_ComCall((*name), L"Item", args, argc)
    //   Pattern F2: 局部 Object 变量 (Dim x As Object) — knownObjectVars_ 后期绑定
    //              → vb6_ComCall(name, L"Item", args, argc)
    // 检查顺序: Pattern F (参数优先, 遮蔽类字段) > Pattern G (类字段) > Pattern F2 (局部 Object)
    if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr
        && !node.positional.empty() && node.named.empty()) {
        auto& idExpr = static_cast<IdentifierExpr&>(*node.callee);
        std::string nameLower = Symbol::toLower(idExpr.name);
        std::string nameMLower = "m_" + nameLower;
        bool handled = false;

        // ---- Pattern F: Object 参数 (ByRef 或 ByVal) ----
        // currentProc_->params 中查找同名参数, 非数组, 非ParamArray, 类型为 Object/Variant.
        // ByRef Object 在 C 中是 void** → emitExpr emit "(*name)"; ByVal Object 是 void* → emit "name".
        if (!handled && currentProc_) {
            for (auto& p : currentProc_->params) {
                if (Symbol::toLower(p.name) == nameLower) {
                    bool isArray = (static_cast<uint16_t>(p.type) & static_cast<uint16_t>(Vb6Type::Array)) != 0;
                    if (!isArray && !p.isParamArray
                        && (p.type == Vb6Type::Object || p.type == Vb6Type::Variant)) {
                        emitExpr(*node.callee);  // emits "(*name)" for ByRef or "name" for ByVal
                        std::string objExpr = std::move(lastExpr_);
                        std::vector<std::string> packedArgs;
                        for (size_t i = 0; i < node.positional.size(); i++) {
                            std::string packFn = comPackExpr(*node.positional[i]);
                            emitExpr(*node.positional[i]);
                            { std::string resolved = resolveComMarkerForPack(packFn);
                              if (!resolved.empty()) lastExpr_ = resolved; }
                            packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
                        }
                        int32_t argc = (int32_t)packedArgs.size();
                        std::string argsArray = "(void*[]){";
                        for (int i = 0; i < argc; i++) {
                            if (i > 0) argsArray += ", ";
                            argsArray += packedArgs[i];
                        }
                        argsArray += "}";
                        lastExpr_ = "vb6_ComCall(" + objExpr + ", L\"Item\", "
                                  + argsArray + ", " + std::to_string(argc) + ")";
                        handled = true;
                    }
                    break;  // 参数匹配即终止 (无论是否 handled)
                }
            }
        }

        // ---- Pattern G: 隐式 Me.field (类模块上下文) ----
        // isClassModule_ && name 不是局部变量 && 不是参数 → 类数据字段.
        // emitExpr(*node.callee) 会 emit "me->field" (+ Dim As New 自动实例化守卫).
        if (!handled && isClassModule_ && currentProc_
            && !knownLocalVars_.count(nameLower)) {
            // 确认 name 不是当前过程的参数 (Pattern F 已处理)
            bool isParam = false;
            for (auto& p : currentProc_->params) {
                if (Symbol::toLower(p.name) == nameLower) { isParam = true; break; }
            }
            if (!isParam) {
                // Variant 字段 → vb6_VariantArrayGet(&me->field, idx)
                if (classVariantMembers_.count(nameLower) || classVariantMembers_.count(nameMLower)) {
                    emitExpr(*node.callee);  // emits "me->field"
                    std::string fieldExpr = std::move(lastExpr_);
                    emitExpr(*node.positional[0]);
                    // Fix 084o: 索引为 Variant 时转 Long
                    std::string idx = toLongIfVariant(std::move(lastExpr_), node.positional[0].get());
                    lastExpr_ = "vb6_VariantArrayGet(&" + fieldExpr + ", " + idx + ")";
                    handled = true;
                } else {
                    // void* COM 字段 → vb6_ComCall(me->field, L"Item", args, argc)
                    bool isVoidPtr = false;
                    if (classVoidFieldMap_) {
                        auto itV = classVoidFieldMap_->find(moduleName_);
                        if (itV != classVoidFieldMap_->end()) {
                            if (itV->second.count(nameLower) || itV->second.count(nameMLower)) {
                                isVoidPtr = true;
                            }
                        }
                    }
                    if (isVoidPtr) {
                        emitExpr(*node.callee);  // emits "me->field" (+ auto-instantiate guard)
                        std::string objExpr = std::move(lastExpr_);
                        std::vector<std::string> packedArgs;
                        for (size_t i = 0; i < node.positional.size(); i++) {
                            std::string packFn = comPackExpr(*node.positional[i]);
                            emitExpr(*node.positional[i]);
                            { std::string resolved = resolveComMarkerForPack(packFn);
                              if (!resolved.empty()) lastExpr_ = resolved; }
                            packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
                        }
                        int32_t argc = (int32_t)packedArgs.size();
                        std::string argsArray = "(void*[]){";
                        for (int i = 0; i < argc; i++) {
                            if (i > 0) argsArray += ", ";
                            argsArray += packedArgs[i];
                        }
                        argsArray += "}";
                        lastExpr_ = "vb6_ComCall(" + objExpr + ", L\"Item\", "
                                  + argsArray + ", " + std::to_string(argc) + ")";
                        handled = true;
                    } else if (classTypedFieldMap_) {
                        // Fix 037b: typed (非 void*) 对象字段 — 项目类或 COM 接口
                        auto itT = classTypedFieldMap_->find(moduleName_);
                        if (itT != classTypedFieldMap_->end()) {
                            auto itF = itT->second.find(nameLower);
                            if (itF == itT->second.end()) itF = itT->second.find(nameMLower);
                            if (itF != itT->second.end()) {
                                const std::string& fieldType = itF->second;
                                if (fieldType.compare(0, 4, "COM:") == 0) {
                                    // Pattern K: COM 接口字段 (如 Header As Dictionary)
                                    // → vb6_ComCall((void*)me->field, L"Item", args, argc)
                                    emitExpr(*node.callee);
                                    std::string objExpr = std::move(lastExpr_);
                                    std::vector<std::string> packedArgs;
                                    for (size_t i = 0; i < node.positional.size(); i++) {
                                        std::string packFn = comPackExpr(*node.positional[i]);
                                        emitExpr(*node.positional[i]);
                                        { std::string resolved = resolveComMarkerForPack(packFn);
                                          if (!resolved.empty()) lastExpr_ = resolved; }
                                        packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
                                    }
                                    int32_t argc = (int32_t)packedArgs.size();
                                    std::string argsArray = "(void*[]){";
                                    for (int i = 0; i < argc; i++) {
                                        if (i > 0) argsArray += ", ";
                                        argsArray += packedArgs[i];
                                    }
                                    argsArray += "}";
                                    lastExpr_ = "vb6_ComCall((void*)" + objExpr + ", L\"Item\", "
                                              + argsArray + ", " + std::to_string(argc) + ")";
                                    handled = true;
                                } else if (node.positional.size() == 1) {
                                    // Pattern L: 项目类字段 (如 Rows As cCollection)
                                    // → vb6_<Type>_prop_get_Item(me->field, vb6_VariantFromValue(arg))
                                    std::string itemFn = resolveClassMemberCall(fieldType, "Item");
                                    if (!itemFn.empty()) {
                                        emitExpr(*node.callee);
                                        std::string objExpr = std::move(lastExpr_);
                                        emitExpr(*node.positional[0]);
                                        std::string arg = std::move(lastExpr_);
                                        lastExpr_ = itemFn + "(" + objExpr + ", vb6_VariantFromValue(" + arg + "))";
                                        lastExprNeedsObjectUnpack_ = true;  // Set 语句需转 void*
                                        handled = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // ---- Pattern F2: 局部 Object 变量 (后期绑定) ----
        // knownObjectVars_: Dim x As Object → C 中 x 是 void*, x(idx) 应为 vb6_ComCall(x, ...).
        if (!handled && knownObjectVars_.count(nameLower)) {
            emitExpr(*node.callee);  // emits "name"
            std::string objExpr = std::move(lastExpr_);
            std::vector<std::string> packedArgs;
            for (size_t i = 0; i < node.positional.size(); i++) {
                std::string packFn = comPackExpr(*node.positional[i]);
                emitExpr(*node.positional[i]);
                { std::string resolved = resolveComMarkerForPack(packFn);
                  if (!resolved.empty()) lastExpr_ = resolved; }
                packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
            }
            int32_t argc = (int32_t)packedArgs.size();
            std::string argsArray = "(void*[]){";
            for (int i = 0; i < argc; i++) {
                if (i > 0) argsArray += ", ";
                argsArray += packedArgs[i];
            }
            argsArray += "}";
            lastExpr_ = "vb6_ComCall(" + objExpr + ", L\"Item\", "
                      + argsArray + ", " + std::to_string(argc) + ")";
            handled = true;
        }

        if (handled) return;
    }
