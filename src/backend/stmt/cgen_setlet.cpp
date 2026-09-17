#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>

namespace vb6c3 {

// --- cgen_setlet.cpp: Set/Let 语句与 Mid$ 语句生成 ---

void CCodeGen::visit(SetStmt& node) {
    if (!node.target || !node.value) return;

    // 检测 Set obj = Nothing → vb6_ReleaseObject(&obj)
    if (node.value->kind == ASTNodeKind::LiteralExpr) {
        auto& lit = static_cast<LiteralExpr&>(*node.value);
        if (lit.literalKind == LiteralKind::Nothing) {
            emitExpr(*node.target);
            std::string target = std::move(lastExpr_);

            // COM属性SetRef Nothing: Set obj.Property = Nothing → vb6_ComSetRef(obj, L"Property", NULL)
            if (isComMarker_) {
                isComMarker_ = false;
                c_.emitLine("vb6_ComSetRef(" + comObjExpr_ + ", L\"" + comMemberName_ + "\", NULL);  /* COM SetRef Nothing */");
                comObjExpr_.clear();
                comMemberName_.clear();
                return;
            }

            // Fix 056b: Set obj.Prop = Nothing → 属性setter调用传 NULL
            // target 形如 vb6_cXxx_prop_set_fClient(obj) (来自 emitExpr 属性访问路径),
            // 不能做 &(prop_set_...) (void* 左值) → C2198/C2440.
            // Fix 083b: 匹配条件放宽 — 类名前缀使函数名形如 vb6_cXxx_prop_set_fClient(,
            // 原先的 "prop_set_(" 子串匹配不到 (prop_set_ 后是函数名不是左括号)
            if ((target.find("prop_set_") != std::string::npos && target.find("prop_set_") < target.find('(')) ||
                (target.find("prop_let_") != std::string::npos && target.find("prop_let_") < target.find('('))) {
                if (!target.empty() && target.back() == ')') {
                    target = target.substr(0, target.size() - 1) + ", NULL)";
                }
                c_.emitLine(target + ";  /* Set Nothing (property setter) */");
                return;
            }

            // Fix 086: 目标表达式解析为空对象 stub ((void*)0) — 无可释放引用,
            // 跳过整条语句 (避免 &(void*)0 → C2101; 例: Set Response.fClient = Nothing
            // 中 Response.fClient 链被解析为 COM NULL stub).
            if (target == "(void*)0" || target == "NULL" || target == "0") {
                return;
            }

            // P6.3: 早期绑定COM变量 → vb6_ComReleaseTyped
            std::string targetLower = target;
            std::transform(targetLower.begin(), targetLower.end(), targetLower.begin(), ::tolower);
            // Fix 084aa: Set dict(key) = Nothing 的目标是 vb6_ComCall(...) 函数调用,
            // 对调用结果取址 &vb6_ComCall(...) → C2102. 检测"以标识符开头+含("的
            // 函数调用形态, 改用 (&(void*){...}) 复合字面量包装 (仅释放引用).
            // me->Field / Client->Context->SSE 等可寻址形态不含 '(' 保持 &target.
            bool isCallTarget = !target.empty()
                && (std::isalpha(static_cast<unsigned char>(target[0])) || target[0] == '_');
            if (isCallTarget) {
                size_t paren = target.find('(');
                if (paren != std::string::npos) {
                    for (size_t j = 0; j < paren; j++) {
                        char ch = target[j];
                        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') {
                            isCallTarget = false;
                            break;
                        }
                    }
                } else {
                    isCallTarget = false;
                }
            }
            std::string relObjTarget = isCallTarget ? "(&(void*){" + target + "})" : ("&" + target);
            if (knownClassVars_.count(targetLower)) {
                // 项目类实例 (Dim c As CCircle / Dim WithEvents s As EventSource) 不是 COM
                // 对象, 没有引用计数: 调用 vb6_ReleaseObject 会对纯 C 结构体走 vtable
                // Release → 运行期 0xC0000005. Set Nothing 只需断开引用.
                c_.emitLine(target + " = NULL;  /* Set Nothing (项目类实例) */");
            } else if (knownTypedComVars_.count(targetLower)) {
                c_.emitLine("vb6_ComReleaseTyped((void**)" + relObjTarget + ");  /* Set Nothing (early bound) */");
            } else {
                c_.emitLine("vb6_ReleaseObject((void**)" + relObjTarget + ");  /* Set Nothing */");
            }
            return;
        }
    }

    // P25: Set ctrl.Property = COM_value (e.g. Set Picture2.Picture = ImageList1.ListImages(i).Picture)
    // SetStmt的emitExpr左侧走属性读取路径, 但Picture赋值需要写函数
    if (node.target->kind == ASTNodeKind::MemberAccessExpr) {
        auto& _setMa = static_cast<MemberAccessExpr&>(*node.target);
        if (_setMa.object && _setMa.object->kind == ASTNodeKind::IdentifierExpr) {
            auto& _setId = static_cast<IdentifierExpr&>(*_setMa.object);
            std::string _setObjLower = _setId.name;
            std::transform(_setObjLower.begin(), _setObjLower.end(), _setObjLower.begin(), ::tolower);
            auto _setItCtrl = knownFormControls_.find(_setObjLower);
            if (_setItCtrl != knownFormControls_.end()) {
                std::string _setWriteFn = getControlPropWriteFn(_setItCtrl->second, _setMa.memberName);
                if (!_setWriteFn.empty()) {
                    emitExpr(*node.value);
                    // Resolve COM marker on RHS
                    // Bug #3 fix: also detect vb6_ComIface_Picture* function return
                    bool _rhsIsComPicture = false;
                    if (isComMarker_) {
                        if (comMemberName_ == "Picture" || comMemberName_ == "picture") {
                            _rhsIsComPicture = true;
                            resolveComValue("Object");
                        } else {
                            resolveComValue("Object");
                        }
                    } else if (lastExprIsComPicture_) {
                        _rhsIsComPicture = true;
                        lastExprIsComPicture_ = false;  // consume the flag
                    }
                    std::string _setValExpr = std::move(lastExpr_);
                    // Use SetControlPictureFromCom for COM IPictureDisp
                    if (_rhsIsComPicture && _setWriteFn.find("SetControlPicture") != std::string::npos) {
                        _setWriteFn = "vb6_SetControlPictureFromCom";
                    }
                    c_.emitLine(_setWriteFn + "(" + makeCtrlHwndArg(_setObjLower, _setItCtrl->second) + ", " + _setValExpr + ");  /* Set Control Property */");
                    return;
                }
            }
        }
    }

