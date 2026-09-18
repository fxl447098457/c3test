// cgen_expr_member_class_module.inc - 函数体片段：类实例成员分派与模块名.方法、模块前缀解析
// 由 src/backend/expr/cgen_expr_member.cpp 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 553~774 行，逐行未改。

            // 优先级2: 类实例成员访问: obj.Method → vb6_ClassName_MethodName(obj)
            // Fix 010r-10: 使用map查找, 可获取类名用于方法分发
            // Fix 011r-1: 当obj在knownClassVars_中时, 优先用resolveClassMemberCall
            // 精确解析该类的方法/属性, 避免memSym捡错模块的同类同名方法/属性
            // P6.4+: 类模块默认实例 (VB_PredeclaredId=True) 的裸类名 (cTT.CreateToolTip
            // / cTT.EnableTooltips(...) 在 frm 里) — 注册为"类实例变量"并按类精确
            // 解析成员 (注入 me 首参与类型化形参), 对象表达式用 vb6_cls_X_Default().
            std::string defaultInstCls = registerDefaultInstanceClass(objLower);
            auto itClassVar = knownClassVars_.find(objLower);
            if (itClassVar == knownClassVars_.end() && node.object) {
                // Fix 090al: 链式对象 (Db.Sql(s).Param(p).QueryParam) 或局部类变量
                // 未注册 knownClassVars_ → AST 推断类名再分发 (含值上下文补参),
                // 否则无括号裸调用只发 this → C2198 参数太少.
                std::string inferredCls = inferClassTypeOfExpr(*node.object);
                if (!inferredCls.empty()
                    && !resolveClassMemberCall(inferredCls, node.memberName).empty()) {
                    itClassVar = knownClassVars_.emplace(objLower, inferredCls).first;
                }
            }
            if (itClassVar != knownClassVars_.end()) {
                // 用对象的真实类名查找方法, 防止跨模块同名冲突
                std::string resolvedFn = resolveClassMemberCall(itClassVar->second, node.memberName);
                if (!resolvedFn.empty()) {
                    std::string objExpr;
                    if (!defaultInstCls.empty()) {
                        objExpr = "vb6_cls_" + cIdent(defaultInstCls) + "_Default()";
                    } else {
                        emitExpr(*node.object);
                        objExpr = std::move(lastExpr_);
                    }
                    // Fix 089h: 值上下文无括号方法引用 (Trim(FileStream.ReadLine)
                    // → vb6_cToolsStream_ReadLine(me->FileStream) 只发 this) —
                    // C2198 参数太少. 补默认参数 (Optional/必选), 与链式值上下文
                    // / With 分支 (Fix 044a) 一致. asCallCallee_ 由外层补参.
                    // 属性 (prop_get_/prop_let_/prop_set_) 不 pad — findClassMemberCallParams
                    // 对 Get+Let 并存属性返回 Let 参数, 无括号属性读只发 me 即正确
                    // (prop_get_State(oClient)), pad 反而 C2197 参数过多.
                    if (!asCallCallee_
                        && resolvedFn.find("_prop_") == std::string::npos) {
                        std::vector<ParameterInfo> params89h;
                        bool isBuiltin89h = false;
                        if (findClassMemberCallParams(itClassVar->second, node.memberName,
                                                       params89h, isBuiltin89h)
                            && !params89h.empty() && !isBuiltin89h) {
                            std::string argList89h = objExpr;
                            for (size_t i = 0; i < params89h.size(); i++) {
                                const auto& param = params89h[i];
                                argList89h += ", ";
                                std::string defVal89h;
                                if (param.hasDefaultValue && !param.defaultValueExpr.empty()) {
                                    defVal89h = param.defaultValueExpr;
                                } else {
                                    defVal89h = defaultValue(param.type);
                                }
                                if (param.isByVal) {
                                    argList89h += defVal89h;
                                } else {
                                    std::string cType89h = mapType(param.type);
                                    if (param.type == Vb6Type::Variant
                                        || param.type == Vb6Type::Empty
                                        || param.type == Vb6Type::Null
                                        || param.type == Vb6Type::Object) {
                                        argList89h += "&(" + cType89h + "){0}";
                                    } else {
                                        argList89h += "&(" + cType89h + "){" + defVal89h + "}";
                                    }
                                }
                            }
                            for (size_t i = 0; i < params89h.size(); i++) {
                                const auto& param = params89h[i];
                                if (param.isOptional && !param.isParamArray) {
                                    argList89h += ", 0";
                                }
                            }
                            lastExpr_ = resolvedFn + "(" + argList89h + ")";
                        } else {
                            lastExpr_ = resolvedFn + "(" + objExpr + ")";
                        }
                    } else {
                        lastExpr_ = resolvedFn + "(" + objExpr + ")";
                    }
                    return;
                }
                // resolveClassMemberCall返回空 → 该类中无此方法/属性 → 数据字段访问
                // 落入下方cross-module fallback分支处理 obj->member
            }

            // 优先级3: 模块名.方法名: MathUtils.Add → vb6_MathUtils_Add
            //    object名称不是已知变量, 但成员是函数 → 视为模块限定调用
            bool isVarName = false;
            auto* objSym = symTab_.lookup(objIdent.name);
            if (!objSym) objSym = symTab_.lookupModule(objIdent.name);
            if (objSym && (objSym->kind == SymbolKind::Variable || objSym->kind == SymbolKind::Parameter)) {
                isVarName = true;
            }
            // Fix 084x: 补充检查cgen层跟踪集合 (与M22分支一致), 防止过程级
            // 局部类变量/UDT变量被误判为"模块名", 落入优先级3 "模块名.方法名"
            // 用 memSym->sourceModule 生成 vb6_<他类>_<member> 函数名 (C2065).
            // 例: oCallback.Socket.Accept 中 oCallback 是 Dim As cClientCallback 局部
            // 变量, 不在模块作用域符号表 → 原判定 isVarName=false, memSym 命中
            // cTlsSocket.Socket 属性 → 生成 vb6_cTlsSocket_Socket (错误).
            if (!isVarName && knownUdtVars_.count(objLower)) isVarName = true;
            if (!isVarName && knownClassVars_.find(objLower) != knownClassVars_.end()) isVarName = true;
            if (!isVarName && knownNewVars_.count(objLower)) isVarName = true;
            if (!isVarName && knownObjectVars_.count(objLower)) isVarName = true;
            if (!isVarName && knownTypedComVars_.count(objLower)) isVarName = true;
            if (!isVarName && knownIfaceVars_.count(objLower)) isVarName = true;

            // Fix 083c: obj 是当前模块的属性(返回类对象) → pvSocket.GetLocalHost(...)
            // 不能按"模块名.方法"处理(会丢失对象实例导致 me 参数缺失 C2198),
            // 生成: vb6_<ObjClass>_<Method>((void*)vb6_<Mod>_prop_get_pvSocket(me), args...)
            if (!isVarName && objSym && objSym->kind == SymbolKind::PropertyGet &&
                !objSym->variableTypeName.empty()) {
                std::string propMod = objSym->isExternal ? objSym->sourceModule
                                    : (isClassModule_ ? moduleName_ : "");
                if (!propMod.empty()) {  // 仅类模块属性带 me
                    std::string propClass = objSym->variableTypeName;
                    std::string resolvedFn = resolveClassMemberCall(propClass, node.memberName);
                    if (!resolvedFn.empty()) {
                        std::string propFn = cProcName("prop_get_" + objIdent.name, objSym->access, propMod);
                        std::string objExpr = propFn + "((void*)me)";
                        lastExpr_ = resolvedFn + "((void*)" + objExpr + ")";
                        return;
                    }
                }
            }

            // Fix 086: 成员命中本模块符号但限定符是另一个模块名时, 优先在该模块
            // 中解析成员常量 (modSerialPortAPI.SetDTR — 本模块恰有同名 Sub SetDTR,
            // 误生成 vb6_modSerialPortAPI_SetDTR → C2065; 实际取的是模块常量 SETDTR=5).
            if (!isVarName && !memSym->isExternal) {
                std::string qModLower = Symbol::toLower(objIdent.name);
                std::string curModLower86 = Symbol::toLower(moduleName_);
                if (!qModLower.empty() && qModLower != curModLower86 && symTab_.moduleScope()) {
                    const std::string want86 = Symbol::toLower(node.memberName);
                    bool qResolved = false;
                    for (const auto& [qkey, qsym] : symTab_.moduleScope()->symbols()) {
                        if (!qsym || !qsym->isExternal) continue;
                        if (qsym->lowerName != want86 && Symbol::toLower(qsym->name) != want86) continue;
                        if (Symbol::toLower(qsym->sourceModule) != qModLower) continue;
                        if (qsym->kind == SymbolKind::Constant && qsym->hasConstValue) {
                            if (qsym->constType == Vb6Type::Long || qsym->constType == Vb6Type::Integer
                                || qsym->constType == Vb6Type::Boolean || qsym->constType == Vb6Type::Byte
                                || qsym->constType == Vb6Type::Error) {
                                lastExpr_ = std::to_string(qsym->constIntValue);
                                return;
                            }
                            if (qsym->constType == Vb6Type::Single || qsym->constType == Vb6Type::Double) {
                                lastExpr_ = std::to_string(qsym->constFloatValue);
                                return;
                            }
                        }
                        break;
                    }
                    // 注入被本地同名符号阻止时, 查 driver 预扫描的模块公共常量表
                    if (!qResolved && modulePublicConsts_) {
                        auto itMod = modulePublicConsts_->find(qModLower);
                        if (itMod != modulePublicConsts_->end()) {
                            auto itVal = itMod->second.find(want86);
                            if (itVal != itMod->second.end()) {
                                lastExpr_ = std::to_string(itVal->second);
                                return;
                            }
                        }
                    }
                }
            }

            if (!isVarName) {
                // 确定函数名的模块前缀
                std::string sourceMod;
                // Fix 092v: `Common.Version()` — 限定符是**模块名**时以限定模块为准.
                // Common4DLL.bas 与 cVBMAN.cls 都有 Version: lookupModule("Version")
                // 命中 cVBMAN.Version → 前缀取 memSym->sourceModule = "cVBMAN" →
                // vb6_cVBMAN_Version(...) 少了 me 参数 → C2198
                // (cHttpServerResponse.cls 440; 实参已按 Common 版形参正确生成
                // vb6_Common_Version(void**, int)). 这里改用限定符对应的规范模块名.
                std::string modCanon092v;
                {
                    const std::string objL092v = Symbol::toLower(objIdent.name);
                    if (symTab_.moduleScope()) {
                        for (const auto& [k092v, s092v] : symTab_.moduleScope()->symbols()) {
                            if (!s092v || !s092v->isExternal) continue;
                            if (Symbol::toLower(s092v->sourceModule) == objL092v) {
                                modCanon092v = s092v->sourceModule;
                                break;
                            }
                        }
                    }
                    // Fix 092y: 兜底 — 限定符本身就是工程内的**标准模块**名时, 直接
                    // 采用其规范名. 当该模块的公开过程与其它类的成员**全部**同键冲突时
                    // (Common 的 Version/Path 与 cVBMAN 的 Version/Path 同键,
                    // storageKey(Function)=lowerName → defineExternal 先到先得, Common
                    // 的两个成员全被丢弃), 本模块作用域内不存在任何
                    // sourceModule=="Common" 的符号 → 上面的扫描落空 → sourceMod 误取
                    // memSym->sourceModule ("cVBMAN") → vb6_cVBMAN_Version 少 me 参数 →
                    // C2198 (cHttpServerResponse.cls 440 Common.Version()).
                    // 仅对非类模块生效, 不改变 类名.成员 / 窗体名.成员 的现有语义.
                    if (modCanon092v.empty()) {
                        for (const auto& extMod092y : externalModules_) {
                            if (Symbol::toLower(extMod092y) != objL092v) continue;
                            Symbol* extSym092y = symTab_.lookup(extMod092y);
                            if (extSym092y && extSym092y->kind == SymbolKind::Class) break;
                            modCanon092v = extMod092y;
                            break;
                        }
                    }
                }
                if (!modCanon092v.empty()) {
                    sourceMod = (Symbol::toLower(modCanon092v) == Symbol::toLower(moduleName_))
                                ? "" : modCanon092v;
                } else if (memSym->isExternal) {
                    sourceMod = memSym->sourceModule;
                } else {
                    // 同模块调用也允许 Module.Method 语法
                    // 检查object名是否匹配当前模块名
                    std::string modLower = moduleName_;
                    std::transform(modLower.begin(), modLower.end(), modLower.begin(), ::tolower);
                    if (objLower == modLower) {
                        sourceMod = "";  // 同模块, 不需要前缀
                    } else {
                        sourceMod = objIdent.name;  // 假设object名就是模块名
                    }
                }
                std::string funcName = cProcName(node.memberName, memSym->access, sourceMod);
                lastExpr_ = funcName;  // 仅输出函数名, 参数由IndexOrCallExpr添加
                return;
            }
        }
    }
