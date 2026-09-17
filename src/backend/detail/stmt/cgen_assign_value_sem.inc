// cgen_assign_value_sem.inc - visit(AssignmentStmt&) 函数体片段：值语义（返回值变量 / BSTR / Variant 赋值）
// 由 src/backend/stmt/cgen_assign.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 1056~1362 行，逐行未改。

    // 如果赋值目标是当前Function/PropertyGet名 (VB6语义: 设置返回值), 替换为返回值变量
    if (currentProc_ && (currentProc_->kind == SymbolKind::Function ||
                         currentProc_->kind == SymbolKind::PropertyGet)) {
        std::string procCName = cProcName(currentProc_->name, currentProc_->access, currentProc_->sourceModule);
        if (target == procCName) {
            target = currentReturnVar_;
        }
    }

    // BSTR赋值检测: 如果目标是BSTR变量, 使用vb6_BSTR_Assign防止悬垂指针和双重释放
    bool targetIsBstr = false;
    {
        // 检查目标是否是已知BSTR变量 (me->field 或 模块级变量)
        std::string checkName = target;
        if (checkName.substr(0, 4) == "me->") checkName = checkName.substr(4);  // 去掉me->前缀
        // Fix 025: (*name) 解引用形式 — ByRef BSTR/Variant 参数写穿透, 剥掉 (* ... ) 取内部名
        if (checkName.size() > 4 && checkName[0] == '(' && checkName[1] == '*'
            && checkName.back() == ')') {
            checkName = checkName.substr(2, checkName.size() - 3);
        }
        std::string lower = checkName;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (knownBstrVars_.count(lower)) targetIsBstr = true;
        // 也检查不含前缀的原始名 (Property Let参数等)
        if (node.target->kind == ASTNodeKind::IdentifierExpr) {
            auto& id = static_cast<IdentifierExpr&>(*node.target);
            std::string idLower = id.name;
            std::transform(idLower.begin(), idLower.end(), idLower.begin(), ::tolower);
            if (knownBstrVars_.count(idLower)) targetIsBstr = true;
        }
    }
    // P8.4: Variant赋值检测 - 如果目标是Variant变量, 用Variant构造函数包装值
    bool targetIsVariant = false;
    {
        std::string checkName = target;
        if (checkName.substr(0, 4) == "me->") checkName = checkName.substr(4);
        // Fix 025: (*name) 解引用形式 — ByRef Variant 参数 (Dim X As Variant 走 ByRef),
        //   codegen emit 写穿透为 (*X) = ...; 必须剥掉 (* ... ) 才能在 knownVariantVars_ 查到 X.
        if (checkName.size() > 4 && checkName[0] == '(' && checkName[1] == '*'
            && checkName.back() == ')') {
            checkName = checkName.substr(2, checkName.size() - 3);
        }
        std::string lower = checkName;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (knownVariantVars_.count(lower)) targetIsVariant = true;
        // Fix 054: 也检查 classVariantMembers_ — 类成员 Variant 字段 (如 me->m_P1)
        if (classVariantMembers_.count(lower)) targetIsVariant = true;
        if (node.target->kind == ASTNodeKind::IdentifierExpr) {
            auto& id = static_cast<IdentifierExpr&>(*node.target);
            std::string idLower = id.name;
            std::transform(idLower.begin(), idLower.end(), idLower.begin(), ::tolower);
            if (knownVariantVars_.count(idLower)) targetIsVariant = true;
        }
    }
    if (targetIsBstr) {
        // Fix 038b-6: BSTR 目标 + Variant 值 → 先提取 BSTR
        // 使用 cExprIsVariant (C 字符串级) + knownVariantVars_ 检测.
        bool valueIsVariant = cExprIsVariant(value);
        if (!valueIsVariant && node.value && node.value->kind == ASTNodeKind::IdentifierExpr) {
            auto& id = static_cast<IdentifierExpr&>(*node.value);
            std::string idLower = id.name;
            std::transform(idLower.begin(), idLower.end(), idLower.begin(), ::tolower);
            if (knownVariantVars_.count(idLower)) valueIsVariant = true;
        }
        // Fix 089j: RHS 为类 Variant 字段 (me->mParentsColKey 等) 也视为
        // Variant 表达式 — cExprIsVariant 只查函数前缀, me-> 成员不命中,
        // 导致 PropertyGet 里 `ret = me->VarField` 不转 → C2440.
        if (!valueIsVariant && value.compare(0, 4, "me->") == 0) {
            std::string rhsMember = value.substr(4);
            std::transform(rhsMember.begin(), rhsMember.end(), rhsMember.begin(), ::tolower);
            if (classVariantMembers_.count(rhsMember)) valueIsVariant = true;
        }
        if (valueIsVariant) {
            value = "vb6_VariantToString(" + value + ")";
        }
        c_.emitLine("vb6_BSTR_Assign(&" + target + ", " + value + ");");
    } else if (targetIsVariant) {
        // P8.4: 包装值为vb6_VARIANT, 先释放旧BSTR
        std::string wrappedValue = wrapVariantValue(node.value.get(), value);
        c_.emitLine("vb6_VariantClear(&" + target + ");");
        c_.emitLine(target + " = " + wrappedValue + ";");
    } else if (target.compare(0, 10, "vb6_PA_Get") == 0) {
        // Fix 091c: ParamArray 元素赋值 OutVars(i) = value — vb6_PA_GetXxx 是取值
        // 函数 (右值), 直接赋值 → C2106 "左操作数必须为左值" (cToolsArray.c 130);
        // 且 Variant RHS 传给具体类型 Set 形参需提取 → C2440 (cToolsArray.c 158/167).
        // 改写为 vb6_PA_SetXxx(args, <按类型提取后的 value>).
        size_t lp091c = target.find('(');
        size_t rp091c = target.rfind(')');
        if (lp091c != std::string::npos && rp091c != std::string::npos && rp091c > lp091c) {
            std::string args091c = target.substr(lp091c + 1, rp091c - lp091c - 1);
            std::string kind091c = target.substr(10, lp091c - 10);  // "Long"/"Double"/"BSTR"/...
            bool valIsVar091c = cExprIsVariant(value);
            if (!valIsVar091c && node.value && node.value->kind == ASTNodeKind::IdentifierExpr) {
                auto& id091c = static_cast<IdentifierExpr&>(*node.value);
                std::string idLower091c = id091c.name;
                std::transform(idLower091c.begin(), idLower091c.end(), idLower091c.begin(), ::tolower);
                if (knownVariantVars_.count(idLower091c)) valIsVar091c = true;
            }
            std::string conv091c = value;
            if (valIsVar091c) {
                if (kind091c == "Long" || kind091c == "LongPtr") {
                    conv091c = "vb6_VariantToLong(" + value + ")";
                } else if (kind091c == "Double") {
                    conv091c = "vb6_VariantToDouble(" + value + ")";
                } else if (kind091c == "BSTR") {
                    conv091c = "vb6_VariantToString(" + value + ")";
                }
            }
            c_.emitLine("vb6_PA_Set" + kind091c + "(" + args091c + ", " + conv091c + ");");
        } else {
            c_.emitLine(target + " = " + value + ";");
        }
    } else if (target.find("vb6_VariantArrayGet(") == 0) {
        // Fix 038 Group 2: vb6_VariantArrayGet(&arr, idx) = value
        //   → vb6_VariantArraySet(&arr, idx, vb6_VariantFromValue(value))
        // VariantArrayGet 返回右值, 不能赋值; 改写为 VariantArraySet.
        size_t argStart = target.find('(');
        size_t argEnd = target.rfind(')');
        if (argStart != std::string::npos && argEnd != std::string::npos && argEnd > argStart) {
            std::string args = target.substr(argStart + 1, argEnd - argStart - 1);
            c_.emitLine("vb6_VariantArraySet(" + args + ", vb6_VariantFromValue(" + value + "));");
        } else {
            c_.emitLine(target + " = " + value + ";");
        }
    } else if (target.find("VB6_SA_AT(vb6_VARIANT,") != std::string::npos) {
        // Fix 038 Group 1: VB6_SA_AT(vb6_VARIANT, arr, idx) = value
        //   Variant 数组元素赋值: value 不是 VARIANT 时需要用 VariantFromValue 包装.
        //   vb6_VariantFromValue 对已存在的 VARIANT 是 identity (no-op), 安全.
        c_.emitLine(target + " = vb6_VariantFromValue(" + value + ");");
    } else {
        // Fix 038b-6/045: 具体类型目标 + Variant 值 → 自动提取
        // 覆盖: TimeOut = obj.Method() (Method 返回 Variant, TimeOut 是 Long)
        //       VB6_SA_AT(BSTR, arr, i) = func() (func 返回 Variant)
        //       int32_t_var = vb6_VariantArrayGet(...)
        // 使用 cExprIsVariant (C 字符串级) + knownVariantVars_ 检测.
        bool valueIsVariant = cExprIsVariant(value);
        if (!valueIsVariant && node.value && node.value->kind == ASTNodeKind::IdentifierExpr) {
            auto& id = static_cast<IdentifierExpr&>(*node.value);
            std::string idLower = id.name;
            std::transform(idLower.begin(), idLower.end(), idLower.begin(), ::tolower);
            if (knownVariantVars_.count(idLower)) valueIsVariant = true;
        }
        // Fix 089j: RHS 为类 Variant 字段 (me->mParentsColKey 等) 也视为
        // Variant 表达式 — cExprIsVariant 只查函数前缀, me-> 成员不命中,
        // 导致 PropertyGet 里 `ret = me->VarField` 不转 → C2440.
        if (!valueIsVariant && value.compare(0, 4, "me->") == 0) {
            std::string rhsMember = value.substr(4);
            std::transform(rhsMember.begin(), rhsMember.end(), rhsMember.begin(), ::tolower);
            if (classVariantMembers_.count(rhsMember)) valueIsVariant = true;
        }
        // Fix 092c: RHS 是当前过程的返回变量 (vb6_ret_<Proc>) 且过程返回类型为
        // Variant (未声明返回类型 / As Variant) — cExprIsVariant 只认函数前缀,
        // 裸返回变量名不命中 → 目标为具体类型时不做提取 → C2440:
        //   cDialog 393: Function ShowOpen(...) As Variant 内
        //     If VarType(ShowOpen) = vbString Then mData.mstrFileName = ShowOpen
        //     → me->mData.mstrFileName = vb6_ret_ShowOpen (vb6_VARIANT→BSTR).
        if (!valueIsVariant && !currentReturnVar_.empty()
            && value == currentReturnVar_
            && currentReturnCType_ == "vb6_VARIANT") {
            valueIsVariant = true;
        }
        if (valueIsVariant) {
            // 检查目标类型
            std::string convertedValue = value;
            if (target.find("VB6_SA_AT(BSTR,") != std::string::npos
                || target.find("VB6_SA_ND_AT2(BSTR,") != std::string::npos) {
                convertedValue = "vb6_VariantToString(" + value + ")";
            } else if (target.find("VB6_SA_AT(int32_t,") != std::string::npos
                       || target.find("VB6_SA_AT(int16_t,") != std::string::npos
                       || target.find("VB6_SA_AT(uint8_t,") != std::string::npos
                       || target.find("VB6_SA_AT(LONG,") != std::string::npos
                       || target.find("VB6_SA_ND_AT2(int32_t,") != std::string::npos
                       || target.find("VB6_SA_ND_AT2(int16_t,") != std::string::npos
                       || target.find("VB6_SA_ND_AT2(uint8_t,") != std::string::npos
                       || target.find("VB6_SA_ND_AT2(LONG,") != std::string::npos) {
                convertedValue = "vb6_VariantToLong(" + value + ")";
            } else if (target.find("VB6_SA_AT(double,") != std::string::npos
                       || target.find("VB6_SA_AT(float,") != std::string::npos
                       || target.find("VB6_SA_ND_AT2(double,") != std::string::npos
                       || target.find("VB6_SA_ND_AT2(float,") != std::string::npos) {
                convertedValue = "vb6_VariantToDouble(" + value + ")";
            } else if (!currentReturnVar_.empty() && target == currentReturnVar_
                       && !currentReturnCType_.empty()) {
                // Fix 092f: 赋值目标是当前过程的返回变量 → 按返回 C 类型提取 Variant
                // 值. 返回变量不在 UDT 字段/数组元素判定内 (inferUdtFieldVb6Type
                // 对 vb6_ret_X 返回 Unknown) → 之前直接赋值触发 C2440:
                //   cHttpClient 399: Function ReturnBody() As Byte() 内
                //     ReturnBody = Inst.ResponseBody
                //     → vb6_ret_ReturnBody = vb6_VariantFromComResult(...)
                //       (vb6_VARIANT → vb6_SafeArray1D*).
                std::string retCt092f = currentReturnCType_;
                retCt092f.erase(std::remove(retCt092f.begin(), retCt092f.end(), ' '),
                                retCt092f.end());
                if (retCt092f == "vb6_SafeArray1D*") {
                    convertedValue = "vb6_VariantToSafeArray1D(" + value + ")";
                } else if (retCt092f == "BSTR") {
                    convertedValue = "vb6_VariantToString(" + value + ")";
                } else if (retCt092f == "void*") {
                    convertedValue = "vb6_VariantToObjectVal(" + value + ")";
                } else if (retCt092f == "double" || retCt092f == "float") {
                    convertedValue = "vb6_VariantToDouble(" + value + ")";
                } else if (retCt092f == "int32_t" || retCt092f == "int16_t"
                           || retCt092f == "uint8_t" || retCt092f == "LONG"
                           || retCt092f == "int64_t") {
                    convertedValue = "vb6_VariantToLong(" + value + ")";
                }
            } else {
                // Fix 084n: UDT 字段目标 — 按字段 Vb6Type 转换 Variant RHS
                // (如 cZipArchive 的 .FileName As String ← vb6_VariantArrayGet(...)
                //  → vb6_VariantToString; uBuf.MaxMatch As Long ← At() → vb6_VariantToLong)
                Vb6Type udtFieldT = inferUdtFieldVb6Type(node.target.get());
                if (udtFieldT == Vb6Type::String) {
                    convertedValue = "vb6_VariantToString(" + value + ")";
                } else if (udtFieldT == Vb6Type::Long || udtFieldT == Vb6Type::Integer
                           || udtFieldT == Vb6Type::Byte || udtFieldT == Vb6Type::Boolean
                           || udtFieldT == Vb6Type::ULong || udtFieldT == Vb6Type::LongPtr) {
                    convertedValue = "vb6_VariantToLong(" + value + ")";
                } else if (udtFieldT == Vb6Type::Double || udtFieldT == Vb6Type::Single
                           || udtFieldT == Vb6Type::Currency || udtFieldT == Vb6Type::Date) {
                    convertedValue = "vb6_VariantToDouble(" + value + ")";
                } else if (udtFieldT == Vb6Type::Object) {
                    convertedValue = "vb6_VariantToObjectVal(" + value + ")";
                } else if ((static_cast<uint16_t>(udtFieldT) & static_cast<uint16_t>(Vb6Type::Array))
                           == static_cast<uint16_t>(Vb6Type::Array)) {
                    convertedValue = "vb6_VariantToSafeArray1D(" + value + ")";
                }
                // 若已按 UDT 字段类型转换, 跳过已知变量集合检测
                if (convertedValue != value) {
                    // UDT 字段转换已完成
                } else {
                // 检查已知 Long/Double/ByteArray 变量
                std::string checkName = target;
                if (checkName.substr(0, 4) == "me->") checkName = checkName.substr(4);
                if (checkName.size() > 4 && checkName[0] == '(' && checkName[1] == '*'
                    && checkName.back() == ')') {
                    checkName = checkName.substr(2, checkName.size() - 3);
                }
                std::string lower = checkName;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if (knownLongVars_.count(lower)) {
                    convertedValue = "vb6_VariantToLong(" + value + ")";
                } else if (knownDoubleVars_.count(lower)) {
                    convertedValue = "vb6_VariantToDouble(" + value + ")";
                } else if (knownObjectVars_.count(lower)) {
                    // Fix 045: Object (void*) target + Variant value → extract object
                    convertedValue = "vb6_VariantToObjectVal(" + value + ")";
                } else if (knownByteArrayVars_.count(lower)) {
                    // Fix 062: Byte array target + Variant value → extract SafeArray1D*
                    convertedValue = "vb6_VariantToSafeArray1D(" + value + ")";
                } else if (knownClassVars_.count(lower)) {
                    // Fix 086: 类类型目标 (vb6_cls_X*) + Variant值 → 提取对象指针
                    convertedValue = "(vb6_cls_" + cIdent(knownClassVars_[lower])
                                   + "*)vb6_VariantToObjectVal(" + value + ")";
                } else if (knownArrays_.count(lower)) {
                    // Fix 086: 数组目标 (vb6_SafeArray1D*) + Variant值 → 提取数组
                    convertedValue = "vb6_VariantToSafeArray1D(" + value + ")";
                } else if (node.target && node.target->kind == ASTNodeKind::IdentifierExpr) {
                    auto& id = static_cast<IdentifierExpr&>(*node.target);
                    std::string idLower = id.name;
                    std::transform(idLower.begin(), idLower.end(), idLower.begin(), ::tolower);
                    if (knownLongVars_.count(idLower)) {
                        convertedValue = "vb6_VariantToLong(" + value + ")";
                    } else if (knownDoubleVars_.count(idLower)) {
                        convertedValue = "vb6_VariantToDouble(" + value + ")";
                    } else if (knownObjectVars_.count(idLower)) {
                        convertedValue = "vb6_VariantToObjectVal(" + value + ")";
                    } else if (knownByteArrayVars_.count(idLower)) {
                        convertedValue = "vb6_VariantToSafeArray1D(" + value + ")";
                    } else if (knownClassVars_.count(idLower)) {
                        // Fix 086: 类类型目标 (vb6_cls_X*) + Variant值
                        convertedValue = "(vb6_cls_" + cIdent(knownClassVars_[idLower])
                                       + "*)vb6_VariantToObjectVal(" + value + ")";
                    } else if (knownArrays_.count(idLower)) {
                        // Fix 086: 数组目标 (vb6_SafeArray1D*) + Variant值
                        convertedValue = "vb6_VariantToSafeArray1D(" + value + ")";
                    }
                }
                }
            }
            c_.emitLine(target + " = " + convertedValue + ";");
        } else {
            // Fix 091l: Variant 目标 ← 具体类型值 (SafeArray1D*/数组载体/标量/COM 结果).
            // C 侧目标是 vb6_VARIANT 结构体, 直接赋 SafeArray1D* 等 → C2440
            // (ToolsLogs: Dim EnumLevelNames As Variant ← Array(...) 物化 _arr_0).
            // vb6_VariantFromValue 是 _Generic 宏: 标量→Variant{Long,Int,Double,Bool,
            // Byte}, BSTR→VariantString, SafeArray1D*→VariantArray, 指针→VariantObject.
            std::string tgtKey091l = target;
            if (tgtKey091l.compare(0, 4, "me->") == 0) {
                tgtKey091l = tgtKey091l.substr(4);
            } else if (tgtKey091l.size() > 4 && tgtKey091l[0] == '(' && tgtKey091l[1] == '*'
                       && tgtKey091l.back() == ')') {
                tgtKey091l = tgtKey091l.substr(2, tgtKey091l.size() - 3);
            }
            std::string tgtLower091l = tgtKey091l;
            std::transform(tgtLower091l.begin(), tgtLower091l.end(),
                           tgtLower091l.begin(), ::tolower);
            bool tgtIsVariant091l = knownVariantVars_.count(tgtLower091l)
                                    || classVariantMembers_.count(tgtLower091l);
            if (!tgtIsVariant091l && !valueIsVariant) {
                tgtIsVariant091l = cExprIsVariant(target);
            }
            if (tgtIsVariant091l && !valueIsVariant && !value.empty()) {
                value = "vb6_VariantFromValue(" + value + ")";
            }
            // Fix 092w: Byte 数组目标 + 具体值 (StrConv/字符串) → 字节数组语义
            if (tgtIsByteArray092w && !tgtIsVariant091l && !value.empty()) {
                value = rewriteByteArrayValue(value);
            }
            c_.emitLine(target + " = " + value + ";");
        }
    }