    // P6.8: Set obj.Prop = value — 跨类 Friend/Public Property Set/Let 赋值
    // SetStmt 之前没有属性分支: target 被 emitExpr 当表达式生成 prop_set_ 单参数,
    // 后续 tryRewriteCOMLvalue 只认 prop_get_ 前缀, 导致 "prop_set_(obj) = value"
    // 左值错误 (C2440/C2198). 此处用符号信息直接生成完整属性调用.
    if (node.target->kind == ASTNodeKind::MemberAccessExpr) {
        auto& _ma = static_cast<MemberAccessExpr&>(*node.target);
        Symbol* _psSym = symTab_.lookupModuleByKind(_ma.memberName, SymbolKind::PropertySet);
        if (!_psSym) {
            _psSym = symTab_.lookupModuleByKind(_ma.memberName, SymbolKind::PropertyLet);
        }
        if (_psSym) {
            std::string _objClass = inferClassTypeOfExpr(*_ma.object);
            // Fix 084g-2: 外部注入的跨类属性符号 — 对象类型无法从符号表推断
            // (cgen阶段访问不到过程作用域局部变量) 时, 由外部符号提供 sourceModule.
            // Fix 084y-2: 同 P6.7 — 对象类与属性所属类不一致时, 属性是其他类的
            // 同名成员, 不属于当前对象 (Set oCallback.Socket 命中 cTlsSocket.Socket
            // PropertySet, 而 oCallback 是 cClientCallback, Socket 是数据字段) →
            // 跳过属性路径, 交给 visit(MemberAccessExpr) 生成 obj->field 字段赋值.
            bool _objClassMatchesProp = true;
            if (!_objClass.empty() && _psSym->isExternal) {
                std::string _psModLower = Symbol::toLower(_psSym->sourceModule);
                std::string _objClassLower = Symbol::toLower(_objClass);
                _objClassMatchesProp = (_psModLower == _objClassLower);
            }
            if (_objClassMatchesProp && (!_objClass.empty() || _psSym->isExternal)) {
                std::string _verb = (_psSym->kind == SymbolKind::PropertySet) ? "prop_set_" : "prop_let_";
                std::string _src = _psSym->isExternal ? _psSym->sourceModule : _objClass;
                std::string _fn = cProcName(_verb + _ma.memberName, _psSym->access, _src);
                emitExpr(*_ma.object);
                std::string _objE = std::move(lastExpr_);
                emitExpr(*node.value);
                std::string _valE = std::move(lastExpr_);
                // Object 形参 (void** ByRef 槽): 对象指针实参包装为 &(void*){...}
                // 复合字面量, 与 cgen_expr 的 ByRef 实参规则一致.
                std::string _arg2 = _valE;
                if (_arg2 != "NULL" && _arg2 != "0" && _arg2[0] != '&'
                    && _arg2.find("vb6_ComPack") == std::string::npos) {
                    _arg2 = "&(void*){" + _valE + "}";
                }
                c_.emitLine(_fn + "(" + _objE + ", " + _arg2 + ");  /* Set Property */");
                return;
            }
        }
    }

    // Fix 090ae: 链式 COM 默认属性索引写 (P25b helper) — SetStmt 原先缺失该分支,
    // Set Data(Line)(Col) = Dat 的 LHS 被 emitExpr 按链式读生成
    // vb6_VariantFromComResult(vb6_ComCall(...)) = value (非左值) → C2440.
    // 此处分流为 vb6_ComSetPropArg(ComCallObject(...), ...) 链写.
    if (tryEmitChainedComWrite(node.target.get(), node.value.get())) {
        return;
    }

    emitExpr(*node.target);
    std::string target = std::move(lastExpr_);

    // COM属性SetRef: Set obj.Property = objRef → vb6_ComSetRef(obj, L"Property", objRef)
    if (isComMarker_) {
        isComMarker_ = false;
        std::string objExpr = std::move(comObjExpr_);
        std::string memberName = std::move(comMemberName_);
    emitExpr(*node.value);
    // COM属性值: 如果右侧是COM属性, 解析为值
    if (isComMarker_) resolveComValue();
    std::string value = std::move(lastExpr_);
        c_.emitLine("vb6_ComSetRef(" + objExpr + ", L\"" + memberName + "\", " + value + ");  /* COM SetRef */");
        return;
    }

    emitExpr(*node.value);
    // Set语句: 如果右侧是COM调用返回的VARIANT*, 需要解封为对象
    if (isComMarker_) {
        // COM属性值作为对象引用: vb6_ComUnpackObject(vb6_ComGetProp(...))
        resolveComValue("Object");
    }
    std::string value = std::move(lastExpr_);

