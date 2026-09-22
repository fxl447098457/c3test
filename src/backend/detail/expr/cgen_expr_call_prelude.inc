// cgen_expr_call_prelude.inc - visit(IndexOrCallExpr&) 函数体片段：头部局部变量、ParamArray 识别与 Variant 数组索引识别
// 由 src/backend/expr/cgen_expr_call.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 13~299 行，逐行未改。

    // 检测数组访问: arr(i) — callee 是 IdentifierExpr 且在已知数组集合中
    // VB6 不区分数组索引和函数调用, 统一为 IndexOrCallExpr
    bool isArrayAccess = false;
    std::string arrName;
    std::string arrNameLower;  // Fix 056: 小写名称用于查找 arrayUdtElemTypes_ / arrayDimCounts_
    Vb6Type arrElemType = Vb6Type::Variant;



    // P14.1.5: Check if identifier is a ParamArray parameter of current procedure

    bool isParamArrayAccess = false;

    std::string paName;

    if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr && node.named.empty() && currentProc_) {

        auto& ident = static_cast<IdentifierExpr&>(*node.callee);

        for (auto& p : currentProc_->params) {

            if (p.isParamArray) {

                std::string pLower = p.name;

                std::transform(pLower.begin(), pLower.end(), pLower.begin(), ::tolower);

                std::string iLower = ident.name;

                std::transform(iLower.begin(), iLower.end(), iLower.begin(), ::tolower);

                if (pLower == iLower) {

                    isParamArrayAccess = true;

                    paName = cIdent(p.name);

                    break;

                }

            }

        }

    }



    if (isParamArrayAccess) {

        // ParamArray access: args(i) -> vb6_PA_GetLong(args, i) or vb6_PA_GetBSTR(args, i)

        if (node.positional.size() == 1) {

            emitExpr(*node.positional[0]);

            std::string index = std::move(lastExpr_);

            lastExpr_ = "vb6_PA_GetLong(" + paName + ", " + index + ")";

        } else {

            lastExpr_ = paName;  // bare reference to the SAFEARRAY*

        }

        return;

    }

    if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr && node.named.empty()) {
        auto& ident = static_cast<IdentifierExpr&>(*node.callee);
        std::string lower = ident.name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        // 先查已知数组集合 (cgen过程中维护)
        if (knownArrays_.count(lower)) {
            isArrayAccess = true;
            arrName = cIdent(ident.name);
            arrNameLower = lower;  // Fix 056: 保存小写名
            arrElemType = arrayElemTypes_[lower];
            // Fix 010o: 类模块成员数组需要 me-> 前缀 (除非是局部变量)
            if (isClassModule_ && currentProc_ && !knownLocalVars_.count(lower)) {
                arrName = "me->" + arrName;
            }
            // Fix 010r-6 rev2: ByRef array param needs dereference (*name) since it's now **
            if (currentProc_) {
                for (auto& param : currentProc_->params) {
                    if (Symbol::toLower(param.name) == lower && !param.isByVal
                        && (static_cast<uint16_t>(param.type) & static_cast<uint16_t>(Vb6Type::Array))) {
                        arrName = "(*" + arrName + ")";
                        break;
                    }
                }
            }
        } else {
            // 再查符号表 (模块级数组)
            Symbol* sym = symTab_.lookupModule(ident.name);
            if (sym && sym->kind == SymbolKind::Variable && sym->isArray) {
                isArrayAccess = true;
                arrName = cIdent(ident.name);
                arrNameLower = lower;  // Fix 056: 保存小写名
                arrElemType = sym->type;
                // Fix 010o: 类模块成员数组需要 me-> 前缀
                if (isClassModule_ && currentProc_ && !sym->isExternal && !knownLocalVars_.count(lower)) {
                    arrName = "me->" + arrName;
                }
                // Fix 010r-6 rev2: ByRef array param needs dereference (*name) since it's now **
                if (currentProc_) {
                    for (auto& param : currentProc_->params) {
                        if (Symbol::toLower(param.name) == lower && !param.isByVal
                            && (static_cast<uint16_t>(param.type) & static_cast<uint16_t>(Vb6Type::Array))) {
                            arrName = "(*" + arrName + ")";
                            break;
                        }
                    }
                }
            }
        }
    }
    // Variant数组索引: a(i) 其中a是Variant变量(可能持有SafeArray)
    // VB6: a = Array(1,2,3); MsgBox a(0) → vb6_VariantArrayGet(&a, 0)
    if (!isArrayAccess && node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr && node.named.empty() && !node.positional.empty()) {
        auto& vIdent = static_cast<IdentifierExpr&>(*node.callee);
        std::string vLower = vIdent.name;
        std::transform(vLower.begin(), vLower.end(), vLower.begin(), ::tolower);
        if (knownVariantVars_.count(vLower)) {
            emitExpr(*node.positional[0]);
            // Fix 084o: 索引为 Variant 时转 Long (vb6_VariantArrayGet 第二参是 int32_t)
            std::string vIndex = toLongIfVariant(std::move(lastExpr_), node.positional[0].get());
            // Fix 091k: ByRef Variant 参数在 C 侧已是 vb6_VARIANT* (cgen_util 参数映射
            // ByRef→指针), 再取址会传 vb6_VARIANT** → C4047/C4024 warning 且语义错
            // (cToolsArray prop_let_Extend/DeArray 的 Vars(i)/Arr(i)).
            std::string vArg = "&" + cIdent(vIdent.name);
            if (currentProc_) {
                for (auto& p091k : currentProc_->params) {
                    if (Symbol::toLower(p091k.name) != vLower) continue;
                    bool paArr091k = (static_cast<uint16_t>(p091k.type)
                                      & static_cast<uint16_t>(Vb6Type::Array)) != 0;
                    if (!p091k.isByVal && !paArr091k && !p091k.isParamArray) {
                        vArg = cIdent(vIdent.name);
                    }
                    break;
                }
            }
            lastExpr_ = "vb6_VariantArrayGet(" + vArg + ", " + vIndex + ")";
            return;
        }
    // P24-10: COM默认属性调用 — obj(args) 其中obj是COM变量, 等价于 obj.DefaultMember(args)
    // VB6: dict(0) → dict.Item(0), collection(1) → collection._Item(1)
    // DISPID_VALUE=0标识默认成员, 在TypeLib解析时已提取到Symbol::comDefaultMemberName
    if (!isArrayAccess && node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr && node.named.empty() && !node.positional.empty()) {
        auto& comIdent = static_cast<IdentifierExpr&>(*node.callee);
        std::string comLower = comIdent.name;
        std::transform(comLower.begin(), comLower.end(), comLower.begin(), ::tolower);

        // 前期绑定COM变量 (Dim d As Dictionary → knownTypedComVars_["d"] = &Dictionary)
        auto itTyped = knownTypedComVars_.find(comLower);
        if (itTyped != knownTypedComVars_.end() && !itTyped->second->comDefaultMemberName.empty()) {
            const Symbol* comSym = itTyped->second;
            emitExpr(*node.callee);
            std::string objExpr = std::move(lastExpr_);
            std::string defMember = comSym->comDefaultMemberRealName;

            // 查找默认成员签名以确定返回类型
            std::string defLower = comSym->comDefaultMemberName;
            auto itSig = comSym->comMethods.find(defLower);

            // 有参数的默认属性调用: obj(args) → vb6_ComCall*(obj, L"Item", args, argc)
            std::vector<std::string> packedArgs;
            for (size_t i = 0; i < node.positional.size(); i++) {
                std::string packFn = comPackExpr(*node.positional[i]);
                emitExpr(*node.positional[i]);
                { std::string resolved = resolveComMarkerForPack(packFn); if (!resolved.empty()) lastExpr_ = resolved; }
                packedArgs.push_back(packFn + "(" + lastExpr_ + ")");
            }
            int32_t argc = (int32_t)packedArgs.size();
            std::string argsArray = "(void*[]){";
            for (int i = 0; i < argc; i++) {
                if (i > 0) argsArray += ", ";
                argsArray += packedArgs[i];
            }
            argsArray += "}";

            std::string callArgs = objExpr + ", L\"" + defMember + "\", " + argsArray + ", " + std::to_string(argc);

            // 根据签名返回类型选择类型化调用函数
            if (itSig != comSym->comMethods.end()) {
                std::string returnType = mapType(itSig->second.returnType);
                if (returnType == "BSTR") {
                    lastExpr_ = "vb6_ComCallBSTR(" + callArgs + ")";
                } else if (returnType == "int32_t" || returnType == "int16_t") {
                    lastExpr_ = "vb6_ComCallInt(" + callArgs + ")";
                } else if (returnType == "double" || returnType == "float") {
                    lastExpr_ = "vb6_ComCallDouble(" + callArgs + ")";
                } else if (returnType == "void*") {
                    lastExpr_ = "vb6_ComCallObject(" + callArgs + ")";
                } else {
                    lastExpr_ = "vb6_ComCall(" + callArgs + ")";
                }
            } else {
                lastExpr_ = "vb6_ComCall(" + callArgs + ")";
            }
            return;
        }

        // P24-10 TODO: 后期绑定COM变量 (Dim obj As Object → knownObjectVars_)
        // 后期绑定的 obj(args) 需要VARIANT返回类型推导, 暂不支持
        // 前期绑定 (Dim d As Dictionary) 已完全支持
    }

    }


    if (isArrayAccess) {
        // P8.1: 数组元素访问, 支持多维
        emitExpr(*node.callee);
        std::string callee = std::move(lastExpr_);

        int dimCount = 1;
        std::string dcKey = arrNameLower.empty() ? arrName : arrNameLower;
        auto itDc = arrayDimCounts_.find(dcKey);
        if (itDc != arrayDimCounts_.end()) dimCount = itDc->second;

        // Fix 055: UDT数组元素使用实际UDT C类型而非vb6_VARIANT
        std::string elemCType;
        std::string udKey = arrNameLower.empty() ? arrName : arrNameLower;
        auto itUdtArr = arrayUdtElemTypes_.find(udKey);
        if (itUdtArr != arrayUdtElemTypes_.end()) {
            elemCType = itUdtArr->second;  // e.g. "vb6_type_RECT"
        } else {
            elemCType = mapSaElemCType(arrElemType);
        }

        // Fix 081f: Dynamic array params default dimCount=1, but VB6 arr(i,j) has 2 indices.
        // Use positional.size() as the authoritative dimCount when it's >= 2.
        int actualDimCount = dimCount;
        if ((int)node.positional.size() >= 2 && (int)node.positional.size() > dimCount) {
            actualDimCount = (int)node.positional.size();
        }

        // Fix 152: VB6 整体数组引用 `arr()` (空括号, 0 实参)。仅对 **2 维及以上**
        // 拦截为装箱: 此前 2D+ 落入下方按维数取元素的路径, 在 positional 为空时
        // 越界读 indices[0]/indices[1] → std::string 解引用垃圾 → 0xC0000005
        // (VBFlexGrid.ctl:7451 `SaveArray = StrArr()`)。
        // 一维 `arr()` (含 lvalue 整体数组赋值 `bColors() = ...`) 保持既有行为:
        // 走下方 first 分支生成 VB6_SA_AT(elem, arr, 0) (是左值, 可赋可读),
        // 不能改成 vb6_VariantArray(...) 否则 C2106 非左值 (Charts2020)。
        //
        // Fix 170 改写了这一口径: **空括号 = 整个数组**, 一维右值不再是 0 号元素
        // (VB6_SA_AT(elem, A, 0) 让 `StrToVar = B()` 只搬走第一个字节、
        //  `UBound(B())` 拿到的是字节而非载体), 左值更致命 —— 未 ReDim 的动态数组
        // data==NULL, 写 0 号元素即解引用 NULL → 0xC0000005
        // (Common.bas:1479 `B() = Text`, VBFlexGridDemo 启动崩溃)。
        // 载体名对左值/右值都合法 (赋值目标是 vb6_SafeArray1D*, 不再需要
        // vb6_VariantArray 包装 → 旧 C2106 顾虑不适用), 而 2D+ 右值仍保留 Fix 152
        // 的装箱 (赋给 Variant 目标时由 wrapVariantValue 处理)。
        if (node.positional.empty() && (wholeArrayLvalue_ || actualDimCount <= 1)) {
            lastExpr_ = arrName;
            return;
        }
        if (node.positional.empty() && actualDimCount >= 2) {
            lastExpr_ = "vb6_VariantArray((void*)" + arrName + ")";
            return;
        }

        if (actualDimCount == 1 || node.positional.size() == 1) {
            // 一维访问: arr(i) -> VB6_SA_AT(type, arr, i)
            std::string index = "0";
            if (!node.positional.empty()) {
                emitExpr(*node.positional[0]);
                // Fix 090an: Variant 下标 (如 CopyMemory 中 baBuffer(maxLen),
                // maxLen As Optional Variant ByRef) — VB6_SA_AT 宏内 [(idx)-lBound]
                // 对 vb6_VARIANT 做减法 → C2088. 与 084o 一致转 Long.
                index = toLongIfVariant(std::move(lastExpr_), node.positional[0].get());
            }
            lastExpr_ = "VB6_SA_AT(" + elemCType + ", " + arrName + ", " + index + ")";
        } else if (actualDimCount == 2 && node.positional.size() == 2) {
            // 二维访问: arr(i, j) -> VB6_SA_ND_AT2(type, (vb6_SafeArrayND*)arr, i, j)
            // Fix 056: 动态数组声明为vb6_SafeArray1D*但ReDim后可能是ND, 需要强转
            emitExpr(*node.positional[0]);
            std::string idx0 = toLongIfVariant(std::move(lastExpr_), node.positional[0].get());
            emitExpr(*node.positional[1]);
            std::string idx1 = toLongIfVariant(std::move(lastExpr_), node.positional[1].get());
            std::string ndArr = "(vb6_SafeArrayND*)" + arrName;
            lastExpr_ = "VB6_SA_ND_AT2(" + elemCType + ", " + ndArr + ", " + idx0 + ", " + idx1 + ")";
        } else if (actualDimCount == 3 && node.positional.size() == 3) {
            // 三维访问: arr(i, j, k) -> VB6_SA_ND_AT3(type, (vb6_SafeArrayND*)arr, i, j, k)
            emitExpr(*node.positional[0]);
            std::string idx0 = std::move(lastExpr_);
            emitExpr(*node.positional[1]);
            std::string idx1 = std::move(lastExpr_);
            emitExpr(*node.positional[2]);
            std::string idx2 = std::move(lastExpr_);
            std::string ndArr = "(vb6_SafeArrayND*)" + arrName;
            lastExpr_ = "VB6_SA_ND_AT3(" + elemCType + ", " + ndArr + ", " + idx0 + ", " + idx1 + ", " + idx2 + ")";
        } else {
            // 4+维: 通用通过vb6_SafeArrayND_Offset + 直接指针访问
            std::vector<std::string> indices;
            for (auto& arg : node.positional) {
                emitExpr(*arg);
                indices.push_back(std::move(lastExpr_));
            }
            // 构建indices数组 + offset计算
            std::string offVar = "_ndoff_" + std::to_string(tempCounter_++);
            c_.emitLine("int " + offVar + " = vb6_SafeArrayND_Offset(" + arrName + ", " + std::to_string(actualDimCount) + ", (int[]){" + indices[0] + ", " + indices[1] + "});");
            lastExpr_ = "((" + elemCType + "*)(((char*)" + arrName + "->data) + " + offVar + "))[0]";
        }
        return;
    }
