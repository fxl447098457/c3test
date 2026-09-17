// cgen_expr_call_arg_variant.inc - visit(IndexOrCallExpr&) 函数体片段：位置实参发射（二）—— ByVal Variant 包装与 Variant 提取（位于实参 for 循环内）
// 由 src/backend/expr/cgen_expr_call.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 2137~2453 行，逐行未改。

        // 处理 callee 声明 "ByVal x As Variant" 而实参是标量/BSTR/SafeArray/class ptr 等情形.
        // _Generic 在编译期按实类型选择 ctor: 标量->VariantLong/Int/Double, BSTR->String,
        // SafeArray1D*->Array, void*/class ptr->Object, vb6_VARIANT->Identity(no-op).
        // 仅对 ByVal Variant 生效 (ByRef Variant 走上面复合字面量路径, 取地址需左值).
        if (!isByRef && i < calleeParams.size() && calleeParams[i].isByVal
            && calleeParams[i].type == Vb6Type::Variant) {
            // Fix 084d: 实参若是类对象表达式 (如 me->Database, 类方法返回类对象),
            // 保持对象指针直传, 不要套 vb6_VariantFromValue — 符号表可能把类类型
            // 参数误记为 Variant, 若包装成 vb6_VARIANT 传给 vb6_cls_cXxx* 参数
            // 会触发 C2440 (如 cHttpServer.c LoadFromDatabase(..., me->Database)).
            bool argIsClassObj = false;
            if (i < node.positional.size()) {
                std::string acName091s = inferClassTypeOfExpr(*node.positional[i]);
                if (!acName091s.empty()) {
                    // Fix 091s: 只有"项目类"(SymbolKind::Class) 实参才跳过打包
                    // (084d 的类指针直传语义). Object/COM 类实参 (void*) 传给
                    // ByVal Variant 形参时仍需 vb6_VariantFromValue 包装 —
                    // Demo 857/859: Debug.Print ToolsJsonVba.ConvertToJson(Json)
                    // (Json As Object; ConvertToJson 第 1 参 ByVal JsonValue As
                    // Variant) 原样传 void* → C2440 (void* → vb6_VARIANT).
                    auto* acSym091s = symTab_.lookup(acName091s);
                    argIsClassObj = (acSym091s && acSym091s->kind == SymbolKind::Class);
                }
            }
            if (!argIsClassObj) {
                argVal = "vb6_VariantFromValue(" + argVal + ")";
            } else if (i < node.positional.size()
                       && node.positional[i]->kind == ASTNodeKind::IdentifierExpr) {
                // Fix 091s2: 实参是 Object/COM 变量 (void*) 而形参是 ByVal Variant
                // → 仍需打包 (inferClassTypeOfExpr 对 Object 变量可能返回内建
                // "Object" 类名而误判为项目类实参). Demo 857/859:
                // ToolsJsonVba.ConvertToJson(Json) (Json As Object).
                std::string on091s2 = Symbol::toLower(
                    static_cast<IdentifierExpr&>(*node.positional[i]).name);
                if (knownObjectVars_.count(on091s2) || knownTypedComVars_.count(on091s2)) {
                    argVal = "vb6_VariantFromValue(" + argVal + ")";
                }
            }
        }
        // Fix 029: 反向强制 — ByVal 具体类型参数 + 实参确定为 Variant: 自动调用提取函数.
        // 与 Fix 024 P2 互补: P2 处理 V(callee)=Variant,V(arg)=scalar; 此处处理
        // V(callee)=concrete,V(arg)=Variant. 覆盖 C2440 子类 to_int32/to_BSTR/to_SafeArray.
        // 严格判断: 用 isDefinitelyVariantExpr 避免 inferExprType 默认回退到 Variant
        // 造成对内置函数 (LenB 等) / UDT 字段访问 (uAddr.sin_addr) 的错误包装.
        if (!isByRef && i < calleeParams.size() && calleeParams[i].isByVal
            && calleeParams[i].type != Vb6Type::Variant) {
            // 处理 Array 标志位 (例如 Variant() 参数对应 Vb6Type::Variant|Array)
            bool paramIsArray = (static_cast<uint16_t>(calleeParams[i].type)
                                  & static_cast<uint16_t>(Vb6Type::Array)) != 0;
            Vb6Type paramBase = static_cast<Vb6Type>(
                static_cast<uint16_t>(calleeParams[i].type)
                & ~static_cast<uint16_t>(Vb6Type::Array));
            bool argIsVariantArr = false;
            bool argIsVariant = isDefinitelyVariantExpr(*node.positional[i], &argIsVariantArr);
            // Fix 038b-2: 字符串级 Variant 检测回退 — 补充 isDefinitelyVariantExpr
            // 无法识别的 C 级 Variant 表达式 (vb6_VariantArrayGet, vb6_VariantFromComResult 等)
            if (!argIsVariant && !argIsVariantArr) {
                argIsVariant = cExprIsVariant(argVal);
            }
            if (argIsVariant || argIsVariantArr) {
                // Fix 090c1: 实参顶层是 COM 结果 (void* 承载 Variant) 且判为
                // Variant — 提取前先 vb6_VariantFromComResult (同 090c0).
                // cLogs MakeLogContent: Join(CacheDatas(i), Span) 的 CacheDatas(i)
                // = vb6_ComCall(...) → 原生成 VariantToSafeArray1D(ComCall) C2440.
                bool isComResultVal2 =
                    (argVal.find("vb6_ComCall(") == 0)
                    || (argVal.find("vb6_ComGetProp(") == 0)
                    || (argVal.find("vb6_ComGetObjectProp(") == 0);
                if (isComResultVal2) {
                    argVal = "vb6_VariantFromComResult(" + argVal + ")";
                }
                // Fix 091k: 实参是当前过程的 ParamArray 参数时, C 侧已是 SAFEARRAY*
                // (cgen 参数映射), 不能再按 Variant 数组提取 SafeArray1D* — 否则
                // UBound(ParamArray) 生成 vb6_PA_UBound(vb6_VariantToSafeArray1D(OutVars))
                // C2440 (cToolsArray DeArray).
                bool argIsPA091k = false;
                if (i < node.positional.size()
                    && node.positional[i]->kind == ASTNodeKind::IdentifierExpr && currentProc_) {
                    std::string paLower091k = Symbol::toLower(
                        static_cast<IdentifierExpr&>(*node.positional[i]).name);
                    for (auto& p091k : currentProc_->params) {
                        if (Symbol::toLower(p091k.name) == paLower091k) {
                            argIsPA091k = p091k.isParamArray;
                            break;
                        }
                    }
                }
                // Fix 092g: 实参是字面量数组 (Array(...) → 匿名 _arr_N) — C 侧
                // 本身已是 vb6_SafeArray1D* (由 vb6_ArraySetBSTR 逐个填充),
                // 不能再按 Variant 数组提取 (提取函数形参是 vb6_VARIANT) → C2440:
                //   cPLI 56: Cmd = Join(Array(a, b, c), " ")
                //     → vb6_Join(vb6_VariantToSafeArray1D(_arr_0), ...).
                bool argIsLiteralArr092g = (argVal.compare(0, 5, "_arr_") == 0);
                if (paramIsArray && !argIsPA091k && !argIsLiteralArr092g
                    && (paramBase == Vb6Type::Variant || paramBase == Vb6Type::Byte
                    || paramBase == Vb6Type::String || paramBase == Vb6Type::Long)) {
                    // 数组参数: 从 Variant 提取 SafeArray1D*
                    argVal = "vb6_VariantToSafeArray1D(" + argVal + ")";
                } else if (paramBase == Vb6Type::Long || paramBase == Vb6Type::Integer
                           || paramBase == Vb6Type::Byte || paramBase == Vb6Type::Boolean) {
                    argVal = "vb6_VariantToLong(" + argVal + ")";
                } else if (paramBase == Vb6Type::Double || paramBase == Vb6Type::Single
                           || paramBase == Vb6Type::Currency || paramBase == Vb6Type::Date) {
                    // Fix 090r: Date 形参 (OLE date = double) 与 Currency 同取
                    // vb6_VariantToDouble — 缺 Date 导致实参是 Variant 数组元素
                    // (vb6_VariantArrayGet) 时裸传 → C2440 VARIANT→double
                    // (cZipArchive pvVfsOpen: pvToFileTime(me, arr(i)) 形参 ByVal Date)
                    argVal = "vb6_VariantToDouble(" + argVal + ")";
                } else if (paramBase == Vb6Type::String) {
                    // Fix 049b/092a: 仅当 C 表达式**顶层**已是 BSTR 时跳过提取.
                    // 旧实现用子串 find("vb6_BSTR") — 对"内部实参含
                    // vb6_BSTR_FromStr(...)"的 Variant 表达式天然命中 → 整体跳过
                    // 提取 → C2440 (vb6_VARIANT→BSTR):
                    //   cAesCBC 25 StrConv(LoadResData("AES.CBC","JSCRIPT"), 64, 0)
                    //   → argVal = vb6_LoadResData(vb6_BSTR_FromStr(..), ..) 含子串.
                    // 改为与 5080 同源的**顶层前缀**判断 (只看表达式开头).
                    // 注: 进入本分支的前提是实参已被判为 Variant
                    // (isDefinitelyVariantExpr / cExprIsVariant), 顶层判断足以避免
                    // 对 MsgBox 专用逻辑/4622 分支已转换结果的二次包装.
                    static const char* bstrTopPrefixes092a[] = {
                        "vb6_BSTR_",
                        "VB6_SA_AT(BSTR,",
                        "vb6_VariantToString(",
                        "vb6_BSTR_FromStr("};
                    bool topIsBstr092a = false;
                    for (auto* bp092a : bstrTopPrefixes092a) {
                        if (argVal.compare(0, strlen(bp092a), bp092a) == 0) {
                            topIsBstr092a = true;
                            break;
                        }
                    }
                    if (!topIsBstr092a) {
                        argVal = "vb6_VariantToString(" + argVal + ")";
                    }
                } else if (paramBase == Vb6Type::Object) {
                    // 右值兼容: 避免对函数返回值取址
                    argVal = "vb6_VariantToObjectVal(" + argVal + ")";
                }
            }
        }
        // Fix 086: ByVal Variant形参兜底 — 实参为具体标量/BSTR/对象指针时,
        // 用 _Generic vb6_VariantFromValue 包装. _Generic 按实参C类型自动选择
        // 构造函数 (int→VariantLong, BSTR→VariantString, SafeArray*→VariantArray,
        // 类指针→VariantObject, 已是VARIANT→恒等), 从根源消除 int/BSTR/double→
        // vb6_VARIANT 方向的 C2440.
        if (!isByRef && i < calleeParams.size() && calleeParams[i].isByVal
            && (calleeParams[i].type == Vb6Type::Variant
                || calleeParams[i].type == Vb6Type::Empty)) {
            bool alreadyVariant = cExprIsVariant(argVal);
            if (!alreadyVariant && node.positional[i]->kind == ASTNodeKind::IdentifierExpr) {
                auto& idArg86 = static_cast<IdentifierExpr&>(*node.positional[i]);
                if (knownVariantVars_.count(Symbol::toLower(idArg86.name))) alreadyVariant = true;
            }
            // &(x) 形态是 ByRef 风格临时/取址, 不适合按值包装
            if (!alreadyVariant && argVal.compare(0, 2, "&(") != 0) {
                Vb6Type argT86 = inferExprType(*node.positional[i]);
                bool argIsArr86 = (static_cast<uint16_t>(argT86) & static_cast<uint16_t>(Vb6Type::Array)) != 0;
                Vb6Type argBase86 = static_cast<Vb6Type>(
                    static_cast<uint16_t>(argT86) & ~static_cast<uint16_t>(Vb6Type::Array));
                bool scalarLike =
                    argT86 == Vb6Type::Long || argT86 == Vb6Type::Integer
                    || argT86 == Vb6Type::Boolean || argT86 == Vb6Type::Byte
                    || argT86 == Vb6Type::Double || argT86 == Vb6Type::Single
                    || argT86 == Vb6Type::Currency || argT86 == Vb6Type::Date
                    || argT86 == Vb6Type::LongPtr || argT86 == Vb6Type::String
                    || argT86 == Vb6Type::Object || argBase86 == Vb6Type::Object
                    || argIsArr86;
                if (scalarLike) {
                    argVal = "vb6_VariantFromValue(" + argVal + ")";
                }
            }
        }
        // Fix 038b-2: calleeParams 为空 (运行时/内置函数) 时的参数类型转换.
        // 通过 getRuntimeParamCType 查找期望的 C 类型, 当实参为 Variant 时
        // 自动插入 VARIANT→具体类型提取函数. 解决 vb6_ErrRaise, vb6_BSTR_Assign,
        // vb6_BSTR_Concat, vb6_StrCmp 等运行时函数的 C2440 错误.
        // 注意: 仅使用 cExprIsVariant (C 字符串级) 和 knownVariantVars_ 检测,
        // 不使用 isDefinitelyVariantExpr (AST 级), 因为符号表中的 Variant 返回类型
        // 可能与实际 C 函数返回类型不一致 (如 prop_get 返回 void* 而非 vb6_VARIANT).
        // Fix 091e: 形参符号类型不可信 (无非空参数表, 或符号表把 builtin 形参登记为
        // Variant/Empty — 如 LenB/Len 被推断为 Variant) 时同样查运行时表. 此前仅
        // calleeParams 为空才查表, 导致 Variant 实参原样传给 BSTR 形参:
        // cTlsSocket 200 vb6_LenB_BSTR(vb6_VariantFromComResult(ComGetProp(...))) C2440.
        bool rtParamTypeUsable = (i >= calleeParams.size())
            || calleeParams[i].type == Vb6Type::Variant
            || calleeParams[i].type == Vb6Type::Empty;
        if (!isByRef && rtParamTypeUsable) {
            std::string rtParamType = getRuntimeParamCType(callee, i);
            if (!rtParamType.empty() && rtParamType != "vb6_VARIANT"
                 && rtParamType != "vb6_VARIANT*") {
                bool argIsVariant = cExprIsVariant(argVal);
                // 也检查已知 Variant 变量
                if (!argIsVariant && node.positional[i]->kind == ASTNodeKind::IdentifierExpr) {
                    auto& idArg = static_cast<IdentifierExpr&>(*node.positional[i]);
                    std::string argLower = idArg.name;
                    std::transform(argLower.begin(), argLower.end(), argLower.begin(), ::tolower);
                    if (knownVariantVars_.count(argLower)) argIsVariant = true;
                }
                // Fix 089i: 函数调用返回 Variant 的实参 (VB 未声明返回类型 =
                // Variant, 如 json_ParseErrorMessage(...) 作为 Err.Raise 的
                // Description) — cExprIsVariant 只查字符串前缀, 对
                // "vb6_<mod>_<fn>(...)" 用户函数调用文本不生效 → C2440.
                // 查被调函数符号的返回类型: kind Function/PropertyGet 且
                // returnType Variant → C 层返回 vb6_VARIANT, 需按参数类型提取.
                // 不匹配对象属性 (As Object → void*, returnType 非 Variant).
                if (!argIsVariant && node.positional[i]->kind == ASTNodeKind::IndexOrCallExpr) {
                    auto& callNode89i = static_cast<IndexOrCallExpr&>(*node.positional[i]);
                    if (callNode89i.callee
                        && callNode89i.callee->kind == ASTNodeKind::IdentifierExpr) {
                        std::string calleeLower89i =
                            static_cast<IdentifierExpr&>(*callNode89i.callee).name;
                        std::transform(calleeLower89i.begin(), calleeLower89i.end(),
                                       calleeLower89i.begin(), ::tolower);
                        for (const auto& [k89i, s89i] : symTab_.moduleScope()->symbols()) {
                            if (s89i->kind != SymbolKind::Function
                                && s89i->kind != SymbolKind::PropertyGet) continue;
                            if (s89i->lowerName != calleeLower89i) continue;
                            // 返回类型语义 Variant (VB 未声明返回 = Variant).
                            // 对象属性 As Object → void* (returnType Object),
                            // 不会误匹配; PropertyGet 的 Unknown 属性(可能 void*)
                            // 保守不包, 仅模块 Function 的 Unknown(未推断出类型但
                            // 模块函数 C 返回 vb6_VARIANT)才包.
                            bool retIsVar89i =
                                (s89i->type == Vb6Type::Variant
                                 || s89i->type == Vb6Type::Empty);
                            if (s89i->kind == SymbolKind::PropertyGet) {
                                if (retIsVar89i) argIsVariant = true;
                            } else if (retIsVar89i
                                       || s89i->type == Vb6Type::Unknown) {
                                argIsVariant = true;
                            }
                            break;
                        }
                    }
                }
                if (argIsVariant) {
                    // Fix 090c0: 实参是 COM 结果且被判定为 Variant —
                    // vb6_ComCall/vb6_ComGetProp 返回 void* (COM Variant 承载),
                    // 提取 (VariantToSafeArray1D 等期望 vb6_VARIANT 按值) 前需先
                    // vb6_VariantFromComResult 转为 vb6_VARIANT. 如 cLogs
                    // MakeLogContent: Join(CacheDatas(i), Span) — Collection 元素
                    // 是 Variant 数组 → 原生成 VariantToSafeArray1D(vb6_ComCall(...))
                    // → C2440 (无法从 void* 转换为 vb6_VARIANT).
                    // 精确前缀避免误伤 vb6_ComCallInt( (返回 int32) /
                    // vb6_ComGetStringProp( (返回 BSTR) / vb6_ComCallObject( (对象).
                    bool isComResultVal =
                        (argVal.find("vb6_ComCall(") == 0)
                        || (argVal.find("vb6_ComGetProp(") == 0)
                        || (argVal.find("vb6_ComGetObjectProp(") == 0);
                    if (isComResultVal) {
                        argVal = "vb6_VariantFromComResult(" + argVal + ")";
                    }
                    if (rtParamType == "int32_t" || rtParamType == "int16_t"
                        || rtParamType == "uint8_t" || rtParamType == "LONG") {
                        argVal = "vb6_VariantToLong(" + argVal + ")";
                    } else if (rtParamType == "double" || rtParamType == "float") {
                        argVal = "vb6_VariantToDouble(" + argVal + ")";
                    } else if (rtParamType == "BSTR") {
                        // Fix 089k: 顶层表达式是否已是 BSTR — 原 049b 用
                        // argVal.find("vb6_BSTR") 子串包含判断, 对内部实参含
                        // vb6_BSTR_FromStr(...) 的用户函数调用 (如
                        // json_ParseErrorMessage(...)) 误判"已 BSTR" → 跳过
                        // vb6_VariantToString 包装 → C2440 (vb6_VARIANT→BSTR).
                        // 收紧为剥外层括号后的顶层前缀判断.
                        auto exprTopIsBstr = [](const std::string& s) -> bool {
                            std::string t = s;
                            while (t.size() >= 2 && t.front() == '(' && t.back() == ')') {
                                int depth = 0;
                                bool whole = true;
                                for (size_t k = 0; k < t.size(); k++) {
                                    if (t[k] == '(') depth++;
                                    else if (t[k] == ')') {
                                        depth--;
                                        if (depth == 0 && k < t.size() - 1) {
                                            whole = false;
                                            break;
                                        }
                                    }
                                }
                                if (!whole) break;
                                t = t.substr(1, t.size() - 2);
                            }
                            static const char* bstrTopPrefixes[] = {
                                "vb6_BSTR_",
                                "VB6_SA_AT(BSTR,",
                                // 已由 vb6_VariantToString 转换的结果必为 BSTR,
                                // 顶层识别避免二次包装 → C2440 (BSTR→vb6_VARIANT).
                                "vb6_VariantToString(",
                                // Fix 091h: 移除 "vb6_VariantFromComResult(" —
                                // 该函数返回 vb6_VARIANT (vb6rtl.h:918), 不是 BSTR.
                                // 旧前缀把 COM 属性结果误判为"已 BSTR" → 跳过提取 →
                                // C2440 "vb6_VARIANT → BSTR" (cTlsSocket 200 LenB,
                                // cToolsList 25 prop_let_Filter, cAesCBC 25 StrConv,
                                // Demo_Database 310/506/678 BSTR_Concat).
                                "vb6_BSTR_FromStr("};
                            for (auto* bp : bstrTopPrefixes) {
                                size_t bpl = strlen(bp);
                                if (t.compare(0, bpl, bp) == 0) return true;
                            }
                            return false;
                        };
                        if (!exprTopIsBstr(argVal)) {
                            argVal = "vb6_VariantToString(" + argVal + ")";
                        }
                    } else if (rtParamType == "void*") {
                        argVal = "vb6_VariantToObjectVal(" + argVal + ")";
                    } else if (rtParamType == "vb6_SafeArray1D*") {
                        argVal = "vb6_VariantToSafeArray1D(" + argVal + ")";
                    }
                }
            }
        }
        args.push_back(std::move(argVal));
    }

    // Fix 081c: For named-arg path, track which Optional params were actually passed
    // (before gap-filling sets filled[i]=true for padding params too)