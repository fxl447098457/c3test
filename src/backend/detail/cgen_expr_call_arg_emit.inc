// cgen_expr_call_arg_emit.inc - visit(IndexOrCallExpr&) 函数体片段：位置实参发射（一）—— UDT 打包、Declare ANSI 与 ByRef 取地址（位于实参 for 循环内）
// 由 src/backend/expr/cgen_expr_call.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 1771~2136 行，逐行未改。

    for (size_t i = 0; i < node.positional.size(); i++) {
        // Fix 090q: 实参是「返回 UDT 的函数调用」→ Declare As Any ByRef 打包须传
        // 临时 UDT 地址 (void*)&(vb6_type_X){...}; 否则强转 struct 值 → C2440
        // (cZipArchive pvVfsSetEof: SetFileTime ..., pvToFileTime(...) As Any)
        std::string argUdtRetCType090q;
        if (node.positional[i] && node.positional[i]->kind == ASTNodeKind::IndexOrCallExpr) {
            auto& nc090q = static_cast<IndexOrCallExpr&>(*node.positional[i]);
            if (nc090q.callee && nc090q.callee->kind == ASTNodeKind::IdentifierExpr) {
                auto& nid090q = static_cast<IdentifierExpr&>(*nc090q.callee);
                auto it090q = funcUdtRetCType_.find(Symbol::toLower(nid090q.name));
                if (it090q != funcUdtRetCType_.end()) argUdtRetCType090q = it090q->second;
            }
        }
        emitExpr(*node.positional[i]);
      // COM属性标记残留: MsgBox dic.Count 等场景 — 参数是COM属性读取但标记未被消费
        // 统一用后期绑定(IDispatch), 避免vtable签名不匹配问题
        if (isComMarker_) {
            isComMarker_ = false;
            std::string objExpr = std::move(comObjExpr_);
            std::string memName = std::move(comMemberName_);
            if (isEarlyBoundCom_ && earlyBoundSym_) {
                isEarlyBoundCom_ = false;
                const Symbol* comSym = earlyBoundSym_;
                earlyBoundSym_ = nullptr;
                std::string memLower = memName;
                std::transform(memLower.begin(), memLower.end(), memLower.begin(), ::tolower);
                auto it = comSym->comMethods.find(memLower);
                if (it != comSym->comMethods.end() && it->second.isPropertyGet) {
                    const auto& sig = it->second;
                    std::string returnType = mapType(sig.returnType);
                    if (returnType == "int32_t" || returnType == "int16_t") {
                        lastExpr_ = "vb6_ComGetIntProp(" + objExpr + ", L\"" + memName + "\")";
                    } else if (returnType == "BSTR") {
                        lastExpr_ = "vb6_ComGetStringProp(" + objExpr + ", L\"" + memName + "\")";
                    } else if (returnType == "double" || returnType == "float") {
                        lastExpr_ = "vb6_ComGetDoubleProp(" + objExpr + ", L\"" + memName + "\")";
                    } else if (returnType == "void*") {
                        lastExpr_ = "vb6_ComGetObjectProp(" + objExpr + ", L\"" + memName + "\")";
                    } else {
                        lastExpr_ = "vb6_VariantFromComResult(vb6_ComGetProp(" + objExpr + ", L\"" + memName + "\"))";
                    }
                } else {
                    lastExpr_ = "vb6_VariantFromComResult(vb6_ComGetProp(" + objExpr + ", L\"" + memName + "\"))";
                }
            } else {
                lastExpr_ = "vb6_VariantFromComResult(vb6_ComGetProp(" + objExpr + ", L\"" + memName + "\"))";  /* P25: late-bound VARIANT */
            }
        }
        std::string argVal = std::move(lastExpr_);

        // M22: Declare ANSI函数 - ByVal String参数需要BSTR->ANSI转换
        // 生成临时char*变量, 调用后释放, 无内存泄露
        if (isDeclareAnsiCall && i < calleeParams.size() && calleeParams[i].isByVal
            && calleeParams[i].type == Vb6Type::String) {
            std::string ansiVar = "_ansi_" + std::to_string(ansiCounter_++);
            c_.emitLine("char* " + ansiVar + " = vb6_BSTR_ToANSI(" + argVal + ");");
            ansiTempsToFree_.push_back(ansiVar);
            argVal = ansiVar;
        }

        // ByRef参数: 调用点传指针. 如果实参已经是解引用形式(*x), 取地址还原为x;
        // 如果是普通变量, 加&取地址
        // Fix 072: VB6 允许 ByVal 覆盖 ByRef 声明 (如 SHCreateMemStream(ByVal 0, 0))
        bool argHasByValOverride = node.byvalOverrides.count(i) > 0;
        // Fix 078 rev2: ByRef array parameters are now vb6_SafeArray1D**,
        // so they DO need & at the call site — same as other ByRef params.
        // Removed isArrayParamType exclusion so arrays get & just like non-arrays.
        bool isByRef = (i < calleeParams.size() && !calleeParams[i].isByVal && !calleeParams[i].isParamArray)
                       && !argHasByValOverride;
        bool calleeParamIsArray = (i < calleeParams.size()
            && (static_cast<uint16_t>(calleeParams[i].type) & static_cast<uint16_t>(Vb6Type::Array)));
        if (isByRef) {
            if (argVal.size() > 3 && argVal.substr(0, 2) == "(*" && argVal.back() == ')') {
                // (*x) → depends on callee param type:
                // If callee param is array (vb6_SafeArray1D**), arg (*x) means x is already vb6_SafeArray1D**,
                // so pass x directly (not &x which would be ***).
                // If callee param is non-array, (*x) → depends on whether x is a ByRef param of current function:
                //   - If x is a current-function ByRef param (C type T*), then (*x) is T, and callee expects T*.
                //     Since x itself is already T*, pass x directly (not &x which would be T**).
                //   - Otherwise (e.g. local variable holding a pointer), (*x) → &x to restore the pointer.
                std::string innerName = argVal.substr(2, argVal.size() - 3);
                if (calleeParamIsArray) {
                    argVal = innerName;
                } else {
                    // Fix 079: check if innerName is a ByRef param of the current function
                    bool isCurrentByRefParam = false;
                    if (currentProc_) {
                        for (auto& p : currentProc_->params) {
                            if (Symbol::toLower(p.name) == Symbol::toLower(innerName) && !p.isByVal && !p.isParamArray) {
                                bool paramIsArray = (static_cast<uint16_t>(p.type) & static_cast<uint16_t>(Vb6Type::Array)) != 0;
                                if (!paramIsArray) {
                                    isCurrentByRefParam = true;
                                }
                                break;
                            }
                        }
                    }
                    if (isCurrentByRefParam) {
                        // ByRef param of current function → already T*, pass directly
                        argVal = innerName;
                    } else {
                        // Local pointer variable → restore pointer with &
                        argVal = "&" + innerName;
                    }
                }
            } else if (argVal.size() > 2 && argVal.substr(0, 2) == "me" && argVal[2] == '-') {
                // me->field → &(me->field) (类成员字段取地址)
                argVal = "&(" + argVal + ")";
            } else {
                // Fix 064: As Any (Vb6Type::Unknown) ByRef 参数 — 传 void* 指针,
                // 不包装为 VARIANT. VB6 中 As Any 表示"任意类型指针",
                // 对数组元素应传 &(VB6_SA_AT(...)) 即首元素地址.
                // UDT ByRef 参数同理 — C 函数签名已是 UDT*, 传 &argVal 即可.
                bool isAsAnyParam = (i < calleeParams.size() && calleeParams[i].type == Vb6Type::Unknown);
                bool isUdtByRefParam = (i < calleeParams.size() && calleeParams[i].type == Vb6Type::UserDefinedType);

                // Fix 064b: VB6_SA_AT(...) 数组元素作为 ByRef 实参时,
                // 不应包装为 VARIANT 复合字面量, 应直接取地址.
                // 典型: PolyPolygon(hDC, uPoints(0), aSizes(0), nCount)
                //   uPoints(0) → VB6_SA_AT(vb6_type_POINTAPI, uPoints, 0)
                //   应生成 (void*)&(VB6_SA_AT(...)) 而非 (&(vb6_VARIANT){.bstrVal=...})
                bool isSaAtExpr = (argVal.find("VB6_SA_AT(") == 0);
                if (isSaAtExpr && !isAsAnyParam && !isUdtByRefParam) {
                    // 数组元素作为 ByRef 参数: 当 calleeParams 缺失或类型不匹配时,
                    // 对 Declare 函数的 As Any 参数一律传 void*
                    if (calleeIsDeclare || i >= calleeParams.size()) {
                        isAsAnyParam = true;
                    }
                }

                if (isAsAnyParam || isUdtByRefParam) {
                    // As Any ByRef / UDT ByRef: 左值取地址, 非左值(字面量等)强转为void*
                    // Fix 069b: 对字面量(如 0)不能取地址 &(0) → C2101,
                    //   应直接强转 (void*)(intptr_t)(0).
                    bool isSimpleIdent = !argVal.empty() && (std::isalpha(static_cast<unsigned char>(argVal[0])) || argVal[0] == '_');
                    if (isSimpleIdent) {
                        for (char c : argVal) {
                            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
                                isSimpleIdent = false;
                                break;
                            }
                        }
                    }
                    // Fix 056b: UDT字段链 (uFile.Data.ftLastWriteTime) 也是左值,
                    // 作为 ByRef UDT/AsAny 实参应取地址 &(x) 而非强转 (void*)(intptr_t)(x)
                    // Fix 084m: 字段链须允许 -> — With变量展开 (._vb6_with_2->SendBuffer)
                    // 也是左值字段链, 若判为非左值会生成 (void*)(intptr_t)(udt) → C2440
                    bool isUdtFieldChain = !argVal.empty() && (std::isalpha(static_cast<unsigned char>(argVal[0])) || argVal[0] == '_');
                    if (isUdtFieldChain) {
                        for (size_t ci = 0; ci < argVal.size(); ci++) {
                            char c = argVal[ci];
                            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.') continue;
                            if (c == '-' && ci + 1 < argVal.size() && argVal[ci + 1] == '>') { ci++; continue; }
                            isUdtFieldChain = false;
                            break;
                        }
                    }
                    bool isSaAt = (argVal.find("VB6_SA_AT(") == 0);
                    // Fix 086: 宏常量/NULL 不是左值 — TLS_LOCAL_LEGACY_VERSION 是
                    // #define, &(宏) → C2101; NULL 同理.
                    bool isConstMacro = (argVal == "NULL") || isConstIdent(argVal);
                    // Fix 090k: (*uFile).BufferArray / (*uFile)->field — ByRef UDT
                    // 参数解引用的字段链也是左值 (As Any / CopyMemory 实参应取地址
                    // &(field), 而非下方 else 的 (void*)(intptr_t)(expr) 强转 —
                    // Variant 字段值强转指针 → C2440 "无法从 vb6_VARIANT 转 intptr_t").
                    bool isDerefFieldChain = false;
                    if (!argVal.empty() && argVal[0] == '(' && argVal.size() > 2 && argVal[1] == '*') {
                        size_t depth090k = 0;
                        for (size_t ci090k = 0; ci090k < argVal.size(); ci090k++) {
                            char c090k = argVal[ci090k];
                            if (c090k == '(') depth090k++;
                            else if (c090k == ')') {
                                depth090k--;
                                if (depth090k == 0) {
                                    if (ci090k + 1 < argVal.size() &&
                                        (argVal[ci090k + 1] == '.' ||
                                         (argVal[ci090k + 1] == '-' && ci090k + 2 < argVal.size()
                                          && argVal[ci090k + 2] == '>'))) {
                                        isDerefFieldChain = true;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                    bool isLValue = (isSimpleIdent || isUdtFieldChain || isSaAt || argVal.find("me->") == 0
                        || (argVal.size() > 4 && argVal[0] == '(' && argVal[1] == '*' && argVal.back() == ')')
                        || isDerefFieldChain)
                        && !isConstMacro;
                    if (isLValue) {
                        // 左值: 变量名、数组元素、me->field、(*ptr) 解引用 — 可以取地址
                        argVal = "(void*)&(" + argVal + ")";
                    } else if (!argUdtRetCType090q.empty()) {
                        // Fix 090q: 实参是返回 UDT 的函数调用 — VB6 把返回值拷入临时
                        // UDT 再传址 (As Any ByRef). C 不能直接强转 struct 值
                        // ((void*)(intptr_t)(struct) → C2440), 也不能用复合字面量
                        // 内联初始化 ((T){fnRetUdt()} MSVC C 报 C2440 "初始化…无法从
                        // T 转换") — 只能先求值到已声明临时再取址. 声明行挂 pending,
                        // 在包含本实参的语句行输出前落地 (CodeEmitter::flushPending),
                        // 保证 "声明先于引用". 例: cZipArchive pvVfsSetEof 内
                        // SetFileTime(..., pvToFileTime(dLastWriteTime)) (FILETIME).
                        std::string tmpAny090q = "_vb6_anytmp" + std::to_string(tempCounter_++);
                        c_.addPending(argUdtRetCType090q + " " + tmpAny090q
                                      + " = " + argVal + ";");
                        argVal = "(void*)&" + tmpAny090q;
                    } else if (isConstMacro && argVal != "NULL") {
                        // Fix 086: 宏常量 → 用对应类型的复合字面量承载地址
                        std::string ccType = mapType(constIdentType(argVal));
                        if (ccType.empty() || ccType == "vb6_VARIANT") ccType = "int32_t";
                        argVal = "(void*)&(" + ccType + "){" + argVal + "}";
                    } else {
                        // 非左值: 字面量或复杂表达式 — 直接强转为 void*
                        argVal = "(void*)(intptr_t)(" + argVal + ")";
                    }
                } else {
                // Fix 072b: ByVal 覆盖 As Any 参数 — 传 (void*)(intptr_t)(val),
                // 不走复合字面量路径. 典型: SHCreateMemStream(ByVal 0, 0) 中
                // pInit As Any 是 ByRef, 但 ByVal 0 覆盖为传值, 应生成 (void*)0.
                bool isAsAnyByVal = (i < calleeParams.size() && calleeParams[i].type == Vb6Type::Unknown)
                                    && argHasByValOverride;
                if (isAsAnyByVal) {
                    argVal = "(void*)(intptr_t)(" + argVal + ")";
                } else {
                // 变量/非左值 → 复合字面量取地址; 变量 → &变量
                // 检查是否是简单标识符 (变量名, 以字母/下划线开头)
                bool isSimpleIdent = !argVal.empty() && (std::isalpha(static_cast<unsigned char>(argVal[0])) || argVal[0] == '_');
                if (isSimpleIdent) {
                    for (char c : argVal) {
                        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
                            isSimpleIdent = false;
                            break;
                        }
                    }
                }
                if (isSimpleIdent && !argVal.empty()) {
                    // Fix 079: If the simple identifier is a ByRef param of the current function,
                    // it's already a pointer (T*) — pass directly, don't add &.
                    bool argIsCurrentByRefParam = false;
                    if (currentProc_) {
                        for (auto& p : currentProc_->params) {
                            if (Symbol::toLower(p.name) == Symbol::toLower(argVal) && !p.isByVal && !p.isParamArray) {
                                argIsCurrentByRefParam = true;
                                break;
                            }
                        }
                    }
                    if (argIsCurrentByRefParam) {
                        // ByRef param of current function → already T*, pass directly
                    } else if (isConstIdent(argVal)) {
                        // Fix 084aa: 常量是 #define 宏 (vb6_BSTR_FromStr(...) 或数值字面量),
                        // 取址 &ERR_X 展开为函数结果取址 → C2102. 按形参类型复合字面量包装.
                        Vb6Type pType = Vb6Type::Variant;
                        if (i < calleeParams.size()) pType = calleeParams[i].type;
                        argVal = wrapConstArgForByRef(argVal, pType);
                    } else if (argVal == "NULL" || argVal == "nullptr") {
                        // Fix 092l: NULL/nullptr 字面量不可取址 — &NULL 触发
                        // C2101 "常量上的 &" (cTlsSocket 440:
                        //   FireOnCertificate(me, &NULL) → 应为 (&(vb6_VARIANT){0})).
                        // 按形参类型改用 C11 复合字面量.
                        std::string ct092l = "vb6_VARIANT";
                        if (i < calleeParams.size()) ct092l = mapType(calleeParams[i].type);
                        argVal = "(&(" + ct092l + "){0})";
                    } else {
                        argVal = "&" + argVal;
                    }
                } else {
                    // 字面量或复杂表达式: 使用C11复合字面量
                    // &(int32_t){10} 或 &(double){3.14}
                    std::string cType = "int32_t";
                    if (i < calleeParams.size()) {
                        cType = mapType(calleeParams[i].type);
                        // Fix 092e: ByRef 数组形参 (如 cToolsUtf8.Decode(ByRef
                        // Utf() As Byte)) 的 C 形参是 vb6_SafeArray1D** (Fix 078
                        // rev2), 复合字面量类型必须一致 — mapType(Byte|Array)
                        // 只给出 "uint8_t*" → (&(uint8_t*){Variant}) C2440
                        // "无法从 vb6_VARIANT 转换为 uint8_t *"
                        // (cHttpClient 418: ToolsUtf8.Decode(Inst.ResponseBody)).
                        bool isArrP092e = (static_cast<uint16_t>(calleeParams[i].type)
                                           & static_cast<uint16_t>(Vb6Type::Array)) != 0;
                        if (isArrP092e && !calleeParams[i].isByVal
                            && !calleeParams[i].isParamArray) {
                            cType = "vb6_SafeArray1D**";
                        }
                    }
                    // M22: VARIANT类型需要指定字段初始化 (.vt=VT_xxx, .field=value)
                    if (cType == "vb6_VARIANT") {
                        // 推断实参的VB6类型来决定VARIANT字段
                        Vb6Type argVbType = inferExprType(*node.positional[i]);
                        switch (argVbType) {
                            case Vb6Type::String:
                                // Fix 027b: 外层加括号让预处理器把 {a,b} 内的逗号视为同一参数
                                // (避免宏调用时 #define RtlCopyMemory(D,s,l) memcpy(D,s,l) 把复合字面量
                                //  内部的逗号错算成宏实参分隔符 → C4002 参数过多)
                                argVal = "(&(vb6_VARIANT){.vt=VT_BSTR, .bstrVal=" + argVal + "})";
                                break;
                            case Vb6Type::Long:
                            case Vb6Type::Integer:
                                argVal = "(&(vb6_VARIANT){.vt=VT_I4, .lVal=(int32_t)(" + argVal + ")})";
                                break;
                            case Vb6Type::Double:
                            case Vb6Type::Single:
                                argVal = "(&(vb6_VARIANT){.vt=VT_R8, .dblVal=(double)(" + argVal + ")})";
                                break;
                            case Vb6Type::Boolean:
                                argVal = "(&(vb6_VARIANT){.vt=VT_BOOL, .boolVal=(int16_t)(" + argVal + ")})";
                                break;
                            case Vb6Type::Byte:
                                argVal = "(&(vb6_VARIANT){.vt=VT_UI1, .bVal=(uint8_t)(" + argVal + ")})";
                                break;
                            case Vb6Type::Date:
                                argVal = "(&(vb6_VARIANT){.vt=VT_DATE, .dblVal=(double)(" + argVal + ")})";
                                break;
                            default:
                                // Fix 054: Variant 或未知类型
                                // 1. 实参是 UDT 的 Variant 字段 (With 成员如 .SourceFile,
                                //    .FileName 等 As Variant) → 值已是 vb6_VARIANT,
                                //    ByRef Variant 形参直接取地址 &(...), 不要包装成
                                //    {.vt=VT_BSTR, .bstrVal=...} (Variant→BSTR C2440).
                                // 2. C 表达式已是 Variant (vb6_VariantArrayGet 等),
                                //    提取 BSTR 值到 .bstrVal (避免 VARIANT→BSTR C2440)
                                // 3. 其他未知类型保持原样 (UDT 等单独处理)
                                if (i < node.positional.size()
                                    && inferUdtFieldVb6Type(node.positional[i].get()) == Vb6Type::Variant) {
                                    argVal = "&(" + argVal + ")";
                                } else if (cExprIsVariant(argVal)) {
                                    argVal = "(&(vb6_VARIANT){.vt=VT_BSTR, .bstrVal=vb6_VariantToString(" + argVal + ")})";
                                } else {
                                    argVal = "(&(vb6_VARIANT){.vt=VT_BSTR, .bstrVal=" + argVal + "})";
                                }
                                break;
                        }
                    } else {
                        // Fix 090d: ByRef 具体类型形参 + 实参是 vb6_VARIANT 值 —
                        // 复合字面量初始化 (&(double){vb6_VARIANT}) 触发 C2440
                        // (vb6_VARIANT→double/BSTR/... 初始化失败). 按 cType 先提取.
                        // 例: cHttpServerCookies Encode: FormatHttpDate(CK.Expires)
                        //     (形参 ByRef Date, Expires As Variant 属性 →
                        //     vb6_cHttpServerCookieAttr_prop_get_Expires 返回 vb6_VARIANT);
                        //     cHttpServerResponse File: ComputeETag(fileInfo.DateLastModified,
                        //     ...) (COM 属性 → vb6_VariantFromComResult(...) vb6_VARIANT).
                        bool argIsVariant090d = isDefinitelyVariantExpr(*node.positional[i]);
                        if (!argIsVariant090d) argIsVariant090d = cExprIsVariant(argVal);
                        if (argIsVariant090d) {
                            if (cType == "double" || cType == "float") {
                                argVal = "vb6_VariantToDouble(" + argVal + ")";
                            } else if (cType == "BSTR") {
                                argVal = "vb6_VariantToString(" + argVal + ")";
                            } else if (cType == "int32_t" || cType == "int16_t"
                                       || cType == "uint8_t" || cType == "int64_t"
                                       || cType == "LONG") {
                                argVal = "vb6_VariantToLong(" + argVal + ")";
                            } else if (cType == "vb6_SafeArray1D*"
                                       || cType == "vb6_SafeArray1D**") {
                                argVal = "vb6_VariantToSafeArray1D(" + argVal + ")";
                            } else if (cType == "void*") {
                                argVal = "vb6_VariantToObjectVal(" + argVal + ")";
                            }
                        }
                        // Fix 027b: 同样加外层括号防止预处理器把复合字面量 `{a, b}` 内的逗号算成宏实参分隔符.
                        argVal = "(&(" + cType + "){" + argVal + "})";
                    }
                }
                } // end Fix 072b else (As Any ByVal override)
                    } // end Fix 064 else
            }
        } // Fix 024 P2: ByVal Variant 参数 — 调用点用 vb6_VariantFromValue 包装实参.