    // P16: Set cmd = Command1 → value应为vb6_hwnd_Command1而非默认属性值
    {
        std::string targetLower = target;
        std::transform(targetLower.begin(), targetLower.end(), targetLower.begin(), ::tolower);
        if (knownWithEventsCtrlVars_.count(targetLower)) {
            // value可能是vb6_GetControlXxx(vb6_hwnd_Name)形式, 需提取为vb6_hwnd_Name
            size_t hwndPos = value.find("vb6_hwnd_");
            if (hwndPos != std::string::npos) {
                size_t endPos = value.find(")", hwndPos);
                if (endPos != std::string::npos) {
                    value = value.substr(hwndPos, endPos - hwndPos);
                }
            }
        }
    }

    // 如果ComCall/ComGetProp返回VARIANT*含对象, 需要UnpackObject
    // 使用一体化函数: vb6_ComCallObject 内部完成 UnpackObject+VarFree
    if (value.find("vb6_ComCall(") == 0) {
        // vb6_ComCall(obj, L"Method", args, argc) → vb6_ComCallObject(obj, L"Method", args, argc)
        value = "vb6_ComCallObject" + value.substr(strlen("vb6_ComCall"));
    }

    // Fix 037b: 项目类 typed 字段的 Item 属性调用返回 VARIANT, Set 语句需要提取 void* 对象引用.
    // 检测: value 以 "vb6_" 开头且第一个 '(' 恰在 "_prop_get_Item" 之后 → 顶层 Item 调用.
    // 嵌套场景 (如 SomeFunc(obj.Rows(1))) 的第一个 '(' 在 SomeFunc 之后, 不会被误匹配.
    if (value.find("vb6_") == 0
        && value.find("vb6_VariantToObjectVal") == std::string::npos) {
        size_t itemPos = value.find("_prop_get_Item(");
        if (itemPos != std::string::npos) {
            size_t firstParen = value.find('(');
            if (firstParen == itemPos + 15) {  // 15 = strlen("_prop_get_Item")
                value = "vb6_VariantToObjectVal(" + value + ")";
            }
        }
    }
    lastExprNeedsObjectUnpack_ = false;  // 清除标记 (字符串检测已覆盖)

    // P6.3: 早期绑定COM变量赋值: Set fso = CreateObject("X") → fso = (Type*)vb6_ComCreateTyped(L"X", "{IID}")
    // 检查target是否是早期绑定COM变量, 且value是vb6_CreateObject
    if (value.find("vb6_CreateObject(") == 0) {
        // 提取target变量名
        std::string targetLower = target;
        std::transform(targetLower.begin(), targetLower.end(), targetLower.begin(), ::tolower);
        auto it = knownTypedComVars_.find(targetLower);
        if (it != knownTypedComVars_.end()) {
            const Symbol* comSym = it->second;
            // 生成类型转换: (vb6_ComIface_<Iface>*)vb6_ComCreateTyped(progId, iidStr)
            std::string ifaceName = comSym->name;
            if (comSym->kind == SymbolKind::ComClass && !comSym->comDefaultIfaceName.empty()) {
                ifaceName = comSym->comDefaultIfaceName;
            }
            std::string ifaceType = "vb6_ComIface_" + cIdent(ifaceName);
            // 从vb6_CreateObject(progId)中提取progId参数
            size_t start = value.find('(');
            size_t end = value.rfind(')');
            if (start != std::string::npos && end != std::string::npos && end > start) {
                std::string progIdArg = value.substr(start + 1, end - start - 1);
                std::string iidStr = comSym->comIidStr.empty() ? "" : "\"" + comSym->comIidStr + "\"";
                if (!iidStr.empty()) {
                    value = "(" + ifaceType + "*)vb6_ComCreateTyped(" + progIdArg + ", " + iidStr + ")";
                }
                // 如果没有IID, 降级为后期绑定 (保持vb6_CreateObject)
            }
        }
    }

    // P6.3: 早期绑定COM的vtable调用返回对象 → 自动类型转换
    // vb6_ComVtableGetObject(...) → (Type*)vb6_ComVtableGetObject(...)
    if (value.find("vb6_ComVtableGetObject(") == 0) {
        std::string targetLower = target;
        std::transform(targetLower.begin(), targetLower.end(), targetLower.begin(), ::tolower);
        auto it = knownTypedComVars_.find(targetLower);
        if (it != knownTypedComVars_.end()) {
            const Symbol* comSym = it->second;
            std::string ifaceName = comSym->name;
            if (comSym->kind == SymbolKind::ComClass && !comSym->comDefaultIfaceName.empty()) {
                ifaceName = comSym->comDefaultIfaceName;
            }
            std::string ifaceType = "vb6_ComIface_" + cIdent(ifaceName);
            value = "(" + ifaceType + "*)" + value;
        }
    }

    // P6.3: 早期绑定COM变量Set Nothing → vb6_ComReleaseTyped
    // (已在前面的Nothing分支处理, 但那里用的是vb6_ReleaseObject)
    // 这里检查target是否是早期绑定变量, 将vb6_ReleaseObject改为vb6_ComReleaseTyped

