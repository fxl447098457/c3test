#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>

namespace vb6c3 {

// --- cgen_stmt.cpp: 语句生成 ---

void CCodeGen::emitStmtList(StmtList& stmts, bool emitResumePoints) {
    for (auto& stmt : stmts) {
        if (!stmt) continue;

        // P14.1.2: 在受保护区块中为每条语句生成resume点
        if (emitResumePoints && inProtectedBlock_ &&
            stmt->kind != ASTNodeKind::OnErrorStmt &&
            stmt->kind != ASTNodeKind::LabelStmt) {
            int pt = resumePointCounter_++;
            c_.emitLine("vb6_err_resume_point = " + std::to_string(pt) + ";");
            c_.emitLine("vb6_err_resume_next_point = " + std::to_string(pt + 1) + ";");
            c_.emitLine("vb6_resume_" + std::to_string(pt) + ":;");
            dispatchPoints_.push_back(pt);
        }

        switch (stmt->kind) {
            case ASTNodeKind::AssignmentStmt:  visit(static_cast<AssignmentStmt&>(*stmt)); break;
            case ASTNodeKind::SetStmt:         visit(static_cast<SetStmt&>(*stmt)); break;
            case ASTNodeKind::LetStmt:         visit(static_cast<LetStmt&>(*stmt)); break;
            case ASTNodeKind::IfStmt:          visit(static_cast<IfStmt&>(*stmt)); break;
            case ASTNodeKind::ForStmt:         visit(static_cast<ForStmt&>(*stmt)); break;
            case ASTNodeKind::ForEachStmt:     visit(static_cast<ForEachStmt&>(*stmt)); break;
            case ASTNodeKind::DoLoopStmt:      visit(static_cast<DoLoopStmt&>(*stmt)); break;
            case ASTNodeKind::WhileWendStmt:   visit(static_cast<WhileWendStmt&>(*stmt)); break;
            case ASTNodeKind::SelectCaseStmt:  visit(static_cast<SelectCaseStmt&>(*stmt)); break;
            case ASTNodeKind::WithStmt:        visit(static_cast<WithStmt&>(*stmt)); break;
            case ASTNodeKind::GoToStmt:        visit(static_cast<GoToStmt&>(*stmt)); break;
                        case ASTNodeKind::OnGoSubStmt:
                visit(static_cast<OnGoSubStmt&>(*stmt));
                break;
            case ASTNodeKind::OnGoToStmt:
                visit(static_cast<OnGoToStmt&>(*stmt));
                break;
            case ASTNodeKind::MidStmt:
                visit(static_cast<MidStmt&>(*stmt));
                break;
case ASTNodeKind::GoSubStmt:       visit(static_cast<GoSubStmt&>(*stmt)); break;
            case ASTNodeKind::OnErrorStmt:     visit(static_cast<OnErrorStmt&>(*stmt)); break;
    case ASTNodeKind::ResumeStmt:      visit(static_cast<ResumeStmt&>(*stmt)); break;
    case ASTNodeKind::ErrorStmt:       visit(static_cast<ErrorStmt&>(*stmt)); break;
            case ASTNodeKind::ExitStmt:        visit(static_cast<ExitStmt&>(*stmt)); break;
            case ASTNodeKind::CallStmt:        visit(static_cast<CallStmt&>(*stmt)); break;
            case ASTNodeKind::ReDimStmt:       visit(static_cast<ReDimStmt&>(*stmt)); break;
            case ASTNodeKind::EraseStmt:       visit(static_cast<EraseStmt&>(*stmt)); break;
            case ASTNodeKind::LabelStmt:       visit(static_cast<LabelStmt&>(*stmt)); break;
            case ASTNodeKind::LocalDeclStmt:   visit(static_cast<LocalDeclStmt&>(*stmt)); break;
            // 文件 I/O
            case ASTNodeKind::OpenStmt:        visit(static_cast<OpenStmt&>(*stmt)); break;
            case ASTNodeKind::CloseStmt:       visit(static_cast<CloseStmt&>(*stmt)); break;
            case ASTNodeKind::PrintStmt:       visit(static_cast<PrintStmt&>(*stmt)); break;
            case ASTNodeKind::WriteStmt:       visit(static_cast<WriteStmt&>(*stmt)); break;
            case ASTNodeKind::LineInputStmt:   visit(static_cast<LineInputStmt&>(*stmt)); break;
            case ASTNodeKind::InputStmt:       visit(static_cast<InputStmt&>(*stmt)); break;
            case ASTNodeKind::GetStmt:         visit(static_cast<GetStmt&>(*stmt)); break;
            case ASTNodeKind::PutStmt:         visit(static_cast<PutStmt&>(*stmt)); break;
            case ASTNodeKind::SeekStmt:        visit(static_cast<SeekStmt&>(*stmt)); break;
            case ASTNodeKind::LockStmt:        visit(static_cast<LockStmt&>(*stmt)); break;
            case ASTNodeKind::UnlockStmt:      visit(static_cast<UnlockStmt&>(*stmt)); break;
            case ASTNodeKind::ResetStmt:        visit(static_cast<ResetStmt&>(*stmt)); break;
            case ASTNodeKind::WidthStmt:       visit(static_cast<WidthStmt&>(*stmt)); break;
            case ASTNodeKind::KillStmt:        visit(static_cast<KillStmt&>(*stmt)); break;
            case ASTNodeKind::NameStmt:        visit(static_cast<NameStmt&>(*stmt)); break;
            case ASTNodeKind::MkDirStmt:       visit(static_cast<MkDirStmt&>(*stmt)); break;
            case ASTNodeKind::RmDirStmt:       visit(static_cast<RmDirStmt&>(*stmt)); break;
            case ASTNodeKind::ChDirStmt:       visit(static_cast<ChDirStmt&>(*stmt)); break;
            case ASTNodeKind::ChDriveStmt:     visit(static_cast<ChDriveStmt&>(*stmt)); break;
            case ASTNodeKind::FileCopyStmt:    visit(static_cast<FileCopyStmt&>(*stmt)); break;
            case ASTNodeKind::RaiseEventStmt:  visit(static_cast<RaiseEventStmt&>(*stmt)); break;
            case ASTNodeKind::BeepStmt:        visit(static_cast<BeepStmt&>(*stmt)); break;
            case ASTNodeKind::DoEventsStmt:    visit(static_cast<DoEventsStmt&>(*stmt)); break;
            case ASTNodeKind::EndStmt:
                c_.emitLine("vb6_End();");
                break;
            case ASTNodeKind::StopStmt:
                c_.emitLine("__debugbreak();");  // MSVC intrinsic
                break;
            case ASTNodeKind::ReturnStmt:
                if (hasGoSub_) {
                    // VB6 GoSub Return: 弹出返回地址并跳转
                    c_.emitLine("if (vb6_gosub_sp <= 0) { /* P17.4: GoSub stack underflow */ return; }");
c_.emitLine("switch(vb6_gosub_stack[--vb6_gosub_sp]) {");
                    c_.indent();
                    for (int i = 0; i < gosubReturnCounter_; i++) {
                        c_.emitLine("case " + std::to_string(i) + ": goto vb6_gosub_ret_" + std::to_string(i) + ";");
                    }
                    c_.dedent();
                    c_.emitLine("}");
                } else if (currentProc_ && currentProc_->kind == SymbolKind::Function) {
                    c_.emitLine("return " + currentReturnVar_ + ";");
                } else {
                    c_.emitLine("return;");
                }
                break;
            // 文件I/O和杂项语句暂不处理, 后续P4阶段
            default:
                c_.emitLine("/* unhandled stmt: " + std::string(stmt->kindName()) + " */");
                break;
        }
    }
}

void CCodeGen::visit(Block& node) {
    emitStmtList(node.stmts);
}

void CCodeGen::visit(AssignmentStmt& node) {
    if (!node.target || !node.value) return;
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
                    std::string valExpr = std::move(lastExpr_);
                    c_.emitLine(writeFn + "(" + makeCtrlHwndArg(objLower, itCtrl->second) + ", " + valExpr + ");  /* Control Property */");
                    return;
                }
                diag_.warn(DiagnosticID::CodeGenUnsupportedFeature, SourceLocation{},
                    std::string("P7.5: Unknown control property write '") + objId.name + "." + maExpr.memberName +
                    "' for control type, generating struct field access (may not compile)");
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
                if (knownClassVars_.count(objLower)) {
                    // 生成Property Let调用: vb6_prop_let_Name(obj, value)
                    std::string prefix = (propLetSym->kind == SymbolKind::PropertySet) ? "prop_set_" : "prop_let_";
                    std::string funcName = cProcName(prefix + maExpr.memberName, propLetSym->access,
                                                      propLetSym->isExternal ? propLetSym->sourceModule : "");
                    emitExpr(*maExpr.object);
                    std::string objExpr = std::move(lastExpr_);
                    emitExpr(*node.value);
                    std::string valExpr = std::move(lastExpr_);
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
        auto itCtrl = knownFormControls_.find(tgtLower);
        if (itCtrl != knownFormControls_.end()) {
            const char* defaultProp = getDefaultPropertyName(itCtrl->second);
            if (defaultProp) {
                std::string writeFn = getControlPropWriteFn(itCtrl->second, defaultProp);
                if (!writeFn.empty()) {
                    emitExpr(*node.value);
                    std::string valExpr = std::move(lastExpr_);
                    c_.emitLine(writeFn + "(" + makeCtrlHwndArg(tgtLower, itCtrl->second) + ", " + valExpr + ");  /* default prop: ." + std::string(defaultProp) + " */");
                    return;
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
            Symbol* memSym = symTab_.lookupModule(wmExpr.memberName);
            if (memSym && memSym->kind == SymbolKind::PropertyLet) {
                std::string propFn = "prop_let_" + wmExpr.memberName;
                std::string funcName = cProcName(propFn, memSym->access,
                    memSym->isExternal ? memSym->sourceModule : "");
                emitExpr(*node.value);
                c_.emitLine(funcName + "(" + tempVar + ", " + lastExpr_ + ");");
                return;
            }
            break;
        }
        default:
            break;
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
        }
        resolveComValue(unpackHint);
    }
    std::string value = std::move(lastExpr_);

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
        std::string lower = checkName;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (knownVariantVars_.count(lower)) targetIsVariant = true;
        if (node.target->kind == ASTNodeKind::IdentifierExpr) {
            auto& id = static_cast<IdentifierExpr&>(*node.target);
            std::string idLower = id.name;
            std::transform(idLower.begin(), idLower.end(), idLower.begin(), ::tolower);
            if (knownVariantVars_.count(idLower)) targetIsVariant = true;
        }
    }
    if (targetIsBstr) {
        c_.emitLine("vb6_BSTR_Assign(&" + target + ", " + value + ");");
    } else if (targetIsVariant) {
        // P8.4: 包装值为vb6_VARIANT, 先释放旧BSTR
        std::string wrappedValue = wrapVariantValue(node.value.get(), value);
        c_.emitLine("vb6_VariantClear(&" + target + ");");
        c_.emitLine(target + " = " + wrappedValue + ";");
    } else {
        c_.emitLine(target + " = " + value + ";");
    }
}

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

            // P6.3: 早期绑定COM变量 → vb6_ComReleaseTyped
            std::string targetLower = target;
            std::transform(targetLower.begin(), targetLower.end(), targetLower.begin(), ::tolower);
            if (knownTypedComVars_.count(targetLower)) {
                c_.emitLine("vb6_ComReleaseTyped((void**)&" + target + ");  /* Set Nothing (early bound) */");
            } else {
                c_.emitLine("vb6_ReleaseObject((void**)&" + target + ");  /* Set Nothing */");
            }
            return;
        }
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
                if (knownClassVars_.count(valLower)) {
                    value = ifaceType + "_wrap(" + value + ")";
                }
            }
        }
    }

    c_.emitLine(target + " = " + value + ";  /* Set */");

    // P6.5: WithEvents变量事件连接
    // Set obj = newInst → 如果obj是WithEvents变量, 设置事件接收器
    {
        std::string targetLower = target;
        std::transform(targetLower.begin(), targetLower.end(), targetLower.begin(), ::tolower);
        auto itWE = knownWithEventsVars_.find(targetLower);
        if (itWE != knownWithEventsVars_.end()) {
            std::string sourceClass = itWE->second;
            std::string sinkName = "vb6_events_" + cIdent(sourceClass);
            // 生成事件连接: if (target) { static sink = {...}; target->events = &sink; }
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
            // 查找源类的类符号获取事件列表 (用lookup跨模块查找)
            auto* srcClsSym = symTab_.lookup(sourceClass);
            if (srcClsSym && srcClsSym->kind == SymbolKind::Class) {
                // 遍历源类的事件，查找当前模块中是否有 targetName_EventName 处理器
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
                        std::string wrapperName = "vb6_evt_wrap_" + targetLower + "_" + cIdent(evtName);
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
            c_.emitLine(target + "->events = &" + targetLower + "_sink;");
            c_.dedent();
            c_.emitLine("}");
            } else if (srcClsSym && srcClsSym->kind == SymbolKind::ComClass && srcClsSym->comHasSourceIface) {
            // P13.23: External COM WithEvents - vb6_CreateEventSink + vb6_ComAdvise
            c_.emitLine("if (" + target + ") {");
            c_.indent();
                std::vector<std::string> dispids;
                std::vector<std::string> callbacks;
                for (auto& evtName : srcClsSym->eventNames) {
                    std::string handlerName = target + "_" + evtName;
                    auto* handlerSym = symTab_.lookup(handlerName);
                    if (handlerSym) {
                        std::string evtLower = evtName;
                        std::transform(evtLower.begin(), evtLower.end(), evtLower.begin(), ::tolower);
                        auto itDispId = srcClsSym->comEventDispids.find(evtLower);
                        int dispid = (itDispId != srcClsSym->comEventDispids.end()) ? itDispId->second : 0;
                        dispids.push_back(std::to_string(dispid));
                        callbacks.push_back("vb6_com_evt_" + targetLower + "_" + cIdent(evtName));
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
                        callbacksInit += "(void(*)(VARIANT*,int,VARIANT*))" + callbacks[ci];
                    }
                    c_.emitLine("static void (*" + callbacksVar + "[])(VARIANT*,int,VARIANT*) = {" + callbacksInit + "};");
                    std::string sinkVar = targetLower + "_comsink";
                    c_.emitLine("void* " + sinkVar + " = vb6_CreateEventSink(" +
                        dispidsVar + ", (void**)" + callbacksVar + ", " + std::to_string(dispids.size()) + ");");
                    std::string iidStr = srcClsSym->comSourceIfaceIid;
                    c_.emitLine("static int " + targetLower + "_evt_cookie = 0;");
                    c_.emitLine("static const char* " + targetLower + "_evt_iid = \"" + iidStr + "\";");
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
        }
        resolveComValue(unpackHint);
    }
    std::string value = std::move(lastExpr_);

    c_.emitLine(target + " = " + value + ";  /* Let */");
}

void CCodeGen::visit(IfStmt& node) {
    emitExpr(*node.condition);
    if (isComMarker_) resolveComValue("Int");  // If条件通常是Boolean/整数
    c_.emitLine("if (" + lastExpr_ + ") {");
    c_.indent();
    emitStmtList(node.thenBody);
    c_.dedent();

    for (auto& elseif : node.elseIfs) {
        emitExpr(*elseif->condition);
        c_.emitLine("} else if (" + lastExpr_ + ") {");
        c_.indent();
        emitStmtList(elseif->body);
        c_.dedent();
    }

    if (!node.elseBody.empty()) {
        c_.emitLine("} else {");
        c_.indent();
        emitStmtList(node.elseBody);
        c_.dedent();
    }

    c_.emitLine("}");
}

void CCodeGen::visit(ElseIfClause& node) {
    // 由IfStmt内部处理
}

void CCodeGen::visit(ForStmt& node) {
    emitExpr(*node.start);
    std::string start = std::move(lastExpr_);
    emitExpr(*node.end);
    std::string end = std::move(lastExpr_);

    std::string step = "1";
    if (node.step) {
        emitExpr(*node.step);
        step = std::move(lastExpr_);
    }

    std::string var = cIdent(node.varName);

    // P14.3.2: 嵌套循环栈 - 支持Exit For跳转到正确层
    std::string exitLabel = "vb6_loop_exit_" + std::to_string(labelCounter_++);
    loopStack_.push_back({ExitKind::For, exitLabel});

    c_.emitLine("{");
    c_.indent();
    c_.emitLine("int32_t " + var + "_end = " + end + ";");
    c_.emitLine("int32_t " + var + "_step = " + step + ";");
    c_.emitLine(var + " = " + start + ";");
    c_.emitLine("if (" + var + "_step > 0) {");
    c_.indent();
    c_.emitLine("for (; " + var + " <= " + var + "_end; " + var + " += " + var + "_step) {");
    c_.indent();
    emitStmtList(node.body);
    c_.dedent();
    c_.emitLine("}");
    c_.dedent();
    c_.emitLine("} else {");
    c_.indent();
    c_.emitLine("for (; " + var + " >= " + var + "_end; " + var + " += " + var + "_step) {");
    c_.indent();
    emitStmtList(node.body);
    c_.dedent();
    c_.emitLine("}");
    c_.dedent();
    c_.emitLine("}");
    c_.dedent();
    c_.emitLine("}");
    c_.emitLine(exitLabel + ":;  /* Exit For target */");

    loopStack_.pop_back();
}

void CCodeGen::visit(ForEachStmt& node) {
    // P14.3.2: 嵌套循环栈
    std::string exitLabel = "vb6_loop_exit_" + std::to_string(labelCounter_++);
    loopStack_.push_back({ExitKind::For, exitLabel});
    // P12.4: For Each item In collection
    // 支持: 数组(VB6 SafeArray)迭代
    // 暂不支持: COM集合(IEnumVARIANT)迭代

    // 检测集合是否为数组
    bool isCollArray = false;
    std::string collArrName;
    Vb6Type collElemType = Vb6Type::Variant;

    if (node.collection->kind == ASTNodeKind::IdentifierExpr) {
        auto& ident = static_cast<IdentifierExpr&>(*node.collection);
        std::string lower = ident.name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        // 查已知数组集合
        if (knownArrays_.count(lower)) {
            isCollArray = true;
            collArrName = cIdent(ident.name);
            collElemType = arrayElemTypes_[lower];
        } else {
            // 查符号表
            Symbol* sym = symTab_.lookupModule(ident.name);
            if (sym && sym->kind == SymbolKind::Variable && sym->isArray) {
                isCollArray = true;
                collArrName = cIdent(ident.name);
                collElemType = sym->type;
            }
        }
    }

    std::string var = cIdent(node.varName);
    int tmpIdx = tempCounter_++;

    if (isCollArray) {
        // For Each item In arr -> SafeArray index iteration
        // {
        //     int32_t _fe_i0, _fe_lb0, _fe_ub0;
        //     _fe_lb0 = vb6_LBound(arr, 1);
        //     _fe_ub0 = vb6_UBound(arr, 1);
        //     for (_fe_i0 = _fe_lb0; _fe_i0 <= _fe_ub0; _fe_i0++) {
        //         vb6_item = VB6_SA_AT(VARIANT, arr, _fe_i0);
        //         // body
        //     }
        // }
        c_.emitLine("{");
        c_.indent();
        std::string idxVar = "_fe_i" + std::to_string(tmpIdx);
        std::string lbVar = "_fe_lb" + std::to_string(tmpIdx);
        std::string ubVar = "_fe_ub" + std::to_string(tmpIdx);
        c_.emitLine("int32_t " + idxVar + ", " + lbVar + ", " + ubVar + ";");
        c_.emitLine(lbVar + " = vb6_LBound(" + collArrName + ", 1);");
        c_.emitLine(ubVar + " = vb6_UBound(" + collArrName + ", 1);");
        c_.emitLine("for (" + idxVar + " = " + lbVar + "; " + idxVar + " <= " + ubVar + "; " + idxVar + "++) {");
        c_.indent();

        // 赋值循环变量: vb6_item = VB6_SA_AT(elemCType, arr, _fe_i0)
        std::string elemCType = mapSaElemCType(collElemType);
        c_.emitLine(var + " = VB6_SA_AT(" + elemCType + ", " + collArrName + ", " + idxVar + ");");

        emitStmtList(node.body);
        c_.dedent();
        c_.emitLine("}");
        c_.dedent();
        c_.emitLine("}");
    } else {
        // 非数组集合: 暂不支持 (COM _NewEnum / IEnumVARIANT 留待P13)
        c_.emitLine("/* For Each: collection type not supported (array only) */");
        // 仍然发出循环体（一次），避免语义完全缺失
        emitStmtList(node.body);
    }
    c_.emitLine(exitLabel + ":;  /* Exit For target */");

    loopStack_.pop_back();
}


void CCodeGen::visit(DoLoopStmt& node) {
    // P14.3.2: 嵌套循环栈
    std::string exitLabel = "vb6_loop_exit_" + std::to_string(labelCounter_++);
    loopStack_.push_back({ExitKind::Do, exitLabel});

    switch (node.loopKind) {
        case DoLoopKind::DoWhileLoop:
            if (node.condition) {
                emitExpr(*node.condition);
                if (isComMarker_) resolveComValue("Int");
                c_.emitLine("while (" + lastExpr_ + ") {");
            } else {
                c_.emitLine("while (1) {");
            }
            c_.indent();
            emitStmtList(node.body);
            c_.dedent();
            c_.emitLine("}");
            break;

        case DoLoopKind::DoUntilLoop:
            if (node.condition) {
                emitExpr(*node.condition);
                c_.emitLine("while (!(" + lastExpr_ + ")) {");
            } else {
                c_.emitLine("while (1) {");
            }
            c_.indent();
            emitStmtList(node.body);
            c_.dedent();
            c_.emitLine("}");
            break;

        case DoLoopKind::DoLoopWhile:
            c_.emitLine("do {");
            c_.indent();
            emitStmtList(node.body);
            c_.dedent();
            if (node.condition) {
                emitExpr(*node.condition);
                c_.emitLine("} while (" + lastExpr_ + ");");
            } else {
                c_.emitLine("} while (1);");
            }
            break;

        case DoLoopKind::DoLoopUntil:
            c_.emitLine("do {");
            c_.indent();
            emitStmtList(node.body);
            c_.dedent();
            if (node.condition) {
                emitExpr(*node.condition);
                c_.emitLine("} while (!(" + lastExpr_ + "));");
            } else {
                c_.emitLine("} while (1);");
            }
            break;

        case DoLoopKind::DoLoop:
            c_.emitLine("do {");
            c_.indent();
            emitStmtList(node.body);
            c_.dedent();
            c_.emitLine("} while (1);");
            break;
    }
    c_.emitLine(exitLabel + ":;  /* Exit Do target */");

    loopStack_.pop_back();
}

void CCodeGen::visit(WhileWendStmt& node) {
    // P14.3.2: 嵌套循环栈 (While...Wend 等同于 Do While...Loop)
    std::string exitLabel = "vb6_loop_exit_" + std::to_string(labelCounter_++);
    loopStack_.push_back({ExitKind::Do, exitLabel});

    emitExpr(*node.condition);
    c_.emitLine("while (" + lastExpr_ + ") {");
    c_.indent();
    emitStmtList(node.body);
    c_.dedent();
    c_.emitLine("}");
    c_.emitLine(exitLabel + ":;  /* Exit Do target */");

    loopStack_.pop_back();
}

void CCodeGen::visit(SelectCaseStmt& node) {
    // 推断测试表达式的类型
    Vb6Type testType = inferExprType(*node.testExpr);
    bool isStringSelect = TypeSystem::isString(testType);
    bool isFloatSelect = TypeSystem::isFloat(testType);

    emitExpr(*node.testExpr);
    if (isComMarker_) resolveComValue();
    std::string testVar = lastExpr_;

    // 为test创建临时变量
    std::string tempVar = "_vb6_select_" + std::to_string(tempCounter_++);
    c_.emitLine("{");
    c_.indent();

    // 根据测试表达式类型选择临时变量类型
    std::string tempType;
    if (isStringSelect) {
        tempType = "BSTR";
    } else if (isFloatSelect) {
        tempType = "double";
    } else {
        tempType = "int32_t";
    }

    // 声明并初始化临时变量, 保存测试表达式的值
    c_.emitLine(tempType + " " + tempVar + " = " + testVar + ";");

    // 用if-else if链代替switch (VB6 Select Case支持范围比较和字符串)
    bool first = true;
    for (auto& caseClause : node.cases) {
        // 构建条件: 同一Case子句的多个值用||连接 (Case 1, 2, 3 → val==1 || val==2 || val==3)
        std::string combinedCond;

        for (size_t vi = 0; vi < caseClause->values.size(); vi++) {
            auto& cv = caseClause->values[vi];
            std::string cond;

            if (cv.isIsClause) {
                // Case Is > 0 → tempVar > 0
                // cv.value 是 BinaryExpr(IdentifierExpr("Is"), op, rightOperand)
                if (cv.value && cv.value->kind == ASTNodeKind::BinaryExpr) {
                    auto& binExpr = static_cast<BinaryExpr&>(*cv.value);
                    emitExpr(*binExpr.right);
                    std::string rightVal = std::move(lastExpr_);
                    if (isStringSelect) {
                        // 字符串比较: vb6_StrCmp(tempVar, rightVal) op 0
                        cond = "vb6_StrCmp(" + tempVar + ", " + rightVal + ") " + mapBinaryOp(binExpr.op) + " 0";
                    } else {
                        cond = tempVar + " " + mapBinaryOp(binExpr.op) + " " + rightVal;
                    }
                } else {
                    // Case Is (无比较符) → 非零/非空
                    if (isStringSelect) {
                        cond = tempVar + " != NULL && " + tempVar + "[0] != 0";
                    } else {
                        cond = tempVar + " != 0";
                    }
                }
            } else if (cv.toValue) {
                // Case 1 To 10 → tempVar >= 1 && tempVar <= 10
                emitExpr(*cv.value);
                std::string lo = std::move(lastExpr_);
                emitExpr(*cv.toValue);
                std::string hi = std::move(lastExpr_);
                if (isStringSelect) {
                    // 字符串范围比较: wcscmp >= lo && wcscmp <= hi
                    cond = "vb6_StrCmp(" + tempVar + ", " + lo + ") >= 0 && vb6_StrCmp(" + tempVar + ", " + hi + ") <= 0";
                } else {
                    cond = tempVar + " >= " + lo + " && " + tempVar + " <= " + hi;
                }
            } else {
                // 精确匹配
                emitExpr(*cv.value);
                if (isStringSelect) {
                    cond = "vb6_StrCmp(" + tempVar + ", " + lastExpr_ + ") == 0";
                } else {
                    cond = tempVar + " == " + lastExpr_;
                }
            }

            if (!combinedCond.empty()) combinedCond += " || ";
            combinedCond += "(" + cond + ")";
        }

        if (first) {
            c_.emitLine("if (" + combinedCond + ") {");
            first = false;
        } else {
            c_.emitLine("} else if (" + combinedCond + ") {");
        }
        c_.indent();
        emitStmtList(caseClause->body);
        c_.dedent();
    }

    if (!node.elseCase.empty()) {
        c_.emitLine("} else {");
        c_.indent();
        emitStmtList(node.elseCase);
        c_.dedent();
    }

    if (!first) {
        c_.emitLine("}");
    }
    c_.dedent();
    c_.emitLine("}");
}

void CCodeGen::visit(CaseClause& node) {
    // 由SelectCaseStmt内部处理
}

void CCodeGen::visit(WithStmt& node) {
    // P17.1: With块 — 根据对象类型创建适当类型的临时变量
    WithObjInfo withInfo;
    std::string tempVar = "_vb6_with_" + std::to_string(tempCounter_++);
    std::string tempType = "void*";

    // 检测With对象类型: 遍历表达式判断
    if (node.object) {
        std::string objNameLower;
        if (node.object->kind == ASTNodeKind::IdentifierExpr) {
            auto& idExpr = static_cast<IdentifierExpr&>(*node.object);
            objNameLower = idExpr.name;
            std::transform(objNameLower.begin(), objNameLower.end(), objNameLower.begin(), ::tolower);
        }

        if (!objNameLower.empty()) {
            // P16: WithEvents控件 (优先于普通控件)
            auto itWE = knownWithEventsCtrlVars_.find(objNameLower);
            if (itWE != knownWithEventsCtrlVars_.end()) {
                withInfo.kind = WithObjKind::WithEventsCtrl;
                withInfo.ctrlType = itWE->second;
                auto itOrig = knownWithEventsCtrlOrigNames_.find(objNameLower);
                withInfo.ctrlOrigName = (itOrig != knownWithEventsCtrlOrigNames_.end())
                    ? "vb6_hwnd_" + cIdent(itOrig->second) : "vb6_hwnd_" + cIdent(objNameLower);
                tempType = "HWND";
            } else {
                // 窗体控件
                auto itCtrl = knownFormControls_.find(objNameLower);
                if (itCtrl != knownFormControls_.end()) {
                    withInfo.kind = WithObjKind::FormControl;
                    withInfo.ctrlType = itCtrl->second;
                    if (itCtrl->second == FrmControlType::Menu) {  // P20-36: Menu stores lowercase name for makeCtrlHwndArg
                        withInfo.ctrlOrigName = objNameLower;
                    } else {
                        withInfo.ctrlOrigName = "vb6_hwnd_" + cIdent(objNameLower);
                    }
                    tempType = "HWND";
                }
            }

            // COM对象变量检测
            if (withInfo.kind == WithObjKind::Unknown) {
                if (knownObjectVars_.count(objNameLower)) {
                    withInfo.kind = WithObjKind::COMObject;
                }
            }

            // 类实例变量检测
            if (withInfo.kind == WithObjKind::Unknown) {
                if (knownClassVars_.count(objNameLower)) {
                    withInfo.kind = WithObjKind::ClassInstance;
                }
            }
        }
    }

    withObjectInfoStack_.push_back(withInfo);

    // P17.1: 抑制With对象表达式的默认属性解析
    bool prevSuppress = suppressDefaultProp_;
    if (withInfo.kind == WithObjKind::FormControl || withInfo.kind == WithObjKind::WithEventsCtrl) {
        suppressDefaultProp_ = true;
    }

    emitExpr(*node.object);

    suppressDefaultProp_ = prevSuppress;

    if (!withObjectInfoStack_.empty() && withObjectInfoStack_.back().kind == WithObjKind::FormControl && withObjectInfoStack_.back().ctrlType == FrmControlType::Menu) {  // P20-36
        c_.emitLine("int " + tempVar + " = 0;  /* Menu: no HWND, props use (hmenu,menuId) */");
    } else {
        c_.emitLine(tempType + " " + tempVar + " = (" + tempType + ")" + lastExpr_ + "  /* With object ref */;");
    }

    withObjectVars_.push_back(tempVar);

    c_.emitLine("{");
    c_.indent();
    emitStmtList(node.body);
    c_.dedent();
    c_.emitLine("}");

    withObjectVars_.pop_back();
    withObjectInfoStack_.pop_back();
}

void CCodeGen::visit(GoToStmt& node) {
    c_.emitLine("goto vb6_label_" + cIdent(node.labelName) + ";");
}

void CCodeGen::visit(OnErrorStmt& node) {
    switch (node.errorKind) {
        case OnErrorKind::GoToLabel: {
            // On Error GoTo label
            // 生成 setjmp 保护点，错误发生时 longjmp 回来后跳转到对应标签
            // C代码:
            //   if (setjmp(vb6_error_jmp_buf) != 0) {
            //       goto vb6_label_ErrorHandler;
            //   }
            //   vb6_err_jmp_active = 1;
            c_.emitLine("if (setjmp(vb6_local_err_jmp) != 0) {");
            c_.indent();
            if (hasResume_) {
                c_.emitLine("vb6_err_in_handler = 0;");
            }
            c_.emitLine("goto vb6_label_" + cIdent(node.labelName) + ";");
            c_.dedent();
            c_.emitLine("}");
            c_.emitLine("vb6_error_jmp_ptr = &vb6_local_err_jmp;");
            c_.emitLine("vb6_err_jmp_active = 1;");
            c_.emitLine("vb6_error_jmp_set = 1;");
            c_.emitLine("vb6_err_resume_next = 0;");  // P14.1.2: 清除Resume Next模式
            // P14.1.2: 标记进入受保护区块
            if (hasResume_) {
                inProtectedBlock_ = true;
                currentErrorHandlerLabel_ = node.labelName;
                // resumePointCounter_不重置: 避免标签重定义
            }
            // 需要setjmp头文件
            needSetjmp_ = true;
            break;
        }
        case OnErrorKind::ResumeNext: {
            // On Error Resume Next
            c_.emitLine("vb6_err_resume_next = 1;");
            break;
        }
        case OnErrorKind::GoToZero:
            // On Error GoTo 0: 禁用错误处理
            c_.emitLine("vb6_err_resume_next = 0;");
            c_.emitLine("vb6_err_jmp_active = 0;");
            c_.emitLine("vb6_error_jmp_set = 0;");
            break;
    }
}



// P14.1.2: Resume语句 -- 在错误处理器中恢复执行
void CCodeGen::visit(ResumeStmt& node) {
    switch (node.resumeKind) {
        case ResumeKind::ResumeHere:
            c_.emitLine("vb6_err_dispatch = vb6_err_resume_point;");
            c_.emitLine("vb6_ErrClear();");
            c_.emitLine("goto vb6_err_dispatch_switch;");
            break;
        case ResumeKind::ResumeNext:
            c_.emitLine("vb6_err_dispatch = vb6_err_resume_next_point;");
            c_.emitLine("vb6_ErrClear();");
            c_.emitLine("goto vb6_err_dispatch_switch;");
            break;
        case ResumeKind::ResumeLabel:
            c_.emitLine("vb6_ErrClear();");
            c_.emitLine("goto vb6_label_" + cIdent(node.labelName) + ";");
            break;
    }
}

// P14.1.3: Error语句 -- 触发运行时错误
void CCodeGen::visit(ErrorStmt& node) {
    emitExpr(*node.errorNumber);
    std::string errNum = std::move(lastExpr_);
    c_.emitLine("vb6_RaiseError(" + errNum + ", NULL);");
}
void CCodeGen::visit(ExitStmt& node) {
    switch (node.exitKind) {
        case ExitKind::Do:
        case ExitKind::For: {
            // P14.3.2: 从循环栈查找匹配的跳出标签
            std::string targetLabel;
            for (int i = (int)loopStack_.size() - 1; i >= 0; --i) {
                if (loopStack_[i].kind == node.exitKind) {
                    targetLabel = loopStack_[i].exitLabel;
                    break;
                }
            }
            if (!targetLabel.empty()) {
                c_.emitLine("goto " + targetLabel + ";");
            } else {
                c_.emitLine("break;  /* fallback: no matching loop in stack */");
            }
            break;
        }
        case ExitKind::Sub:
            c_.emitLine("return;");
            break;
        case ExitKind::Function:
            c_.emitLine("return " + currentReturnVar_ + ";");
            break;
        case ExitKind::Property:
            c_.emitLine("return;");
            break;
    }
}

void CCodeGen::visit(CallStmt& node) {
    if (node.callee) {
        // 检测 Debug.Print 调用: 特殊处理多参数输出
        if (node.callee->kind == ASTNodeKind::IndexOrCallExpr) {
            auto& call = static_cast<IndexOrCallExpr&>(*node.callee);
            if (call.callee && call.callee->kind == ASTNodeKind::MemberAccessExpr) {
                auto& member = static_cast<MemberAccessExpr&>(*call.callee);
                if (member.object && member.object->kind == ASTNodeKind::IdentifierExpr) {
                    auto& objIdent = static_cast<IdentifierExpr&>(*member.object);
                    std::string objLower = objIdent.name;
                    std::transform(objLower.begin(), objLower.end(), objLower.begin(), ::tolower);
                    std::string memLower = member.memberName;
                    std::transform(memLower.begin(), memLower.end(), memLower.begin(), ::tolower);

                    if (objLower == "debug" && memLower == "print") {
                        // Debug.Print: 逐参数输出, 最后换行
                        // 每个参数转为BSTR后用vb6_DebugWriteBSTR输出
                        if (call.positional.empty()) {
                            c_.emitLine("vb6_DebugWriteNewline();");
                        } else {
                            // 已知返回BSTR的内置函数前缀
                            static const std::vector<std::string> bstrFuncs = {
                                "vb6_BSTR_FromStr", "vb6_Left", "vb6_Right", "vb6_Mid",
                                "vb6_UCase", "vb6_LCase", "vb6_UCase_str", "vb6_LCase_str",
                                "vb6_Trim", "vb6_LTrim", "vb6_RTrim", "vb6_Chr",
                                "vb6_Str", "vb6_CStr", "vb6_Format", "vb6_Hex", "vb6_Oct",
                                "vb6_Replace", "vb6_Space", "vb6_String", "vb6_StrReverse",
                                "vb6_BSTR_Concat", "vb6_BSTR_Empty", "vb6_App_Path", "vb6_App_EXEName", "vb6_Command", "vb6_CurDir", "vb6_Environ", "vb6_Dir", "vb6_IIfBSTR", "vb6_CDec", "vb6_GetControlText", "vb6_GetControlCaption"
                            };
                            auto isBstrExpr = [&](const std::string& expr) -> bool {
                                for (auto& prefix : bstrFuncs) {
                                    if (expr.compare(0, prefix.size(), prefix) == 0) return true;
                                }
                                // vb6_BSTR_ 开头的都是 BSTR
                                if (expr.compare(0, 8, "vb6_BSTR") == 0) return true;
                                // VB6_SA_AT(BSTR, ...) 也是 BSTR
                                if (expr.find("VB6_SA_AT(BSTR,") != std::string::npos) return true;
                                // 已知BSTR变量名 (小写匹配)
                                std::string lower = expr;
                                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                                if (knownBstrVars_.count(lower)) return true;
                                return false;
                            };

                            // 已知返回double的内置函数前缀
                            static const std::vector<std::string> doubleFuncs = {
                                "vb6_Sin", "vb6_Cos", "vb6_Tan", "vb6_Atn",
                                "vb6_Log", "vb6_Exp", "vb6_Sqr", "vb6_Rnd",
                                "vb6_Round", "vb6_Fix", "vb6_Int",
                                "vb6_CDbl", "vb6_CSng", "vb6_Val",
                                "vb6_Abs"
                            };
                            auto isDoubleExpr = [&](const std::string& expr) -> bool {
                                for (auto& prefix : doubleFuncs) {
                                    if (expr.compare(0, prefix.size(), prefix) == 0) return true;
                                }
                                // 已知double变量名 (小写匹配)
                                std::string lower = expr;
                                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                                if (knownDoubleVars_.count(lower)) return true;
                                // 包含浮点字面量 (如 3.14)
                                // 检查是否包含小数点且不是函数调用
                                if (expr.find('.') != std::string::npos && expr.find('(') == std::string::npos) return true;
                                return false;
                            };

                            for (size_t j = 0; j < call.positional.size(); j++) {
                                emitExpr(*call.positional[j]);
                                std::string val = std::move(lastExpr_);

                                // COM属性读取 (P6.2): isComMarker_标志
                                if (isComMarker_) {
                                    isComMarker_ = false;
                                    // 使用一体化函数, 内部处理VARIANT清理
                                    std::string comPropCall = "vb6_ComGetStringProp(" + comObjExpr_ + ", L\"" + comMemberName_ + "\")";
                                    c_.emitLine("{");
                                    c_.indent();
                                    c_.emitLine("wchar_t* _dbg_com_bstr = " + comPropCall + ";");
                                    c_.emitLine("vb6_DebugWriteBSTR(_dbg_com_bstr);");
                                    c_.emitLine("vb6_BSTR_Free(_dbg_com_bstr);");
                                    c_.dedent();
                                    c_.emitLine("}");
                                    comObjExpr_.clear();
                                    comMemberName_.clear();
                                    continue;
                                }

                                if (isBstrExpr(val)) {
                                    // 已经是BSTR, 直接输出
                                    c_.emitLine("vb6_DebugWriteBSTR(" + val + ");");
                                } else if (isDoubleExpr(val)) {
                                    // 浮点数, 用DebugWriteDouble输出
                                    c_.emitLine("vb6_DebugWriteDouble((double)(" + val + "));");
                                } else {
                                    // 整数/布尔值, 用DebugWriteLong输出
                                    c_.emitLine("vb6_DebugWriteLong((int32_t)(" + val + "));");
                                }
                            }
                            c_.emitLine("vb6_DebugWriteNewline();");
                        }
                        return;
                    }
            }
        }
    }

        emitExpr(*node.callee);
        // 语句级调用: 确保表达式被求值(即使是void调用)
        // 如果结果是函数名(不含括号), 自动添加()调用
        std::string callExpr = lastExpr_;

        // COM调用检测 (P6.2): isComMarker_标志
        if (isComMarker_) {
            isComMarker_ = false;
            // 无括号的COM方法调用: obj.Method → vb6_ComCall(obj, L"Method", NULL, 0)
            callExpr = "vb6_ComCall(" + comObjExpr_ + ", L\"" + comMemberName_ + "\", NULL, 0)";
            comObjExpr_.clear();
            comMemberName_.clear();
        } else if (callExpr.find('(') == std::string::npos) {
            // P14.1.5: Check if callee is a ParamArray function (needs NULL SAFEARRAY* arg)
            bool calleeHasPA = false;
            if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr) {
                auto& idExpr = static_cast<IdentifierExpr&>(*node.callee);
                Symbol* sym = symTab_.lookupModule(idExpr.name);
                if (!sym) sym = symTab_.lookup(idExpr.name);
                if (sym && (sym->kind == SymbolKind::Sub || sym->kind == SymbolKind::Function)) {
                    for (auto& p : sym->params) {
                        if (p.isParamArray) { calleeHasPA = true; break; }
                    }
                }
            }
            if (calleeHasPA) {
                callExpr += "(NULL)";
            } else {
                callExpr += "()";
            }
        }

        // ComCall返回VARIANT*, 需要释放 (语句级调用丢弃返回值)
        if (callExpr.find("vb6_ComCall(") == 0) {
            // ComCall返回可能含对象的VARIANT*, 用VarFree避免Release对象
            c_.emitLine("vb6_ComVarFree((void*)" + callExpr + ");  /* COM call, discard result */");
        } else if (callExpr.find("vb6_ComGetProp(") == 0) {
            c_.emitLine("vb6_ComVarClear((void*)" + callExpr + ");  /* COM prop get, discard result */");
        } else {
            c_.emitLine(callExpr + ";");
        }
    }
}

