// cgen_expr_member_precheck.inc - 函数体片段：前置分派 —— AddressOf 模块限定函数 / Err·内建对象 / 控件数组 / 内置对象方法
// 由 src/backend/expr/cgen_expr_member.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 13~131 行，逐行未改。

    // Fix 086: AddressOf 模块限定函数 (AddressOf ToolsTlsThunks.pvCallCollectionItem)
    // — 解析器将 `ToolsTlsThunks.pvCallCollectionItem` 交给 AddressOfExpr(仅取
    // object名) + 外层MemberAccessExpr(成员名). 此前生成非法的
    // (void*)vb6_<mod>_<mod>.<member> (C2065). 在此合成为真正的函数指针.
    if (node.object && node.object->kind == ASTNodeKind::AddressOfExpr) {
        auto& ao86 = static_cast<AddressOfExpr&>(*node.object);
        (void)ao86;
        std::string fnName86 = node.memberName;
        Symbol* fnSym86 = symTab_.lookupModule(fnName86);
        if (!fnSym86) fnSym86 = symTab_.lookup(fnName86);
        if (fnSym86 && (fnSym86->kind == SymbolKind::Sub || fnSym86->kind == SymbolKind::Function)) {
            lastExpr_ = "(void*)" + cProcName(fnName86, fnSym86->access,
                fnSym86->isExternal ? fnSym86->sourceModule : "");
        } else {
            lastExpr_ = "(void*)" + cProcName(fnName86, AccessLevel::Private);
        }
        return;
    }
    // Fix 010k: Err/builtin object member access — check FIRST before anything else
    // This must be at the very top to avoid any other code path consuming the node
    if (node.object && node.object->kind == ASTNodeKind::IdentifierExpr) {
        auto& _objId = static_cast<IdentifierExpr&>(*node.object);
        std::string _objLower = _objId.name;
        std::transform(_objLower.begin(), _objLower.end(), _objLower.begin(), ::tolower);
        std::string _memLower = node.memberName;
        std::transform(_memLower.begin(), _memLower.end(), _memLower.begin(), ::tolower);

        if (_objLower == "err" || _objLower == "lasterror") {
            // Fix 056b: LastError 属性返回 VBA.ErrObject (全局单例),
            // .Number/.Description/.Source 等价于 Err.Number 等.
            if (_memLower == "number")      { lastExpr_ = "vb6_ErrNumber()";      return; }
            if (_memLower == "description") { lastExpr_ = "vb6_ErrDescription()"; return; }
            if (_memLower == "source")      { lastExpr_ = "vb6_ErrSource()";      return; }
            if (_memLower == "lastdllerror") { lastExpr_ = "GetLastError()";       return; }
            if (_memLower == "helpfile")    { lastExpr_ = "(BSTR)0";              return; }
            if (_memLower == "helpcontext") { lastExpr_ = "0";                    return; }
            if (_memLower == "clear")       { lastExpr_ = "vb6_ErrClear";         return; }
            if (_memLower == "raise")       { lastExpr_ = "vb6_ErrRaise";         return; }
        }

        // Fix 084y-6: VBA 模块成员 (VBA.vbCr / VBA.Replace 等). 编译器不注册
        // "VBA" 模块符号 → M22 fallback 生成 vb6_VBA_<member> (rtl 无此名 →
        // C2065). 常量内联字面量; 函数生成裸 rtl 名 vb6_<member> (去 $ 后缀,
        // 如 VBA.Mid$ → vb6_Mid), 由外层 IndexOrCallExpr 的 Fix 033 回退
        // (lookupModule + lookup 全表) 解析参数表并填充 Optional 默认值.
        if (_objLower == "vba" || _objLower == "vba5") {
            static const std::unordered_map<std::string, std::string> vbaConsts = {
                {"vbcr", "13"}, {"vblf", "10"}, {"vbtab", "9"},
                {"vbnullchar", "0"}, {"vbnullstring", "vb6_BSTR_FromStr(L\"\")"},
                {"vbcrif", "vb6_BSTR_FromStr(L\"\\r\\n\")"},
                {"vbnewline", "vb6_BSTR_FromStr(L\"\\r\\n\")"},
                {"vbempty", "0"}, {"vbnull", "1"}, {"vbinteger", "2"}, {"vblong", "3"},
                {"vbsingle", "4"}, {"vbdouble", "5"}, {"vbcurrency", "6"}, {"vbdate", "7"},
                {"vbstring", "8"}, {"vbobject", "9"}, {"vberror", "10"}, {"vbboolean", "11"},
                {"vbvariant", "12"}, {"vbdecimal", "14"}, {"vbbyte", "17"}, {"vblonglong", "20"},
                {"vbuserdefinedtype", "36"}, {"vbarray", "8192"},
                {"vbtrue", "-1"}, {"vbfalse", "0"},
            };
            auto itC = vbaConsts.find(_memLower);
            if (itC != vbaConsts.end()) { lastExpr_ = itC->second; return; }
            std::string fnName = node.memberName;
            if (!fnName.empty() && fnName.back() == '$') fnName.pop_back();
            Symbol* vbaFn = symTab_.lookup(fnName);
            if (vbaFn && (vbaFn->kind == SymbolKind::Function || vbaFn->kind == SymbolKind::Sub
                          || vbaFn->kind == SymbolKind::PropertyGet
                          || vbaFn->kind == SymbolKind::DeclareSub
                          || vbaFn->kind == SymbolKind::DeclareFunc)) {
                // Fix 086: VBA.Now() 等无参内置函数作为裸值引用 (非callee上下文,
                // 如 pvToFileTime(VBA.Now)) — 需带调用括号, 否则函数设计符裸名 → C2440
                static const std::unordered_set<std::string> vbaZeroArgFns = {
                    "now", "date", "time", "timer", "freefile", "command",
                    "curdir", "erl", "doevents"
                };
                // Fix 093a: 用符号表里的声明拼写 (VBA.varType → vb6_VarType). 此前用
                // 源码拼写 → vb6_varType, 与 RTL/intrinsic 符号名不一致 → LNK2019.
                std::string fnCanon = vbaFn->name.empty() ? fnName : vbaFn->name;
                std::string fnLower86 = Symbol::toLower(fnCanon);
                if (fnLower86 == "rnd") {
                    lastExpr_ = "vb6_Rnd(1)";
                } else if (vbaZeroArgFns.count(fnLower86)) {
                    lastExpr_ = "vb6_" + cIdent(fnCanon) + "()";
                } else {
                    lastExpr_ = "vb6_" + cIdent(fnCanon);
                }
                return;
            }
        }
    }

    // Fix 110n: CallByName(...).成员 — RTL 的 vb6_CallByName 返回 vb6_VARIANT, 其值
    // 可能是对象 (VbGet 取到 Font 等对象属性). 对 VARIANT 结构体取成员在 C 里非法:
    //   CallByName(oCtrl, PropFont, VbGet).Size
    //     → vb6_CallByName(...).Size  C2039 ("Size" 不是 "vb6_VARIANT" 的成员)
    // 赋值时更会退化成 "<struct> = <double>" C2088. 必须走 COM 晚绑定: 先从 VARIANT
    // 取出对象 (vb6_VariantToObjectVal) 再 vb6_ComGetXxxProp / ComSetProp.
    // 读上下文由外层 (BinaryExpr/赋值) 的 resolveComValue 消费 marker;
    // 写上下文由 AssignmentStmt 的 COM SetProp 分支消费.
    // 实测: Charts 2020 ClsResizer.cls 107/147.
    if (node.object && node.object->kind == ASTNodeKind::IndexOrCallExpr) {
        auto& cbCall = static_cast<IndexOrCallExpr&>(*node.object);
        if (cbCall.callee && cbCall.callee->kind == ASTNodeKind::IdentifierExpr) {
            auto& cbId = static_cast<IdentifierExpr&>(*cbCall.callee);
            if (Symbol::toLower(cbId.name) == "callbyname") {
                emitExpr(*node.object);
                std::string cbObjExpr = std::move(lastExpr_);
                comObjExpr_ = "vb6_VariantToObjectVal(" + cbObjExpr + ")";
                comMemberName_ = node.memberName;
                isComMarker_ = true;
                isEarlyBoundCom_ = false;
                earlyBoundSym_ = nullptr;
                lastExpr_ = comObjExpr_;
                return;
            }
        }
    }

    // P7.6: 控件数组属性读取 ctrlArr(idx).Property
    // 必须在IdentifierExpr分支前检查, 因为cmdBtn(0)的object是IndexOrCallExpr
    if (node.object && node.object->kind == ASTNodeKind::IndexOrCallExpr) {
        auto& idxExpr = static_cast<IndexOrCallExpr&>(*node.object);
        if (idxExpr.callee && idxExpr.callee->kind == ASTNodeKind::IdentifierExpr) {
            auto& arrIdent = static_cast<IdentifierExpr&>(*idxExpr.callee);
            std::string arrNameLower = arrIdent.name;
            std::transform(arrNameLower.begin(), arrNameLower.end(), arrNameLower.begin(), ::tolower);
            if (knownControlArrays_.count(arrNameLower)) {
                auto itCtrl = knownFormControls_.find(arrNameLower);
                if (itCtrl != knownFormControls_.end()) {
                    std::string readFn = getControlPropReadFn(itCtrl->second, node.memberName);
                    if (!readFn.empty()) {
                        std::string idxArg;
                        if (!idxExpr.positional.empty()) {
                            emitExpr(*idxExpr.positional[0]);
                            idxArg = std::move(lastExpr_);
                        } else {
                            idxArg = "0";
                        }
                        lastExpr_ = readFn + "(vb6_CtrlArr_GetAt(&vb6_arr_" + cIdent(arrIdent.name) + ", " + idxArg + "))";
                        return;
                    }
                }
            }
        }
    }
    // 内置对象方法: Debug.Print → vb6_DebugPrint
    // 检查 object 是否是 IdentifierExpr