    // P6.4: 接口引用赋值: Set ifaceRef = obj → ifaceRef = vb6_iface_IFoo_wrap(obj)
    {
        std::string targetLower = target;
        std::transform(targetLower.begin(), targetLower.end(), targetLower.begin(), ::tolower);
        auto itIface = knownIfaceVars_.find(targetLower);
        if (itIface != knownIfaceVars_.end()) {
            std::string ifaceName = itIface->second;
            std::string ifaceType = "vb6_iface_" + cIdent(ifaceName);
            // 如果右侧值包含_New()或是指向类实例的变量, 包装为接口引用
            if (value.find("_New()") != std::string::npos) {
                value = ifaceType + "_wrap(" + value + ")";
            } else {
                // 简单变量引用: value可能是类实例指针变量名
                std::string valLower = value;
                std::transform(valLower.begin(), valLower.end(), valLower.begin(), ::tolower);
                if (knownClassVars_.find(valLower) != knownClassVars_.end()) {
                    value = ifaceType + "_wrap(" + value + ")";
                } else {
                    // P6.4+: RHS 是 Variant (ByRef 参数/表达式), 只含对象引用.
                    // 需唯一实现类才能安全 cast:
                    //   vb6_iface_IFoo_wrap((vb6_cls_Impl*)vb6_VariantToObjectVal(<var>))
                    // 生成的串已含 "vb6_VariantToObjectVal", Fix 038b-6 下方守卫会跳过,
                    // 不会二次转换. 无唯一实现类时无法推断 → 报编译错误 (绝不静默误编).
                    bool rhsIsVariant = cExprIsVariant(value);
                    // ByRef Variant 参数解引用形式 "(*vName)": cExprIsVariant 剥掉前导
                    // '(' 检测不到 ("*vName)" 无前缀命中). 用 AST 标识符兜底判定.
                    if (!rhsIsVariant && node.value
                        && node.value->kind == ASTNodeKind::IdentifierExpr) {
                        auto& idv = static_cast<IdentifierExpr&>(*node.value);
                        std::string vl = idv.name;
                        std::transform(vl.begin(), vl.end(), vl.begin(), ::tolower);
                        if (knownVariantVars_.count(vl)) rhsIsVariant = true;
                    }
                    if (rhsIsVariant) {
                        const std::string impl = singleImplementationClass(ifaceName);
                        if (impl.empty()) {
                            diag_.error(DiagnosticID::CodeGenUnsupportedFeature, SourceLocation{},
                                "P6.4: Set <interface> = Variant 需要接口 '" + ifaceName +
                                "' 在全项目有唯一实现类, 当前无法推断转换类型; "
                                "可将 RHS 改为 objptr/类型化对象变量后重试");
                        } else {
                            value = ifaceType + "_wrap((vb6_cls_" + cIdent(impl) +
                                    "*)vb6_VariantToObjectVal(" + value + "))";
                        }
                    }
                }
            }
        }
    }

    // Fix 010r-16: Set 语句中, 当 LHS 是非左值的 COM 调用或 Property Get
    // (常见于 With-block 跨模块成员: Set .Request = value, Set dict.Item(k) = v)
    // 重写为 vb6_ComSetProp / vb6_ComSetPropArg / prop_set_ 调用.
    if (tryRewriteCOMLvalue(target, value, node.value.get(), /*isSet=*/true)) {
        return;
    }

    // Fix 038b-6 + 051 共用: 判定 LHS 是否是 Variant 容器
    // (Variant 变量 / Variant 数组元素 / UDT Variant 字段). Set 到 Variant 容器
    // 语义 = 把对象引用存进 Variant (拷贝/FromValue), 而非提取对象到 typed 指针.
    std::string checkName = target;
    if (checkName.substr(0, 4) == "me->") checkName = checkName.substr(4);
    if (checkName.size() > 4 && checkName[0] == '(' && checkName[1] == '*'
        && checkName.back() == ')') {
        checkName = checkName.substr(2, checkName.size() - 3);
    }
    std::string targetLower = checkName;
    std::transform(targetLower.begin(), targetLower.end(), targetLower.begin(), ::tolower);
    bool targetIsVariant = knownVariantVars_.count(targetLower) > 0;
    // Fix 090af: Variant 数组元素 (VB6_SA_AT(vb6_VARIANT, arr, i)) 也是 Variant 容器
    if (!targetIsVariant && checkName.find("VB6_SA_AT(vb6_VARIANT,") == 0) {
        targetIsVariant = true;
    }
    // Fix 091p: 类字段 (me->field) 的 Variant 判定 — knownVariantVars_ 每过程
    // clear(), 类模块字段不在其中 (091m 回灌只覆盖标准模块级变量) → Set
    // m_vUserData = Value 被误判为 typed 目标 → VariantToObjectVal 提取 →
    // C2440 (cWinsock 172: void* → vb6_VARIANT). 字段访问带 me-> 前缀,
    // 用字段集合单独判定, 不污染裸名集合.
    if (!targetIsVariant && target.rfind("me->", 0) == 0
        && classVariantFields_.count(targetLower) > 0) {
        targetIsVariant = true;
    }
    // Fix 084n: UDT 的 Variant 字段 (如 ZipFileInfo.SourceFile As Variant)
    // Set .SourceFile = obj → 也需 vb6_VariantFromValue 包装对象指针 (void*→vb6_VARIANT C2440)
    if (!targetIsVariant) {
        targetIsVariant = (inferUdtFieldVb6Type(node.target.get()) == Vb6Type::Variant);
    }