void CCodeGen::visit(ReDimStmt& node) {
    std::string cName = cIdent(node.varName);
    Vb6Type elemType = resolveArrayElemType(node.asType.get());
    std::string saElemType = mapSaElemType(elemType);

    if (node.dimensions.empty()) return;

    int dimCount = (int)node.dimensions.size();

    if (dimCount == 1) {
        // 涓€缁?ReDim (淇濇寔鍘熸湁1D浠ｇ爜)
        auto& dim = node.dimensions[0];
        std::string lBound = "0";
        std::string uBound = "0";
        if (dim.lower) {
            emitExpr(*dim.lower);
            lBound = std::move(lastExpr_);
        }
        if (dim.upper) {
            emitExpr(*dim.upper);
            uBound = std::move(lastExpr_);
        }

        if (node.preserve) {
            c_.emitLine(cName + " = vb6_SafeArrayReDimPreserve1D(" + cName + ", " + lBound + ", " + uBound + ");");
        } else {
            c_.emitLine("vb6_SafeArrayDestroy1D(" + cName + ");");
            c_.emitLine(cName + " = vb6_SafeArrayReDim1D(" + saElemType + ", " + lBound + ", " + uBound + ");");
        }
    } else {
        // P8.1: 澶氱淮 ReDim
        std::string boundsVar = "_redim_bounds_" + cName;
        c_.emitLine("vb6_SafeArrayBound " + boundsVar + "[] = {");
        c_.indent();
        for (int d = 0; d < dimCount; d++) {
            auto& dim = node.dimensions[d];
            std::string lb = "0", ub = "0";
            if (dim.lower) { emitExpr(*dim.lower); lb = std::move(lastExpr_); }
            if (dim.upper) { emitExpr(*dim.upper); ub = std::move(lastExpr_); }
            std::string trailing = (d < dimCount - 1) ? "," : "";
            c_.emitLine("{" + lb + ", " + ub + "}" + trailing);
        }
        c_.dedent();
        c_.emitLine("};");
        if (node.preserve) {
            c_.emitLine(cName + " = vb6_SafeArrayReDimPreserveND(" + cName + ", " + std::to_string(dimCount) + ", " + boundsVar + ");");
        } else {
            c_.emitLine("vb6_SafeArrayDestroyND(" + cName + ");");
            c_.emitLine(cName + " = vb6_SafeArrayReDimND(" + saElemType + ", " + std::to_string(dimCount) + ", " + boundsVar + ");");
        }

        // 鏇存柊鏁扮粍缁村害淇℃伅
        std::string lower = node.varName;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        arrayDimCounts_[lower] = dimCount;
    }
}

