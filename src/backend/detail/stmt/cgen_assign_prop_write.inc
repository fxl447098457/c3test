// cgen_assign_prop_write.inc - visit(AssignmentStmt&) 函数体片段：属性写入分派（类 Property Let / 默认属性 / With 块 / 链式 COM）
// 由 src/backend/stmt/cgen_assign.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 406~828 行，逐行未改。

    // P6.7: 检测类Property Let赋值: obj.Prop = value → vb6_prop_let_Prop(obj, value)
    // 在emitExpr左侧前, 先检查target是否为MemberAccessExpr且成员是Property
    if (node.target->kind == ASTNodeKind::MemberAccessExpr) {
        auto& maExpr = static_cast<MemberAccessExpr&>(*node.target);
        // 查找Property Let符号
        auto* propLetSym = symTab_.lookupModuleByKind(maExpr.memberName, SymbolKind::PropertyLet);
        if (!propLetSym) {
            // 也尝试Property Set
            propLetSym = symTab_.lookupModuleByKind(maExpr.memberName, SymbolKind::PropertySet);
        }
        if (propLetSym) {
            // 检查对象是否是类实例变量
            if (maExpr.object && maExpr.object->kind == ASTNodeKind::IdentifierExpr) {
                auto& objId = static_cast<IdentifierExpr&>(*maExpr.object);
                std::string objLower = objId.name;
                std::transform(objLower.begin(), objLower.end(), objLower.begin(), ::tolower);
                std::string objClass;
                auto itClassVar = knownClassVars_.find(objLower);
                if (itClassVar != knownClassVars_.end()) {
                    objClass = itClassVar->second;
                } else {
                    // Fix 084g: 局部变量/参数类对象 (如 Dim Response As cHttpServerResponse)
                    // 不在 knownClassVars_ 中, 通过符号表推断类名
                    objClass = inferClassTypeOfExpr(*maExpr.object);
                }
                // Fix 084g-2: 外部注入的跨类属性符号 (Friend/Public Property) —
                // lookupModuleByKind 已确认该成员是某类的属性, 即使对象类型无法
                // 从符号表推断 (cgen阶段访问不到过程作用域的局部变量类型), 也
                // 生成属性调用; sourceModule 由外部符号提供.
                // Fix 084j: UDT 变量的成员是结构体字段而非类属性 — lookupModuleByKind
                // 会全局命中同名类属性 (如 dcb As MODBUS_DCB 的 .BaudRate 命中
                // cModbusSlave.BaudRate; m_uCtx 的 .LastError 命中 cZipArchive.LastError),
                // 误生成 vb6_cX_prop_let_Y(udtVar, ...) → C2440. 此类对象必须跳过,
                // 交给 visit(MemberAccessExpr) 的 Fix 031 UDT 字段路径生成 obj.field.
                // Fix 084y: 对象类与属性所属类不一致时, 该属性是其他类的同名成员,
                // 不属于当前对象 — lookupModuleByKind 全局命中, 需校验类归属.
                // 例: Set oCallback.Socket = New cTlsReMaster 中 oCallback 是
                // cClientCallback, Socket 是数据字段, 但全局命中 cTlsSocket.Socket
                // PropertySet → 原生成 vb6_cTlsSocket_prop_set_Socket(oCallback,..)
                // (C2065 未声明 + C2440 参数类型错误). 必须跳过属性路径, 交给
                // visit(MemberAccessExpr) 生成 oCallback->Socket = ... 字段赋值.
                bool objClassMatchesProp = true;
                // Fix 093a: 属性符号与对象类必须同类. 非 external 符号 (属性定义于
                // 本模块) 的"所属类"即当前模块名 — 此前只在 isExternal 时校验, 导致
                // `HttpSvr.MaxCacheFileSize = v` (MaxCacheFileSize 在本类 cHttpServer
                // 是 Property, 而 HttpSvr 是 cHttpServerSvr, 该名在后者是公有字段)
                // 误发 vb6_cHttpServerSvr_prop_let_MaxCacheFileSize → LNK2019
                // (cSSEClient.RequestTimeOut / cHttpServerSvr.CacheTTLSeconds 同类).
                if (!objClass.empty()) {
                    std::string propModLower = propLetSym->isExternal
                        ? propLetSym->sourceModule
                        : (isClassModule_ ? moduleName_ : std::string());
                    std::transform(propModLower.begin(), propModLower.end(), propModLower.begin(), ::tolower);
                    std::string objClassLower = objClass;
                    std::transform(objClassLower.begin(), objClassLower.end(), objClassLower.begin(), ::tolower);
                    objClassMatchesProp = (propModLower == objClassLower);
                }
                // Fix 092b: objClass 无法推断且对象是 Object/COM 后期绑定变量时,
                // 该属性写应走 COM 路径 (vb6_ComSetProp / tryEmitChainedComWrite) —
                // 否则 lookupModuleByKind 全局命中他类同名 Property Let, 误生成
                // 类属性调用 (cToolsList 25: Rs As Object 的 COM 属性写
                // "Rs.Filter = ..." → vb6_cDialog_prop_let_Filter(Rs, ...),
                // C2440 "vb6_VARIANT→BSTR" 并丢失 COM 后期绑定语义).
                // Fix 084g-2 的"objClass 空 + isExternal 即生成"过宽, 此处收窄.
                bool objIsComVar092b = false;
                if (objClass.empty()) {
                    objIsComVar092b = knownObjectVars_.count(objLower) > 0
                                      || knownTypedComVars_.count(objLower) > 0;
                }
                if (!objIsComVar092b && !knownUdtVars_.count(objLower) && objClassMatchesProp &&
                    (!objClass.empty() || propLetSym->isExternal)) {
                    // 生成Property Let调用: vb6_prop_let_Name(obj, value)
                    std::string prefix = (propLetSym->kind == SymbolKind::PropertySet) ? "prop_set_" : "prop_let_";
                    // Fix 010r-10: 使用map中的类名作为sourceModule
                    std::string sourceModule = propLetSym->isExternal ? propLetSym->sourceModule : objClass;
                    // Fix 093a: 成员名按类内声明拼写规范化 (VB6 大小写不敏感),
                    // 避免调用点拼写与类定义不一致 → LNK2019.
                    std::string funcName = cProcName(
                        prefix + canonicalClassMemberName(sourceModule, maExpr.memberName),
                        propLetSym->access, sourceModule);
                    emitExpr(*maExpr.object);
                    std::string objExpr = std::move(lastExpr_);
                    // Fix 133y: 目标是工程内 UserControl 子控件 (czUI:czLabel3.Caption,
                    // ucChartBar1.ChartStyle 等)。对象在 knownUserControlCtrlVars_
                    // (Fix 112 已登记), 裸 emitExpr 输出控件名标识符 (无声明) →
                    // C2065。this 应取宿主窗口的 UserControl 实例 (与读取路径
                    // cgen_expr_member_form_builtin.inc 一致)。
                    auto itUC133y = knownUserControlCtrlVars_.find(objLower);
                    if (itUC133y != knownUserControlCtrlVars_.end()) {
                        auto itCtrl133y = knownFormControls_.find(objLower);
                        std::string hwndArg133y = (itCtrl133y != knownFormControls_.end())
                            ? makeCtrlHwndArg(objLower, itCtrl133y->second)
                            : ("vb6_hwnd_" + cIdent(objId.name));
                        objExpr = "(vb6_cls_" + cIdent(itUC133y->second)
                                + "*)vb6_UC_InstanceOf(" + hwndArg133y + ")";
                    }
                    emitExpr(*node.value);
                    std::string valExpr = std::move(lastExpr_);
                    // Fix 091n: 按 Let/Set **写方向**参数表适配值实参 —
                    //  · Variant 形参 ← 具体类型值 (Byte 数组/SafeArray1D* 等) 需
                    //    packLetValueArg 打包 (cWinsock: oClient.UserData = baBuffer,
                    //    实参 vb6_SafeArray1D* → C2440 "→ vb6_VARIANT");
                    //  · BSTR 形参 ← Variant 值 (COM 属性结果) 需提取
                    //    (cToolsList: Rs.Filter = <ComGetProp 结果>, C2440 "→ BSTR").
                    // 注意不能用裸 propLetSym->params: lookupModuleByKind 会全局命中
                    // 他类同名属性 (其 Variant 参数表) → 误打包 → 大规模 C2440
                    // (cHttpServer/cDataBase/cJson 的 Rs.Status = 200 等).
                    {
                        std::vector<ParameterInfo> wp091n;
                        bool isSet091n = (propLetSym->kind == SymbolKind::PropertySet);
                        if (!sourceModule.empty()
                            && findClassMemberWriteParams(sourceModule, maExpr.memberName,
                                                          isSet091n, wp091n)
                            && !wp091n.empty()) {
                            const ParameterInfo& lastP091n = wp091n.back();
                            if (lastP091n.type == Vb6Type::Variant) {
                                // 仅打包"数组载体"值: 直接给具体类型标量 (如
                                // Dictionary.CompareMode = 1, C 形参 int32_t)
                                // 打包会 C2440 — 符号表 Variant 判定对 COM/后期
                                // 绑定属性不可靠.
                                bool arrCarrier091n = valExpr.find("vb6_SafeArray1D") != std::string::npos
                                    || valExpr.rfind("_arr_", 0) == 0
                                    || valExpr.find("vb6_Split(") == 0
                                    || valExpr.find("vb6_ArrayCreate") == 0
                                    || valExpr.find("vb6_VariantToSafeArray1D") == 0;
                                if (!arrCarrier091n && node.value
                                    && node.value->kind == ASTNodeKind::IdentifierExpr) {
                                    std::string vLower091n = Symbol::toLower(
                                        static_cast<IdentifierExpr&>(*node.value).name);
                                    arrCarrier091n = knownArrays_.count(vLower091n) > 0
                                                     || knownByteArrayVars_.count(vLower091n) > 0;
                                }
                                if (arrCarrier091n) {
                                    valExpr = packLetValueArg(lastP091n, node.value.get(), valExpr);
                                }
                            } else if (lastP091n.type == Vb6Type::String
                                       && cExprIsVariant(valExpr)) {
                                valExpr = wrapToBSTR(valExpr, *node.value);
                            }
                        }
                    }
                    c_.emitLine(funcName + "(" + objExpr + ", " + valExpr + ");  /* Property Let */");
                    return;
                }
            }
        }
    }

    // P11.7: 默认属性写入 — 如果赋值目标是已知窗体控件标识符 (如 Label1 = value)
    // 自动转换为默认属性写入 (如 Label1.Caption = value → vb6_SetControlText(...))
    if (node.target->kind == ASTNodeKind::IdentifierExpr) {
        auto& tgtId = static_cast<IdentifierExpr&>(*node.target);
        std::string tgtLower = tgtId.name;
        std::transform(tgtLower.begin(), tgtLower.end(), tgtLower.begin(), ::tolower);
        // Fix 056b: 裸标识符目标可能是类属性 (pvState = sckOpen → vb6_<Class>_prop_let_pvState(me, value))
        // 之前漏到此路径生成裸名赋值 → C2065 "未声明的标识符"
        // Fix 083a: 当前过程的参数/局部变量优先于属性符号 — 例如 SyncReceiveText 的
        // Optional ByVal TimeOut 参数会被全局符号表误匹配为 cHttpServerSession.TimeOut 属性,
        // 生成 vb6_cHttpServerSession_prop_let_TimeOut(me, ...) → 错误调用
        {
            bool isLocalTarget = knownLocalVars_.count(tgtLower);
            if (!isLocalTarget && currentProc_) {
                for (const auto& p : currentProc_->params) {
                    std::string pLower = p.name;
                    std::transform(pLower.begin(), pLower.end(), pLower.begin(), ::tolower);
                    if (pLower == tgtLower) { isLocalTarget = true; break; }
                }
            }
            Symbol* propSym = nullptr;
            if (!isLocalTarget) propSym = symTab_.lookupModuleByKind(tgtId.name, SymbolKind::PropertyLet);
            if (!propSym && !isLocalTarget) propSym = symTab_.lookupModuleByKind(tgtId.name, SymbolKind::PropertySet);
            if (propSym) {
                // 属性过程内部 PropertyName = value 是设置返回值, 不是PropertyLet调用
                // (如 Property Get pvThunkGlobalData 内: pvThunkGlobalData = Val(...) → vb6_ret_xxx = ...)
                // Fix 090f: 普通 Function 内 "FuncName = expr" 也是返回赋值 (VB6 允许,
                // 如 cCollection.Items(): Items = Array()). 此前仅识别 Property 过程,
                // 当全局属性符号与函数同名时 (cJson.Items Property Let/Set 存在,
                // lookupModuleByKind 命中), 被误拦截生成 vb6_cJson_prop_let_Items(me,..)
                // → C2198 参数太少 (Items() 无参却按 2 参带参属性拼调用).
                bool isAssigningReturnValue = false;
                if (currentProc_) {
                    std::string curName = currentProc_->name;
                    std::transform(curName.begin(), curName.end(), curName.begin(), ::tolower);
                    std::string tgtName = tgtId.name;
                    std::transform(tgtName.begin(), tgtName.end(), tgtName.begin(), ::tolower);
                    if (curName == tgtName &&
                        (currentProc_->kind == SymbolKind::Function ||
                         currentProc_->kind == SymbolKind::PropertyGet ||
                         currentProc_->kind == SymbolKind::PropertyLet ||
                         currentProc_->kind == SymbolKind::PropertySet)) {
                        isAssigningReturnValue = true;
                    }
                }
                // Fix 093a: 本类成员字段优先 — `recvBuffer = data` (cClientCallback.cls,
                // recvBuffer 是本类字段) 被全局符号表误命中 cWinsock 的 Property Let
                // RecvBuffer → vb6_cWinsock_prop_let_recvBuffer((void*)me, ...) → LNK2019.
                // 命中本类字段时不走属性路径, 交给下方通用赋值生成 me->recvBuffer = ...;
                if (!isAssigningReturnValue && !isOwnClassField(tgtId.name)) {
                    std::string prefix = (propSym->kind == SymbolKind::PropertySet) ? "prop_set_" : "prop_let_";
                    std::string funcName = cProcName(prefix + tgtId.name, propSym->access,
                        propSym->isExternal ? propSym->sourceModule : (isClassModule_ ? moduleName_ : ""));
                    emitExpr(*node.value);
                    std::string valExpr = std::move(lastExpr_);
                    c_.emitLine(funcName + (isClassModule_ ? "((void*)me, " : "(") + valExpr + ");  /* Property Let/Set (bare) */");
                    return;
                }
            }
        }
        auto itCtrl = knownFormControls_.find(tgtLower);
        if (itCtrl != knownFormControls_.end()) {
            const char* defaultProp = getDefaultPropertyName(itCtrl->second);
            if (defaultProp) {
                std::string writeFn = getControlPropWriteFn(itCtrl->second, defaultProp);
                if (!writeFn.empty()) {
                    emitExpr(*node.value);
                    std::string valExpr = std::move(lastExpr_);
                    // M22: Text/Caption default prop writes need BSTR value
                    if (writeFn.find("SetControlText") != std::string::npos ||
                        writeFn.find("SetMenuCaption") != std::string::npos) {
                        valExpr = wrapToBSTR(valExpr, *node.value);
                    }
                    c_.emitLine(writeFn + "(" + makeCtrlHwndArg(tgtLower, itCtrl->second) + ", " + valExpr + ");  /* default prop: ." + std::string(defaultProp) + " */");
                    return;
                }
            }
        }
        // P20-31: WithEvents控件变量默认属性写入
        if (node.target->kind == ASTNodeKind::IdentifierExpr) {
            auto& tgtId2 = static_cast<IdentifierExpr&>(*node.target);
            std::string tgtLower2 = tgtId2.name;
            std::transform(tgtLower2.begin(), tgtLower2.end(), tgtLower2.begin(), ::tolower);
            auto itWECtrl = knownWithEventsCtrlVars_.find(tgtLower2);
            if (itWECtrl != knownWithEventsCtrlVars_.end()) {
                const char* defaultProp2 = getDefaultPropertyName(itWECtrl->second);
                if (defaultProp2) {
                    std::string writeFn2 = getControlPropWriteFn(itWECtrl->second, defaultProp2);
                    if (!writeFn2.empty()) {
                        auto itOrig2 = knownWithEventsCtrlOrigNames_.find(tgtLower2);
                        std::string weVarName2 = (itOrig2 != knownWithEventsCtrlOrigNames_.end()) ? itOrig2->second : cIdent(tgtId2.name);
                        emitExpr(*node.value);
                        std::string valExpr2 = std::move(lastExpr_);
                        c_.emitLine(writeFn2 + "(" + weVarName2 + ", " + valExpr2 + ");  /* WithEvents ctrl default prop: ." + std::string(defaultProp2) + " */");
                        return;
                    }
                }
            }
        }
    }
    // P17.1: WithMemberExpr作为赋值目标 (With块内 .Property = value)
    if (node.target->kind == ASTNodeKind::WithMemberExpr && !withObjectVars_.empty() && !withObjectInfoStack_.empty()) {
        auto& wmExpr = static_cast<WithMemberExpr&>(*node.target);
        const auto& info = withObjectInfoStack_.back();
        const std::string& tempVar = withObjectVars_.back();

        switch (info.kind) {
        case WithObjKind::FormControl: {
            std::string writeFn = getControlPropWriteFn(info.ctrlType, wmExpr.memberName);
            if (!writeFn.empty()) {
                emitExpr(*node.value);
                if (info.ctrlType == FrmControlType::Menu) {  // P20-36
                    std::string mnuLower = info.ctrlOrigName;
                    std::transform(mnuLower.begin(), mnuLower.end(), mnuLower.begin(), ::tolower);
                    c_.emitLine(writeFn + "(" + makeCtrlHwndArg(mnuLower, info.ctrlType) + ", " + lastExpr_ + ");  /* With menu prop write */");
                } else {
                    // M22: Text/Caption prop writes need BSTR
                    { std::string _v = lastExpr_;
                      if (writeFn.find("SetControlText") != std::string::npos || writeFn.find("SetMenuCaption") != std::string::npos) {
                          lastExpr_ = wrapToBSTR(_v, *node.value);
                      }
                    }
                    c_.emitLine(writeFn + "(" + tempVar + ", " + lastExpr_ + ");");
                }
                return;
            }
            break;
        }
        case WithObjKind::WithEventsCtrl: {
            std::string writeFn = getControlPropWriteFn(info.ctrlType, wmExpr.memberName);
            if (!writeFn.empty()) {
                emitExpr(*node.value);
                c_.emitLine(writeFn + "(" + info.ctrlOrigName + ", " + lastExpr_ + ");");
                return;
            }
            break;
        }
        case WithObjKind::COMObject: {
            emitExpr(*node.value);
            std::string valExpr = std::move(lastExpr_);
            std::string packFn = comPackExpr(*node.value);
            c_.emitLine("vb6_ComSetProp(" + tempVar + ", L\"" + wmExpr.memberName + "\", " +
                         packFn + "(" + valExpr + "));  /* With COM SetProp */");
            return;
        }
        case WithObjKind::ClassInstance: {
            // Fix 011r-1: 优先用 info.className 精确查找该类的 Property Let/Set
            // (原 symTab_.lookupModule 会捡错模块, 导致 Pattern A2 在 With 块赋值时出错)
            // Fix 011r-1b: VB6 case-insensitive — 类名匹配与函数名输出均用规范类名 (来自符号表)
            if (!info.className.empty()) {
                std::string letFn, setFn;
                const Symbol* letPropSym = nullptr;  // Fix 090x: 记录 PropertyLet 符号以取末参方向
                const Symbol* setPropSym = nullptr;
                std::string canonicalClassName;  // 与struct定义一致的大小写
                std::string classNameLower = Symbol::toLower(info.className);
                if (symTab_.moduleScope()) {
                    std::string memberLower = Symbol::toLower(wmExpr.memberName);
                    for (const auto& [key, sym] : symTab_.moduleScope()->symbols()) {
                        if (sym->lowerName != memberLower) continue;
                        bool matches = false;
                        if (sym->isExternal) {
                            if (Symbol::toLower(sym->sourceModule) == classNameLower) {
                                matches = true;
                                if (canonicalClassName.empty()) canonicalClassName = sym->sourceModule;
                            }
                        } else if (isClassModule_ && Symbol::toLower(moduleName_) == classNameLower) {  // Fix 013: moduleName_ = VB_Name
                            matches = true;
                            if (canonicalClassName.empty()) canonicalClassName = moduleName_;
                        }
                        if (!matches) continue;
                        if (sym->kind == SymbolKind::PropertyLet) {
                            letFn = "vb6_" + cIdent(canonicalClassName) + "_prop_let_" + cIdent(wmExpr.memberName);
                            letPropSym = sym.get();
                        } else if (sym->kind == SymbolKind::PropertySet) {
                            setFn = "vb6_" + cIdent(canonicalClassName) + "_prop_set_" + cIdent(wmExpr.memberName);
                            setPropSym = sym.get();
                        }
                    }
                }
                if (!letFn.empty()) {
                    emitExpr(*node.value);
                    // Fix 090x: With 块 .Expires = vb6_DateAdd(...) — 属性
                    // Property Let Expires(v As Variant) ByRef 值参是 vb6_VARIANT*,
                    // double/BSTR/int 实参裸拼 → C2440 (cHttpServer.c 913). 与
                    // Fix 090w (Pattern C/D2) 同款值参打包, 复用 packLetValueArg.
                    // Fix 185: 除 Variant 外, ByRef 值类型形参 (String/Boolean/
                    // Integer/Long/...) 的 C 形参同样是 <T>* (VB6 默认 ByRef),
                    // 实参裸传值会被当地址解引用 → 非确定 AV/堆破坏.
                    if (letPropSym && !letPropSym->params.empty()
                        && (letPropSym->params.back().type == Vb6Type::Variant
                            || !letPropSym->params.back().isByVal)) {
                        lastExpr_ = packLetValueArg(letPropSym->params.back(),
                                                    node.value.get(), lastExpr_);
                    }
                    c_.emitLine(letFn + "(" + tempVar + ", " + lastExpr_ + ");  /* With class prop_let_ */");
                    return;
                }
                if (!setFn.empty()) {
                    emitExpr(*node.value);
                    c_.emitLine(setFn + "(" + tempVar + ", " + lastExpr_ + ");  /* With class prop_set_ (fallback) */");
                    return;
                }
                // Fix 090h: 跨模块 storageKey 冲突 (如 CookieAttr.Value 的 value$pl
                // 被先注入的其它类同名属性覆盖, 消费模块作用域只留一个外部符号)
                // 使上面 moduleScope 扫描漏掉 PropertyLet/Set → 误走字段写 →
                // C2039 (CookieAttr 无 Value 数据字段, With 块 .Value= 生成
                // _vb6_with_9->Value, cHttpServer.c 908). findClassMemberCallParams
                // Phase B 从 Class 符号自身的 memberParams/memberProcKinds 确认
                // 该类确实有该属性成员 → 生成 prop_let_ 调用.
                if (letFn.empty() && setFn.empty() && !info.className.empty()) {
                    std::vector<ParameterInfo> clsParams090h;
                    bool clsBuiltin090h = false;
                    std::string canonCls090h = info.className;
                    if (symTab_.moduleScope()) {
                        auto clsIt090h = symTab_.moduleScope()->symbols().find(
                            Symbol::toLower(info.className));
                        if (clsIt090h != symTab_.moduleScope()->symbols().end()
                            && clsIt090h->second->kind == SymbolKind::Class) {
                            canonCls090h = clsIt090h->second->name;
                        }
                    }
                    if (findClassMemberCallParams(canonCls090h, wmExpr.memberName,
                                                  clsParams090h, clsBuiltin090h)) {
                        emitExpr(*node.value);
                        // Fix 185: 与主路径一致 — ByRef 值类型形参需打包成 <T>*.
                        std::string val090h = lastExpr_;
                        if (!clsParams090h.empty()
                            && (clsParams090h.back().type == Vb6Type::Variant
                                || !clsParams090h.back().isByVal)) {
                            val090h = packLetValueArg(clsParams090h.back(),
                                                      node.value.get(), val090h);
                        }
                        c_.emitLine("vb6_" + cIdent(canonCls090h) + "_prop_let_"
                                    + cIdent(wmExpr.memberName) + "(" + tempVar + ", "
                                    + val090h + ");  /* With class prop_let_ (Fix 090h) */");
                        return;
                    }
                }
                // 既无Let也无Set → 视为数据字段写: tempVar->member = value
                emitExpr(*node.value);
                // Fix 092m: RHS 是 COM/typed-COM 成员读时必须先消费 COM 标记 —
                // 否则 lastExpr_ 只含对象本身 (cHttpServer 372/373:
                //   .IP/.Port = me->m_oServer, 属性名整体丢失 = 编译通过但语义错),
                // 且标记泄漏到下一条语句 (374: .ConnectAt = ... L"RemotePort" C2440).
                // 无 hint 的 resolveComValue 在早期绑定签名可用时按 returnType 取
                // ComGetStringProp/IntProp/DoubleProp, 否则回退 BSTR.
                if (isComMarker_) {
                    // Fix 092m: 按目标字段类型解包 (String→ComGetStringProp /
                    // Long→ComGetIntProp / Date→ComGetDoubleProp). 目标类字段类型表
                    // 由语义分析填充; 表缺失时 helper 回退 "BSTR".
                    resolveComValue(classFieldComUnpackHint(info.className,
                                                            wmExpr.memberName));
                }
                c_.emitLine(tempVar + "->"
                            + cIdent(canonicalClassFieldName(info.className,
                                                             wmExpr.memberName))
                            + " = " + lastExpr_ + ";  /* With class field write */");
                return;
            }

            // className未知 — 使用原symTab查找(可能捡错模块, 但无法避免)
            Symbol* memSym = symTab_.lookupModule(wmExpr.memberName);
            if (!memSym) memSym = symTab_.lookup(wmExpr.memberName);
            if (memSym && memSym->kind == SymbolKind::PropertyLet) {
                std::string propFn = "prop_let_" + wmExpr.memberName;
                std::string funcName = cProcName(propFn, memSym->access,
                    memSym->isExternal ? memSym->sourceModule : (isClassModule_ ? moduleName_ : ""));
                emitExpr(*node.value);
                c_.emitLine(funcName + "(" + tempVar + ", " + lastExpr_ + ");");
                return;
            }
            if (memSym && memSym->kind == SymbolKind::PropertySet) {
                std::string propFn = "prop_set_" + wmExpr.memberName;
                std::string funcName = cProcName(propFn, memSym->access,
                    memSym->isExternal ? memSym->sourceModule : (isClassModule_ ? moduleName_ : ""));
                emitExpr(*node.value);
                c_.emitLine(funcName + "(" + tempVar + ", " + lastExpr_ + ");  /* With class PropertySet */");
                return;
            }
            // Fix 010n: 未知成员赋值 → COM后期绑定
            // .DataMember = value → vb6_ComSetProp(obj, L"Member", packedValue)
            emitExpr(*node.value);
            std::string valExpr = std::move(lastExpr_);
            std::string packFn = comPackExpr(*node.value);
            c_.emitLine("vb6_ComSetProp(" + tempVar + ", L\"" + wmExpr.memberName + "\", " +
                         packFn + "(" + valExpr + "));  /* With class .unknown COM SetProp */");
            return;
        }
        case WithObjKind::BuiltinObject: {
            // Fix 010l: .Property = value on builtin object (Err/App/etc.)
            // Err properties are read-only in C RTL — emit as comment to avoid MSVC error
            emitExpr(*node.value);
            c_.emitLine("/* With " + info.ctrlOrigName + "." + wmExpr.memberName +
                        " = <value> — builtin object property write (no-op) */");
            return;
        }
        default:
            break;
        }
    }