    // Fix 038b-6: Set 语句中 Variant 值 → 对象引用提取
    // 当 RHS 是 Variant (如 vb6_VariantFromStackVARIANT, vb6_VariantArrayGet,
    // Variant 变量等) 而 LHS 是 typed 对象指针时, 用 vb6_VariantToObjectVal 提取.
    // 使用 cExprIsVariant (C 字符串级) + knownVariantVars_ 检测.
    // Fix 090af: LHS 是 Variant 容器时跳过本步 — Set d(0) = d(0).Root 的 RHS 是
    // VariantFromComResult(ComGetProp(...)) (Variant 值), 应直接拷贝进容器
    // (051 的 vb6_VariantFromValue _Generic Identity 安全处理), 而非先 ToObjectVal
    // 提取 void* 再重包 (多余且二次语义).
    {
        bool valueIsVariant = cExprIsVariant(value);
        if (!valueIsVariant && node.value && node.value->kind == ASTNodeKind::IdentifierExpr) {
            auto& id = static_cast<IdentifierExpr&>(*node.value);
            std::string idLower = id.name;
            std::transform(idLower.begin(), idLower.end(), idLower.begin(), ::tolower);
            if (knownVariantVars_.count(idLower)) valueIsVariant = true;
        }
        if (valueIsVariant && !targetIsVariant
            && value.find("vb6_VariantToObjectVal") == std::string::npos) {
            value = "vb6_VariantToObjectVal(" + value + ")";
        }
    }

    // Fix 051: Set 语句中 void* → vb6_VARIANT 包装
    // 当 LHS 是 Variant 变量 (如 vb6_ret_Xxx, Dim x As Variant) 而 RHS 是
    // void* (对象指针, 如 vb6_ComCallObject, 类字段访问, vb6_VariantToObjectVal 等) 时,
    // 用 vb6_VariantFromValue 包装. vb6_VariantFromValue 是 _Generic 宏:
    //   - void* → vb6_VariantObject (包装对象指针)
    //   - vb6_VARIANT → vb6_VariantIdentity (no-op, 安全)
    // 因此始终包装是安全的, 只需避免对已包装的表达式双重包装.
    // Fix 090af: targetIsVariant 判定上移共用 (含 Variant 数组元素 LHS).
    if (targetIsVariant
        && value.find("vb6_VariantFromValue(") != 0
        && value.find("vb6_VariantFromComResult(") != 0) {
        value = "vb6_VariantFromValue(" + value + ")";
    }