void CCodeGen::visit(EraseStmt& node) {
    for (auto& name : node.varNames) {
        std::string cName = cIdent(name);
        // P8.1: 鏍规嵁缁村害鏁伴€夋嫨1D/ND閿€姣?
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        auto it = arrayDimCounts_.find(lower);
        if (it != arrayDimCounts_.end() && it->second > 1) {
            c_.emitLine("vb6_SafeArrayDestroyND(" + cName + "); " + cName + " = NULL;");
        } else {
            c_.emitLine("vb6_SafeArrayDestroy1D(" + cName + "); " + cName + " = NULL;");
        }
    }
}

void CCodeGen::visit(OpenStmt& node) {
    // VB6: Open pathname For Mode [Access access] As #filenumber
    // C:   vb6_Open(pathname, mode, access, filenumber)
    emitExpr(*node.pathName);
    std::string pathName = std::move(lastExpr_);

    // OpenMode → 数值: Input=1, Output=2, Random=4, Append=8, Binary=16
    int32_t modeVal = 1;
    switch (node.mode) {
        case OpenMode::Input:   modeVal = 1; break;
        case OpenMode::Output:  modeVal = 2; break;
        case OpenMode::Random:  modeVal = 4; break;
        case OpenMode::Append:  modeVal = 8; break;
        case OpenMode::Binary:  modeVal = 16; break;
    }

    // OpenAccess → 数值: Read=1, Write=2, ReadWrite=3, Default=0
    int32_t accessVal = 0;
    switch (node.access) {
        case OpenAccess::Read:      accessVal = 1; break;
        case OpenAccess::Write:     accessVal = 2; break;
        case OpenAccess::ReadWrite: accessVal = 3; break;
        case OpenAccess::Default:   accessVal = 0; break;
    }

    emitExpr(*node.fileNumber);
    std::string fnum = std::move(lastExpr_);

    std::string recLen = "0";
    if (node.recordLength) {
        emitExpr(*node.recordLength);
        recLen = std::move(lastExpr_);
    }
    c_.emitLine("vb6_Open(" + pathName + ", " + std::to_string(modeVal) + ", " +
                std::to_string(accessVal) + ", " + fnum + ", " + recLen + ");");
}

