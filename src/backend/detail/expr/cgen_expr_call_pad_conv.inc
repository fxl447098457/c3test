// cgen_expr_call_pad_conv.inc - visit(IndexOrCallExpr&) 函数体片段：Optional 参数补齐与返回/实参类型修正链、末尾收口
// 由 src/backend/expr/cgen_expr_call.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 3251~3623 行，逐行未改。

    if (calleeParams.size() > 0 && args.size() < calleeParams.size() && paIndex < 0 && !calleeIsBuiltin) {
        for (size_t i = args.size(); i < calleeParams.size(); i++) {
            if (i > 0 || !args.empty()) argList += ", ";
            const auto& param = calleeParams[i];
            // Determine the default value (explicit or type-zero)
            std::string defVal;
            if (param.hasDefaultValue && !param.defaultValueExpr.empty()) {
                defVal = param.defaultValueExpr;
            } else {
                defVal = defaultValue(param.type);
            }
            // ByRef params need pointer, ByVal need value
            if (param.isByVal) {
                argList += defVal;
            } else {
                // ByRef: pass address of compound literal: &(type){defVal}
                std::string cType = mapType(param.type);
                // P20-36: Variant/struct types can't use {funcCall()} compound literal
                if (param.type == Vb6Type::Variant || param.type == Vb6Type::Empty ||
                    param.type == Vb6Type::Null || param.type == Vb6Type::Object) {
                    argList += "&(" + cType + "){0}";
                } else {
                    argList += "&(" + cType + "){" + defVal + "}";
                }
            }
                    }

                    // Fix 056: ND版本需要vb6_SafeArrayND*参数, 动态数组声明为1D*
                    if (!args.empty()) {
                        args[0] = "(vb6_SafeArrayND*)" + args[0];
                    }

                }
    // P20-36: IsMissing support - append _has_ flags for Optional params
    // For each Optional param in calleeParams: 1 if actually passed, 0 if padded
    // Fix 030b: builtin 跳过 (RTL C 函数无 _has_ 尾叜)
    // Fix 042a: Declare 函数也跳过 (Declare C 签名无 _has_ 尾叜, 但 Optional padding 仍需要)
    if (calleeParams.size() > 0 && paIndex < 0 && !calleeIsBuiltin && !calleeIsDeclare) {
        bool hasOptional = false;
        for (size_t i = 0; i < calleeParams.size(); i++) if (calleeParams[i].isOptional && !calleeParams[i].isParamArray) { hasOptional = true; break; }
        if (hasOptional) {
        for (size_t i = 0; i < calleeParams.size(); i++) {
            const auto& param = calleeParams[i];
            if (param.isOptional && !param.isParamArray) {
                if (!argList.empty()) argList += ", ";
                // Fix 081c: For named-arg path, use actuallyPassedParams to determine _has_ flag.
                // For positional-arg path, args.size() already equals actual arg count (padding
                // doesn't increase args.size()), so (i < args.size()) is correct.
                if (!actuallyPassedParams.empty() && i < actuallyPassedParams.size()) {
                    argList += actuallyPassedParams[i] ? "1" : "0";
                } else {
                    // Fix 142: 省略的位置实参 (node.omittedArgs) 视为未传 → _has_=0
                    argList += (i < args.size() && !node.omittedArgs.count(i)) ? "1" : "0";
                }
            }
        }
        }  // end if (hasOptional)
    }  // end if (calleeParams.size() > 0 && ...)

    // P6.6: 类模块中调用同类方法(包括递归), 需要自动添加me作为第一个参数
    // 如 Factorial(N-1) -> vb6_MathLib_Factorial(me, (N-1))
    if (classMethodObjArg.empty() && isClassModule_ && currentProc_) {
        std::string modPrefix = "vb6_" + cIdent(moduleName_) + "_";
        if (callee.find(modPrefix) == 0) {
            classMethodObjArg = "(void*)me";
        }
    }

    // P6.5: 如果classMethodObjArg非空, 需要将其作为第一个参数插入
    if (!classMethodObjArg.empty()) {
        if (argList.empty()) {
            argList = classMethodObjArg;
        } else {
            argList = classMethodObjArg + ", " + argList;
        }
    }

    // M22-fix: CStr类型适配 — 根据参数类型选择正确的CStr变体
    if (callee == "vb6_CStr" && !node.positional.empty()) {
        auto& firstArg = node.positional[0];
        if (firstArg->kind == ASTNodeKind::IdentifierExpr) {
            auto& idArg = static_cast<IdentifierExpr&>(*firstArg);
            std::string argLower = idArg.name;
            std::transform(argLower.begin(), argLower.end(), argLower.begin(), ::tolower);
            // 参数是BSTR变量 → CStr是空操作, 直接使用参数
            if (knownBstrVars_.count(argLower)) {
                emitExpr(*firstArg);
                return;
            }
            // Fix 117c: 参数是Single(float)变量 → vt_R4 语义 (必须优先于 Double 判定,
            // 因为 Single 变量同时登记在 knownDoubleVars_ 里以复用既有取值路径)
            if (knownSingleVars_.count(argLower)) {
                callee = "vb6_CStrSingle";
            }
            // 参数是int32_t/Long变量 → 用vb6_CStrLong
            else if (knownLongVars_.count(argLower)) {
                callee = "vb6_CStrLong";
            }
            // 参数是double变量 → 用vb6_CStrDbl
            else if (knownDoubleVars_.count(argLower)) {
                callee = "vb6_CStrDbl";
            }
            // 参数是Variant变量 → 保留vb6_CStr(VARIANT)
        } else {


            // 非标识符表达式: 推断类型选择CStr变体
            Vb6Type argType = inferExprType(*firstArg);
            if (argType == Vb6Type::String) {
                // Fix 056b: CStr(String) 是空操作 → 直接使用参数
                // (否则生成 vb6_CStr(BSTR) → C2440: 无法从BSTR转换为vb6_VARIANT)
                if (!args.empty()) { lastExpr_ = args[0]; return; }
                lastExpr_ = "vb6_BSTR_Empty()";
                return;
            }
            if (argType == Vb6Type::Long || argType == Vb6Type::Integer) {
                callee = "vb6_CStrLong";
            } else if (argType == Vb6Type::Single) {
                callee = "vb6_CStrSingle";   // Fix 117c: VT_R4 语义
            } else if (argType == Vb6Type::Double) {
                callee = "vb6_CStrDbl";
            }
            // P25: COM属性取值已在args[0]中, 根据取值函数选择CStr变体或跳过
            if (!args.empty()) {
                const std::string& a0 = args[0];
                if (a0.find("vb6_ComGetStringProp") == 0 || a0.find("vb6_ComCallBSTR") == 0) {
                    // 已是BSTR, CStr是空操作
                    lastExpr_ = a0;
                    return;
                } else if (a0.find("vb6_ComGetIntProp") == 0 || a0.find("vb6_ComVtableGetInt") == 0) {
                    callee = "vb6_CStrLong";
                } else if (a0.find("vb6_ComGetDoubleProp") == 0 || a0.find("vb6_ComVtableGetDouble") == 0) {
                    callee = "vb6_CStrDbl";
                }
            }
        }
        // Fix 056b: callee 仍为 vb6_CStr 且实参非Variant → 用 vb6_VariantFromValue 包装
        // (如 CStr((int32_t)GetCurrentThreadId()) / CStr(模块级Long变量) →
        //  vb6_CStr(vb6_VariantFromValue(x)), 由 _Generic 按实参类型自动包装, 消除 C2440)
        if (callee == "vb6_CStr" && !args.empty()) {
            // Fix 121: 实参是**裸 COM 结果** (vb6_ComCall/ComCallByDispid/ComGetProp/
            // ComGetObjectProp 返回 void*, 承载 COM VARIANT*) 时必须先用
            // vb6_VariantFromComResult 解引用成 vb6_VARIANT — vb6_VariantFromValue
            // 会按 _Generic 匹配 void* → 得到空 Variant, 于是 CStr(Collection.Item(i))
            // 打印空串; 图表把系列值读成空/噪声 → 坐标轴量级完全错
            // (Charts 2020 柱图/面积图 Y 轴数字大 100 倍; ferock 0.10.6 同修复,
            //  此处合并为并集: 含 ComCallByDispid/ComGetObjectProp)。
            const std::string& a0 = args[0];
            bool comRes121 = (a0.compare(0, 12, "vb6_ComCall(") == 0)
                || (a0.compare(0, 21, "vb6_ComCallByDispid(") == 0)
                || (a0.compare(0, 15, "vb6_ComGetProp(") == 0)
                || (a0.compare(0, 21, "vb6_ComGetObjectProp(") == 0);
            if (comRes121) {
                lastExpr_ = "vb6_CStr(vb6_VariantFromComResult(" + a0 + "))";
            } else {
                lastExpr_ = "vb6_CStr(vb6_VariantFromValue(" + a0 + "))";
            }
            return;
        }
    }
    // P8.4: Variant参数适配 — 如果目标函数不接受Variant但参数是Variant类型, 使用V后缀函数
    // CInt(Variant)→vb6_CIntV, CDbl(Variant)→vb6_CDblV, CLng(Variant)→vb6_CLngV
    // Fix 090d: 扩展覆盖 CByte/CSng/CBool (CByte("&H"&hex) 字符串实参同样需解析适配)
    if (callee == "vb6_CInt" || callee == "vb6_CLng" || callee == "vb6_CDbl"
        || callee == "vb6_CByte" || callee == "vb6_CSng" || callee == "vb6_CBool") {
        // 检查第一个参数是否为Variant变量
        bool firstArgIsVariant = false;
        bool firstArgIsBstr = false;
        if (!node.positional.empty()) {
            auto& firstArg = node.positional[0];
            if (firstArg->kind == ASTNodeKind::IdentifierExpr) {
                auto& idArg = static_cast<IdentifierExpr&>(*firstArg);
                std::string argLower = idArg.name;
                std::transform(argLower.begin(), argLower.end(), argLower.begin(), ::tolower);
                if (knownVariantVars_.count(argLower)) firstArgIsVariant = true;
                else if (knownBstrVars_.count(argLower)) firstArgIsBstr = true;
            }
            // Fix 036: 非标识符 Variant 表达式 (函数返回 Variant 的调用/类方法等) 也需 V 后缀
            if (!firstArgIsVariant) {
                firstArgIsVariant = isDefinitelyVariantExpr(*firstArg);
            }
            // Fix 038b-4: 字符串级 Variant 检测回退 — 捕获 vb6_VariantArrayGet 等表达式
            if (!firstArgIsVariant && !args.empty()) {
                firstArgIsVariant = cExprIsVariant(args[0]);
            }
        }
        if (firstArgIsVariant) {
            if (callee == "vb6_CInt") callee = "vb6_CIntV";
            else if (callee == "vb6_CLng") callee = "vb6_CLngV";
            else if (callee == "vb6_CDbl") callee = "vb6_CDblV";
            else if (callee == "vb6_CByte") callee = "vb6_CByteV";
            else if (callee == "vb6_CSng") callee = "vb6_CSngV";
            else if (callee == "vb6_CBool") callee = "vb6_CBoolV";
            // Fix 084l: 撤销 Fix 038b-2 的 Variant→double 提前提取 (getRuntimeParamCType
            // 把 CLng/CDbl/CInt 形参当作 double, 已将 lRet 替换为 vb6_VariantToDouble(lRet)).
            // V 后缀函数直接接受 vb6_VARIANT — 保留提取会产生 vb6_CLngV(double) → C2440.
            if (!args.empty()) {
                static const char* extractPrefixes[] = {
                    "vb6_VariantToDouble(", "vb6_VariantToLong("};
                for (auto* pre : extractPrefixes) {
                    size_t pl = strlen(pre);
                    if (args[0].compare(0, pl, pre) == 0 && args[0].size() > pl + 1) {
                        args[0] = args[0].substr(pl, args[0].size() - pl - 1);
                        argList.clear();
                        for (size_t ai = 0; ai < args.size(); ai++) {
                            if (ai > 0) argList += ", ";
                            argList += args[ai];
                        }
                        break;
                    }
                }
            }
        }
        // Fix 036: BSTR 参数 → vb6_NumVal 转换为 double (CInt/CLng/CDbl/CByte/CSng/CBool
        // 接 double). Fix 090d: vb6_Val 不识别 "&H" 前缀 (Val("&HFF")=0), 而 VB6 类型
        // 转换函数解析 &H/&O → CByte("&H" & hex) 需 vb6_NumVal; 十进制语义不变.
        // Fix 084c: 扩展检测到字符串级 BSTR 表达式 (vb6_Trim/vb6_BSTR_Concat/vb6_CStr
        // /vb6_VariantToString 等), 不仅限于已知 BSTR 标识符 — 否则
        // vb6_CLng(vb6_Trim(vb6_VariantToString(...))) 触发 C2440 "BSTR → double".
        bool argIsBstrExpr = firstArgIsBstr;
        if (!argIsBstrExpr && !args.empty()) {
            std::string& a0 = args[0];
            argIsBstrExpr =
                a0.find("vb6_Trim(") == 0 || a0.find("vb6_LTrim(") == 0 ||
                a0.find("vb6_RTrim(") == 0 || a0.find("vb6_StrConv(") == 0 ||
                a0.find("vb6_Mid(") == 0 || a0.find("vb6_Left(") == 0 ||
                a0.find("vb6_Right(") == 0 || a0.find("vb6_Replace(") == 0 ||
                a0.find("vb6_String(") == 0 || a0.find("vb6_Format(") == 0 ||
                a0.find("vb6_UCase(") == 0 || a0.find("vb6_LCase(") == 0 ||
                a0.find("vb6_Space(") == 0 || a0.find("vb6_IIfBSTR(") == 0 ||
                a0.find("vb6_Chr(") == 0 || a0.find("vb6_ChrW(") == 0 ||
                a0.find("vb6_BSTR_Concat(") == 0 || a0.find("vb6_BSTR_FromStr(") == 0 ||
                a0.find("vb6_CStr") == 0 || a0.find("vb6_VariantToString(") == 0 ||
                a0.find("VB6_SA_AT(BSTR,") != std::string::npos;
        }
        if (argIsBstrExpr && !args.empty()) {
            args[0] = "vb6_NumVal(" + args[0] + ")";
            argList.clear();
            for (size_t i = 0; i < args.size(); i++) {
                if (i > 0) argList += ", ";
                argList += args[i];
            }
        }
    }
    // MsgBox自动BSTR转换: MsgBox期望BSTR, 非BSTR的prompt参数需包装
    // 注意: 只转换第一个参数(prompt), 不能把整个argList包进去(MsgBox v, , title时argList含3个参数)
    if (callee == "vb6_MsgBox" || callee == "vb6_MsgBox1") {
        if (!args.empty()) {
            std::string& firstArg = args[0];
            if (firstArg.find("vb6_ComGetIntProp") == 0 ||
                firstArg.find("vb6_ComVtableGetInt") == 0) {
                firstArg = "vb6_CStrLong(" + firstArg + ")";
            } else if (firstArg.find("vb6_ComGetDoubleProp") == 0 ||
                       firstArg.find("vb6_ComVtableGetDouble") == 0) {
                firstArg = "vb6_CStrDbl(" + firstArg + ")";
            } else if (firstArg.find("vb6_VariantFromComResult") == 0) {
                // COM调用结果(VARIANT*)→vb6_VARIANT, 需转BSTR
                firstArg = "vb6_VariantToString(" + firstArg + ")";
            } else if (firstArg.find("vb6_ComCall(") == 0) {
                // P24-01: 后期绑定COM调用返回VARIANT*, 需解包转BSTR
                firstArg = "vb6_VariantToString(vb6_VariantFromComResult(" + firstArg + "))";
            } else if (firstArg.find("vb6_VariantToString(") == 0) {
                // Fix 090u: 实参已是 VariantToString(...) (参数打包段对 Variant 实参
                // 按 BSTR 形参已转换一次, 如 MsgBox .FindFirst(...) As Variant /
                // MsgBox .Value(1,6) As Variant) — 再包一层 → VariantToString(BSTR)
                // C2440 "BSTR→vb6_VARIANT". 此处保持一层.
            } else if (!node.positional.empty() && inferExprType(*node.positional[0]) == Vb6Type::Variant) {
                // Variant类型变量/表达式: MsgBox v → vb6_VariantToString(v)
                firstArg = "vb6_VariantToString(" + firstArg + ")";
            }
        }
    }
    // P25: MsgBox BSTR转换可能修改了args[0], 需重建argList
    if (callee == "vb6_MsgBox" || callee == "vb6_MsgBox1") {
        argList.clear();
        for (size_t i = 0; i < args.size(); i++) {
            if (i > 0) argList += ", ";
            argList += args[i];
        }
    }
    // MsgBox(prompt) -> vb6_MsgBox1(prompt)
    // MsgBox(prompt, buttons) -> vb6_MsgBox(prompt, buttons, NULL)
    if (callee == "vb6_MsgBox") {
        if (node.positional.size() == 1) {
            callee = "vb6_MsgBox1";
        } else if (node.positional.size() == 2) {
            argList += ", NULL";
        }
    }
    // P26: Format 第一个参数需要包装为 vb6_VARIANT
    if (callee == "vb6_Format" && !args.empty()) {
        Vb6Type argType = inferExprType(*node.positional[0]);
        if (argType == Vb6Type::Long || argType == Vb6Type::Integer) {
            args[0] = "vb6_VariantLong(" + args[0] + ")";
        } else if (argType == Vb6Type::Double || argType == Vb6Type::Single) {
            args[0] = "vb6_VariantDouble(" + args[0] + ")";
        } else if (argType == Vb6Type::String) {
            args[0] = "vb6_VariantString(" + args[0] + ")";
        } else if (argType == Vb6Type::Boolean) {
            args[0] = "vb6_VariantInt((int16_t)(" + args[0] + "))";
        } else if (argType == Vb6Type::Byte) {
            args[0] = "vb6_VariantInt((int16_t)(" + args[0] + "))";
        } else if (argType == Vb6Type::Date) {
            args[0] = "vb6_VariantDouble((double)(" + args[0] + "))";
        }
        // Fix 036: Format fallback — 未匹配类型 (Currency/Unknown 等, 非 Variant) 用
        // vb6_VariantFromValue 包装. Variant 类型无需包装 (已是 vb6_VARIANT).
        else if (argType != Vb6Type::Variant) {
            args[0] = "vb6_VariantFromValue(" + args[0] + ")";
        }
        argList.clear();
        for (size_t i = 0; i < args.size(); i++) {
            if (i > 0) argList += ", ";
            argList += args[i];
        }
    }
    // Fix 036: CStr fallback — 当特殊分支未匹配 (Boolean/Byte/Date/Currency/常量/
    // 未注册函数返回值/Const BSTR 等), callee 仍为 vb6_CStr (接 vb6_VARIANT). 用
    // vb6_VariantFromValue 包装 args[0], _Generic 按实参 C 类型自动选择 Variant ctor,
    // 避免 concrete → VARIANT C2440. 对已是 vb6_VARIANT 的实参为 identity (no-op), 安全.
    // 但需排除确定 Variant 的表达式 (如 Me.Segments(i) 返回 Variant): _Generic 宏
    // 对某些 Variant 表达式展开可能产生逗号问题 → C2197, 故用 isDefinitelyVariantExpr
    // 跳过, 保留 vb6_CStr(variantExpr) 原样 (vb6_CStr 接 VARIANT, 直接可用).
    if (callee == "vb6_CStr" && !args.empty()) {
        bool argIsDefVariant = false;
        if (!node.positional.empty()) {
            argIsDefVariant = isDefinitelyVariantExpr(*node.positional[0]);
        }
        // Fix 038b-4: 字符串级 Variant 检测 — 如果是 C 级 Variant 表达式,
        // 也跳过 VariantFromValue 包装 (vb6_CStr 直接接 VARIANT)
        if (!argIsDefVariant) {
            argIsDefVariant = cExprIsVariant(args[0]);
        }
        if (!argIsDefVariant) {
            // Fix 121b: 同 Fix 121 — 裸 COM 结果必须解引用 (VariantFromComResult),
            // 否则 _Generic 按 void* 包装得到空 Variant。
            bool comRes121b = (args[0].compare(0, 12, "vb6_ComCall(") == 0)
                || (args[0].compare(0, 15, "vb6_ComGetProp(") == 0)
                || (args[0].compare(0, 21, "vb6_ComGetObjectProp(") == 0);
            args[0] = comRes121b
                ? "vb6_VariantFromComResult(" + args[0] + ")"
                : "vb6_VariantFromValue(" + args[0] + ")";
            argList.clear();
            for (size_t i = 0; i < args.size(); i++) {
                if (i > 0) argList += ", ";
                argList += args[i];
            }
        }
    }
    // Fix 040d: CallByName variadic args packing
    // C signature: vb6_CallByName(void* obj, const wchar_t* procName, int32_t callType,
    //                              void* args, int32_t argc)
    // First 3 VB6 args map to obj/procName/callType. Extra args (4+) are method
    // arguments, packed into (void*[]){vb6_ComPackValue(arg), ...} with argc.
    if (callee == "vb6_CallByName" && args.size() >= 3) {
        if (args.size() == 3) {
            // No extra method args: pass NULL, 0
            argList = args[0] + ", " + args[1] + ", " + args[2] + ", NULL, 0";
        } else {
            // Pack extra args [3..] into void*[] array using vb6_ComPackValue
            std::string argsArray = "(void*[]){";
            for (size_t i = 3; i < args.size(); i++) {
                if (i > 3) argsArray += ", ";
                argsArray += "vb6_ComPackValue(" + args[i] + ")";
            }
            argsArray += "}";
            int32_t extraArgc = (int32_t)args.size() - 3;
            argList = args[0] + ", " + args[1] + ", " + args[2] + ", "
                    + argsArray + ", " + std::to_string(extraArgc);
        }
    }
    // Fix 065: LenB(UDT) — MSVC _Generic 对自定义结构体类型匹配有问题,
    // 直接生成 sizeof(vb6_type_XXX) 而不经 vb6_LenB _Generic 宏
    if (callee == "vb6_LenB" && args.size() == 1) {
        // 检查参数是否是已知 UDT 变量
        std::string argLower = args[0];
        // 去除可能的 With 前缀: _vb6_with_N. → 提取变量名
        // 去除 me-> 前缀
        if (argLower.substr(0, 4) == "me->") argLower = argLower.substr(4);
        std::transform(argLower.begin(), argLower.end(), argLower.begin(), ::tolower);
        auto udtIt = knownUdtVars_.find(argLower);
        if (udtIt != knownUdtVars_.end()) {
            lastExpr_ = "(int32_t)sizeof(" + udtIt->second + ")";
            return;
        }
        // Fix 084h: UDT 数组元素 LenB(VB6_SA_AT(vb6_type_XXX, arr, idx)) →
        // sizeof(vb6_type_XXX)。_Generic 宏无法匹配自定义结构体类型, 且
        // 数组元素类型未知, 直接由类型名生成 sizeof。
        {
            std::string rawArg = args[0];
            std::string rawLower = rawArg;
            std::transform(rawLower.begin(), rawLower.end(), rawLower.begin(), ::tolower);
            const char* saPrefix = "vb6_sa_at(vb6_type_";
            if (rawLower.compare(0, 19, saPrefix) == 0) {
                size_t comma = rawArg.find(',');
                if (comma != std::string::npos) {
                    std::string typeName = rawArg.substr(19, comma - 19);
                    lastExpr_ = "(int32_t)sizeof(" + typeName + ")";
                    return;
                }
            }
        }
    }
    // Fix 108c: 内建函数实参的类型适配 (走到此处说明没有专门分支处理).
    //  (a) 数值型内建函数收到"无类型 COM 调用结果" (vb6_ComCall → void*, 指向
    //      VARIANT) 时先解包为数值, 否则 void* 传入 double 形参报 C2440
    //      (Charts 2020 ucChartBar SumSerieValues: vb6_Abs(vb6_ComCall(...))).
    //  (b) 形参为 String 的内建函数收到数值实参时按 VB6 隐式转换转 BSTR,
    //      否则 float 传入 BSTR 形参报 C2440
    //      (Charts 2020 ucChartArea: vb6_Replace(m_LabelsFormats, "{V}", yRange)) .
    {
        static const std::unordered_set<std::string> numericBuiltins108c = {
            "vb6_Abs", "vb6_Int", "vb6_Fix", "vb6_Sgn", "vb6_Sqr", "vb6_Sin",
            "vb6_Cos", "vb6_Tan", "vb6_Atn", "vb6_Log", "vb6_Exp", "vb6_Round",
            "vb6_CDbl", "vb6_CSng", "vb6_CInt", "vb6_CLng", "vb6_CByte",
            "vb6_CBool", "vb6_Hex", "vb6_Oct",
        };
        static const std::unordered_map<std::string, std::vector<size_t>> strArgPos108c = {
            {"vb6_Replace",    {0, 1, 2}},
            {"vb6_StrComp",    {0, 1}},
            {"vb6_StrReverse", {0}},
            {"vb6_Trim",       {0}},
            {"vb6_LTrim",      {0}},
            {"vb6_RTrim",      {0}},
            {"vb6_UCase",      {0}},
            {"vb6_LCase",      {0}},
            {"vb6_StrConv",    {0}},
        };
        bool argsFixed108c = false;
        // argList 在更早处已按 args 拼好, 其后还有"补默认参数"分支向 argList 追加
        // (如 vb6_Replace 的 ", 1, -1, 0"). 因此不能整体重建 argList —— 只能就地把
        // 被改写的实参文本替换掉, 否则会丢掉默认参数 (曾导致 vb6_Replace 只剩 3 参).
        size_t searchFrom108c = 0;
        auto replaceInArgList108c = [&](const std::string& oldVal,
                                        const std::string& newVal) {
            size_t p = argList.find(oldVal, searchFrom108c);
            if (p == std::string::npos) return;
            argList.replace(p, oldVal.size(), newVal);
            searchFrom108c = p + newVal.size();
        };
        if (numericBuiltins108c.count(callee)) {
            for (auto& a : args) {
                if (a.compare(0, 12, "vb6_ComCall(") == 0) {
                    std::string wrapped =
                        "vb6_VariantToDouble(vb6_VariantFromComResult(" + a + "))";
                    replaceInArgList108c(a, wrapped);
                    a = wrapped;
                    argsFixed108c = true;
                }
            }
        }
        // Fix 108c-2: vb6_Val 形参为 BSTR, 收到"无类型 COM 调用结果"
        // (vb6_ComCall → void*, 指向 VARIANT) 时解包为字符串, 而非数值 —
        // 若走 numericBuiltins108c 的 vb6_VariantToDouble 包裹, double 传入
        // BSTR 形参报 C2440 (BalloonTooltips cTT.cls: Val(colTooltips(sKey))).
        if (callee == "vb6_Val") {
            for (auto& a : args) {
                bool isComResult = (a.compare(0, 12, "vb6_ComCall(") == 0
                                    || a.compare(0, 12, "vb6_ComGetProp(") == 0);
                if (isComResult) {
                    std::string wrapped =
                        "vb6_VariantToString(vb6_VariantFromComResult(" + a + "))";
                    replaceInArgList108c(a, wrapped);
                    a = wrapped;
                    argsFixed108c = true;
                }
            }
        }
        auto itStrPos = strArgPos108c.find(callee);
        if (itStrPos != strArgPos108c.end()) {
            for (size_t pos : itStrPos->second) {
                if (pos >= args.size() || pos >= node.positional.size()) continue;
                if (!node.positional[pos]) continue;
                Vb6Type at108c = inferExprType(*node.positional[pos]);
                if (at108c != Vb6Type::Single && at108c != Vb6Type::Double &&
                    at108c != Vb6Type::Long && at108c != Vb6Type::Integer &&
                    at108c != Vb6Type::Currency && at108c != Vb6Type::Decimal &&
                    at108c != Vb6Type::Boolean) {
                    continue;  // 已是字符串/对象等, 不介入
                }
                if (args[pos].find("vb6_CStr") != std::string::npos) continue;
                std::string wrapped108c;
                if (at108c == Vb6Type::Long || at108c == Vb6Type::Integer ||
                    at108c == Vb6Type::Boolean) {
                    wrapped108c = "vb6_CStrLong((int32_t)(" + args[pos] + "))";
                } else {
                    wrapped108c = "vb6_CStrDbl((double)(" + args[pos] + "))";
                }
                replaceInArgList108c(args[pos], wrapped108c);
                args[pos] = wrapped108c;
                argsFixed108c = true;
            }
        }

        // Fix 108c-3: vb6_ObjPtr(ifaceVar) — 接口引用是值结构体, 不能直接作为
        // uintptr_t 实参 (C2172). 对象指针位于 .obj 字段: 改写为 (<var>).obj.
        // mdlSubclass.bas: vb6_ObjPtr(Subclass) → vb6_ObjPtr((Subclass).obj).
        // 仅精确匹配裸接口变量名, 不触碰其它形式 (LongPtr 变量等走别处转换).
        if (callee == "vb6_ObjPtr" && !args.empty() && !node.positional.empty()
            && node.positional[0] && node.positional[0]->kind == ASTNodeKind::IdentifierExpr) {
            std::string aLower = args[0];
            std::transform(aLower.begin(), aLower.end(), aLower.begin(), ::tolower);
            if (knownIfaceVars_.count(aLower)) {
                std::string newVal = "(" + args[0] + ").obj";
                replaceInArgList108c(args[0], newVal);
                args[0] = newVal;
                argsFixed108c = true;
            }
        }
        (void)argsFixed108c;
    }

    lastExpr_ = callee + "(" + argList + ")";