    // Fix 098: Set 左值是函数调用表达式 → 非左值 (C2106), 且多半缺参 (C2198).
    // VB6 语义: Set FnName = obj 在 FnName 不是当前函数自身时, 是对**别的函数
    // 返回值变量**的误写, VB6 编译器接受但赋值不改变本函数返回值. 等效生成:
    // 求值 RHS 后丢弃 (语义 = 该 Set 不影响任何可见状态).
    // 来源: ToolsJs.NewObj 内 "Set NewArr = MSSC.Eval(\"{};\")" (vbman 源码
    // 疑似笔误, NewArr 是 ParamArray 函数, 生成为 vb6_ToolsJs_NewArr() = ...)
    // → C2198+C2106 (ToolsJs.c:45).
    // 注: 左值 = 当前函数名时 M22 判定已在 emitExpr(IdentifierExpr) 转为
    // currentReturnVar_ (vb6_ret_NewObj), 不会落入本分支.
    {
        bool targetIsCallExpr = !target.empty() && target.back() == ')'
            && target.find('(') != std::string::npos
            && target.compare(0, 4, "me->") != 0
            && target.find("->") == std::string::npos
            && target.find(".") == std::string::npos
            && target.find("[") == std::string::npos
            && target.find("VB6_SA_AT(") != 0
            && target.find("vb6_VariantArrayGet(") != 0
            && target.find("vb6_PA_Get") != 0
            && target.find("prop_get_") == std::string::npos
            && target.find("prop_let_") == std::string::npos
            && target.find("prop_set_") == std::string::npos;
        if (targetIsCallExpr) {
            // 左括号前必须全是标识符字符 (排除 wb6_ComCall(...) 等包装调用已
            // 由前面的 tryRewriteCOMLvalue/链式写分支处理的情况兜底)
            size_t lp = target.find('(');
            bool isPlainFn = true;
            for (size_t ci = 0; ci < lp; ci++) {
                char ch = target[ci];
                if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') {
                    isPlainFn = false;
                    break;
                }
            }
            if (isPlainFn && lp > 0 && target.compare(0, 4, "vb6_") == 0) {
                c_.emitLine("(void)(" + value + ");  /* Set to non-lvalue function ref (Fix 098, VB6 no-op) */");
                return;
            }
        }
    }

    c_.emitLine(target + " = " + value + ";  /* Set */");

    // P6.5: WithEvents变量事件连接
    // Set obj = newInst -> 如果obj是WithEvents变量, 设置事件接收器
    {
        std::string targetLower = target;
        std::transform(targetLower.begin(), targetLower.end(), targetLower.begin(), ::tolower);
        auto itWE = knownWithEventsVars_.find(targetLower);
        if (itWE != knownWithEventsVars_.end()) {
            std::string sourceClass = itWE->second;
            std::string sinkName = "vb6_events_" + cIdent(sourceClass);
            // 查找源类符号，区分内部类与外部COM类
            auto* srcClsSym = symTab_.lookup(sourceClass);
            if (srcClsSym && srcClsSym->kind == SymbolKind::Class) {
                // 内部类 WithEvents: if (target) { static sink = {...}; target->events = &sink; }
                // 需要查找当前模块中是否有 obj_EventName 形式的事件处理器
                c_.emitLine("if (" + target + ") {");
                c_.indent();
                // 生成静态事件接收器实例
                c_.emitLine("static " + sinkName + " " + targetLower + "_sink = {");
                c_.indent();
                // handler: 指向当前对象(me)
                std::string handlerExpr = isClassModule_ ? "(void*)me" : "NULL";
                c_.emitLine(".handler = " + handlerExpr + ",");
                // 为源类的每个事件生成回调指针
                bool firstEvent = true;
                for (auto& evtName : srcClsSym->eventNames) {
                    std::string handlerName = target + "_" + evtName;  // VB6: obj_Click
                    std::string handlerLower = handlerName;
                    std::transform(handlerLower.begin(), handlerLower.end(), handlerLower.begin(), ::tolower);
                    // 查找处理器函数符号 (用lookup跨模块查找)
                    auto* handlerSym = symTab_.lookup(handlerName);
                    std::string cbField = ".on" + cIdent(evtName) + " = ";
                    if (handlerSym) {
                        // 生成包装函数名: vb6_evt_<EventName>_wrap_<varName>
                        std::string wrapperName = evtWrapperName(targetLower, evtName);
                        cbField += wrapperName;
                    } else {
                        cbField += "NULL";
                    }
                    if (!firstEvent || srcClsSym->eventNames.size() > 1) {
                        // 多事件时加逗号
                    }
                    c_.emitLine(cbField + ",");
                    firstEvent = false;
                }
                c_.dedent();
                c_.emitLine("};");
                // WithEvents 变量在 C 侧声明为 void* (类实例统一用对象指针表示),
                // 直接生成 "var->events" 会触发 C2223 ("->" 的左侧必须指向结构/联合)。
                // 必须先 cast 回具体类结构体指针, 与 cgen_expr.cpp 中 void* 成员访问同机制。
                c_.emitLine("((vb6_cls_" + cIdent(sourceClass) + "*)" + target + ")->events = &"
                            + targetLower + "_sink;");
                c_.dedent();
                c_.emitLine("}");
            } else if (srcClsSym && srcClsSym->kind == SymbolKind::ComClass && srcClsSym->comHasSourceIface) {
                // P13.23: External COM WithEvents
                std::string iidStr = srcClsSym->comSourceIfaceIid;
                std::string iidInit = emitGuidInitializer(iidStr);
                // Static declarations at function scope (before the if block)
                c_.emitLine("static int " + targetLower + "_evt_cookie = 0;");
                c_.emitLine("static const char* " + targetLower + "_evt_iid = \"" + iidStr + "\";");
                if (!iidInit.empty()) {
                    c_.emitLine("static const IID " + targetLower + "_evt_iid_struct = " + iidInit + ";");
                }
                c_.emitLine("if (" + target + ") {");
                c_.indent();
                c_.emitLine("if (" + targetLower + "_evt_cookie != 0) {");
                c_.indent();
                c_.emitLine("vb6_ComUnadvise((IUnknown*)" + target + ", " + targetLower + "_evt_iid, " + targetLower + "_evt_cookie);");
                c_.emitLine(targetLower + "_evt_cookie = 0;");
                c_.dedent();
                c_.emitLine("}");
                if (srcClsSym->comSourceIfaceIsDispOnly) {
                    // dispinterface: use existing IDispatch sink
                    std::vector<std::string> dispids;
                    std::vector<std::string> callbacks;
                    for (auto& evtName : srcClsSym->eventNames) {
                        std::string handlerName = target + "_" + evtName;
                        auto* handlerSym = symTab_.lookup(handlerName);
                        if (handlerSym) {
                            std::string evtLower = Symbol::toLower(evtName);
                            auto itDispId = srcClsSym->comEventDispids.find(evtLower);
                            int dispid = (itDispId != srcClsSym->comEventDispids.end()) ? itDispId->second : 0;
                            dispids.push_back(std::to_string(dispid));
                            callbacks.push_back(comEvtWrapperName(targetLower, evtName));
                        }
                    }
                    if (!dispids.empty()) {
                        std::string dispidsVar = targetLower + "_evt_dispids";
                        std::string dispidsInit;
                        for (size_t di = 0; di < dispids.size(); di++) {
                            if (di > 0) dispidsInit += ", ";
                            dispidsInit += dispids[di];
                        }
                        c_.emitLine("static int " + dispidsVar + "[] = {" + dispidsInit + "};");
                        std::string callbacksVar = targetLower + "_evt_cbs";
                        std::string callbacksInit;
                        for (size_t ci = 0; ci < callbacks.size(); ci++) {
                            if (ci > 0) callbacksInit += ", ";
                            callbacksInit += "(void(*)(void*,VARIANT*,int,VARIANT*))" + callbacks[ci];
                        }
                        c_.emitLine("static void (*" + callbacksVar + "[])(void*,VARIANT*,int,VARIANT*) = {" + callbacksInit + "};");
                        std::string sinkVar = targetLower + "_comsink";
                        std::string iidStructRef = iidInit.empty() ? "NULL" : "&" + targetLower + "_evt_iid_struct";
                        // P13.23: handler = 当前类实例(me), 传入包装函数供事件处理器调用
                        std::string handlerExpr = isClassModule_ ? "(void*)me" : "NULL";
                        c_.emitLine("void* " + sinkVar + " = vb6_CreateEventSink(" +
                            dispidsVar + ", (void**)" + callbacksVar + ", " + std::to_string(dispids.size()) +
                            ", " + iidStructRef + ", " + handlerExpr + ");");
                        c_.emitLine("vb6_ComAdvise((IUnknown*)" + target + ", " + targetLower + "_evt_iid, " + sinkVar + ", &" + targetLower + "_evt_cookie);");
                    }
                } else {
                    // vtable source interface: use custom vtable sink
                    std::string sinkVar = targetLower + "_comsink";
                    std::string sinkHandlerExpr = isClassModule_ ? "(void*)me" : "NULL";
                    c_.emitLine("void* " + sinkVar + " = vb6_vsink_" + targetLower + "_create(" + sinkHandlerExpr + ");");
                    c_.emitLine("vb6_ComAdvise((IUnknown*)" + target + ", " + targetLower + "_evt_iid, " + sinkVar + ", &" + targetLower + "_evt_cookie);");
                }
                c_.dedent();
                c_.emitLine("}");
            }
        }
    }

    // P16: WithEvents控件变量赋值 - 无需额外操作
    // Set cmd = Command1 → cmd = ctrl_Command1 (HWND拷贝已在赋值行完成)
    // 事件通过WndProc的HWND匹配分发，不需要COM Sink/Advise
    {
        std::string targetLower = target;
        std::transform(targetLower.begin(), targetLower.end(), targetLower.begin(), ::tolower);
        auto itCtrl = knownWithEventsCtrlVars_.find(targetLower);
        if (itCtrl != knownWithEventsCtrlVars_.end()) {
            // 控件WithEvents变量已在赋值行 target = value; 完成HWND拷贝
        }
    }
}