void CCodeGen::visit(CloseStmt& node) {
    if (node.fileNumbers.empty()) {
        c_.emitLine("vb6_CloseAll();");
    } else {
        for (auto& fn : node.fileNumbers) {
            emitExpr(*fn);
            c_.emitLine("vb6_Close(" + lastExpr_ + ");");
        }
    }
}

void CCodeGen::visit(PrintStmt& node) {
    // Print #fnum, expr1; expr2; ...
    emitExpr(*node.fileNumber);
    std::string fnum = std::move(lastExpr_);

    // BSTR表达式检测 (复用Debug.Print相同逻辑)
    static const std::vector<std::string> bstrFuncs = {
        "vb6_BSTR_FromStr", "vb6_Left", "vb6_Right", "vb6_Mid",
        "vb6_UCase", "vb6_LCase", "vb6_UCase_str", "vb6_LCase_str",
        "vb6_Trim", "vb6_LTrim", "vb6_RTrim", "vb6_Chr",
        "vb6_Str", "vb6_CStr", "vb6_Format", "vb6_Hex", "vb6_Oct",
        "vb6_Replace", "vb6_Space", "vb6_String", "vb6_StrReverse",
        "vb6_BSTR_Concat", "vb6_BSTR_Empty", "vb6_App_Path", "vb6_App_EXEName", "vb6_Command", "vb6_CurDir", "vb6_Environ", "vb6_Dir", "vb6_IIfBSTR", "vb6_CDec", "vb6_GetControlText", "vb6_GetControlCaption"
    };
    auto isBstrExpr = [&](const std::string& expr) -> bool {
        for (auto& prefix : bstrFuncs) {
            if (expr.compare(0, prefix.size(), prefix) == 0) return true;
        }
        if (expr.compare(0, 8, "vb6_BSTR") == 0) return true;
        if (expr.find("VB6_SA_AT(BSTR,") != std::string::npos) return true;
        std::string lower = expr;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (knownBstrVars_.count(lower)) return true;
        return false;
    };

    if (node.outputList.empty()) {
        c_.emitLine("vb6_Print(" + fnum + ", NULL);");
    } else {
        for (auto& expr : node.outputList) {
            emitExpr(*expr);
            std::string val = lastExpr_;
            if (isBstrExpr(val)) {
                // 已经是BSTR, 直接传给vb6_Print
                c_.emitLine("vb6_Print(" + fnum + ", " + val + ");");
            } else {
                // 非BSTR: 转换为BSTR后输出
                c_.emitLine("vb6_Print(" + fnum + ", vb6_Str((int32_t)(" + val + ")));");
            }
        }
    }
}

