// cgen_expr_call_arg_emit.inc - visit(IndexOrCallExpr&) 函数体片段：位置实参发射（一）—— UDT 打包、Declare ANSI 与 ByRef 取地址（位于实参 for 循环内）
// 由 src/backend/expr/cgen_expr_call.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 1771~2136 行，逐行未改。

    for (size_t i = 0; i < node.positional.size(); i++) {
        // Fix 142: 省略的位置实参 (VB6 Foo a, , c) — 按形参缺省值填充, 不发射
        // 占位表达式 (占位 0/Empty 会覆盖缺省值, 且 _has_ 标志会被误判为已传).
        if (!calleeIsBuiltin && !calleeIsDeclare && i < calleeParams.size()
            && node.omittedArgs.count(i)) {
            const auto& op142 = calleeParams[i];
            std::string odefVal142;
            if (op142.hasDefaultValue && !op142.defaultValueExpr.empty()) {
                odefVal142 = op142.defaultValueExpr;
            } else {
                odefVal142 = defaultValue(op142.type);
            }
            if (op142.isByVal || op142.isParamArray) {
                args.push_back(odefVal142);
            } else {
                std::string ocType142 = mapType(op142.type);
                if (op142.type == Vb6Type::Variant || op142.type == Vb6Type::Empty
                    || op142.type == Vb6Type::Null || op142.type == Vb6Type::Object) {
                    args.push_back("&(" + ocType142 + "){0}");
                } else {
                    args.push_back("&(" + ocType142 + "){" + odefVal142 + "}");
                }
            }
            continue;
        }
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
        // 形参是对象类型 (Object/Control) 且实参是窗体控件标识符时, 抑制默认属性
        // 解析: VB6 把控件作为对象引用传递, 而非默认属性值. 否则 ByRef Object
        // 实参生成默认属性 (BSTR) 指针, 被调方 *objControl 读到 VARIANT.vt
        // (如 VT_BSTR=8) → 运行期崩溃 (BalloonTooltips cTT.CreateToolTip).
        bool savedSuppressProp = suppressDefaultProp_;
        if (i < calleeParams.size()
            && static_cast<Vb6Type>(static_cast<uint16_t>(calleeParams[i].type)
                 & ~static_cast<uint16_t>(Vb6Type::Array)) == Vb6Type::Object
            && node.positional[i] && node.positional[i]->kind == ASTNodeKind::IdentifierExpr) {
            std::string idLower = Symbol::toLower(
                static_cast<IdentifierExpr&>(*node.positional[i]).name);
            if (knownFormControls_.count(idLower) || knownWithEventsCtrlVars_.count(idLower)) {
                suppressDefaultProp_ = true;
            }
        }
        emitExpr(*node.positional[i]);
        suppressDefaultProp_ = savedSuppressProp;
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
            // Fix 110x: BSTR 形参的实参若是 Variant 表达式 (后期绑定控件属性读
            // vb6_VariantFromComResult(vb6_ComGetProp(...)) 等), 直接交给
            // vb6_BSTR_ToANSI(BSTR) → C2440 "无法从 vb6_VARIANT 转换为 BSTR"
            // (Charts 2020 PropPagLP.c 256: CreateWindowEx(..., LabelPlus1.Caption, ...)).
            if (cExprIsVariant(argVal)) {
                argVal = "vb6_VariantToString(" + argVal + ")";
            }
            c_.emitLine("char* " + ansiVar + " = vb6_BSTR_ToANSI(" + argVal + ");");
            ansiTempsToFree_.push_back(ansiVar);
            argVal = ansiVar;
        }

        // Fix 175: 用户 Sub/Function/Property 的 **ByVal String 形参**收到数值/日期实参
        // (`ShowStr(d)` / `ShowStr(n)`)。VB6 按 CStr 隐式转字符串; C 侧形参是 BSTR,
        // 裸传 double → C2440, 裸传 int32_t → 只报 C4047 警告, 运行期把整数当指针
        // 解引用 (0xC0000005 或读到垃圾字符串)。按形参类型补 wrapToBSTR。
        if (!isDeclareAnsiCall && i < calleeParams.size() && calleeParams[i].isByVal
            && calleeParams[i].type == Vb6Type::String && node.positional[i]
            && !cExprIsVariant(argVal)) {
            Vb6Type at175 = inferExprType(*node.positional[i]);
            if (at175 == Vb6Type::Date || at175 == Vb6Type::Double || at175 == Vb6Type::Single
                || at175 == Vb6Type::Currency || at175 == Vb6Type::Long
                || at175 == Vb6Type::Integer || at175 == Vb6Type::Byte
                || at175 == Vb6Type::Boolean || at175 == Vb6Type::LongPtr) {
                argVal = wrapToBSTR(argVal, *node.positional[i]);
            }
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
            // Fix 158b: ByRef **数组**形参 (C 形参 vb6_SafeArray1D**) 收到"整体数组引用"
            // 实参. 1 维 arr() (空括号) 在 prelude 生成 VB6_SA_AT(T, arr, 0) (元素左值);
            // 直接作为 ByRef 数组实参会落入下方复合字面量 (&(vb6_SafeArray1D**){元素}),
            // 元素 T 与 ** 指针类型不符 → C2440 (VBFlexGrid.ctl SortGrid/MergeSort:
            //   TextDisplayMergeSortRec(..., Data(), ...) — ReDim Data(Row1 To Row2)
            //   As TINDIRECTMERGESORTDATA 局部数组). 正确语义: 传调用方数组指针
            //   变量/成员的地址 &arr / &me->field. 已是当前函数 ByRef 数组参数的
            //   (*Data) 形态由下方 (*x) 分支还原为 Data, 不在此处处理 (其还原走
            //   calleeParamIsArray 分支把 (*x) → x).
            bool wholeArr158 = false;
            if (calleeParamIsArray && argVal.rfind("VB6_SA_AT(", 0) == 0) {
                std::string inner158 = argVal.substr(10);
                if (!inner158.empty() && inner158.back() == ')') inner158.pop_back();
                std::vector<std::string> parts158;
                size_t cut158 = 0;
                for (size_t k158 = 0; k158 <= inner158.size(); k158++) {
                    if (k158 == inner158.size() || inner158[k158] == ',') {
                        parts158.push_back(inner158.substr(cut158, k158 - cut158));
                        cut158 = k158 + 1;
                    }
                }
                std::string name158, idx158;
                if (parts158.size() == 3) {
                    name158 = parts158[1];
                    idx158 = parts158[2];
                    size_t b158 = name158.find_first_not_of(" \t");
                    if (b158 == std::string::npos) b158 = 0;
                    size_t e158 = name158.find_last_not_of(" \t");
                    if (e158 != std::string::npos) name158 = name158.substr(b158, e158 - b158 + 1);
                    b158 = idx158.find_first_not_of(" \t");
                    if (b158 == std::string::npos) b158 = 0;
                    e158 = idx158.find_last_not_of(" \t");
                    if (e158 != std::string::npos) idx158 = idx158.substr(b158, e158 - b158 + 1);
                }
                // 仅字面 0 下标 (= 空括号整体引用) 时重写. 两种形态:
                //   1. 名字是标识符 (Data / me->field / _vb6_with_N->field) → &name.
                //   2. 名字是 (*X) — 当前函数内 ByRef 数组参数的还原形态 (X 已是
                //      vb6_SafeArray1D**), 直接传 X, 不加 & (否则成 *** C2440).
                //       (MergeSortRec 递归: Call MergeSortRec(..., Data(), Sort))
                if (idx158 == "0" && !name158.empty()) {
                    if (name158.size() > 3 && name158[0] == '('
                        && name158[1] == '*' && name158.back() == ')') {
                        // 判 X 是纯标识符(|->|链 也允许, 如 (*me->Arr))
                        std::string inner158 = name158.substr(2, name158.size() - 3);
                        bool innerOk158 = !inner158.empty()
                            && (std::isalpha(static_cast<unsigned char>(inner158[0])) || inner158[0] == '_');
                        if (innerOk158) {
                            argVal = inner158;
                            wholeArr158 = true;
                        }
                    } else if (std::isalpha(static_cast<unsigned char>(name158[0]))) {
                        argVal = "&" + name158;
                        wholeArr158 = true;
                    }
                }
            }
            if (wholeArr158) {
                // 已重写为单个取址形式, 跳过下方 ByRef 分发.
            } else if (argVal.size() > 3 && argVal.substr(0, 2) == "(*" && argVal.back() == ')') {
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
                    Vb6Type curPType189 = Vb6Type::Unknown;
                    if (currentProc_) {
                        for (auto& p : currentProc_->params) {
                            if (Symbol::toLower(p.name) == Symbol::toLower(innerName) && !p.isByVal && !p.isParamArray) {
                                bool paramIsArray = (static_cast<uint16_t>(p.type) & static_cast<uint16_t>(Vb6Type::Array)) != 0;
                                if (!paramIsArray) {
                                    isCurrentByRefParam = true;
                                    curPType189 = p.type;
                                }
                                break;
                            }
                        }
                    }
                    if (isCurrentByRefParam) {
                        // ByRef param of current function → already T*, pass directly
                        // Fix 189: 例外 — 被调方形参是 ByRef Variant (C: vb6_VARIANT*)
                        // 时不能直传 T*: 被调方 vb6_VariantFromValue((*p)) 会把 T 的
                        // 字节当 VARIANT.vt 解析 → 字段错位 → 解引用垃圾 BSTR →
                        // 0xC0000005. 按当前形参的 VB 类型重打包为字段式 VARIANT.
                        std::string rep189;
                        if (i < calleeParams.size()
                            && (calleeParams[i].type == Vb6Type::Variant
                                || calleeParams[i].type == Vb6Type::Empty)) {
                            rep189 = wrapByRefVariantParamArg(innerName, curPType189);
                        }
                        argVal = rep189.empty() ? innerName : rep189;
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

                // Fix 164x: 左值判定必须先剥离 UDT 字段链的注释后缀
                // "  /* udt objfield <CType> */" (cgen_util_classtype.cpp:392 追加).
                // 否则 Release.IUnk 判为字段链时因含空格/星号落入强转分支,
                // CopyMemory 的第一实参生成 (void*)(intptr_t)(Release.IUnk) —
                // 传字段值而非字段地址 → 运行时空指针写崩溃.
                // 仅剥离用于左值判断, 输出仍用原样 argVal 保留注释.
                std::string argValLv = argVal;
                size_t lvCmPos = argValLv.find("  /* udt objfield ");
                if (lvCmPos != std::string::npos) {
                    argValLv = argValLv.substr(0, lvCmPos);
                }

                // Fix 064b: VB6_SA_AT(...) 数组元素作为 ByRef 实参时,
                // 不应包装为 VARIANT 复合字面量, 应直接取地址.
                // 典型: PolyPolygon(hDC, uPoints(0), aSizes(0), nCount)
                //   uPoints(0) → VB6_SA_AT(vb6_type_POINTAPI, uPoints, 0)
                //   应生成 (void*)&(VB6_SA_AT(...)) 而非 (&(vb6_VARIANT){.bstrVal=...})
                bool isSaAtExpr = (argValLv.find("VB6_SA_AT(") == 0);
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
                    bool isSimpleIdent = !argValLv.empty() && (std::isalpha(static_cast<unsigned char>(argValLv[0])) || argValLv[0] == '_');
                    if (isSimpleIdent) {
                        for (char c : argValLv) {
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
                    // Fix 190: 再允许 [] 下标 — 定长数组元素也是左值。VBFlexGrid.ctl:28348
                    //   CopyMemory .szText(0), ByVal StrPtr(Text), LenB(Text)
                    // 经 With 展开成 _vb6_with_776->szText[0]; 旧判定因含方括号视其为非左值,
                    // 落入下方 (void*)(intptr_t)(expr) 兜底 → 把首元素的**值**当成目的地址。
                    // NMTTDI 已清零, 该值即 0 → RtlMoveMemory 写 0x0, 鼠标悬停即 0xC0000005。
                    // 方括号内是下标表达式(右值, 任意写法), 括号外仍只允许 标识符/./->
                    bool isUdtFieldChain = !argValLv.empty() && (std::isalpha(static_cast<unsigned char>(argValLv[0])) || argValLv[0] == '_');
                    int chainSubscriptDepth = 0;
                    if (isUdtFieldChain) {
                        for (size_t ci = 0; ci < argValLv.size(); ci++) {
                            char c = argValLv[ci];
                            if (c == '[') { chainSubscriptDepth++; continue; }
                            if (c == ']') {
                                if (chainSubscriptDepth == 0) { isUdtFieldChain = false; break; }
                                chainSubscriptDepth--; continue;
                            }
                            if (chainSubscriptDepth > 0) continue;
                            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.') continue;
                            if (c == '-' && ci + 1 < argValLv.size() && argValLv[ci + 1] == '>') { ci++; continue; }
                            isUdtFieldChain = false;
                            break;
                        }
                        if (chainSubscriptDepth != 0) isUdtFieldChain = false;
                    }
                    bool isSaAt = (argValLv.find("VB6_SA_AT(") == 0);
                    // Fix 086: 宏常量/NULL 不是左值 — TLS_LOCAL_LEGACY_VERSION 是
                    // #define, &(宏) → C2101; NULL 同理.
                    bool isConstMacro = (argValLv == "NULL") || isConstIdent(argValLv);
                    // Fix 090k: (*uFile).BufferArray / (*uFile)->field — ByRef UDT
                    // 参数解引用的字段链也是左值 (As Any / CopyMemory 实参应取地址
                    // &(field), 而非下方 else 的 (void*)(intptr_t)(expr) 强转 —
                    // Variant 字段值强转指针 → C2440 "无法从 vb6_VARIANT 转 intptr_t").
                    bool isDerefFieldChain = false;
                    if (!argValLv.empty() && argValLv[0] == '(' && argValLv.size() > 2 && argValLv[1] == '*') {
                        size_t depth090k = 0;
                        for (size_t ci090k = 0; ci090k < argValLv.size(); ci090k++) {
                            char c090k = argValLv[ci090k];
                            if (c090k == '(') depth090k++;
                            else if (c090k == ')') {
                                depth090k--;
                                if (depth090k == 0) {
                                    if (ci090k + 1 < argValLv.size() &&
                                        (argValLv[ci090k + 1] == '.' ||
                                         (argValLv[ci090k + 1] == '-' && ci090k + 2 < argValLv.size()
                                          && argValLv[ci090k + 2] == '>'))) {
                                        isDerefFieldChain = true;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                    bool isLValue = (isSimpleIdent || isUdtFieldChain || isSaAt || argValLv.find("me->") == 0
                        || (argValLv.size() > 4 && argValLv[0] == '(' && argValLv[1] == '*' && argValLv.back() == ')')
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
                    } else if (argVal.compare(0, 20, "vb6_VariantArrayGet(") == 0) {
                        // Fix 158f: Variant 数组元素整体引用作为 As Any ByRef 实参
                        // (VTableHandle.bas: VariantCopy ArgListRev(i),
                        // ArgList(UBound(ArgList) - i)). 元素是右值 (vb6_VariantArrayGet
                        // 返回值), (void*)(intptr_t)(vb6_VARIANT) → C2440. 用 RTL
                        // vb6_VariantArrayElemPtr(数组, 下标) 取底层槽位地址 — dest/src
                        // 都指向真实元素, VariantCopy 读写落位正确.
                        std::string inner158f = argVal.substr(20);
                        if (!inner158f.empty() && inner158f.back() == ')') inner158f.pop_back();
                        argVal = "vb6_VariantArrayElemPtr(" + inner158f + ")";
                    } else if (isDefinitelyVariantExpr(*node.positional[i]) || cExprIsVariant(argVal)) {
                        // Fix 158g: Variant 值 (Variant 属性读取函数返回) 作为 As Any
                        // ByRef 实参 — VBFlexGrid.ctl: VariantCopy Cell, .CellTag
                        // (CellTag As Variant → vb6_VBFlexGrid_prop_get_CellTag(me)
                        // 返回 vb6_VARIANT). VB6 传临时副本地址; 复合字面量
                        // (&(vb6_VARIANT){expr}) 编译且运行时正确 (src 以值拷贝),
                        // (void*)(intptr_t)(vb6_VARIANT 结构值) → C2440.
                        // Fix 160-H: 上面那句"编译正确"只对**标量**实参成立。
                        // `(vb6_VARIANT){x}` 是**位置**初始化 → 用 x 初始化首个成员
                        // `.vt` (vb6_vartype 枚举), 而不是整体拷贝。实参本身就是
                        // vb6_VARIANT 时 → VBFlexGrid.c:22606
                        //   C2440 "初始化: 无法从 vb6_VARIANT 转换为 vb6_vartype"。
                        // 本文件其它分支要么用指定初始化器 (.vt=VT_BSTR,.bstrVal=…)
                        // 要么用 {0}, 只有此处漏了。改走 Fix 090q 的成熟套路:
                        // 求值到已声明临时再取址, 声明行挂 pending 保证先于引用。
                        if (cExprIsVariant(argVal)) {
                            std::string tmpVar160h = "_vb6_vartmp" + std::to_string(tempCounter_++);
                            c_.addPending("vb6_VARIANT " + tmpVar160h + " = " + argVal + ";");
                            argVal = "(void*)&" + tmpVar160h;
                        } else {
                            argVal = "(&(vb6_VARIANT){" + argVal + "})";
                        }
                    } else {
                        // 合并口径: 本 else 兜底两侧各自加了判别, 两者互斥可共存 ——
                        // 数值字面量取复合字面量地址 (上游 Fix 182), Variant 值取整型
                        // 载荷 (我们的 Fix 160w), 其余维持裸强转。
                        // Fix 182: As Any ByRef 的实参是数值字面量时, VB6 先把字面量求值到
                        // 临时存储, 再把该存储的地址传给形参. 直接强转 (void*)(intptr_t)(v)
                        // 是把数值本身当成指针 — cAsyncSocket 的
                        // ws_setsockopt(m_hSocket, ucsSolTCP, ucsSsoNoDelay, 1&, 4)
                        // 形参为 int32_t*, 收到地址 0x1 → WSAEFAULT(10014),
                        // Create 失败并关闭 socket, 后续 Bind 报 WSAENOTSOCK.
                        // 实测对照: optval=(char*)1 → ret=-1 wsaerr=10014;
                        //           optval=&(int32_t){1} → ret=0.
                        // 0 保持原有 (void*)0 行为 (Fix 069b 场景需要 NULL 指针),
                        // 其余复杂表达式维持原样, 控制影响面.
                        std::string litBody = argVal;
                        char litSuffix = 0;
                        if (!litBody.empty() && (litBody.back() == 'L' || litBody.back() == 'l'
                                                 || litBody.back() == 'f' || litBody.back() == 'F')) {
                            litSuffix = litBody.back();
                            litBody.pop_back();
                        }
                        bool isNumericLiteral = !litBody.empty();
                        for (char c : litBody) {
                            if (!std::isdigit(static_cast<unsigned char>(c))
                                && c != '.' && c != 'e' && c != 'E') {
                                isNumericLiteral = false;
                                break;
                            }
                        }
                        if (isNumericLiteral && litBody != "0") {
                            std::string ccType = "int32_t";
                            if (litBody.find('.') != std::string::npos
                                || litBody.find('e') != std::string::npos
                                || litBody.find('E') != std::string::npos) {
                                ccType = "double";
                            } else if (litSuffix == 'f' || litSuffix == 'F') {
                                ccType = "float";
                            }
                            argVal = "(void*)&(" + ccType + "){" + argVal + "}";
                        } else if (cExprIsVariant(argVal)) {
                            // Fix 160w: Variant 值 (COM 属性读取 vb6_VariantFromComResult 等)
                            // 作为 As Any ByRef 实参的非左值兜底 — (void*)(intptr_t)(struct)
                            // → C2440, 裸传 → C2172 (MainForm.c OleCreatePropertyFrame 参 8
                            // ByVal Pages.pElems: Web 后端该 Variant 的整型载荷即指针值).
                            // 用 vb6_VariantToLongPtr 取整型载荷再按值压回指针形参.
                            argVal = "(void*)(intptr_t)vb6_VariantToLongPtr(" + argVal + ")";
                        } else {
                            // 非左值: 0 或复杂表达式 — 直接强转为 void*
                            argVal = "(void*)(intptr_t)(" + argVal + ")";
                        }
                    }
                } else {
                // Fix 072b: ByVal 覆盖 As Any 参数 — 传 (void*)(intptr_t)(val),
                // 不走复合字面量路径. 典型: SHCreateMemStream(ByVal 0, 0) 中
                // pInit As Any 是 ByRef, 但 ByVal 0 覆盖为传值, 应生成 (void*)0.
                bool isAsAnyByVal = (i < calleeParams.size() && calleeParams[i].type == Vb6Type::Unknown)
                                    && argHasByValOverride;
                if (isAsAnyByVal) {
                    // Fix 160w: ByVal 覆盖 As Any 的实参恰为 vb6_VARIANT (COM 属性
                    // 结果, 如 MainForm.frm:579 OleCreatePropertyFrame(..., ByVal
                    // Pages.pElems, ...) → 实参 vb6_VariantFromComResult(vb6_ComGetProp(
                    // Pages, L"pElems"))). (void*)(intptr_t)(vb6_VARIANT 结构值) →
                    // C2440 "无法从 vb6_VARIANT 转换到 long", 且按位强转也丢指针.
                    // pElems 底层是 Long 载荷的指针值 (IFontDisp** 数组头), 用
                    // vb6_VariantToLongPtr 取整型载荷再按原样压回 void* 实参.
                    if (cExprIsVariant(argVal)) {
                        argVal = "(void*)(intptr_t)vb6_VariantToLongPtr(" + argVal + ")";
                    } else {
                        argVal = "(void*)(intptr_t)(" + argVal + ")";
                    }
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
                    Vb6Type argCurPType189 = Vb6Type::Unknown;
                    if (currentProc_) {
                        for (auto& p : currentProc_->params) {
                            if (Symbol::toLower(p.name) == Symbol::toLower(argVal) && !p.isByVal && !p.isParamArray) {
                                argIsCurrentByRefParam = true;
                                argCurPType189 = p.type;
                                break;
                            }
                        }
                    }
                    if (argIsCurrentByRefParam) {
                        // ByRef param of current function → already T*, pass directly
                        // Fix 189: ByRef Variant 形参时需重打包 (同 (*x) 路径, 见上).
                        std::string rep189b;
                        if (i < calleeParams.size()
                            && (calleeParams[i].type == Vb6Type::Variant
                                || calleeParams[i].type == Vb6Type::Empty)) {
                            rep189b = wrapByRefVariantParamArg(argVal, argCurPType189);
                        }
                        if (!rep189b.empty()) argVal = rep189b;
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
                    } else if (i < calleeParams.size()
                               && !calleeParams[i].isByVal && !calleeParams[i].isParamArray
                               && calleeParams[i].type == Vb6Type::Variant) {
                        // Fix 092y: ByRef Variant 形参 + 简单标识符实参.
                        // C 签名为 vb6_VARIANT*, 直接 &(BSTR) 会把字符串当 Variant
                        // 解析 → 字段错位 → lDataLen=0、QR 段为空 (mdQRTest2_url).
                        // 与 2039 M22 分支一致: 按实参 VB 类型构造字段式复合字面量.
                        Vb6Type argVbType92y = inferExprType(*node.positional[i]);
                        switch (argVbType92y) {
                            case Vb6Type::String:
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
                                // Variant 变量 / UDT 字段 / 未知类型: 保持原样取址,
                                // 值已是 vb6_VARIANT 且可寻址 &x 语义正确.
                                argVal = "&" + argVal;
                                break;
                        }
                    } else if (calleeIsDeclare && i < calleeParams.size()
                               && !calleeParams[i].isByVal && !calleeParams[i].isParamArray
                               && calleeParams[i].type != Vb6Type::Variant
                               && calleeParams[i].type != Vb6Type::Unknown
                               && calleeParams[i].type != Vb6Type::UserDefinedType
                               && !(static_cast<uint16_t>(calleeParams[i].type) & static_cast<uint16_t>(Vb6Type::Array))
                               && argVal.size() > 1 && argVal[0] != '_'
                               && (std::isalpha(static_cast<unsigned char>(argVal[0])))
                               && (cExprIsVariant(argVal)
                                   || knownVariantVars_.count(Symbol::toLower(argVal)) > 0)) {
                        // Fix 133: Variant 变量作为 Declare 标量 ByRef 出参
                        // (Dim hFont: GdipCreateFont(..., hFont), hFont 是 Variant)。
                        // C 签名是 int32_t*, 直接 &hFont 会把字体句柄写进 VARIANT.vt
                        // → 出参永远读不回来 (Charts 2020: GdipMeasureString rc=2,
                        // 标题高度 0 → 标题与绘图区重叠)。
                        // VB6 语义: 传临时标量, 调用后回写 Variant。
                        // Fix 133x: ByRef LongPtr 形参时临时变量必须是 intptr_t,
                        // 否则 GdipCreateFont 把 64 位 GDI+ Font 指针写进 int32 → 截断,
                        // 后续 GdipMeasureString 解引用坏指针 → x64 下 0xC0000005
                        // (Charts 2020 ucChartArea/ucPieChart GetTextSize, 标题文字崩溃)。
                        bool isLp133x = (i < calleeParams.size()
                                        && (calleeParams[i].type == Vb6Type::LongPtr
                                            || calleeParams[i].type == Vb6Type::ULong));
                        std::string vref = "_vbvref" + std::to_string(tempCounter_++);
                        if (isLp133x) {
                            c_.addPending("intptr_t " + vref
                                          + " = vb6_VariantToLongPtr(" + argVal + ");");
                            c_.addPostLine("vb6_VariantSetI8(&" + argVal + ", " + vref + ");");
                        } else {
                            c_.addPending("int32_t " + vref
                                          + " = (int32_t)vb6_VariantToDouble(" + argVal + ");");
                            c_.addPostLine("vb6_VariantSetI4(&" + argVal + ", " + vref + ");");
                        }
                        argVal = "&" + vref;
                    } else {
                        argVal = "&" + argVal;
                    }
                } else {
                    // Fix 092z: 左值字段链 (With对象 _vb6_with_N->字段 / me->字段 /
                    // 虚UDT字段) 也是可寻址左值 → 生成 &(字段) 而非复合字面量.
                    // With 前 ByRef Long 字段 (QRCodegenMakeAlphanumeric →
                    // pvAppendBitsToBuffer ..., .Data, .BitLength) 若走复合字面量
                    // 会复制字段值到临时 int32, 被调方 (*lBitLen)++ 只改临时值 →
                    // 字段永不增长 → 位写入偏移全零 (HELLO WORLD 无法解码).
                    // Fix 164x: 同样先剥离 "  /* udt objfield */" 注释后缀再判左值.
                    std::string argValLv092z = argVal;
                    size_t lvCmPos092z = argValLv092z.find("  /* udt objfield ");
                    if (lvCmPos092z != std::string::npos) {
                        argValLv092z = argValLv092z.substr(0, lvCmPos092z);
                    }
                    bool isLvChain092z = !argValLv092z.empty() && (std::isalpha(static_cast<unsigned char>(argValLv092z[0])) || argValLv092z[0] == '_');
                    if (!isLvChain092z && argValLv092z.size() > 2 && argValLv092z[0] == '(' && argValLv092z[1] == '*') {
                        // Fix 090k: (*ptr)->field / (*ptr).field 解引用字段链也是左值
                        int depth092z = 0;
                        for (size_t ci092z = 0; ci092z < argValLv092z.size(); ci092z++) {
                            char c092z = argValLv092z[ci092z];
                            if (c092z == '(') depth092z++;
                            else if (c092z == ')') {
                                depth092z--;
                                if (depth092z == 0) {
                                    if (ci092z + 1 < argValLv092z.size() &&
                                        (argValLv092z[ci092z + 1] == '.' ||
                                         (argValLv092z[ci092z + 1] == '-' && ci092z + 2 < argValLv092z.size()
                                          && argValLv092z[ci092z + 2] == '>'))) {
                                        isLvChain092z = true;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                    // Fix 113f: VB6_SA_AT(TYPE, arr, idx).field / ->field — UDT 数组
                    // 元素字段访问也是**可寻址左值** (VB6_SA_AT 是 RTL 中唯一产出左值
                    // 的函数式宏, 展开为 ((T*)arr->data)[idx - arr->lBound] 元素).
                    // 必须生成 &(expr) 而非复合字面量**副本**: 否则被调方对 ByRef 数组
                    // 形参 ReDim/写回只改临时副本, 调用方字段永不更新.
                    // Charts 2020 ucTreeMaps.Draw: Squarified m_Serie(i).Values,
                    //   m_Serie(i).Rects, ... 内部 ReDim Rects(...) 写 (*Rects) →
                    //   原走 (&(vb6_SafeArray1D**){...Rects}) 临时副本 →
                    //   me->m_Serie[i].Rects 仍为 NULL → 随后绘制循环
                    //   VB6_SA_AT(vb6_type_RectF, _vb6_with_N->Rects, j) 空指针崩溃
                    //   (0xC0000005 @ ucTreeMaps_Draw, edx=0).
                    // 括号配对后须紧跟 '.' 或 '->' 才算字段链; 且**必须跳过**下方的
                    // 简单字符扫描 — 该扫描不认括号, 会把本形态又判回非左值 (曾因此
                    // 使本 Fix 首次落地无效, 生成代码未变而困惑).
                    bool isSaAtField113f = false;
                    if (argVal.rfind("VB6_SA_AT(", 0) == 0) {
                        int depth113f = 1;
                        for (size_t ci113f = 10; ci113f < argVal.size(); ci113f++) {
                            char c113f = argVal[ci113f];
                            if (c113f == '(') depth113f++;
                            else if (c113f == ')') {
                                depth113f--;
                                if (depth113f == 0) {
                                    if (ci113f + 1 < argVal.size()
                                        && (argVal[ci113f + 1] == '.'
                                            || (argVal[ci113f + 1] == '-'
                                                && ci113f + 2 < argVal.size()
                                                && argVal[ci113f + 2] == '>'))) {
                                        isSaAtField113f = true;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                    if (isSaAtField113f) isLvChain092z = true;
                    if (isLvChain092z && !isSaAtField113f) {
                        for (size_t ci092z = 0; ci092z < argVal.size(); ci092z++) {
                            char c092z = argVal[ci092z];
                            if (std::isalnum(static_cast<unsigned char>(c092z)) || c092z == '_' || c092z == '.') continue;
                            if (c092z == '-' && ci092z + 1 < argVal.size() && argVal[ci092z + 1] == '>') { ci092z++; continue; }
                            isLvChain092z = false;
                            break;
                        }
                    }
                    if (isLvChain092z) {
                        argVal = "&(" + argVal + ")";
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
                    // Fix 120: ByRef 具体数值形参 ← 裸 COM 结果 (void* 承载 COM VARIANT*)
                    // 不能把指针直接截断成数值 —— 取到的是指针低位, 于是颜色/数值全成噪声:
                    //   Charts 2020 ucChartArea.AddLineSeries(..., ByRef SerieColor As OLE_COLOR)
                    //   调用处 cPalette(j) 是 vb6_ComCall(...) → 生成
                    //   &(int32_t){vb6_ComCall(...)} → 面积图系列变成洋红/紫随机色.
                    // 先解出 Variant 的值再放进复合字面量 (Fix 029 只覆盖 ByVal 路径).
                    {
                        bool comRes120 = (argVal.compare(0, 12, "vb6_ComCall(") == 0)
                            || (argVal.compare(0, 15, "vb6_ComGetProp(") == 0)
                            || (argVal.compare(0, 21, "vb6_ComGetObjectProp(") == 0);
                        if (comRes120) {
                            std::string ex120 = "vb6_VariantFromComResult(" + argVal + ")";
                            if (cType == "int32_t")
                                argVal = "(int32_t)vb6_VariantToDouble(" + ex120 + ")";
                            else if (cType == "int16_t")
                                argVal = "(int16_t)vb6_VariantToDouble(" + ex120 + ")";
                            else if (cType == "double")
                                argVal = "vb6_VariantToDouble(" + ex120 + ")";
                            else if (cType == "float")
                                argVal = "(float)vb6_VariantToDouble(" + ex120 + ")";
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
                                } else if (argVal.compare(0, 12, "vb6_ComCall(") == 0
                                           || argVal.compare(0, 15, "vb6_ComGetProp(") == 0
                                           || argVal.compare(0, 21, "vb6_ComGetObjectProp(") == 0) {
                                    // Fix 113l: **裸 COM 结果** (void*, 承载 COM
                                    // VARIANT*) 填入 VT_BSTR 槽时必须先
                                    // vb6_VariantFromComResult 再 vb6_VariantToString;
                                    // 原样把 void* 塞进 .bstrVal 会让 GDI+ 把垃圾当
                                    // 字符串 → 渲染成一排"豆腐块"空框.
                                    //   Charts 2020 ucTreeMaps 项名标签:
                                    //     DrawText(..., Labels.Item(j), ...) (text As Variant)
                                    //   → 原生成 (&(vb6_VARIANT){.vt=VT_BSTR,
                                    //       .bstrVal=vb6_ComCall(...Labels..., L"Item", ...)}).
                                    argVal = "(&(vb6_VARIANT){.vt=VT_BSTR, .bstrVal="
                                           + std::string("vb6_VariantToString(vb6_VariantFromComResult(")
                                           + argVal + "))})";
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
                }
                } // end Fix 072b else (As Any ByVal override)
                    } // end Fix 064 else
            }
        } // Fix 024 P2: ByVal Variant 参数 — 调用点用 vb6_VariantFromValue 包装实参.