void CCodeGen::visit(LetStmt& node) {
    if (!node.target || !node.value) return;

    // P7.5+P7.6: 控件属性写入 (Let语句)
    if (node.target->kind == ASTNodeKind::MemberAccessExpr) {
        auto& maExpr = static_cast<MemberAccessExpr&>(*node.target);
        // P18-C: Printer.CurrentX / Printer.CurrentY 赋值
        if (maExpr.object && maExpr.object->kind == ASTNodeKind::IdentifierExpr) {
            auto& objId = static_cast<IdentifierExpr&>(*maExpr.object);
            std::string objName = objId.name;
            std::transform(objName.begin(), objName.end(), objName.begin(), ::tolower);
            std::string memName = maExpr.memberName;
            std::transform(memName.begin(), memName.end(), memName.begin(), ::tolower);
            if (objName == "printer") {
                emitExpr(*node.value);
                if (memName == "currentx") { c_.emitLine("vb6_Printer_SetCurrentX((int32_t)(" + lastExpr_ + "));"); return; }
                if (memName == "currenty") { c_.emitLine("vb6_Printer_SetCurrentY((int32_t)(" + lastExpr_ + "));"); return; }
            }
        }
        // P7.6: 控件数组属性写入 ctrlArr(idx).Property = value
        if (maExpr.object && maExpr.object->kind == ASTNodeKind::IndexOrCallExpr) {
            auto& idxExpr = static_cast<IndexOrCallExpr&>(*maExpr.object);
            if (idxExpr.callee && idxExpr.callee->kind == ASTNodeKind::IdentifierExpr) {
                auto& arrId = static_cast<IdentifierExpr&>(*idxExpr.callee);
                std::string arrLower = arrId.name;
                std::transform(arrLower.begin(), arrLower.end(), arrLower.begin(), ::tolower);
                if (knownControlArrays_.count(arrLower)) {
                    auto itCtrl = knownFormControls_.find(arrLower);
                    if (itCtrl != knownFormControls_.end()) {
                        std::string writeFn = getControlPropWriteFn(itCtrl->second, maExpr.memberName);
                        if (!writeFn.empty()) {
                            emitExpr(*node.value);
                            std::string valExpr = std::move(lastExpr_);
                            std::string idxArg;
                            if (!idxExpr.positional.empty()) {
                                emitExpr(*idxExpr.positional[0]);
                                idxArg = std::move(lastExpr_);
                                emitExpr(*node.value);
                                valExpr = std::move(lastExpr_);
                            } else {
                                idxArg = "0";
                            }
                            c_.emitLine(writeFn + "(vb6_CtrlArr_GetAt(&vb6_arr_" + cIdent(arrId.name) + ", " + idxArg + "), " + valExpr + ");  /* Let Control Array Property */");
                            return;
                        }
                    }
                }
            }
        }
        // P7.5: 非数组控件属性写入
        if (maExpr.object && maExpr.object->kind == ASTNodeKind::IdentifierExpr) {
            auto& objId = static_cast<IdentifierExpr&>(*maExpr.object);
            std::string objLower = objId.name;
            std::transform(objLower.begin(), objLower.end(), objLower.begin(), ::tolower);
            // P16: WithEvents控件属性写入 (Let)
            {
                auto itWECtrl = knownWithEventsCtrlVars_.find(objLower);
                if (itWECtrl != knownWithEventsCtrlVars_.end()) {
                    std::string writeFn = getControlPropWriteFn(itWECtrl->second, maExpr.memberName);
                    if (!writeFn.empty()) {
                        auto itOrig = knownWithEventsCtrlOrigNames_.find(objLower);
                        std::string weVarName = (itOrig != knownWithEventsCtrlOrigNames_.end()) ? itOrig->second : objLower;
                        emitExpr(*node.value);
                        std::string valExpr = std::move(lastExpr_);
                        c_.emitLine(writeFn + "(" + weVarName + ", " + valExpr + ");  /* Let WithEvents ctrl prop */");
                        return;
                    }
                }
            }
            auto itCtrl = knownFormControls_.find(objLower);
            if (itCtrl != knownFormControls_.end()) {
                std::string writeFn = getControlPropWriteFn(itCtrl->second, maExpr.memberName);
                if (!writeFn.empty()) {
                    emitExpr(*node.value);
                    std::string valExpr = std::move(lastExpr_);
                    c_.emitLine(writeFn + "(" + makeCtrlHwndArg(objLower, itCtrl->second) + ", " + valExpr + ");  /* Let Control Property */");
                    return;
                }
            }
        }
    }

    // P17.1: WithMemberExpr作为Let目标
    if (node.target->kind == ASTNodeKind::WithMemberExpr && !withObjectVars_.empty() && !withObjectInfoStack_.empty()) {
        auto& wmExpr = static_cast<WithMemberExpr&>(*node.target);
        const auto& info = withObjectInfoStack_.back();
        const std::string& tempVar = withObjectVars_.back();

        // FormControl + WithEventsCtrl Only (COM/Class handled via AssignmentStmt)
        if (info.kind == WithObjKind::FormControl) {
            std::string writeFn = getControlPropWriteFn(info.ctrlType, wmExpr.memberName);
            if (!writeFn.empty()) {
                emitExpr(*node.value);
                if (info.ctrlType == FrmControlType::Menu) {  // P20-36
                    std::string mnuLower = info.ctrlOrigName;
                    std::transform(mnuLower.begin(), mnuLower.end(), mnuLower.begin(), ::tolower);
                    c_.emitLine(writeFn + "(" + makeCtrlHwndArg(mnuLower, info.ctrlType) + ", " + lastExpr_ + ");  /* Let With menu prop */");
                } else {
                    c_.emitLine(writeFn + "(" + tempVar + ", " + lastExpr_ + ");");
                }
                return;
            }
        } else if (info.kind == WithObjKind::WithEventsCtrl) {
            std::string writeFn = getControlPropWriteFn(info.ctrlType, wmExpr.memberName);
            if (!writeFn.empty()) {
                emitExpr(*node.value);
                c_.emitLine(writeFn + "(" + info.ctrlOrigName + ", " + lastExpr_ + ");");
                return;
            }
        }
    }
    emitExpr(*node.target);
    std::string target = std::move(lastExpr_);
    emitExpr(*node.value);
    // COM属性值: 根据目标变量类型解封
    if (isComMarker_) {
        std::string unpackHint;
        if (node.target->kind == ASTNodeKind::IdentifierExpr) {
            auto& idExpr = static_cast<IdentifierExpr&>(*node.target);
            std::string lower = idExpr.name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (knownLongVars_.count(lower)) unpackHint = "Long";
            else if (knownDoubleVars_.count(lower)) unpackHint = "Double";
            else if (knownObjectVars_.count(lower)) unpackHint = "Object";
            else if (knownBstrVars_.count(lower)) unpackHint = "BSTR";
            else if (knownVariantVars_.count(lower)) unpackHint = "Variant";
        }
        resolveComValue(unpackHint);
    }
    std::string value = std::move(lastExpr_);

    // Fix 010r-16: Let 语句中, 当 LHS 是非左值的 COM 调用或 Property Get,
    // 重写为 vb6_ComSetProp / vb6_ComSetPropArg / prop_let_ 调用.
    if (tryRewriteCOMLvalue(target, value, node.value.get(), /*isSet=*/false)) {
        return;
    }

    // Fix 038 Group 1/2: Variant 数组元素赋值 (同 AssignmentStmt 路径)
    if (target.find("vb6_VariantArrayGet(") == 0) {
        size_t argStart = target.find('(');
        size_t argEnd = target.rfind(')');
        if (argStart != std::string::npos && argEnd != std::string::npos && argEnd > argStart) {
            std::string args = target.substr(argStart + 1, argEnd - argStart - 1);
            c_.emitLine("vb6_VariantArraySet(" + args + ", vb6_VariantFromValue(" + value + "));  /* Let */");
        } else {
            c_.emitLine(target + " = " + value + ";  /* Let */");
        }
    } else if (target.find("VB6_SA_AT(vb6_VARIANT,") != std::string::npos) {
        c_.emitLine(target + " = vb6_VariantFromValue(" + value + ");  /* Let */");
    } else {
        // Fix 038b-6/045: 具体类型目标 + Variant 值 → 自动提取 (同 AssignmentStmt 路径)
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
            std::string convertedValue = value;
            if (target.find("VB6_SA_AT(BSTR,") != std::string::npos) {
                convertedValue = "vb6_VariantToString(" + value + ")";
            } else if (target.find("VB6_SA_AT(int32_t,") != std::string::npos
                       || target.find("VB6_SA_AT(int16_t,") != std::string::npos
                       || target.find("VB6_SA_AT(uint8_t,") != std::string::npos) {
                convertedValue = "vb6_VariantToLong(" + value + ")";
            } else if (target.find("VB6_SA_AT(double,") != std::string::npos
                       || target.find("VB6_SA_AT(float,") != std::string::npos) {
                convertedValue = "vb6_VariantToDouble(" + value + ")";
            } else {
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
                    }
                }
            }
            c_.emitLine(target + " = " + convertedValue + ";  /* Let */");
        } else {
            c_.emitLine(target + " = " + value + ";  /* Let */");
        }
    }
}

void CCodeGen::visit(MidStmt& node) {
    // P18-A: Mid$(var, start, len) = expr → vb6_MidSet(&var, start, len, expr)
    emitExpr(*node.start);
    std::string startVar = std::move(lastExpr_);
    std::string lenVar;
    if (node.hasLength) {
        emitExpr(*node.length);
        lenVar = std::move(lastExpr_);
    } else {
        lenVar = "0";
    }
    emitExpr(*node.value);
    std::string valueVar = std::move(lastExpr_);
    // Fix 084b: Mid$(var, start, len) = VariantExpr (如 vSplit(i) 数组元素返回
    // vb6_VARIANT) → 需 vb6_VariantToString 转 BSTR, 否则 vb6_MidSet 第4参数
    // 类型不匹配触发 C2440.
    if (cExprIsVariant(valueVar)) {
        valueVar = "vb6_VariantToString(" + valueVar + ")";
    }
    // Emit target variable address
    emitExpr(*node.target);
    std::string targetVar = std::move(lastExpr_);
    c_.emitLine("vb6_MidSet(&" + targetVar + ", " + startVar + ", " + lenVar + ", " + valueVar + ");");
}


} // namespace vb6c3