void CCodeGen::visit(WriteStmt& node) {
    emitExpr(*node.fileNumber);
    std::string fnum = std::move(lastExpr_);

    static const std::vector<std::string> bstrFuncs = {
        "vb6_BSTR_FromStr", "vb6_Left", "vb6_Right", "vb6_Mid",
        "vb6_UCase", "vb6_LCase", "vb6_UCase_str", "vb6_LCase_str",
        "vb6_Trim", "vb6_LTrim", "vb6_RTrim", "vb6_Chr",
        "vb6_Str", "vb6_CStr", "vb6_Format", "vb6_Hex", "vb6_Oct",
        "vb6_Replace", "vb6_Space", "vb6_String", "vb6_StrReverse",
        "vb6_BSTR_Concat", "vb6_BSTR_Empty", "vb6_App_Path", "vb6_App_EXEName", "vb6_Command", "vb6_CurDir", "vb6_Environ", "vb6_Dir", "vb6_IIfBSTR", "vb6_CDec", "vb6_GetControlText", "vb6_GetControlCaption"
    };
    auto isBstrExpr = [&](const std::string& expr) -> bool {
        for (auto& prefix : bstrFuncs) {
            if (expr.compare(0, prefix.size(), prefix) == 0) return true;
        }
        if (expr.compare(0, 8, "vb6_BSTR") == 0) return true;
        if (expr.find("VB6_SA_AT(BSTR,") != std::string::npos) return true;
        std::string lower = expr;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (knownBstrVars_.count(lower)) return true;
        return false;
    };

    if (node.outputList.empty()) {
        c_.emitLine("vb6_Write(" + fnum + ", NULL);");
    } else {
        for (auto& expr : node.outputList) {
            emitExpr(*expr);
            std::string val = lastExpr_;
            if (isBstrExpr(val)) {
                c_.emitLine("vb6_Write(" + fnum + ", " + val + ");");
            } else {
                c_.emitLine("vb6_Write(" + fnum + ", vb6_Str((int32_t)(" + val + ")));");
            }
        }
    }
}

