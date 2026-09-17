#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>

namespace vb6c3 {

// --- cgen_assign.cpp: 赋值语句生成 (Assignment / Set / Let / Mid$) ---

void CCodeGen::visit(AssignmentStmt& node) {
    if (!node.target || !node.value) return;
    // P22: Date$/Time$ statement interception (Date$ = "12-31-2025" / Time$ = "23:59:59")
    if (node.target->kind == ASTNodeKind::IdentifierExpr) {
        auto& _tgtId22 = static_cast<IdentifierExpr&>(*node.target);
        std::string _tgtLower22 = _tgtId22.name;
        std::transform(_tgtLower22.begin(), _tgtLower22.end(), _tgtLower22.begin(), ::tolower);
        if (_tgtLower22.size() > 1 && _tgtLower22.back() == '$') _tgtLower22.pop_back();
        if (_tgtLower22 == "date") {
            emitExpr(*node.value);
            c_.emitLine("vb6_DateSet(" + wrapToBSTR(lastExpr_, *node.value) + ");  /* Date$ = ... */");
            return;
        }
        if (_tgtLower22 == "time") {
            emitExpr(*node.value);
            c_.emitLine("vb6_TimeSet(" + wrapToBSTR(lastExpr_, *node.value) + ");  /* Time$ = ... */");
            return;
        }
    }
    // Fix 010r-14 (Pattern B): Mid$ statement — `Mid$(var, start[, len]) = value`
    // 当 Mid$ 被词法器吃成单一 Identifier "Mid$" 时, 解析器无法走 TokenKind::Mid 分支,
    // 会落到 parseLabelOrAssignmentOrCall(), 把 LHS 当作普通函数调用 (IndexOrCallExpr).
    // 在 codegen 层面识别此模式并改写为 vb6_MidSet(&var, start, len, value); 调用
    // (与 visit(MidStmt) 等价, 仅适用于 AssignmentStmt fallback 路径).
    if (node.target->kind == ASTNodeKind::IndexOrCallExpr) {
        auto& call = static_cast<IndexOrCallExpr&>(*node.target);
        if (call.callee && call.callee->kind == ASTNodeKind::IdentifierExpr) {
            auto& calleeId = static_cast<IdentifierExpr&>(*call.callee);
            std::string calleeLower = calleeId.name;
            std::transform(calleeLower.begin(), calleeLower.end(), calleeLower.begin(), ::tolower);
            // 兼容 "mid$" 和 "mid" 两种写法
            if (calleeLower == "mid$" || calleeLower == "mid") {
                if (call.positional.size() >= 2 && call.positional.size() <= 3) {
                    // 1. 发出 target (字符串变量)
                    emitExpr(*call.positional[0]);
                    std::string midTarget = std::move(lastExpr_);
                    // 2. 发出 start
                    emitExpr(*call.positional[1]);
                    std::string midStart = std::move(lastExpr_);
                    // 3. 发出 length (可选, 缺省为 0 表示 "到字符串末尾")
                    std::string midLen = "0";
                    if (call.positional.size() >= 3) {
                        emitExpr(*call.positional[2]);
                        midLen = std::move(lastExpr_);
                    }
                    // 4. 发出 value (RHS)
                    emitExpr(*node.value);
                    std::string midValue = std::move(lastExpr_);
                    // Fix 086: 与 visit(MidStmt) 的 Fix 084b 一致 — Variant值
                    // (vb6_VariantArrayGet 等) 需转 BSTR, 否则 C2440
                    if (cExprIsVariant(midValue)) {
                        midValue = "vb6_VariantToString(" + midValue + ")";
                    }
                    c_.emitLine("vb6_MidSet(&" + midTarget + ", " + midStart + ", " + midLen + ", " + midValue + ");  /* Mid$ statement */");
                    return;
                }
            }
        }
    }
    // P22: LSet/RSet statement (LSet strVar = expr / RSet strVar = expr)
    if (node.isLSet || node.isRSet) {
        if (node.target->kind == ASTNodeKind::IdentifierExpr) {
            auto& tgtId = static_cast<IdentifierExpr&>(*node.target);
            std::string tgtC = cIdent(tgtId.name);
            // P22-10: UDT LSet — memory copy between UDTs
            std::string tgtLower = tgtId.name;
            std::transform(tgtLower.begin(), tgtLower.end(), tgtLower.begin(), ::tolower);
            auto udtIt = knownUdtVars_.find(tgtLower);
            if (udtIt != knownUdtVars_.end()) {
                // Target is a UDT: LSet/RSet copies raw memory (truncated to target size)
                emitExpr(*node.value);
                std::string srcExpr = lastExpr_;
                std::string udtCType = udtIt->second;
                c_.emitLine("memcpy(&" + tgtC + ", &" + srcExpr + ", sizeof(" + udtCType + "));  /* LSet UDT */");
                return;
            }
            // String LSet/RSet
            emitExpr(*node.value);
            std::string valExpr = wrapToBSTR(lastExpr_, *node.value);
            // 定长字符串使用固定长度而非SysStringLen
            std::string strLen;
            auto fsIt = knownFixedStringLen_.find(tgtLower);
            if (fsIt != knownFixedStringLen_.end()) {
                strLen = fsIt->second;
            } else {
                strLen = "SysStringLen(" + tgtC + ")";
            }
            if (node.isLSet) {
                c_.emitLine("vb6_BSTR_Assign(&" + tgtC + ", vb6_LSet(" + valExpr + ", " + strLen + "));  /* LSet */");
            } else {
                c_.emitLine("vb6_BSTR_Assign(&" + tgtC + ", vb6_RSet(" + valExpr + ", " + strLen + "));  /* RSet */");
            }
            return;
        }
    }
    // P7.5+P7.6: 控件属性写入
    // 情况1: ctrl.Property = value (非数组)
    // 情况2: ctrlArr(idx).Property = value (数组)
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

        // M22: Me.Property = expr — 窗体模块中Me的属性赋值 (如 Me.Caption = myName)
        if (maExpr.object && (maExpr.object->kind == ASTNodeKind::MeExpr ||
                              (maExpr.object->kind == ASTNodeKind::IdentifierExpr &&
                               static_cast<IdentifierExpr&>(*maExpr.object).name.size() == 2 &&
                               (static_cast<IdentifierExpr&>(*maExpr.object).name[0] == 'M' || static_cast<IdentifierExpr&>(*maExpr.object).name[0] == 'm') &&
                               (static_cast<IdentifierExpr&>(*maExpr.object).name[1] == 'e' || static_cast<IdentifierExpr&>(*maExpr.object).name[1] == 'E')))) {
            if (isFormModule_ && !knownFormName_.empty()) {
                std::string memLower = maExpr.memberName;
                std::transform(memLower.begin(), memLower.end(), memLower.begin(), ::tolower);
                std::string formHwnd = "vb6_hwnd_" + moduleName_;
                auto origIt = knownFormControlOriginalNames_.find(knownFormName_);
                if (origIt != knownFormControlOriginalNames_.end()) {
                    formHwnd = "vb6_hwnd_" + cIdent(origIt->second);
                }
                // Form properties: Caption, Text, Visible, etc.
                std::string writeFn = getControlPropWriteFn(FrmControlType::Form, maExpr.memberName);
                if (!writeFn.empty()) {
                    emitExpr(*node.value);
                    std::string valExpr = std::move(lastExpr_);
                    // M22: Text/Caption property writes need BSTR value
                    if (writeFn.find("SetControlText") != std::string::npos ||
                        writeFn.find("SetMenuCaption") != std::string::npos) {
                        valExpr = wrapToBSTR(valExpr, *node.value);
                    }
                    c_.emitLine(writeFn + "(" + formHwnd + ", " + valExpr + ");  /* Me." + maExpr.memberName + " */");
                    return;
                }
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
                                // Re-emit value because we just clobbered lastExpr_
                                emitExpr(*node.value);
                                valExpr = std::move(lastExpr_);
                            } else {
                                idxArg = "0";
                            }
                            c_.emitLine(writeFn + "(vb6_CtrlArr_GetAt(&vb6_arr_" + cIdent(arrId.name) + ", " + idxArg + "), " + valExpr + ");  /* Control Array Property */");
                            return;
                        }
                    }
                }
            }
        }
        // P7.5: 非数组控件属性写入 ctrl.Property = value
        if (maExpr.object && maExpr.object->kind == ASTNodeKind::IdentifierExpr) {
            auto& objId = static_cast<IdentifierExpr&>(*maExpr.object);
            std::string objLower = objId.name;
            std::transform(objLower.begin(), objLower.end(), objLower.begin(), ::tolower);
            // P16: WithEvents控件属性写入
            {
                auto itWECtrl = knownWithEventsCtrlVars_.find(objLower);
                if (itWECtrl != knownWithEventsCtrlVars_.end()) {
                    std::string writeFn = getControlPropWriteFn(itWECtrl->second, maExpr.memberName);
                    if (!writeFn.empty()) {
                        auto itOrig = knownWithEventsCtrlOrigNames_.find(objLower);
                        std::string weVarName = (itOrig != knownWithEventsCtrlOrigNames_.end()) ? itOrig->second : objLower;
                        emitExpr(*node.value);
                        std::string valExpr = std::move(lastExpr_);
                        c_.emitLine(writeFn + "(" + weVarName + ", " + valExpr + ");  /* WithEvents ctrl prop write */");
                        return;
                    }
                }
            }
            auto itCtrl = knownFormControls_.find(objLower);
            if (itCtrl != knownFormControls_.end()) {
                std::string writeFn = getControlPropWriteFn(itCtrl->second, maExpr.memberName);
                if (!writeFn.empty()) {
                    emitExpr(*node.value);
                    // ImageList Picture fix: resolve COM marker on RHS before using value
                    // e.g. Picture2.Picture = ImageList1.ListImages(i).Picture
                    // Bug #3 fix: also detect vb6_ComIface_Picture* function return (e.g. QRCodegenBarcode)
                    bool rhsIsComPicture = false;
                    if (isComMarker_) {
                        if (comMemberName_ == "Picture" || comMemberName_ == "picture") {
                            rhsIsComPicture = true;
                            resolveComValue("Object");
                        } else {
                            resolveComValue();
                        }
                    } else if (lastExprIsComPicture_) {
                        rhsIsComPicture = true;
                        lastExprIsComPicture_ = false;  // consume the flag
                    }
                    std::string valExpr = std::move(lastExpr_);
                    // M22: Text/Caption property writes need BSTR value
                    if (writeFn.find("SetControlText") != std::string::npos ||
                        writeFn.find("SetMenuCaption") != std::string::npos) {
                        valExpr = wrapToBSTR(valExpr, *node.value);
                    }
                    // ImageList Picture fix: use SetControlPictureFromCom for COM IPictureDisp
                    if (rhsIsComPicture && writeFn.find("SetControlPicture") != std::string::npos) {
                        writeFn = "vb6_SetControlPictureFromCom";
                    }
                    c_.emitLine(writeFn + "(" + makeCtrlHwndArg(objLower, itCtrl->second) + ", " + valExpr + ");  /* Control Property */");
                    return;
                }
                diag_.warn(DiagnosticID::CodeGenUnsupportedFeature, SourceLocation{},
                    std::string("P7.5: Unknown control property write '") + objId.name + "." + maExpr.memberName +
                    "' for control type, generating struct field access (may not compile)");
            }
        }
    }

    // M22: 跨模块变量赋值 Module1.myName = expr → vb6_BSTR_Assign(&vb6_Module1_myName, expr)
    if (node.target->kind == ASTNodeKind::MemberAccessExpr) {
        auto& maExpr = static_cast<MemberAccessExpr&>(*node.target);
        if (maExpr.object && maExpr.object->kind == ASTNodeKind::IdentifierExpr) {
            auto& objIdent = static_cast<IdentifierExpr&>(*maExpr.object);
            std::string objLower = objIdent.name;
            std::transform(objLower.begin(), objLower.end(), objLower.begin(), ::tolower);
            // Check if object is NOT a known variable → assume module name
            auto* objSym = symTab_.lookup(objIdent.name);
            if (!objSym) objSym = symTab_.lookupModule(objIdent.name);
            bool isVarName = (objSym && (objSym->kind == SymbolKind::Variable || objSym->kind == SymbolKind::Parameter));
            // M22-fix: 补充检查cgen层跟踪集合，防止过程级UDT/类变量被误判为模块名
            if (!isVarName && knownUdtVars_.count(objLower)) isVarName = true;
            // Fix 086: Variant局部变量 (For Each Ctl In o) 也是合法的赋值目标对象 —
            // 成员写入走COM后期绑定 (vb6_ComSetProp), 而非 Module.X 回退 (C2065)
            if (!isVarName && knownVariantVars_.count(objLower)) isVarName = true;
            if (!isVarName && knownClassVars_.find(objLower) != knownClassVars_.end()) isVarName = true;
            if (!isVarName && knownNewVars_.count(objLower)) isVarName = true;
            if (!isVarName && knownObjectVars_.count(objLower)) isVarName = true;
            if (!isVarName && knownTypedComVars_.count(objLower)) isVarName = true;
            if (!isVarName && knownIfaceVars_.count(objLower)) isVarName = true;

            // Fix 084o-4: 函数名引用返回 UDT 的字段赋值 ("FuncName.Field = expr").
            // VB6 函数体内可用函数名访问返回 UDT 的字段 (如 pvVfsOpen.FileName = STR_BUFFER).
            // 目标展开为 vb6_ret_FuncName.Field (与读取侧 cgen_expr.cpp:203 一致),
            // 而非误当 Module 变量展开为 vb6_FuncName_Field (未声明 + 类型错误).
            if (!isVarName && objSym && objSym->kind == SymbolKind::Function && !objSym->isExternal) {
                std::string curLower084o = currentProc_ ? currentProc_->name : std::string();
                std::transform(curLower084o.begin(), curLower084o.end(), curLower084o.begin(), ::tolower);
                // Fix 084o-5: 函数返回类实例时的字段赋值 ("FuncName.Field = expr",
                // 如 Set ConnInst.Conn = Me.Conn) → vb6_ret_FuncName->Field = expr,
                // 而非误当 Module 变量生成 vb6_FuncName_Field (C2065 未声明).
                bool isUdtRet084o = (currentReturnCType_.rfind("vb6_type_", 0) == 0);
                bool isClsRet084o = !isUdtRet084o
                    && currentReturnCType_.find("vb6_cls_") != std::string::npos;
                if (!curLower084o.empty() && objLower == curLower084o
                    && currentReturnVar_.size() > 8
                    && (isUdtRet084o || isClsRet084o)) {
                    // 查返回 UDT 的字段类型 (与 inferUdtFieldVb6Type 相同的 udtMembers 查询)
                    std::string udtName084o = isUdtRet084o ? currentReturnCType_.substr(9) : std::string();
                    Symbol* udtSym084o = isUdtRet084o ? symTab_.lookupModule(udtName084o) : nullptr;
                    Vb6Type fldType084o = Vb6Type::Unknown;
                    if (udtSym084o && udtSym084o->kind == SymbolKind::UserDefinedType) {
                        std::string memLower084o = maExpr.memberName;
                        std::transform(memLower084o.begin(), memLower084o.end(), memLower084o.begin(), ::tolower);
                        for (auto& mi : udtSym084o->udtMembers) {
                            if (Symbol::toLower(mi.name) == memLower084o) { fldType084o = mi.type; break; }
                        }
                    }
                    std::string fieldAccess084o = currentReturnVar_
                        + (isUdtRet084o ? "." : "->") + cIdent(maExpr.memberName);
                    emitExpr(*node.value);
                    std::string valExpr084o = std::move(lastExpr_);
                    Vb6Type valType084o = inferExprType(*node.value);
                    bool valIsVariant084o = (valType084o == Vb6Type::Variant) || cExprIsVariant(valExpr084o);
                    switch (fldType084o) {
                        case Vb6Type::String:
                            if (valIsVariant084o) valExpr084o = "vb6_VariantToString(" + valExpr084o + ")";
                            c_.emitLine("vb6_BSTR_Assign(&" + fieldAccess084o + ", " + valExpr084o + ");  /* FnRetUdt." + maExpr.memberName + " */");
                            return;
                        case Vb6Type::Long: case Vb6Type::Integer: case Vb6Type::Byte:
                        case Vb6Type::Boolean: case Vb6Type::ULong: case Vb6Type::LongPtr:
                            if (valIsVariant084o) valExpr084o = "vb6_VariantToLong(" + valExpr084o + ")";
                            c_.emitLine(fieldAccess084o + " = " + valExpr084o + ";  /* FnRetUdt." + maExpr.memberName + " */");
                            return;
                        case Vb6Type::Double: case Vb6Type::Single:
                        case Vb6Type::Currency: case Vb6Type::Date:
                            if (valIsVariant084o) valExpr084o = "vb6_VariantToDouble(" + valExpr084o + ")";
                            c_.emitLine(fieldAccess084o + " = " + valExpr084o + ";  /* FnRetUdt." + maExpr.memberName + " */");
                            return;
                        case Vb6Type::Variant:
                            c_.emitLine("vb6_VariantClear(&" + fieldAccess084o + ");");
                            c_.emitLine(fieldAccess084o + " = vb6_VariantFromValue(" + valExpr084o + ");  /* FnRetUdt." + maExpr.memberName + " */");
                            return;
                        default:
                            c_.emitLine(fieldAccess084o + " = " + valExpr084o + ";  /* FnRetUdt." + maExpr.memberName + " */");
                            return;
                    }
                }
                // Fix 086: 函数名持有对象时的成员赋值 (json_ParseObject.CompareMode = 1,
                // json_ParseObject 是返回 Dictionary 的 Function). 此前误判为
                // Module.X → 生成未声明的 vb6_<fn>_<member> (C2065).
                // 此处处于 FnRetUdt 条件块内: objLower==当前过程名 且 kind 是
                // Function/PropertyGet, 仅需按返回C类型分流.
                if (currentReturnCType_.rfind("vb6_cls_", 0) == 0) {
                    emitExpr(*node.value);
                    std::string valExpr = std::move(lastExpr_);
                    c_.emitLine(currentReturnVar_ + "->" + cIdent(maExpr.memberName)
                                + " = " + valExpr + ";  /* FnRetObj." + maExpr.memberName + " */");
                    return;
                }
                // COM对象/void* 函数返回值 → COM后期绑定属性赋值
                emitExpr(*node.value);
                std::string valExpr = std::move(lastExpr_);
                std::string packFn = comPackExpr(*node.value);
                c_.emitLine("vb6_ComSetProp((void*)" + currentReturnVar_ + ", L\""
                            + maExpr.memberName + "\", " + packFn + "(" + valExpr
                            + "));  /* FnRetObj COM SetProp */");
                return;
            }

            // Fix 086: UDT返回属性的字段赋值 (CurLang.Index = 0) — VB6语义是修改
            // 属性返回的临时副本 (赋值被丢弃). 生成 属性调用.字段 的void表达式,
            // 而非 Module.X 回退 (C2065: vb6_CurLang_Index).
            if (!isVarName) {
                Symbol* propSym86 = symTab_.lookupModule(objIdent.name);
                if (propSym86 && propSym86->kind == SymbolKind::PropertyGet
                    && !propSym86->variableTypeName.empty()) {
                    Symbol* udtSym86 = lookupDotted(propSym86->variableTypeName);
                    if (udtSym86 && udtSym86->kind == SymbolKind::UserDefinedType) {
                        emitExpr(*node.value);  // RHS 副作用保留
                        std::string propFn86 = cProcName("prop_get_" + objIdent.name,
                            propSym86->access,
                            propSym86->isExternal ? propSym86->sourceModule : "");
                        std::string propCall86 = propFn86 + "("
                            + (isClassModule_ ? "(void*)me" : "") + ")";
                        c_.emitLine("(void)(" + propCall86 + "." + cIdent(maExpr.memberName)
                                    + ");  /* UDT prop copy assign (VB6 discards) */");
                        return;
                    }
                }
            }

            if (!isVarName && !knownFormControls_.count(objLower)) {
                // Module.varName = expr
                // When module is #included, use unprefixed name
                std::string modName = objIdent.name;
                std::string varName = cIdent(maExpr.memberName);
                std::string modLower = modName;
                std::transform(modLower.begin(), modLower.end(), modLower.begin(), ::tolower);
                bool isIncluded = false;
                for (const auto& extMod : externalModules_) {
                    std::string extLower = extMod;
                    std::transform(extLower.begin(), extLower.end(), extLower.begin(), ::tolower);
                    if (extLower == modLower) { isIncluded = true; break; }
                }
                std::string memberAccess = isIncluded ? varName : ("vb6_" + cIdent(modName) + "_" + varName);
                emitExpr(*node.value);
                std::string valExpr = std::move(lastExpr_);
                std::string memLower = maExpr.memberName;
                std::transform(memLower.begin(), memLower.end(), memLower.begin(), ::tolower);
                // M22: Check if cross-module variable is BSTR type
                // knownBstrVars_ only has current module's BSTR vars, so also check symbol table
                bool isBstrVar = knownBstrVars_.count(memLower) > 0;
                if (!isBstrVar) {
                    Symbol* memSym = symTab_.lookupModule(maExpr.memberName);
                    if (!memSym) memSym = symTab_.lookup(maExpr.memberName);
                    if (memSym && memSym->kind == SymbolKind::Variable && memSym->type == Vb6Type::String) {
                        isBstrVar = true;
                    }
                }
                if (isBstrVar) {
                    c_.emitLine("vb6_BSTR_Assign(&" + memberAccess + ", " + valExpr + ");  /* Module." + maExpr.memberName + " */");
                } else {
                    c_.emitLine(memberAccess + " = " + valExpr + ";  /* Module." + maExpr.memberName + " */");
                }
                return;
            }
        }
    }

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
                    if (letPropSym && !letPropSym->params.empty()
                        && letPropSym->params.back().type == Vb6Type::Variant) {
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
                        c_.emitLine("vb6_" + cIdent(canonCls090h) + "_prop_let_"
                                    + cIdent(wmExpr.memberName) + "(" + tempVar + ", "
                                    + lastExpr_ + ");  /* With class prop_let_ (Fix 090h) */");
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

    // ---- P25b: 链式 COM 默认属性索引赋值 ---- (实现提取为
    // tryEmitChainedComWrite, 090ae; AssignmentStmt/SetStmt 共用)
    if (tryEmitChainedComWrite(node.target.get(), node.value.get())) {
        return;
    }

    // P25: 参数化COM属性赋值检测: dic.Item(key) = value → vb6_ComSetPropArg
    if (node.target->kind == ASTNodeKind::IndexOrCallExpr) {
        auto& callTarget = static_cast<IndexOrCallExpr&>(*node.target);
        if (callTarget.callee && callTarget.callee->kind == ASTNodeKind::MemberAccessExpr) {
            auto& maTarget = static_cast<MemberAccessExpr&>(*callTarget.callee);
            // 检查对象是否是COM变量
            if (maTarget.object && maTarget.object->kind == ASTNodeKind::IdentifierExpr) {
                auto& objId = static_cast<IdentifierExpr&>(*maTarget.object);
                std::string objLower = objId.name;
                std::transform(objLower.begin(), objLower.end(), objLower.begin(), ::tolower);
                bool isComVar = knownObjectVars_.count(objLower) || knownVariantVars_.count(objLower);
                // 也检查前期绑定COM变量
                if (!isComVar) {
                    isComVar = knownTypedComVars_.count(objLower) > 0;
                }
                if (isComVar && !callTarget.positional.empty()) {
                    // 生成: vb6_ComSetPropArg(obj, L"Prop", {pack(arg1),...}, argc, pack(value))
                    emitExpr(*maTarget.object);
                    std::string objExpr = lastExpr_;
                    std::string memberName = maTarget.memberName;
                    // 打包索引参数
                    std::vector<std::string> packedIdxArgs;
                    for (size_t i = 0; i < callTarget.positional.size(); i++) {
                        std::string packFn = comPackExpr(*callTarget.positional[i]);
                        emitExpr(*callTarget.positional[i]);
                        { std::string resolved = resolveComMarkerForPack(packFn); if (!resolved.empty()) lastExpr_ = resolved; }
                        packedIdxArgs.push_back(packFn + "(" + lastExpr_ + ")");
                    }
                    int32_t idxArgc = (int32_t)packedIdxArgs.size();
                    std::string idxArgsArray = "(void*[]){";
                    for (int i = 0; i < idxArgc; i++) {
                        if (i > 0) idxArgsArray += ", ";
                        idxArgsArray += packedIdxArgs[i];
                    }
                    idxArgsArray += "}";
                    // 打包值参数并生成调用
                    emitExpr(*node.value);
                    std::string valExpr = std::move(lastExpr_);
                    std::string packFn = comPackExpr(*node.value);
                    c_.emitLine("vb6_ComSetPropArg(" + objExpr + ", L\"" + memberName + "\", " +
                                 idxArgsArray + ", " + std::to_string(idxArgc) + ", " +
                                 packFn + "(" + valExpr + "));  /* COM SetPropArg */");
                    return;
                }
            }
        }
    }

    // Fix 092u: 当前类内**参数化** Property Let/Set 赋值 —
    //   Extend(Array(A, C)) = Split(...)     (cToolsArray.cls 97, Sub test 内隐式 me)
    // 此前落到通用路径: emitExpr(target) 生成裸名 + 补的 value 占位
    // "Extend(&_arr_1, &(vb6_VARIANT){0})", 再拼 " = <RHS>" → C2106 ("=" 左侧必须
    // 是左值, ToolsArray.c 130). 正确形态是按**写方向**形参表生成
    //   vb6_cToolsArray_prop_let_Extend(me, &Vars, &Value);
    if (node.target->kind == ASTNodeKind::IndexOrCallExpr && isClassModule_) {
        auto& plCall092u = static_cast<IndexOrCallExpr&>(*node.target);
        if (plCall092u.callee && plCall092u.callee->kind == ASTNodeKind::IdentifierExpr
            && !plCall092u.positional.empty() && plCall092u.named.empty()) {
            auto& plId092u = static_cast<IdentifierExpr&>(*plCall092u.callee);
            const std::string plLower092u = Symbol::toLower(plId092u.name);
            // 局部变量 / 当前过程名同名 → 不是属性 (同 642 段规则)
            bool plSkip092u = knownLocalVars_.count(plLower092u) > 0;
            if (!plSkip092u && currentProc_) {
                plSkip092u = (Symbol::toLower(currentProc_->name) == plLower092u);
            }
            if (!plSkip092u) {
                std::vector<ParameterInfo> wp092u;
                bool isSet092u = false;
                bool found092u = findClassMemberWriteParams(moduleName_, plId092u.name,
                                                            false, wp092u);
                Symbol* plSym092u = nullptr;
                if (found092u) {
                    plSym092u = symTab_.lookupModuleByKind(plId092u.name,
                                                           SymbolKind::PropertyLet);
                } else {
                    found092u = findClassMemberWriteParams(moduleName_, plId092u.name,
                                                           true, wp092u);
                    if (found092u) {
                        isSet092u = true;
                        plSym092u = symTab_.lookupModuleByKind(plId092u.name,
                                                               SymbolKind::PropertySet);
                    }
                }
                // 形参数须恰好比括号实参多 1 (末参是 value); 且**不含 Optional 形参** —
                // Optional 在 C 侧另有存在标志参数 (cAsyncSocket 2611:
                // Property Let ThunkPrivateData(pThunk, Optional ByVal Index, ByVal lValue)
                // 的 C 签名是 (me, IUnknown**, int32_t, int32_t, int)), 本路径只按符号表
                // 拼参会 C2198 → 这类仍交给原有 prop_get_ 重写路径 (Pattern C/D2) 处理.
                bool hasOptional092u = false;
                for (const auto& pOpt092u : wp092u) {
                    if (pOpt092u.isOptional) { hasOptional092u = true; break; }
                }
                if (found092u && plSym092u && !hasOptional092u
                    && wp092u.size() == plCall092u.positional.size() + 1) {
                    const std::string tmp092u = "_pv" + std::to_string(tempCounter_++) + "_";
                    std::string args092u;
                    int slot092u = 0;
                    auto pushArg092u = [&](Expr* argNode, const ParameterInfo& pi) {
                        emitExpr(*argNode);
                        std::string argExpr = std::move(lastExpr_);
                        std::string one092u;
                        if (pi.type == Vb6Type::Variant) {
                            if (pi.isByVal) {
                                one092u = "vb6_VariantFromValue(" + argExpr + ")";
                            } else {
                                // ByRef Variant 需可寻址: 先落到栈变量再取址
                                // (复合字面量 {Variant值} 触发 C2440)
                                std::string tv = tmp092u + std::to_string(slot092u++);
                                c_.emitLine("vb6_VARIANT " + tv + " = vb6_VariantFromValue("
                                            + argExpr + ");");
                                one092u = "&" + tv;
                            }
                        } else if (pi.type == Vb6Type::String && cExprIsVariant(argExpr)) {
                            one092u = wrapToBSTR(argExpr, *argNode);
                        } else if (!pi.isByVal) {
                            // ByRef 非 Variant 形参: 简单标识符直接取址; 函数结果 /
                            // 字段链等非左值先落栈变量再取址 — 否则 &<非左值> C2102
                            // (cAsyncSocket 2395: &vb6_BSTR_Concat(...)).
                            bool simple092u = !argExpr.empty()
                                && ((argExpr[0] >= 'a' && argExpr[0] <= 'z')
                                    || (argExpr[0] >= 'A' && argExpr[0] <= 'Z')
                                    || argExpr[0] == '_')
                                && argExpr.find('(') == std::string::npos
                                && argExpr.find("->") == std::string::npos
                                && argExpr.find('.') == std::string::npos
                                && argExpr.find(' ') == std::string::npos;
                            if (simple092u) {
                                one092u = "&" + argExpr;
                            } else {
                                std::string tv = tmp092u + std::to_string(slot092u++);
                                c_.emitLine(mapType(pi.type) + " " + tv + " = " + argExpr + ";");
                                one092u = "&" + tv;
                            }
                        } else {
                            one092u = argExpr;
                        }
                        if (!args092u.empty()) args092u += ", ";
                        args092u += one092u;
                    };
                    for (size_t ai = 0; ai < plCall092u.positional.size(); ai++) {
                        pushArg092u(plCall092u.positional[ai].get(), wp092u[ai]);
                    }
                    pushArg092u(node.value.get(), wp092u.back());
                    c_.emitLine(cProcName((isSet092u ? "prop_set_" : "prop_let_")
                                              + plId092u.name,
                                          plSym092u->access, moduleName_)
                                + "((void*)me, " + args092u
                                + ");  /* Property Let/Set (param, own class) */");
                    return;
                }
            }
        }
    }

    emitExpr(*node.target);
    std::string target = std::move(lastExpr_);

    // COM属性赋值检测 (P6.2): obj.Property = value → vb6_ComSetProp(obj, L"Property", pack(value))
    if (isComMarker_) {
        isComMarker_ = false;
        std::string objExpr = std::move(comObjExpr_);
        std::string memberName = std::move(comMemberName_);
        emitExpr(*node.value);
        std::string valExpr = std::move(lastExpr_);
        std::string packFn = comPackExpr(*node.value);
        c_.emitLine("vb6_ComSetProp(" + objExpr + ", L\"" + memberName + "\", " +
                     packFn + "(" + valExpr + "));  /* COM SetProp */");
        return;
    }

    emitExpr(*node.value);
    // COM属性值: 如果右侧是COM属性, 根据目标变量类型解析为适当C类型
    if (isComMarker_) {
        // 推断目标变量类型, 用于COM值解封
        std::string unpackHint;
        if (node.target->kind == ASTNodeKind::IdentifierExpr) {
            auto& idExpr = static_cast<IdentifierExpr&>(*node.target);
            std::string lower = idExpr.name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            // 从已知变量集合推断类型
            if (knownLongVars_.count(lower)) unpackHint = "Long";
            else if (knownDoubleVars_.count(lower)) unpackHint = "Double";
            else if (knownObjectVars_.count(lower)) unpackHint = "Object";
            else if (knownBstrVars_.count(lower)) unpackHint = "BSTR";
            else if (knownVariantVars_.count(lower)) unpackHint = "Variant";
        }
        resolveComValue(unpackHint);
    }
    std::string value = std::move(lastExpr_);

    // Fix 092w: 目标是否为已知 Byte 数组变量 (vb6_SafeArray1D*) — 赋值右侧为
    // StrConv(...)/字符串构造字节数组内容时需改写 helper.
    bool tgtIsByteArray092w = false;
    {
        std::string tname = target;
        if (tname.compare(0, 4, "me->") == 0) tname = tname.substr(4);
        if (tname.size() > 4 && tname[0] == '(' && tname[1] == '*'
            && tname.back() == ')') {
            tname = tname.substr(2, tname.size() - 3);
        }
        std::string tlower = tname;
        std::transform(tlower.begin(), tlower.end(), tlower.begin(), ::tolower);
        if (knownByteArrayVars_.count(tlower)) tgtIsByteArray092w = true;
    }

    // P6.12: COM方法调用返回值类型化解封
    // vb6_ComCall 返回 VARIANT* (void*), 赋值给typed变量时需用对应解封函数
    // vb6_ComCall(obj, L"Method", args, argc) → vb6_ComCallInt/Double/BSTR/Object(...)
    if (value.find("vb6_ComCall(") == 0) {
        std::string callArgs = value.substr(strlen("vb6_ComCall"));
        // 推断目标变量类型
        std::string comCallType;
        if (node.target->kind == ASTNodeKind::IdentifierExpr) {
            auto& idExpr = static_cast<IdentifierExpr&>(*node.target);
            std::string lower = idExpr.name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (knownLongVars_.count(lower)) comCallType = "Int";
            else if (knownDoubleVars_.count(lower)) comCallType = "Double";
            else if (knownBstrVars_.count(lower)) comCallType = "BSTR";
            else if (knownObjectVars_.count(lower)) comCallType = "Object";
        }
        if (!comCallType.empty()) {
            value = "vb6_ComCall" + comCallType + callArgs;
        }
    }

    // Fix 010r-15+010r-16 (Pattern A/C/D2/F): LHS 是非左值 COM 调用或 Property Get,
    // 改写为对应的 COM SetProp/SetPropArg 或 prop_let_/prop_set_ 调用.
    // 覆盖模式:
    //   A: vb6_ComCall(obj, L"Item", args, n) = value → vb6_ComSetPropArg(...)
    //   F: vb6_ComGetStringProp(obj, L"Prop") = value → vb6_ComSetProp(...)
    //   C/D2: vb6_X_prop_get_Y(args) = value → vb6_X_prop_let_Y(args, value)
    if (tryRewriteCOMLvalue(target, value, node.value.get(), /*isSet=*/false)) {
        return;
    }

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
}


} // namespace vb6c3
