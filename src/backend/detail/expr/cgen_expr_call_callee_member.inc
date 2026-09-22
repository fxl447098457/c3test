// cgen_expr_call_callee_member.inc - visit(IndexOrCallExpr&) 函数体片段：Fix 037 MemberAccessExpr callee（Pattern A / B / C / D）
// 由 src/backend/expr/cgen_expr_call.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 806~1009 行，逐行未改。

    // ---- Fix 037: MemberAccessExpr callee with positional args — array field/COM dispatch ----
    // 处理 obj.field(idx) 模式, field 可能是:
    //   - UDT 数组字段 (固定/动态)     — Pattern A/B → obj.member[idx] / VB6_SA_AT(...)
    //   - 类 Variant 字段持有 SafeArray — Pattern C → vb6_VariantArrayGet(&obj->member, idx)
    //   - 类 void* 字段持有 COM 对象    — Pattern D → vb6_ComCall(obj->member, L"Item", args, argc)
    // 若 member 是该类的方法/属性, resolveClassMemberCall 返回非空 → 不拦截 (正常函数调用路径).
    // 不拦截则 fallback path (line ~2158) 把 field 误当函数调用 → C2064.

    // ---- Fix 158a: MemberAccessExpr callee 带 **空括号** — UDT 动态/固定数组成员
    // 整体引用 obj.member() → 裸数组表达式 obj.member (不带调用括号).
    // 此前 fallback 把它当函数调用 emit 成 obj.member() → C2064 (项不会计算为接受
    // N 参数的函数), 或作为 ByRef 数组实参被包裹成 &(obj.member()) (VBFlexGrid.ctl
    // SortGrid: Call MergeSortRec(..., VBFlexGridCells.Rows(), Sort) — ByRef
    // Data() As TCOLS, C 形参 vb6_SafeArray1D**). 与 Pattern A/B 相同判定
    // (inferUdtTypeOfExpr + udtMembers + isArrayDynamic / arraySize>0), 但空括号
    // 不消费 positional. 固定数组成员 (Pattern A) 空括号引用同样走这里:
    // C 侧固定数组字段是静态数组 obj.member[], 整体引用即裸名 obj.member.
    if (node.callee && node.callee->kind == ASTNodeKind::MemberAccessExpr
        && node.positional.empty() && node.named.empty()) {
        auto& maExpr158 = static_cast<MemberAccessExpr&>(*node.callee);
        std::string memLower158 = Symbol::toLower(maExpr158.memberName);
        if (maExpr158.object) {
            std::string udtCType158 = inferUdtTypeOfExpr(*maExpr158.object);
            const std::string prefix158 = "vb6_type_";
            if (udtCType158.size() > prefix158.size()
                && udtCType158.compare(0, prefix158.size(), prefix158) == 0) {
                std::string udtName158 = udtCType158.substr(prefix158.size());
                Symbol* udtSym158 = symTab_.lookupModule(udtName158);
                if (udtSym158 && udtSym158->kind == SymbolKind::UserDefinedType) {
                    for (auto& mi158 : udtSym158->udtMembers) {
                        if (Symbol::toLower(mi158.name) == memLower158) {
                            if (mi158.isArrayDynamic || mi158.arraySize > 0) {
                                emitExpr(*node.callee);
                                return;
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    if (node.callee && node.callee->kind == ASTNodeKind::MemberAccessExpr
        && !node.positional.empty() && node.named.empty()) {
        auto& maExpr = static_cast<MemberAccessExpr&>(*node.callee);
        std::string memLower = Symbol::toLower(maExpr.memberName);
        std::string memLowerM = "m_" + memLower;
        bool handled = false;

        // ---- Pattern A/B: UDT 数组字段 ----
        // 仅当 maExpr.object 的 UDT 类型可推断 (IdentifierExpr / WithMemberExpr / 嵌套
        // MemberAccessExpr). inferUdtTypeOfExpr 找到 UDT C 类型 → 在 udtMembers 中查 member.
        if (maExpr.object) {
            std::string udtCType = inferUdtTypeOfExpr(*maExpr.object);
            if (!udtCType.empty()) {
                const std::string prefix = "vb6_type_";
                if (udtCType.size() > prefix.size()
                    && udtCType.compare(0, prefix.size(), prefix) == 0) {
                    std::string udtName = udtCType.substr(prefix.size());
                    Symbol* udtSym = symTab_.lookupModule(udtName);
                    if (udtSym && udtSym->kind == SymbolKind::UserDefinedType) {
                        for (auto& mi : udtSym->udtMembers) {
                            if (Symbol::toLower(mi.name) == memLower) {
                                if (mi.arraySize > 0) {
                                    // Pattern A: 固定大小数组成员 → obj.member[idx]
                                    emitExpr(*node.callee);  // emits "obj.member"
                                    std::string fieldExpr = std::move(lastExpr_);
                                    emitExpr(*node.positional[0]);
                                    std::string idx = std::move(lastExpr_);
                                    lastExpr_ = fieldExpr + "[" + idx + "]";
                                    handled = true;
                                } else if (mi.isArrayDynamic) {
                                    // Pattern B: 动态数组成员 → VB6_SA_AT(elemType, obj.member, idx)
                                    emitExpr(*node.callee);
                                    std::string fieldExpr = std::move(lastExpr_);
                                    emitExpr(*node.positional[0]);
                                    std::string idx = std::move(lastExpr_);
                                    // Fix 092s: UDT 元素类型成员数组须用 vb6_type_<UDT>
                                    // (mapSaElemCType 对 UserDefinedType 退化为 vb6_VARIANT
                                    // → 元素大小/取址全错, cTlsSocket 4086/4093)
                                    std::string elemCType = (mi.type == Vb6Type::UserDefinedType
                                                             && !mi.typeRefName.empty())
                                        ? ("vb6_type_" + cIdent(mi.typeRefName))
                                        : mapSaElemCType(mi.type);
                                    lastExpr_ = "VB6_SA_AT(" + elemCType + ", "
                                              + fieldExpr + ", " + idx + ")";
                                    handled = true;
                                } else if (mi.type == Vb6Type::Variant) {
                                    // Pattern J: UDT Variant 字段 (持有 SafeArray) →
                                    // vb6_VariantArrayGet(&obj.field, idx)
                                    emitExpr(*node.callee);  // emits "obj.field"
                                    std::string fieldExpr = std::move(lastExpr_);
                                    emitExpr(*node.positional[0]);
                                    // Fix 084o: 索引为 Variant 时转 Long
                                    std::string idx = toLongIfVariant(std::move(lastExpr_), node.positional[0].get());
                                    lastExpr_ = "vb6_VariantArrayGet(&" + fieldExpr + ", " + idx + ")";
                                    handled = true;
                                } else if (mi.type == Vb6Type::Object) {
                                    // Pattern H: UDT void* (Object) 字段 (持有 COM 对象) →
                                    // vb6_ComCall(obj.field, L"Item", args, argc)
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
                                    lastExpr_ = "vb6_ComCall(" + objExpr + ", L\"Item\", "
                                              + argsArray + ", " + std::to_string(argc) + ")";
                                    handled = true;
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }

        // ---- Pattern C/D: 类实例字段访问 ----
        if (!handled && maExpr.object) {
            std::string className = inferClassTypeOfExpr(*maExpr.object);
            if (!className.empty()) {
                // resolveClassMemberCall 验证 member 是否为方法/属性:
                // 是 → 跳过 (正常函数调用 fallback 处理); 否 → 数据字段 → 检查 Variant/void*
                std::string resolvedFn = resolveClassMemberCall(className, maExpr.memberName);
                if (resolvedFn.empty()) {
                    // Pattern C: Variant 字段 → vb6_VariantArrayGet(&obj->member, idx)
                    if (classVariantMembers_.count(memLower)
                        || classVariantMembers_.count(memLowerM)) {
                        emitExpr(*node.callee);  // emits "obj->member  /* class var .X field */"
                        std::string fieldExpr = std::move(lastExpr_);
                        emitExpr(*node.positional[0]);
                        // Fix 084o: 索引为 Variant 时转 Long
                        std::string idx = toLongIfVariant(std::move(lastExpr_), node.positional[0].get());
                        lastExpr_ = "vb6_VariantArrayGet(&" + fieldExpr + ", " + idx + ")";
                        handled = true;
                    } else {
                        // Pattern D: void* COM 字段 → vb6_ComCall(obj->member, L"Item", args, argc)
                        bool isVoidPtr = false;
                        if (classVoidFieldMap_) {
                            auto itV = classVoidFieldMap_->find(className);
                            if (itV != classVoidFieldMap_->end()) {
                                if (itV->second.count(memLower)
                                    || itV->second.count(memLowerM)) {
                                    isVoidPtr = true;
                                }
                            }
                        }
                        if (isVoidPtr) {
                            // 模仿 line 1880-1938 COM 默认属性调用 (comPackExpr/resolveComMarkerForPack)
                            emitExpr(*node.callee);  // emits "obj->member  /* ... voidptr */"
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
                            // 默认成员名 "Item" — VB6 Collection / ADODB.Recordset 等大多数
                            // void* 字段都是用 .Item(idx) 索引的 (DISPID_VALUE 默认成员).
                            lastExpr_ = "vb6_ComCall(" + objExpr + ", L\"Item\", "
                                      + argsArray + ", " + std::to_string(argc) + ")";
                            handled = true;
                        } else if (classTypedFieldMap_) {
                            // Fix 037b: typed (非 void*) 对象字段 — 项目类或 COM 接口
                            auto itT = classTypedFieldMap_->find(className);
                            if (itT != classTypedFieldMap_->end()) {
                                auto itF = itT->second.find(memLower);
                                if (itF == itT->second.end()) itF = itT->second.find(memLowerM);
                                if (itF != itT->second.end()) {
                                    const std::string& fieldType = itF->second;
                                    if (fieldType.compare(0, 4, "COM:") == 0) {
                                        // Pattern K: COM 接口字段 (如 Header As Dictionary)
                                        // → vb6_ComCall((void*)obj->member, L"Item", args, argc)
                                        emitExpr(*node.callee);
                                        std::string objExpr = std::move(lastExpr_);
                                        std::vector<std::string> packedArgs;
                                        for (size_t i = 0; i < node.positional.size(); i++) {
                                            std::string packFn = comPackExpr(*node.positional[i]);
                                            emitExpr(*node.positional[i]);  // Fix: Pattern K 实参丢失 (漏 emit 致 vb6_ComPackXxx() 零实参), 与 Pattern D 对齐

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
                                        // → vb6_<Type>_prop_get_Item(obj->member, vb6_VariantFromValue(arg))
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
        }

        if (handled) return;
    }