void CCodeGen::visit(LineInputStmt& node) {
    // Line Input #fnum, varName
    emitExpr(*node.fileNumber);
    std::string fnum = std::move(lastExpr_);
    emitExpr(*node.varName);
    std::string varExpr = lastExpr_;
    c_.emitLine(varExpr + " = vb6_LineInput(" + fnum + ");");
}

void CCodeGen::visit(InputStmt& node) {
    // Input #fnum, var1, var2, ...
    emitExpr(*node.fileNumber);
    std::string fnum = std::move(lastExpr_);
    for (auto& var : node.varList) {
        emitExpr(*var);
        std::string varExpr = lastExpr_;
        c_.emitLine("vb6_Input(" + fnum + ", &" + varExpr + ");");
    }
}

void CCodeGen::visit(GetStmt& node) {
    // Get #fnum, [recnum], var
    emitExpr(*node.fileNumber);
    std::string fnum = std::move(lastExpr_);
    emitExpr(*node.varName);
    std::string varExpr = std::move(lastExpr_);
    // 确定变量大小: 根据变量名查询已知类型
    std::string varLower = varExpr;
    std::transform(varLower.begin(), varLower.end(), varLower.begin(), ::tolower);
    std::string bareName = varLower;
    std::string sizeExpr;
    if (knownBstrVars_.count(bareName)) {
        sizeExpr = "0";  // BSTR: varSize=0 让RTL层特殊处理
    } else if (knownDoubleVars_.count(bareName)) {
        sizeExpr = "sizeof(double)";
    } else if (knownLongVars_.count(bareName)) {
        sizeExpr = "sizeof(int32_t)";
    } else {
        sizeExpr = "sizeof(int32_t)";  // 默认: Long
    }
    if (node.recordNumber) {
        emitExpr(*node.recordNumber);
        std::string recnum = std::move(lastExpr_);
        c_.emitLine("vb6_Get(" + fnum + ", " + recnum + ", &" + varExpr + ", " + sizeExpr + ");");
    } else {
        // 无 recnum: 顺序读, recnum=0 表示当前位置
        c_.emitLine("vb6_Get(" + fnum + ", 0, &" + varExpr + ", " + sizeExpr + ");");
    }
}

void CCodeGen::visit(PutStmt& node) {
    // Put #fnum, [recnum], var
    emitExpr(*node.fileNumber);
    std::string fnum = std::move(lastExpr_);
    emitExpr(*node.varName);
    std::string varExpr = std::move(lastExpr_);
    // 确定变量大小: 根据变量名查询已知类型
    std::string varLower = varExpr;
    std::transform(varLower.begin(), varLower.end(), varLower.begin(), ::tolower);
    std::string bareName = varLower;
    std::string sizeExpr;
    if (knownBstrVars_.count(bareName)) {
        sizeExpr = "0";  // BSTR: varSize=0 让RTL层特殊处理
    } else if (knownDoubleVars_.count(bareName)) {
        sizeExpr = "sizeof(double)";
    } else if (knownLongVars_.count(bareName)) {
        sizeExpr = "sizeof(int32_t)";
    } else {
        sizeExpr = "sizeof(int32_t)";  // 默认: Long
    }
    if (node.recordNumber) {
        emitExpr(*node.recordNumber);
        std::string recnum = std::move(lastExpr_);
        c_.emitLine("vb6_Put(" + fnum + ", " + recnum + ", &" + varExpr + ", " + sizeExpr + ");");
    } else {
        c_.emitLine("vb6_Put(" + fnum + ", 0, &" + varExpr + ", " + sizeExpr + ");");
    }
}
void CCodeGen::visit(SeekStmt& node) {
    emitExpr(*node.fileNumber);
    std::string fnum = std::move(lastExpr_);
    emitExpr(*node.position);
    c_.emitLine("fseek(vb6_file_table[" + fnum + "], (long)" + lastExpr_ + ", SEEK_SET);");
}

void CCodeGen::visit(LockStmt& node) {
    emitExpr(*node.fileNumber);
    std::string fnum = std::move(lastExpr_);
    std::string startArg = "0", endArg = "0";
    if (node.start) {
        emitExpr(*node.start);
        startArg = std::move(lastExpr_);
        if (node.end) {
            emitExpr(*node.end);
            endArg = std::move(lastExpr_);
        } else {
            endArg = "0";
        }
    }
    c_.emitLine("vb6_Lock((int32_t)(" + fnum + "), (int64_t)(" + startArg + "), (int64_t)(" + endArg + "));");
}

void CCodeGen::visit(UnlockStmt& node) {
    emitExpr(*node.fileNumber);
    std::string fnum = std::move(lastExpr_);
    std::string startArg = "0", endArg = "0";
    if (node.start) {
        emitExpr(*node.start);
        startArg = std::move(lastExpr_);
        if (node.end) {
            emitExpr(*node.end);
            endArg = std::move(lastExpr_);
        } else {
            endArg = "0";
        }
    }
    c_.emitLine("vb6_Unlock((int32_t)(" + fnum + "), (int64_t)(" + startArg + "), (int64_t)(" + endArg + "));");
}

void CCodeGen::visit(ResetStmt& node) {
    (void)node;
    c_.emitLine("vb6_Reset();");
}

void CCodeGen::visit(WidthStmt& node) {
    c_.emitLine("/* Width: no-op */");
}

void CCodeGen::visit(KillStmt& node) {
    emitExpr(*node.pathName);
    c_.emitLine("vb6_Kill(" + lastExpr_ + ");");
}

void CCodeGen::visit(NameStmt& node) {
    emitExpr(*node.oldPath);
    std::string oldP = std::move(lastExpr_);
    emitExpr(*node.newPath);
    c_.emitLine("vb6_Name(" + oldP + ", " + lastExpr_ + ");");
}

void CCodeGen::visit(MkDirStmt& node) {
    emitExpr(*node.pathName);
    c_.emitLine("vb6_MkDir(" + lastExpr_ + ");");
}

void CCodeGen::visit(RmDirStmt& node) {
    emitExpr(*node.pathName);
    c_.emitLine("vb6_RmDir(" + lastExpr_ + ");");
}

void CCodeGen::visit(ChDirStmt& node) {
    emitExpr(*node.pathName);
    c_.emitLine("vb6_ChDir(" + lastExpr_ + ");");
}

void CCodeGen::visit(ChDriveStmt& node) {
    emitExpr(*node.drive);
    c_.emitLine("vb6_ChDrive(" + lastExpr_ + ");");
}

void CCodeGen::visit(FileCopyStmt& node) {
    emitExpr(*node.source);
    std::string src = std::move(lastExpr_);
    emitExpr(*node.destination);
    c_.emitLine("vb6_FileCopy(" + src + ", " + lastExpr_ + ");");
}

void CCodeGen::visit(LabelStmt& node) {
    c_.emitLine("vb6_label_" + cIdent(node.labelName) + ":;");
    // P14.1.2: 如果这是错误处理器标签，结束受保护区块
    if (inProtectedBlock_ && !currentErrorHandlerLabel_.empty() &&
        Symbol::toLower(node.labelName) == Symbol::toLower(currentErrorHandlerLabel_)) {
        inProtectedBlock_ = false;
    }
}

void CCodeGen::visit(GoSubStmt& node) {
    hasGoSub_ = true;
    // GoSub label: 压入返回地址 → goto label
    int retId = gosubReturnCounter_++;
    c_.emitLine("if (vb6_gosub_sp >= 32) { /* P17.4: GoSub stack overflow */ return; }");
c_.emitLine("vb6_gosub_stack[vb6_gosub_sp++] = " + std::to_string(retId) + ";");
    c_.emitLine("goto vb6_label_" + cIdent(node.labelName) + ";");
    c_.emitLine("vb6_gosub_ret_" + std::to_string(retId) + ":;");
}

void CCodeGen::visit(OnGoSubStmt& node) {
    hasGoSub_ = true;
    // P17.4: On x GoSub label1, label2, ... - computed GoSub
    int retId = gosubReturnCounter_++;
    // Stack overflow guard
    c_.emitLine("if (vb6_gosub_sp >= 32) { /* P17.4: GoSub stack overflow */ return; }");
    // Push return address
    c_.emitLine("vb6_gosub_stack[vb6_gosub_sp++] = " + std::to_string(retId) + ";");
    // Evaluate index and dispatch to selected label
    emitExpr(*node.index);
    std::string idxVar = std::move(lastExpr_);
    c_.emitLine("{");
    c_.indent();
    c_.emitLine("int _gosub_idx = " + idxVar + ";");
    c_.emitLine("if (_gosub_idx >= 1 && _gosub_idx <= " + std::to_string(node.labels.size()) + ") {");
    c_.indent();
    c_.emitLine("switch(_gosub_idx) {");
    c_.indent();
    for (size_t k = 0; k < node.labels.size(); k++) {
        c_.emitLine("case " + std::to_string(k + 1) + ": goto vb6_label_" + cIdent(node.labels[k]) + ";");
    }
    c_.dedent();
    c_.emitLine("}");
    c_.dedent();
    c_.emitLine("}");
    c_.dedent();
    c_.emitLine("}");
    // Return landing point (after On...GoSub)
    c_.emitLine("vb6_gosub_ret_" + std::to_string(retId) + ":;");
}

void CCodeGen::visit(OnGoToStmt& node) {
    // P18-A: On x GoTo label1, label2, ... - computed goto
    emitExpr(*node.index);
    std::string idxVar = std::move(lastExpr_);
    c_.emitLine("{");
    c_.indent();
    c_.emitLine("int _goto_idx = " + idxVar + ";");
    c_.emitLine("if (_goto_idx >= 1 && _goto_idx <= " + std::to_string(node.labels.size()) + ") {");
    c_.indent();
    c_.emitLine("switch(_goto_idx) {");
    c_.indent();
    for (size_t k = 0; k < node.labels.size(); k++) {
        c_.emitLine("case " + std::to_string(k + 1) + ": goto vb6_label_" + cIdent(node.labels[k]) + ";");
    }
    c_.dedent();
    c_.emitLine("}");
    c_.dedent();
    c_.emitLine("}");
    c_.dedent();
    c_.emitLine("}");
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
    // Emit target variable address
    emitExpr(*node.target);
    std::string targetVar = std::move(lastExpr_);
    c_.emitLine("vb6_MidSet(&" + targetVar + ", " + startVar + ", " + lenVar + ", " + valueVar + ");");
}

void CCodeGen::visit(OptionStmt& node) {
    // P18-C: Option Compare Text/Binary
    if (node.optionKind == OptionKind::CompareText) {
        c_.emitLine("g_vb6_optionCompareText = 1; /* Option Compare Text */");
    } else if (node.optionKind == OptionKind::CompareBinary) {
        c_.emitLine("g_vb6_optionCompareText = 0; /* Option Compare Binary */");
    }
    // Option Explicit / Option Base 不影响C代码生成
}

void CCodeGen::visit(LocalDeclStmt& node) {
    if (!node.decl) return;

    switch (node.decl->kind) {
        case ASTNodeKind::VariableDecl: {
            auto& var = static_cast<VariableDecl&>(*node.decl);
            std::string cName = cIdent(var.name);

            // P8.1: 局部数组声明 (支持多维)
            if (!var.dimensions.empty()) {
                Vb6Type elemType = resolveArrayElemType(var.asType.get());
                std::string saElemType = mapSaElemType(elemType);
                int dimCount = (int)var.dimensions.size();

                if (dimCount == 1) {
                    // 一维数组: 保持原有1D代码
                    auto& dim = var.dimensions[0];
                    std::string lBound = "0";
                    std::string uBound = "0";
                    if (dim.lower) {
                        emitExpr(*dim.lower);
                        lBound = std::move(lastExpr_);
                    }
                    if (dim.upper) {
                        emitExpr(*dim.upper);
                        uBound = std::move(lastExpr_);
                    }
                    std::string initCode = "vb6_SafeArrayCreate1D(" + saElemType + ", " + lBound + ", " + uBound + ")";
                    c_.emitLine("vb6_SafeArray1D* " + cName + " = " + initCode + ";");
                } else {
                    // 多维数组: 使用ND运行时
                    std::string boundsVar = "_bounds_" + cName;
                    c_.emitLine("vb6_SafeArrayBound " + boundsVar + "[] = {");
                    c_.indent();
                    for (int d = 0; d < dimCount; d++) {
                        auto& dim = var.dimensions[d];
                        std::string lb = "0", ub = "0";
                        if (dim.lower) { emitExpr(*dim.lower); lb = std::move(lastExpr_); }
                        if (dim.upper) { emitExpr(*dim.upper); ub = std::move(lastExpr_); }
                        std::string trailing = (d < dimCount - 1) ? "," : "";
                        c_.emitLine("{" + lb + ", " + ub + "}" + trailing);
                    }
                    c_.dedent();
                    c_.emitLine("};");
                    c_.emitLine("vb6_SafeArrayND* " + cName + " = vb6_SafeArrayCreateND(" + saElemType + ", " + std::to_string(dimCount) + ", " + boundsVar + ");");
                }

                // 注册到已知数组集合
                std::string lower = var.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                knownArrays_.insert(lower);
                arrayElemTypes_[lower] = elemType;
                arrayDimCounts_[lower] = dimCount;
                break;
            }

            // P8.1: 动态数组声明: Dim arr() As Long → 默认1D, ReDim时可能升级
            if (var.isDynamicArray) {
                Vb6Type elemType = resolveArrayElemType(var.asType.get());
                c_.emitLine("vb6_SafeArray1D* " + cName + " = NULL;");

                // 注册到已知数组集合
                std::string lower = var.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                knownArrays_.insert(lower);
                arrayElemTypes_[lower] = elemType;
                arrayDimCounts_[lower] = 1;  // 动态数组默认1D
                break;
            }

            std::string cType = mapTypeRef(var.asType.get());

            // 记录变量类型集合 (用于Debug.Print和COM解封类型推断)
            if (cType == "BSTR") {
                std::string lower = var.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                knownBstrVars_.insert(lower);
            } else if (cType == "int32_t" || cType == "int16_t" || cType == "VBABOOL") {
                std::string lower = var.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                knownLongVars_.insert(lower);
            } else if (cType == "vb6_VARIANT") {
                // P8.4: 记录Variant类型全局变量
                std::string lower = var.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                knownVariantVars_.insert(lower);
            }

            // 记录Object类型变量名 (COM后期绑定)
            if (cType == "void*") {
                std::string lower = var.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                knownObjectVars_.insert(lower);
            }

            // P6.3: 记录前期绑定COM变量 (Dim x As FileSystemObject)
            // 查找类型名是否对应ComClass符号
            if (var.asType && var.asType->kind == ASTNodeKind::SimpleTypeRef) {
                auto& simple = static_cast<SimpleTypeRef&>(*var.asType);
                auto* comSym = symTab_.lookupModule(simple.name);
                if (comSym && (comSym->kind == SymbolKind::ComClass || comSym->kind == SymbolKind::ComInterface)) {
                    std::string lower = var.name;
                    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                    knownTypedComVars_[lower] = comSym;
                    // 从后期绑定集合中移除 (优先前期绑定)
                    knownObjectVars_.erase(lower);
                }
            }

            // 记录double/single类型变量名 (用于Debug.Print浮点输出)
            if (cType == "double" || cType == "float") {
                std::string lower = var.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                knownDoubleVars_.insert(lower);
            }

            // 记录类类型变量名, 默认值用NULL
            bool isLocalClassType = false;
            bool isLocalUdtType = false;
            bool isLocalComIfaceType = false;
            bool isLocalVb6IfaceType = false;  // P6.4: VB6接口引用
            if (var.asType && var.asType->kind == ASTNodeKind::SimpleTypeRef) {
                auto& simple = static_cast<SimpleTypeRef&>(*var.asType);
                auto* clsSym = symTab_.lookupModule(simple.name);
                if (clsSym && clsSym->kind == SymbolKind::Class) {
                    std::string lower = var.name;
                    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                    // P6.4: 接口类 → knownIfaceVars_ (而非 knownClassVars_)
                    if (clsSym->isInterface) {
                        knownIfaceVars_[lower] = clsSym->name;
                        isLocalVb6IfaceType = true;
                    } else {
                        knownClassVars_.insert(lower);
                        isLocalClassType = true;
                        // P14.3.1: Dim As New自动实例化
                        if (var.isNew) {
                            knownNewVars_[lower] = cIdent(clsSym->name);
                        }
                    }
                }
                if (clsSym && (clsSym->kind == SymbolKind::ComClass || clsSym->kind == SymbolKind::ComInterface)) {
                    isLocalComIfaceType = true;
                }
                // 检查是否是UDT类型
                auto* udtSym = symTab_.lookup(simple.name);
                if (udtSym && udtSym->kind == SymbolKind::UserDefinedType) {
                    isLocalUdtType = true;
                }
            }

            // VB6 Static变量: 跨调用持久化 → C static局部变量
            // 包括: 显式Static声明 或 Static Sub/Function内的所有局部变量
            std::string storageClass = (var.isStatic || inStaticProc_) ? "static " : "";

            if (var.initializer) {
                emitExpr(*var.initializer);
                c_.emitLine(storageClass + cType + " " + cName + " = " + lastExpr_ + ";");
            } else {
                std::string initVal;
                if (isLocalClassType || isLocalComIfaceType) {
                    initVal = "NULL";
                } else if (isLocalVb6IfaceType) {
                    initVal = "{0}";  // P6.4: 接口引用 = {vtbl=NULL, obj=NULL}
                } else if (isLocalUdtType) {
                    initVal = "{0}";
                } else {
                    initVal = defaultValue(
                        var.asType ? typeSys_.resolveTypeName(static_cast<SimpleTypeRef*>(var.asType.get())->name) : Vb6Type::Variant
                    );
                }
                c_.emitLine(storageClass + cType + " " + cName + " = " + initVal + ";");
            }
            break;
        }
        case ASTNodeKind::ConstDecl: {
            auto& con = static_cast<ConstDecl&>(*node.decl);
            std::string cType = mapTypeRef(con.asType.get());
            std::string cName = cIdent(con.name);
            if (con.value) {
                emitExpr(*con.value);
                c_.emitLine("const " + cType + " " + cName + " = " + lastExpr_ + ";");
            }
            break;
        }
        default:
            c_.emitLine("/* unhandled LocalDeclStmt: " + std::string(node.decl->kindName()) + " */");
            break;
    }
}

} // namespace vb6c3
