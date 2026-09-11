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
            case ASTNodeKind::Block:            visit(static_cast<Block&>(*stmt)); break;
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

        // M22: 释放Declare ANSI函数的临时char*变量 (每条语句后统一清理, 无内存泄露)
        for (auto& ansiVar : ansiTempsToFree_) {
            c_.emitLine("vb6_FreeANSI(" + ansiVar + ");");
        }
        ansiTempsToFree_.clear();
    }
}

void CCodeGen::visit(Block& node) {
    emitStmtList(node.stmts);
}

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
                if (!objClass.empty() && propLetSym->isExternal) {
                    std::string propModLower = propLetSym->sourceModule;
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
                    std::string funcName = cProcName(prefix + maExpr.memberName, propLetSym->access, sourceModule);
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
                if (!isAssigningReturnValue) {
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
            c_.emitLine(target + " = " + value + ";");
        }
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
            if (knownTypedComVars_.count(targetLower)) {
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

void CCodeGen::visit(IfStmt& node) {
    emitExpr(*node.condition);
    if (isComMarker_) resolveComValue("Int");  // If条件通常是Boolean/整数
    // Fix 090ag: If <Variant 值> 条件 — VB6 将 Variant 按真值判定 (Empty/0/False 为假).
    // 此前对 As Variant 参数/变量的裸标识符条件生成 if (JsonValue) → C2083 (vb6_VARIANT
    // 结构体比较非法). 需显式 vb6_VariantToBool.
    if (!isComMarker_) {
        bool condIsVariant = cExprIsVariant(lastExpr_);
        if (!condIsVariant && node.condition
            && node.condition->kind == ASTNodeKind::IdentifierExpr) {
            std::string cndLower =
                Symbol::toLower(static_cast<IdentifierExpr&>(*node.condition).name);
            if (knownVariantVars_.count(cndLower)) condIsVariant = true;
        }
        if (condIsVariant && lastExpr_.find("vb6_VariantToBool(") == std::string::npos) {
            lastExpr_ = "vb6_VariantToBool(" + lastExpr_ + ")";
        }
    }
    c_.emitLine("if (" + lastExpr_ + ") {");
    c_.indent();
    emitStmtList(node.thenBody);
    c_.dedent();

    // Fix 063b: ElseIf 条件可能生成 _vcmp_ 临时变量声明,
    // C 不允许在 } 和 else if 之间出现声明语句.
    // 改用 } else { if (...) { 模式, 将 _vcmp_ 声明放入 else 块内.
    int elseIfWrapCount = 0;
    for (auto& elseif : node.elseIfs) {
        c_.emitLine("} else {");
        emitExpr(*elseif->condition);
        c_.emitLine("if (" + lastExpr_ + ") {");
        c_.indent();
        emitStmtList(elseif->body);
        c_.dedent();
        elseIfWrapCount++;
    }

    if (!node.elseBody.empty()) {
        c_.emitLine("} else {");
        c_.indent();
        emitStmtList(node.elseBody);
        c_.dedent();
    }

    c_.emitLine("}");  // 关闭最内层 if/else
    for (int i = 0; i < elseIfWrapCount; i++) {
        c_.emitLine("}");  // 关闭每个 else 包装块
    }
}

void CCodeGen::visit(ElseIfClause& node) {
    // 由IfStmt内部处理
}

void CCodeGen::visit(ForStmt& node) {
    // Fix 092n: For 的三个子表达式相互独立 — 进入每个子表达式前清掉上游残留的
    // COM 标记, 生成后立即按整数上下文消费本次产生的标记. 否则 `For i = .Count
    // To 1 Step -1` (With 对象属性读) 会双重出错 (pvSubClass 708/709):
    //   ① start 只得到对象本身 — `i = _vb6_with_2 /* With COM .Count */`;
    //   ② 该标记残留到 Step, 被 UnaryExpr 内的 `if (isComMarker_) resolveComValue()`
    //      消费 → `i_step = (-vb6_ComGetStringProp(_vb6_with_2, L"Count"))` (C2171).
    auto emitForExpr092n = [&](Expr& e) -> std::string {
        isComMarker_ = false;
        emitExpr(e);
        if (isComMarker_) resolveComValue("Long");
        return std::move(lastExpr_);
    };
    std::string start = emitForExpr092n(*node.start);
    std::string end = emitForExpr092n(*node.end);

    std::string step = "1";
    if (node.step) {
        step = emitForExpr092n(*node.step);
    }

    std::string var = cIdent(node.varName);

    // Fix 022: For 循环变量若为类成员必须以 me->var 形式 emit.
    // 原 Fix 010o 无差别将 For 变量注册到 knownLocalVars_, 但当变量实际是类成员
    // (例: cCsv.cls:51 'Dim i As Long, ii As Long' at module level → class field)
    // 时, 注册会让下游 IdentifierExpr (cgen_expr.cpp:425-432) 跳过 me-> 前缀,
    // 循环体内对 ii 的引用变裸标识符 ii, 但函数内无 C 局部 ii 只有 me->ii
    // → C2065 未声明标识符 (cCsv.c 34 处 ii + 13 处 i 同根因).
    //
    // 修复策略:
    //   1. 若 For 变量是类成员 (且未被局部 Dim 遮蔽), 用 me->var 形式 emit loop
    //      自身赋值/比较/累加语句, 且 **不** 注册到 knownLocalVars_ — 让循环体
    //      内 IdentifierExpr 走 me-> 路径 (cgen_expr.cpp 已支持).
    //   2. 否则保持原 Fix 010o 行为 (注册 + 裸 var), 覆盖局部 Dim 变量、
    //      非类模块变量、跨模块变量等情况.
    // 注: 临时变量 var_end / var_step 是 block-scoped int32_t 局部, 无需 me->.
    std::string lower022 = node.varName;
    std::transform(lower022.begin(), lower022.end(), lower022.begin(), ::tolower);
    bool forVarIsClassMember = isClassModule_
        && classMemberVars_.count(lower022)
        && !knownLocalVars_.count(lower022);  // 局部 Dim 优先遮蔽类成员
    std::string varAcc = forVarIsClassMember ? ("me->" + var) : var;
    // Fix 081g: If For-loop var is a ByRef param (C: int32_t*), use *var for assignments/comparisons
    bool forVarIsByRef = !forVarIsClassMember && knownByRefParams_.count(lower022);
    if (forVarIsByRef) {
        varAcc = "(*" + var + ")";
    }
    if (!forVarIsClassMember && !forVarIsByRef) {
        knownLocalVars_.insert(lower022);  // Fix 010o (保留)
    }

    // P14.3.2: 嵌套循环栈 - 支持Exit For跳转到正确层
    std::string exitLabel = "vb6_loop_exit_" + std::to_string(labelCounter_++);
    loopStack_.push_back({ExitKind::For, exitLabel});

    // Fix 084o: For 循环控制变量为 Variant 时, 初始化/比较/步进必须包装.
    // VB6 语义: Variant 循环变量按 Long 语义驱动循环, 循环体内保持实际索引值.
    // 否则生成 var = 0 (int→vb6_VARIANT)、var <= end (vb6_VARIANT vs int32_t)
    // 引发 C2440 (cToolsHttp 等以 Variant 做循环变量的模块).
    bool forVarIsVariant = knownVariantVars_.count(lower022)
        || (forVarIsClassMember && classVariantMembers_.count(lower022))
        || cExprIsVariant(varAcc);
    if (forVarIsVariant) {
        // Fix 090o: 收集本 For body 内定义的标签, 供 GoTo 后缀配对判定
        forSplitLabelStack_.push_back({});
        collectForBodyLabels(node.body, forSplitLabelStack_.back());
        c_.emitLine("{");
        c_.indent();
        c_.emitLine("int32_t " + var + "_end = " + end + ";");
        c_.emitLine("int32_t " + var + "_step = " + step + ";");
        c_.emitLine(varAcc + " = vb6_VariantFromValue(" + start + ");");
        c_.emitLine("if (" + var + "_step > 0) {");
        c_.indent();
        c_.emitLine("for (; vb6_VariantToLong(" + varAcc + ") <= " + var + "_end; "
                    + varAcc + " = vb6_VariantFromValue(vb6_VariantToLong(" + varAcc + ") + " + var + "_step)) {");
        c_.indent();
        labelCopyIdx_ = 0;
        emitStmtList(node.body);
        c_.dedent();
        c_.emitLine("}");
        c_.dedent();
        c_.emitLine("} else {");
        c_.indent();
        c_.emitLine("for (; vb6_VariantToLong(" + varAcc + ") >= " + var + "_end; "
                    + varAcc + " = vb6_VariantFromValue(vb6_VariantToLong(" + varAcc + ") + " + var + "_step)) {");
        c_.indent();
        labelCopyIdx_ = 1;
        emitStmtList(node.body);
        labelCopyIdx_ = 0;
        c_.dedent();
        c_.emitLine("}");
        c_.dedent();
        c_.emitLine("}");
        c_.dedent();
        c_.emitLine("}");
        c_.emitLine(exitLabel + ":;  /* Exit For target */");

        loopStack_.pop_back();
        forSplitLabelStack_.pop_back();
        return;
    }

    // Fix 090o: 收集本 For body 内定义的标签, 供 GoTo 后缀配对判定
    forSplitLabelStack_.push_back({});
    collectForBodyLabels(node.body, forSplitLabelStack_.back());
    c_.emitLine("{");
    c_.indent();
    c_.emitLine("int32_t " + var + "_end = " + end + ";");
    c_.emitLine("int32_t " + var + "_step = " + step + ";");
    c_.emitLine(varAcc + " = " + start + ";");
    c_.emitLine("if (" + var + "_step > 0) {");
    c_.indent();
    c_.emitLine("for (; " + varAcc + " <= " + var + "_end; " + varAcc + " += " + var + "_step) {");
    c_.indent();
    labelCopyIdx_ = 0;
    emitStmtList(node.body);
    c_.dedent();
    c_.emitLine("}");
    c_.dedent();
    c_.emitLine("} else {");
    c_.indent();
    c_.emitLine("for (; " + varAcc + " >= " + var + "_end; " + varAcc + " += " + var + "_step) {");
    c_.indent();
    labelCopyIdx_ = 1;
    emitStmtList(node.body);
    labelCopyIdx_ = 0;
    c_.dedent();
    c_.emitLine("}");
    c_.dedent();
    c_.emitLine("}");
    c_.dedent();
    c_.emitLine("}");
    c_.emitLine(exitLabel + ":;  /* Exit For target */");

    loopStack_.pop_back();
    forSplitLabelStack_.pop_back();
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
    // Fix 091f: 集合是返回 SafeArray1D* 的表达式 (Split/Filter 等) — 需先物化到
    // 临时数组变量, 再走数组迭代路径. 此前一律走 COM 路径 →
    // vb6_ForEach_Init(vb6_VariantToObjectVal(vb6_Split(...))) C2440
    // (ToolsTlsThunks 754/2913/3182).
    bool collArrIsExpr = false;

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
    } else if (node.collection->kind == ASTNodeKind::IndexOrCallExpr) {
        // Fix 091f: For Each 源为内置返回 String 数组的函数 (Split/Filter) →
        // 数组迭代路径.
        auto& call091f = static_cast<IndexOrCallExpr&>(*node.collection);
        if (call091f.callee && call091f.callee->kind == ASTNodeKind::IdentifierExpr) {
            std::string cn091f = static_cast<IdentifierExpr&>(*call091f.callee).name;
            std::transform(cn091f.begin(), cn091f.end(), cn091f.begin(), ::tolower);
            if (!cn091f.empty() && cn091f.back() == '$') cn091f.pop_back();
            if (cn091f == "split" || cn091f == "filter") {
                isCollArray = true;
                collArrIsExpr = true;
                collElemType = Vb6Type::String;
            } else if (cn091f == "array") {
                // Fix 091g: Array(...) → vb6_ArrayCreate 建 Variant 数组
                // (元素为 vb6_VARIANT). For Each 源为 Array() 时此前走 COM 路径
                // → vb6_VariantToObjectVal(_arr_N) C2440 (ToolsTlsThunks 3178).
                isCollArray = true;
                collArrIsExpr = true;
                collElemType = Vb6Type::Variant;
            }
        }
    }

    // Fix 092q: For Each 遍历当前过程的 ParamArray 参数时, C 侧类型是 SAFEARRAY*
    // (cgen 的参数映射), 而 vb6_LBound/vb6_UBound/VB6_SA_AT 都要求 vb6_SafeArray1D*
    // (`(arr)->data` / `->lBound`) → 对 tagSAFEARRAY 报 C2039 且上下界取值语义错
    // (mdTlsThunks.bas `For Each vElem In a` (a 是 ParamArray), ToolsTlsThunks 5468).
    bool isPA092q = false;
    if (node.collection && node.collection->kind == ASTNodeKind::IdentifierExpr && currentProc_) {
        std::string paLower092q =
            Symbol::toLower(static_cast<IdentifierExpr&>(*node.collection).name);
        for (auto& p092q : currentProc_->params) {
            if (Symbol::toLower(p092q.name) == paLower092q) {
                isPA092q = p092q.isParamArray;
                break;
            }
        }
    }

    std::string var = cIdent(node.varName);
    int tmpIdx = tempCounter_++;

    // Fix 022: ForEach 循环变量若为类成员必须以 me->var 形式 emit (同 ForStmt).
    // 原 Fix 010o 无差别注册循环变量到 knownLocalVars_, 类成员场景会导致 C2065
    // (详见 ForStmt 注释). 此处同步修复.
    std::string lower022fe = node.varName;
    std::transform(lower022fe.begin(), lower022fe.end(), lower022fe.begin(), ::tolower);
    bool forEachVarIsClassMember = isClassModule_
        && classMemberVars_.count(lower022fe)
        && !knownLocalVars_.count(lower022fe);  // 局部 Dim 优先遮蔽
    std::string varAcc = forEachVarIsClassMember ? ("me->" + var) : var;
    if (!forEachVarIsClassMember) {
        knownLocalVars_.insert(lower022fe);  // Fix 010o (保留)
    }

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
        if (isPA092q) {
            // Fix 092q: ParamArray 取值是 Windows VARIANT (SafeArrayGetElement),
            // 需经 vb6_VariantFromComResult 转成 vb6_VARIANT (ToolsTlsThunks 5468
            // 曾因直接赋值报 C2440 "VARIANT" → "vb6_VARIANT").
            c_.emitLine("VARIANT _pa_v" + std::to_string(tmpIdx) + " = {0};");
        }
        // Fix 091f: 表达式集合物化 — 避免 LBound/UBound/元素取值重复调用
        // Split/Filter (会每次重新分配数组).
        std::string arrRef091f = collArrName;
        if (collArrIsExpr) {
            emitExpr(*node.collection);
            std::string tmpArr091f = "_fe_arr" + std::to_string(tmpIdx);
            c_.emitLine("vb6_SafeArray1D* " + tmpArr091f + " = " + lastExpr_ + ";");
            arrRef091f = tmpArr091f;
        }
        if (isPA092q) {
            // Fix 092q: ParamArray 用 PA 专用上下界 (SafeArrayGetElement 语义)
            c_.emitLine(lbVar + " = vb6_PA_LBound(" + arrRef091f + ");");
            c_.emitLine(ubVar + " = vb6_PA_UBound(" + arrRef091f + ");");
        } else {
            c_.emitLine(lbVar + " = vb6_LBound(" + arrRef091f + ", 1);");
            c_.emitLine(ubVar + " = vb6_UBound(" + arrRef091f + ", 1);");
        }
        c_.emitLine("for (" + idxVar + " = " + lbVar + "; " + idxVar + " <= " + ubVar + "; " + idxVar + "++) {");
        c_.indent();

        // 赋值循环变量: vb6_item = VB6_SA_AT(elemCType, arr, _fe_i0)
        // Fix 084o: 循环变量为 Variant 时需 vb6_VariantFromValue 包装 (String 数组
        // 元素是 BSTR, 直接赋给 vb6_VARIANT 触发 C2440, 如 cToolsHttp 的 Pair As Variant)
        std::string elemCType = mapSaElemCType(collElemType);
        bool feVarIsVariant = knownVariantVars_.count(lower022fe)
            || (forEachVarIsClassMember && classVariantMembers_.count(lower022fe))
            || cExprIsVariant(varAcc);
        if (isPA092q) {
            // Fix 092q: ParamArray 元素用 SafeArrayGetElement 直取 (索引与
            // vb6_PA_LBound/UBound 同域), 按循环变量/元素类型选择解包函数.
            std::string paGet092q = "vb6_PA_GetVariant(" + arrRef091f + ", " + idxVar + ")";
            std::string paTmp092q = "_pa_v" + std::to_string(tmpIdx);
            if (feVarIsVariant) {
                c_.emitLine(paTmp092q + " = " + paGet092q + ";");
                c_.emitLine(varAcc + " = vb6_VariantFromComResult(&" + paTmp092q + ");");
            } else if (elemCType == "BSTR") {
                c_.emitLine(varAcc + " = vb6_PA_GetBSTR(" + arrRef091f + ", " + idxVar + ");");
            } else if (elemCType == "double") {
                c_.emitLine(varAcc + " = vb6_PA_GetDouble(" + arrRef091f + ", " + idxVar + ");");
            } else {
                c_.emitLine(varAcc + " = vb6_PA_GetLong(" + arrRef091f + ", " + idxVar + ");");
            }
        } else if (feVarIsVariant) {
            c_.emitLine(varAcc + " = vb6_VariantFromValue(VB6_SA_AT(" + elemCType + ", "
                        + arrRef091f + ", " + idxVar + "));");
        } else {
            c_.emitLine(varAcc + " = VB6_SA_AT(" + elemCType + ", " + arrRef091f + ", " + idxVar + ");");
        }

        emitStmtList(node.body);
        c_.dedent();
        c_.emitLine("}");
        c_.dedent();
        c_.emitLine("}");
    } else {
        // P22-11: COM collection For Each via IEnumVARIANT
        // Generate:
        //   {
        //       void* _fe_enum_N = vb6_ForEach_Init(coll);
        //       VARIANT _fe_var_N;
        //       if (_fe_enum_N) {
        //           while (vb6_ForEach_Next(_fe_enum_N, (void*)&_fe_var_N)) {
        //               var = _fe_var_N;
        //               // body
        //               vb6_ComVarClear((void*)&_fe_var_N);
        //           }
        //           vb6_ForEach_Release(_fe_enum_N);
        //       }
        //   }
        c_.emitLine("{");
        c_.indent();
        std::string enumVar = "_fe_enum_" + std::to_string(tmpIdx);
        std::string feVar = "_fe_var_" + std::to_string(tmpIdx);
        emitExpr(*node.collection);
        // P24-05: 物化COM标记 — For Each的集合表达式可能是COM成员访问(如fso.Drives)
        // COM标记设置了comObjExpr_/comMemberName_但lastExpr_只有基础对象名
        if (isComMarker_) {
            resolveComValue("Object");  // 集合必须是Object(IDispatch*), 生成ComGetObjectProp
        }
        std::string collExpr = lastExpr_;
        // Fix 091q: vb6_ComCall/vb6_ComCallObject 已返回对象指针 (void*),
        // vb6_ForEach_Init 直接接收. 不可再按 Variant 提取 — cLang 26:
        // For Each x In me.LangInfo.Item("LangList") (COM Item 调用, 生成
        // vb6_ComCall(...)) 曾被 isDefinitelyVariantExpr 判为 Variant →
        // vb6_VariantToObjectVal(vb6_ComCall(...)) C2440 (void* → vb6_VARIANT).
        bool collIsObjPtr091q = collExpr.rfind("vb6_ComCall(", 0) == 0
                             || collExpr.rfind("vb6_ComCallObject(", 0) == 0;
        // Fix 040c: vb6_ForEach_Init expects void* (IDispatch*). If the collection
        // expression is a Variant (vb6_VARIANT struct), extract the object pointer.
        if (!collIsObjPtr091q && cExprIsVariant(collExpr)) {
            collExpr = "vb6_VariantToObjectVal(" + collExpr + ")";
        } else if (node.collection && node.collection->kind == ASTNodeKind::IdentifierExpr) {
            auto& ident = static_cast<IdentifierExpr&>(*node.collection);
            std::string lower = ident.name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (knownVariantVars_.count(lower)) {
                collExpr = "vb6_VariantToObjectVal(" + collExpr + ")";
            }
        }
        // Fix 040c: fallback — project class methods returning Variant (e.g. Dictionary.Keys)
        else if (!collIsObjPtr091q && node.collection && isDefinitelyVariantExpr(*node.collection)) {
            collExpr = "vb6_VariantToObjectVal(" + collExpr + ")";
        }
        c_.emitLine("void* " + enumVar + " = vb6_ForEach_Init(" + collExpr + ");");
        c_.emitLine("VARIANT " + feVar + ";");
        c_.emitLine("if (" + enumVar + ") {");
        c_.indent();
        c_.emitLine("while (vb6_ForEach_Next(" + enumVar + ", (void*)&" + feVar + ")) {");
        c_.indent();
        // P24-05: For Each循环变量类型感知赋值
        // Object类型 → vb6_VariantToObject提取IDispatch*
        // Variant类型 → vb6_VariantFromStackVARIANT转换为vb6_VARIANT
        {
            std::string lower = node.varName;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (knownObjectVars_.count(lower) || knownTypedComVars_.count(lower)) {
                c_.emitLine(varAcc + " = vb6_ComUnpackObject(&" + feVar + ");  /* P24-05: For Each Object: VARIANT→IDispatch* */");
            } else if (knownClassVars_.count(lower)) {
                // Fix 090aa: 项目类变量 (vb6_cls_X*) 作 For Each 元素 — 元素是
                // COM VARIANT(IDispatch*), 提取后 cast 到类指针. 此前漏查 knownClassVars_
                // → 走 Variant 分支 oClient = vb6_VARIANT → C2440.
                c_.emitLine(varAcc + " = (vb6_cls_" + cIdent(knownClassVars_[lower])
                            + "*)vb6_ComUnpackObject(&" + feVar + ");  /* P24-05: For Each 项目类: VARIANT→IDispatch*→类指针 */");
            } else {
                c_.emitLine(varAcc + " = vb6_VariantFromStackVARIANT(&" + feVar + ");  /* P24-05: For Each Variant: VARIANT→vb6_VARIANT */");
            }
        }
        emitStmtList(node.body);
        c_.emitLine("VariantClear(&" + feVar + ");  /* P24-03: 栈VARIANT只清不清 */");
        c_.dedent();
        c_.emitLine("}");
        c_.emitLine("vb6_ForEach_Release(" + enumVar + ");");
        c_.dedent();
        c_.emitLine("}");
        c_.dedent();
        c_.emitLine("}");
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
    // Fix 038b-6: 如果测试表达式是 Variant 但临时变量是具体类型, 插入提取函数
    // 仅使用 cExprIsVariant (C 字符串级) 和 knownVariantVars_ 检测.
    {
        std::string initExpr = testVar;
        bool testIsVariant = cExprIsVariant(testVar);
        if (!testIsVariant && node.testExpr && node.testExpr->kind == ASTNodeKind::IdentifierExpr) {
            auto& id = static_cast<IdentifierExpr&>(*node.testExpr);
            std::string idLower = id.name;
            std::transform(idLower.begin(), idLower.end(), idLower.begin(), ::tolower);
            if (knownVariantVars_.count(idLower)) testIsVariant = true;
        }
        if (testIsVariant && !isStringSelect && !isFloatSelect) {
            // int32_t temp = Variant → vb6_VariantToLong(Variant)
            initExpr = "vb6_VariantToLong(" + testVar + ")";
        } else if (testIsVariant && isStringSelect) {
            // BSTR temp = Variant → vb6_VariantToString(Variant)
            initExpr = "vb6_VariantToString(" + testVar + ")";
        } else if (testIsVariant && isFloatSelect) {
            // double temp = Variant → vb6_VariantToDouble(Variant)
            initExpr = "vb6_VariantToDouble(" + testVar + ")";
        }
        c_.emitLine(tempType + " " + tempVar + " = " + initExpr + ";");
    }

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
        // --- Fix 086: 方法调用链对象推断 (With .Sql(...) / With obj.Method(...)) ---
        // With 对象是 IndexOrCallExpr/MemberAccessExpr/WithMemberExpr (方法调用链) 时,
        // 用 inferClassTypeOfExpr 推断返回类. 此前 `With .Sql(...)` 解析为
        // IndexOrCallExpr (非裸 WithMemberExpr), 嵌套With分支不命中 → tempType void*
        // → 成员解析回退全局 lookupModule 捡错符号 (如 .Fetch 命中模块级 Sub Fetch,
        // 生成 vb6_Demo_Fetch(_vb6_with_N) 这类带多余参数的非法调用).
        // 仅接受项目 Class (排除 COM 类/接口/UDT), 其余情况走原有分支.
        if (node.object->kind == ASTNodeKind::IndexOrCallExpr
            || node.object->kind == ASTNodeKind::MemberAccessExpr
            || node.object->kind == ASTNodeKind::WithMemberExpr) {
            std::string inferred = inferClassTypeOfExpr(*node.object);
            if (!inferred.empty()) {
                const Symbol* clsSym = lookupModuleDotted(inferred);
                if (clsSym && clsSym->kind == SymbolKind::Class && !clsSym->isInterface) {
                    withInfo.kind = WithObjKind::ClassInstance;
                    withInfo.className = cIdent(inferred);
                    tempType = "vb6_cls_" + withInfo.className + "*";
                    withInfo.ctrlOrigName = withInfo.className;
                }
            }
        }
        // --- Fix 010n: New表达式检测 (With New ClassName) ---
        if (node.object->kind == ASTNodeKind::NewExpr) {
            auto& newExpr = static_cast<NewExpr&>(*node.object);
            std::string clsLower = newExpr.className;
            std::transform(clsLower.begin(), clsLower.end(), clsLower.begin(), ::tolower);
            // Fix 086: COM类检测 — With New ADODB.Stream 等 COM 类型没有项目
            // Class 符号 (ComClass/ComInterface 或未注册), 不能按项目类生成
            // vb6_cls_<X>* 临时 (类型未定义 → C2065). 转 COMObject 走后期绑定.
            const Symbol* newClsSym = lookupModuleDotted(newExpr.className);
            if (!newClsSym || newClsSym->kind == SymbolKind::ComClass
                || newClsSym->kind == SymbolKind::ComInterface) {
                withInfo.kind = WithObjKind::COMObject;
                tempType = "void*";
                withInfo.ctrlOrigName = cIdent(newExpr.className);
            } else {
                // "With New X" 总是类实例
                withInfo.kind = WithObjKind::ClassInstance;
                withInfo.className = cIdent(newExpr.className);  // Fix 011r-1
                tempType = "vb6_cls_" + withInfo.className + "*";
                withInfo.ctrlOrigName = withInfo.className;
            }
        }
        // --- Fix 010n: WithMemberExpr (嵌套With: With .Method()) ---
        else if (node.object->kind == ASTNodeKind::WithMemberExpr) {
            // 嵌套With: .Member() — 继承外层With的对象类型
            if (!withObjectInfoStack_.empty()) {
                const auto& outerInfo = withObjectInfoStack_.back();
                if (outerInfo.kind == WithObjKind::ClassInstance ||
                    outerInfo.kind == WithObjKind::COMObject) {
                    // 方法返回值通常是同类或另一个类实例
                    withInfo.kind = WithObjKind::ClassInstance;
                    // Fix 011r-1: 继承外层className (对property chain如 .SubObj.SubMethod常见)
                    // 注意: 如外层方法返回不同类实例, 此继承会错误 — 当前简化处理
                    withInfo.className = outerInfo.className;
                    if (!withInfo.className.empty()) {
                        tempType = "vb6_cls_" + withInfo.className + "*";
                    }
                } else if (outerInfo.kind == WithObjKind::FormControl ||
                           outerInfo.kind == WithObjKind::WithEventsCtrl) {
                    // 控件属性返回的对象 → 类实例
                    withInfo.kind = WithObjKind::ClassInstance;
                    // 控件方法返回的对象类型未知, 不设置className
                }
                // UDT/Unknown/BuiltinObject: 保持Unknown (struct.field访问)
            }
        }

        // --- IdentifierExpr: 标识符With对象 ---
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

            // Fix 043b: Early-bound COM variable detection (Dim x As Dictionary, etc.)
            // knownObjectVars_ only contains void* (late-binding) variables.
            // Early-bound COM variables (vb6_ComIface_IDictionary*, etc.) are in
            // knownTypedComVars_. Without this check, With blocks on early-bound
            // COM locals fall through as Unknown, causing .Add to be resolved as
            // the class's own method instead of COM dispatch (C2198).
            if (withInfo.kind == WithObjKind::Unknown) {
                if (knownTypedComVars_.count(objNameLower)) {
                    withInfo.kind = WithObjKind::COMObject;
                }
            }

            // 类实例变量检测
            if (withInfo.kind == WithObjKind::Unknown) {
                auto itClassVar = knownClassVars_.find(objNameLower);
                if (itClassVar != knownClassVars_.end()) {
                    withInfo.kind = WithObjKind::ClassInstance;
                    // Fix 011r-1: 设置className, 让WithMemberExpr能精确解析该类方法
                    withInfo.className = cIdent(itClassVar->second);
                    tempType = "vb6_cls_" + withInfo.className + "*";
                }
            }

            // Fix 043b: Class field detection — knownObjectVars_ and knownClassVars_
            // only contain local variables/parameters, NOT class fields. When a With
            // block targets a class field (e.g., `With Dic` where Dic is `Dim Dic As
            // Dictionary`), we need to check classVoidFieldMap_ (for void*/COM fields)
            // and classTypedFieldMap_ (for typed class fields) to determine the correct
            // WithObjKind. Without this, COM fields like Dictionary fall through as
            // Unknown, and .Add inside the With block gets resolved to the class's own
            // Add method instead of COM dispatch (C2198).
            if (withInfo.kind == WithObjKind::Unknown && isClassModule_) {
                // Check void* (COM) fields first
                if (classVoidFieldMap_) {
                    auto itV = classVoidFieldMap_->find(moduleName_);
                    if (itV != classVoidFieldMap_->end() && itV->second.count(objNameLower)) {
                        withInfo.kind = WithObjKind::COMObject;
                    }
                }
                // Check typed class fields (project class or COM interface)
                if (withInfo.kind == WithObjKind::Unknown && classTypedFieldMap_) {
                    auto itT = classTypedFieldMap_->find(moduleName_);
                    if (itT != classTypedFieldMap_->end()) {
                        auto itField = itT->second.find(objNameLower);
                        if (itField != itT->second.end()) {
                            const std::string& typeName = itField->second;
                            if (typeName.rfind("COM:", 0) == 0) {
                                // COM interface field (e.g., "COM:Dictionary")
                                withInfo.kind = WithObjKind::COMObject;
                            } else {
                                // Project class field (e.g., "cAsyncSocket")
                                withInfo.kind = WithObjKind::ClassInstance;
                                withInfo.className = cIdent(typeName);
                                tempType = "vb6_cls_" + withInfo.className + "*";
                            }
                        }
                    }
                }
            }

            // Fix 010n: UDT变量检测 → 设置正确的struct类型 (而非void*)
            // 这样 struct.field 访问才能通过编译 (C2224修复)
            if (withInfo.kind == WithObjKind::Unknown) {
                auto itUdt = knownUdtVars_.find(objNameLower);
                if (itUdt != knownUdtVars_.end()) {
                    tempType = itUdt->second;  // e.g., "vb6_type_OPENFILENAME"
                    // Keep Unknown kind — struct.field 访问对UDT是正确的
                    // Fix 037: 注册 With 临时变量到 knownUdtVars_, 让嵌套 UDT 字段
                    // 访问 (如 _vb6_with_N.DecrBuffer.Data(0)) 能推断出 UDT 类型,
                    // 正确生成 VB6_SA_AT 而非误当函数调用 (C2064).
                    knownUdtVars_[tempVar] = itUdt->second;
                    knownLocalVars_.insert(tempVar);
                }
            }

            // Fix 054: With目标为当前函数UDT返回值 (如 With QRCodegenMakeBytes → vb6_ret_QRCodegenMakeBytes)
            // 函数返回变量不在 knownUdtVars_ 中, 但返回类型可能是UDT
            if (withInfo.kind == WithObjKind::Unknown && !currentReturnVar_.empty()) {
                std::string retLower = currentReturnVar_;
                std::transform(retLower.begin(), retLower.end(), retLower.begin(), ::tolower);
                if (retLower.find(objNameLower) != std::string::npos) {
                    // objName matches the return variable → check return type
                    if (currentReturnCType_.rfind("vb6_type_", 0) == 0) {
                        tempType = currentReturnCType_;
                        knownUdtVars_[tempVar] = currentReturnCType_;
                        knownLocalVars_.insert(tempVar);
                    }
                }
            }

            // Fix 010l: 内置全局对象检测 (Err/App/Screen/Printer/Clipboard/Debug)
            if (withInfo.kind == WithObjKind::Unknown) {
                if (objNameLower == "err" || objNameLower == "app" ||
                    objNameLower == "screen" || objNameLower == "printer" ||
                    objNameLower == "clipboard" || objNameLower == "debug") {
                    withInfo.kind = WithObjKind::BuiltinObject;
                    withInfo.ctrlOrigName = objNameLower;  // store lowercase name
                }
            }
        }

        // --- Fix 010n: MemberAccessExpr (With obj.member / With me.member) ---
        if (node.object->kind == ASTNodeKind::MemberAccessExpr && withInfo.kind == WithObjKind::Unknown) {
            auto& memExpr = static_cast<MemberAccessExpr&>(*node.object);
            std::string memberLower = memExpr.memberName;
            std::transform(memberLower.begin(), memberLower.end(), memberLower.begin(), ::tolower);

            // Fix 090s: MAE 目标是「宿主类的字段」时按宿主类查字段表 — With
            // Http.RequestDataQuery (Http As cHttpClient, RequestDataQuery As New
            // Dictionary = COM void* 字段): 此前成员不在 knownClassVars_ 后走
            // memSym 全局查找, 捡到 Dictionary 符号按 ClassInstance +
            // vb6_cls_Dictionary* 处理 → .Item("k")=v 生成结构体字段调用 C2039.
            // 正确按宿主类 classVoidFieldMap_/classTypedFieldMap_ 分类: void*/
            // COM: → WithObjKind::COMObject (COM dispatch), 项目类 → ClassInstance.
            std::string hostCls90s = memExpr.object ? inferClassTypeOfExpr(*memExpr.object) : "";
            if (!hostCls90s.empty()) {
                if (classVoidFieldMap_) {
                    auto itV90s = classVoidFieldMap_->find(hostCls90s);
                    if (itV90s != classVoidFieldMap_->end() && itV90s->second.count(memberLower)) {
                        withInfo.kind = WithObjKind::COMObject;
                        withInfo.ctrlOrigName = hostCls90s;
                    }
                }
                if (withInfo.kind == WithObjKind::Unknown && classTypedFieldMap_) {
                    auto itT90s = classTypedFieldMap_->find(hostCls90s);
                    if (itT90s != classTypedFieldMap_->end()) {
                        auto itF90s = itT90s->second.find(memberLower);
                        if (itF90s != itT90s->second.end()) {
                            if (itF90s->second.rfind("COM:", 0) == 0) {
                                withInfo.kind = WithObjKind::COMObject;
                                withInfo.ctrlOrigName = hostCls90s;
                            } else {
                                withInfo.kind = WithObjKind::ClassInstance;
                                withInfo.className = cIdent(itF90s->second);
                                tempType = "vb6_cls_" + withInfo.className + "*";
                                withInfo.ctrlOrigName = withInfo.className;
                            }
                        }
                    }
                }
            }

            // 检查成员是否为类实例变量 (me.member As SomeClass)
            if (knownClassVars_.find(memberLower) != knownClassVars_.end()) {
                withInfo.kind = WithObjKind::ClassInstance;
                // Fix 011r-1: 获取成员的类名, 设置tempType
                auto itClassVar = knownClassVars_.find(memberLower);
                if (itClassVar != knownClassVars_.end()) {
                    withInfo.className = cIdent(itClassVar->second);
                    tempType = "vb6_cls_" + withInfo.className + "*";
                }
            } else if (knownObjectVars_.count(memberLower)) {
                withInfo.kind = WithObjKind::COMObject;
            } else if (knownUdtVars_.count(memberLower)) {
                // UDT成员: 使用struct类型 (如 With ofn → vb6_type_OPENFILENAME)
                auto itUdt = knownUdtVars_.find(memberLower);
                if (itUdt != knownUdtVars_.end()) {
                    tempType = itUdt->second;
                    // Fix 037: 注册 With 临时变量到 knownUdtVars_
                    knownUdtVars_[tempVar] = itUdt->second;
                    knownLocalVars_.insert(tempVar);
                }
            } else if (withInfo.kind == WithObjKind::Unknown) {
                // Fix 090s: kind 已被上面 hostCls90s 字段表解析 (COMObject/ClassInstance)
                // 时不再走 memSym 兜底 — 否则 With Http.RequestDataQuery (RequestDataQuery
                // As New Dictionary = COM void* 字段, A1 已置 COMObject) 被
                // lookup("RequestDataQuery") 捡到 Dictionary 符号 → 覆盖成 ClassInstance
                // + vb6_cls_Dictionary* → .Item(k)=v 生成结构体字段调用 C2039.
                // 尝试从符号表推断类型
                Symbol* memSym = symTab_.lookupModule(memExpr.memberName);
                if (!memSym) memSym = symTab_.lookup(memExpr.memberName);
                if (memSym) {
                    if (memSym->type == Vb6Type::UserDefinedType) {
                        // 查找UDT类型的C标识符
                        // TODO: Symbol没有存储typeRefName, 需要其他方式
                    } else if (memSym->type == Vb6Type::Object) {
                        withInfo.kind = WithObjKind::ClassInstance;
                        // Fix 011r-1: 若Symbol有variableTypeName, 用之; 否则className未知
                        if (!memSym->variableTypeName.empty()) {
                            withInfo.className = cIdent(memSym->variableTypeName);
                            tempType = "vb6_cls_" + withInfo.className + "*";
                        }
                    }
                }
                // 无法确定类型时默认为类实例 (void*不支持.member访问)
                if (withInfo.kind == WithObjKind::Unknown && tempType == "void*") {
                    withInfo.kind = WithObjKind::ClassInstance;
                }
            }
        }

        // --- Fix 010n: IndexOrCallExpr (With arr(idx) / With func()) ---
        if (node.object->kind == ASTNodeKind::IndexOrCallExpr && withInfo.kind == WithObjKind::Unknown) {
            // 数组元素或函数返回值 — 通常是类实例或VARIANT
            // 数组元素访问如 m_uWindowState(0) → VARIANT UDT
            auto& callExpr = static_cast<IndexOrCallExpr&>(*node.object);
            if (callExpr.callee && callExpr.callee->kind == ASTNodeKind::IdentifierExpr) {
                auto& idExpr = static_cast<IdentifierExpr&>(*callExpr.callee);
                std::string arrLower = idExpr.name;
                std::transform(arrLower.begin(), arrLower.end(), arrLower.begin(), ::tolower);
                // Fix 055: 优先检查UDT数组元素类型
                auto itUdtArr = arrayUdtElemTypes_.find(arrLower);
                if (itUdtArr != arrayUdtElemTypes_.end()) {
                    tempType = itUdtArr->second;  // e.g. "vb6_type_RECT"
                    // 注册到knownUdtVars_, 让后续字段访问正确
                    knownUdtVars_[tempVar] = itUdtArr->second;
                    knownLocalVars_.insert(tempVar);
                } else {
                    // 检查是否为已知数组 → 元素类型
                    auto itArr = arrayElemTypes_.find(arrLower);
                    if (itArr != arrayElemTypes_.end()) {
                        if (itArr->second == Vb6Type::UserDefinedType) {
                            tempType = "vb6_VARIANT";  // UDT数组元素存储为VARIANT (fallback, should be caught above)
                        } else if (itArr->second == Vb6Type::Variant || itArr->second == Vb6Type::Object) {
                            tempType = "vb6_VARIANT";
                        }
                    }
                }
            }
            // 函数返回值且仍为void* → 默认按 COM 后期绑定分发
            if (withInfo.kind == WithObjKind::Unknown && tempType == "void*") {
                // Fix 090y: void* With 目标 (COM 方法返回对象, 如 cIni.Section As
                // Dictionary) → COMObject (后期绑定 dispatch). 此前 ClassInstance
                // (className 空) → WithMemberExpr 成员解析落入全局符号表撞名
                // (cTimers.Item) → 左值错误 C2106. UDT 目标 tempType=vb6_type_*
                // 不受影响; Variant-对象目标运行时 dispatch 也更贴合 COM 语义.
                withInfo.kind = WithObjKind::COMObject;
            }
        }

        // --- 最终回退: void* 不支持 .member 访问 → COM 后期绑定分发 ---
        // Fix 090y: (同上方 Fix, 独立于 isClassModule_ 字段检测的最终兜底)
        // UDT struct (vb6_type_*) 目标 tempType 非 void* → 不受影响.
        if (withInfo.kind == WithObjKind::Unknown && tempType == "void*") {
            withInfo.kind = WithObjKind::COMObject;
        }
        // Fix 054: tempType 已解析为 UDT struct (vb6_type_*) → 保持 Unknown, 用 struct.field 访问
        if (withInfo.kind == WithObjKind::ClassInstance && tempType.rfind("vb6_type_", 0) == 0) {
            withInfo.kind = WithObjKind::Unknown;
        }
    }

    // Fix 092o: 本帧 withInfo 延后到目标表达式生成之后再入栈 — 若提前入栈, 嵌套
    // With 的目标表达式 (.ReturnJson()) 会以内层 className (cJson) 解析外层成员
    // (实为 cHttpClient 的 ReturnJson) → 解析失败退化为数据字段访问 (cAliyunCaptcha
    // 222: `_vb6_with_3->ReturnJson()` C2039 + 实参丢失). 见下方 emitExpr 之后的入栈.

    // Fix 010l: BuiltinObject 不需要临时变量 — 属性读写直接映射为RTL函数调用
    if (withInfo.kind == WithObjKind::BuiltinObject) {
        withObjectInfoStack_.push_back(withInfo);  // Fix 092o: BuiltinObject 无目标表达式, 直接入栈
        // 推入占位符以保持 withObjectVars_ 与 withObjectInfoStack_ 同步
        withObjectVars_.push_back(withInfo.ctrlOrigName);
        c_.emitLine("{");
        c_.indent();
        emitStmtList(node.body);
        c_.dedent();
        c_.emitLine("}");
        withObjectVars_.pop_back();
        withObjectInfoStack_.pop_back();
        return;
    }

    // P17.1: 抑制With对象表达式的默认属性解析
    bool prevSuppress = suppressDefaultProp_;
    if (withInfo.kind == WithObjKind::FormControl || withInfo.kind == WithObjKind::WithEventsCtrl) {
        suppressDefaultProp_ = true;
    }

    emitExpr(*node.object);

    suppressDefaultProp_ = prevSuppress;

    // Fix 092o: 目标表达式已生成完毕 (期间保持外层栈顶), 现在把本帧 withInfo 入栈 —
    // body 内的 .成员 解析与下方的 Menu 判定都依赖它.
    withObjectInfoStack_.push_back(withInfo);

    if (!withObjectInfoStack_.empty() && withObjectInfoStack_.back().kind == WithObjKind::FormControl && withObjectInfoStack_.back().ctrlType == FrmControlType::Menu) {  // P20-36
        c_.emitLine("int " + tempVar + " = 0;  /* Menu: no HWND, props use (hmenu,menuId) */");
    } else {
        // Fix 038: C2440 修复 — UDT 同类型转换和 UDT/VARIANT → void* 转换
        bool isUdtTempType = (tempType.rfind("vb6_type_", 0) == 0);
        if (isUdtTempType) {
            // Fix 081j: UDT With块使用指针引用，而非值拷贝
            // VB6中 With uPoints(lIdx) 内 .X = ... 直接修改数组元素
            // C中需要用指针: vb6_type_RECT* _vb6_with = &VB6_SA_AT(...)
            c_.emitLine(tempType + "* " + tempVar + " = &(" + lastExpr_ + ")  /* With object ref (ptr) */;");
        } else if (tempType == "void*") {
            // 检查表达式是否为 UDT 或 VARIANT — 这些类型不能直接 cast 到 void*
            std::string udtCType = inferUdtTypeOfExpr(*node.object);
            if (!udtCType.empty()) {
                // UDT → void*: 取地址获取指针
                c_.emitLine(tempType + " " + tempVar + " = &(" + lastExpr_ + ")  /* With object ref */;");
            } else if (isDefinitelyVariantExpr(*node.object) && inferClassTypeOfExpr(*node.object).empty()) {
                // VARIANT → void*: 用 VariantToObjectVal 提取对象指针
                // Fix 084e: 若表达式实际是类对象 (如 Cookies("name") 返回
                // vb6_cls_cHttpServerCookieAttr* 但被误判为 Variant), 则
                // VariantToObjectVal(对象指针) 触发 C2440, 走下方 (void*) 直转.
                // Fix 090e: 符号表把项目类默认成员属性 (Property Get Cookie()
                // As cHttpServerCookieAttr) 的返回类型误注册为 Variant 时,
                // 生成的 C 表达式是 vb6_cHttpServerCookies_prop_get_Cookie(...)
                // (返回 vb6_cls_cHttpServerCookieAttr*), 并非 vb6_VARIANT 值;
                // 对类指针调 VariantToObjectVal → C2440 (cHttpServerCookies
                // ExpireCookie: With Cookie(Key)). 仅当 C 级确认实参是
                // vb6_VARIANT (cExprIsVariant / 已知 Variant 变量/字段) 时
                // 走 VariantToObjectVal, 否则按对象指针 (void*) 直转.
                bool withIsVariantVal090e = cExprIsVariant(lastExpr_);
                if (!withIsVariantVal090e) {
                    std::string lower090e = lastExpr_;
                    std::transform(lower090e.begin(), lower090e.end(),
                                   lower090e.begin(), ::tolower);
                    if (knownVariantVars_.count(lower090e)) {
                        withIsVariantVal090e = true;
                    } else if (lower090e.compare(0, 4, "me->") == 0) {
                        std::string mem090e = lower090e.substr(4);
                        if (classVariantMembers_.count(mem090e)) {
                            withIsVariantVal090e = true;
                        }
                    }
                }
                if (withIsVariantVal090e) {
                    c_.emitLine(tempType + " " + tempVar + " = vb6_VariantToObjectVal(" + lastExpr_ + ")  /* With object ref */;");
                } else {
                    c_.emitLine(tempType + " " + tempVar + " = (" + tempType + ")" + lastExpr_ + "  /* With object ref */;");
                }
            } else {
                // Fix 092j: inferClassTypeOfExpr 非空 (按 VB 声明推断出项目类) 但
                // C 级表达式实际是 Variant 值 (COM 链结果) → 不能 (void*) 硬转:
                //   cLang 144: With me->LangInfo.Item("LangList").Item(Idx+1)
                //     → (void*)vb6_VariantFromComResult(vb6_ComCall(...)) C2440
                //       "无法从 vb6_VARIANT 转换为 void *";
                // 仅当 C 级确认是 vb6_VARIANT 时改用 VariantToObjectVal 提取.
                if (cExprIsVariant(lastExpr_)) {
                    c_.emitLine(tempType + " " + tempVar + " = vb6_VariantToObjectVal(" + lastExpr_ + ")  /* With object ref */;");
                } else {
                    c_.emitLine(tempType + " " + tempVar + " = (" + tempType + ")" + lastExpr_ + "  /* With object ref */;");
                }
            }
        } else {
            c_.emitLine(tempType + " " + tempVar + " = (" + tempType + ")" + lastExpr_ + "  /* With object ref */;");
        }
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
    // Fix 086: 与 LabelStmt 的方向副本后缀配对
    // Fix 090o: 后缀仅在「目标标签定义在某个被方向拆分的 For body 内」时使用
    // (该标签会被两份副本各定义一次, goto 需与所在副本的 _dN 定义配对)。
    // 目标若在 body 外 (函数级出口标签如 QH/EH, VB6: GoTo 跳出循环到过程尾)
    // 只定义一份, 加后缀会指向不存在的 vb6_label_X_dN → C2094 (cZipArchive 事件取消出口)。
    std::string targetLower = Symbol::toLower(node.labelName);
    bool targetInSplitBody = false;
    for (auto& splitLabels : forSplitLabelStack_) {
        if (splitLabels.count(targetLower)) {
            targetInSplitBody = true;
            break;
        }
    }
    std::string lblSuffix =
        (labelCopyIdx_ > 0 && targetInSplitBody) ? ("_d" + std::to_string(labelCopyIdx_ + 1)) : "";
    c_.emitLine("goto vb6_label_" + cIdent(node.labelName) + lblSuffix + ";");
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
                                "vb6_BSTR_Concat", "vb6_BSTR_Empty", "vb6_App_Path", "vb6_App_EXEName", "vb6_App_HelpFile", "vb6_Command", "vb6_CurDir", "vb6_Environ", "vb6_Dir", "vb6_IIfBSTR", "vb6_GetControlText", "vb6_GetControlCaption"
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

                            // Fix 091r: Debug.Print 参数是否为 Variant 值 —
                            // cExprIsVariant 只认 C 表达式前缀, 不认"Variant 变量名"
                            // (局部/模块级 knownVariantVars_ / 类字段 classVariantFields_).
                            // Demo 671: For Each 循环变量 x As Variant 被赋
                            // vb6_VariantFromStackVARIANT 后 Debug.Print x →
                            // DebugWriteLong((int32_t)(x)) C2440 (vb6_VARIANT→int32_t).
                            auto isVariantVal091r = [&](const std::string& e) -> bool {
                                if (cExprIsVariant(e)) return true;
                                std::string n091r = e;
                                std::string suffix091r;
                                size_t cmt091r = n091r.find("/*");
                                if (cmt091r != std::string::npos) {
                                    suffix091r = n091r.substr(cmt091r);
                                    n091r = n091r.substr(0, cmt091r);
                                }
                                while (!n091r.empty() && (n091r.back() == ' ' || n091r.back() == '\t')) {
                                    n091r.pop_back();
                                }
                                if (n091r.rfind("me->", 0) == 0) n091r = n091r.substr(4);
                                else if (n091r.size() > 4 && n091r[0] == '(' && n091r[1] == '*'
                                         && n091r.back() == ')') {
                                    n091r = n091r.substr(2, n091r.size() - 3);
                                }
                                std::string ln091r = Symbol::toLower(n091r);
                                if (knownVariantVars_.count(ln091r)
                                    || classVariantFields_.count(ln091r)) {
                                    return true;
                                }
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
                                } else if (isVariantVal091r(val)) {
                                    // Fix 090x: Debug.Print x (x As Variant 变量 /
                                    // Variant 表达式) — 运行时值按字符串输出. 此前落入
                                    // DebugWriteLong((int32_t)(x)) → C2440 (无法从
                                    // vb6_VARIANT 转换 int32_t).
                                    // Fix 091r: 判定改用 isVariantVal091r (含
                                    // knownVariantVars_/类字段 兜底).
                                    c_.emitLine("vb6_DebugWriteBSTR(vb6_VariantToString(" + val + "));");
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

        // Fix 015: 标记 callee 上下文, 让 visit(MemberAccessExpr) 的 Fix 015 路径
        // 把链式对象参数通过 pendingChainObj_ 交付, 而不是直接合成 func(wrappedObj)
        // (那样会让下面的 bare-call 判定 callExpr.find('(') != npos 错过 padding).
        pendingChainObj_.clear();
        bool savedAsCallCallee = asCallCallee_;
        asCallCallee_ = true;
        emitExpr(*node.callee);
        asCallCallee_ = savedAsCallCallee;
        // 语句级调用: 确保表达式被求值(即使是void调用)
        // 如果结果是函数名(不含括号), 自动添加()调用
        std::string callExpr = lastExpr_;

        // COM调用检测 (P6.2): isComMarker_标志
        if (isComMarker_) {
            isComMarker_ = false;
            // Fix 086: 无括号的控件方法调用 (List1.Clear) — 与 IndexOrCallExpr
            // 的 P13.3 处理一致, 生成 vb6_ClearList(vb6_hwnd_Listx), 而非
            // vb6_ComCall(list1,...) 裸控制名 (C2065).
            auto itCtrlCS = knownFormControls_.find(comObjExpr_);
            if (itCtrlCS != knownFormControls_.end()
                && (itCtrlCS->second == FrmControlType::ListBox
                    || itCtrlCS->second == FrmControlType::ComboBox)
                && Symbol::toLower(comMemberName_) == "clear") {
                std::string ctrlNameCS = cIdent(knownFormControlOriginalNames_.count(comObjExpr_)
                    ? knownFormControlOriginalNames_[comObjExpr_] : comObjExpr_);
                comObjExpr_.clear();
                comMemberName_.clear();
                c_.emitLine("vb6_ClearList((void*)vb6_hwnd_" + ctrlNameCS + ");  /* ListBox.Clear */");
                return;
            }
            // 无括号的COM方法调用: obj.Method → vb6_ComCall(obj, L"Method", NULL, 0)
            callExpr = "vb6_ComCall(" + comObjExpr_ + ", L\"" + comMemberName_ + "\", NULL, 0)";
            comObjExpr_.clear();
            comMemberName_.clear();
        } else if (callExpr == "0") {
            // M22: void function call returned 0 (no-value), discard entire statement
            return;
        } else if (callExpr.find('(') == std::string::npos) {
            // Fix 010m: Bare call (no parentheses) — build complete arg list
            // Handle: me-prepend for class methods, ParamArray, Optional padding, _has_ flags
            bool calleeHasPA = false;
            std::vector<ParameterInfo> calleeParams;
            // Fix 030b: 跟踪 builtin 状态 — builtin 跳过 padding/IsMissing 尾叜
            bool calleeIsBuiltin = false;

            if (node.callee && node.callee->kind == ASTNodeKind::IdentifierExpr) {
                auto& idExpr = static_cast<IdentifierExpr&>(*node.callee);
                Symbol* sym = symTab_.lookupModule(idExpr.name);
                if (!sym || (sym->kind != SymbolKind::Sub && sym->kind != SymbolKind::Function
                    && sym->kind != SymbolKind::PropertyGet && sym->kind != SymbolKind::PropertyLet
                    && sym->kind != SymbolKind::PropertySet)) {
                    sym = symTab_.lookup(idExpr.name);
                }
                if (sym && (sym->kind == SymbolKind::Sub || sym->kind == SymbolKind::Function
                    || sym->kind == SymbolKind::PropertyGet || sym->kind == SymbolKind::PropertyLet
                    || sym->kind == SymbolKind::PropertySet)) {
                    calleeParams = sym->params;
                    calleeIsBuiltin = sym->isBuiltin;
                }
            }
            // Fix 015: Call X.Y(args).Z (无尾括号) 形态下 node.callee 是 .Z MemberAccessExpr.
            // 此时 Fix 015 emit 出的 lastExpr_ 是裸函数名 "vb6_cDataBase_Exec",
            // 对象参数通过 pendingChainObj_ 传递. 这里需要按成员名查找参数签名,
            // 才能正确填充 Optional 默认参数.
            else if (node.callee && node.callee->kind == ASTNodeKind::MemberAccessExpr) {
                auto& maExpr = static_cast<MemberAccessExpr&>(*node.callee);
                Symbol* sym = symTab_.lookupModule(maExpr.memberName);
                if (sym && (sym->kind == SymbolKind::Sub || sym->kind == SymbolKind::Function
                    || sym->kind == SymbolKind::PropertyGet || sym->kind == SymbolKind::PropertyLet
                    || sym->kind == SymbolKind::PropertySet)) {
                    calleeParams = sym->params;
                    calleeIsBuiltin = sym->isBuiltin;
                }
                // Fix 084y-3: 链式调用对象方法时 (X.Y(args).Z), .Z 是类方法而非
                // 模块成员, lookupModule 必然失败 → calleeParams 为空 → 不填充
                // Optional 默认值 → C2198 参数太少 (如 Exec(Optional RecordsAffected,
                // Optional Options As Long = -1) 声明5参却只传对象1参).
                // 用 inferClassTypeOfExpr 推断对象类 (X.Y(args) → cDataBase),
                // 再按类方法签名取形参表.
                if (calleeParams.empty() && maExpr.object) {
                    std::string chainClass = inferClassTypeOfExpr(*maExpr.object);
                    if (!chainClass.empty()) {
                        std::vector<ParameterInfo> clsParams;
                        bool clsBuiltin = false;
                        if (findClassMemberCallParams(chainClass, maExpr.memberName,
                                                      clsParams, clsBuiltin)) {
                            calleeParams = clsParams;
                            calleeIsBuiltin = clsBuiltin;
                        }
                    }
                }
            }
            // Fix 090s: With 块内无括号类方法调用 (.Start — callee=WithMemberExpr,
            // callExpr 裸函数名, 与 Fix 015 MAE 同协议). visit(WithMemberExpr)
            // asCallCallee_ 已不再拼完整调用而是交付 pendingChainObj_ → 此处
            // 解析形参表才能做 Optional padding, 否则 .Start 声明带 4 个
            // Optional 参只传 this → C2198 参数太少.
            else if (node.callee && node.callee->kind == ASTNodeKind::WithMemberExpr) {
                auto& wmExpr90s = static_cast<WithMemberExpr&>(*node.callee);
                if (!withObjectInfoStack_.empty()) {
                    const auto& wmInfo90s = withObjectInfoStack_.back();
                    if (wmInfo90s.kind == WithObjKind::ClassInstance
                        && !wmInfo90s.className.empty()) {
                        std::vector<ParameterInfo> wmParams90s;
                        bool wmBuiltin90s = false;
                        if (findClassMemberCallParams(wmInfo90s.className,
                                                      wmExpr90s.memberName,
                                                      wmParams90s, wmBuiltin90s)) {
                            calleeParams = wmParams90s;
                            calleeIsBuiltin = wmBuiltin90s;
                        }
                    }
                }
            }

            // Check for ParamArray
            int paIndex = -1;
            for (size_t i = 0; i < calleeParams.size(); i++) {
                if (calleeParams[i].isParamArray) { paIndex = (int)i; calleeHasPA = true; break; }
            }

            // P6.6: 类模块中调用同类方法, 需要自动添加me作为第一个参数
            std::string bareArgList;
            if (isClassModule_ && currentProc_) {
                std::string modPrefix = "vb6_" + cIdent(moduleName_) + "_";
                if (callExpr.find(modPrefix) == 0) {
                    bareArgList = "(void*)me";
                }
            }

            // Fix 015: 链式调用对象参数前置 — 由 visit(MemberAccessExpr).Fix015 交付
            if (!pendingChainObj_.empty()) {
                if (!bareArgList.empty()) bareArgList += ", ";
                bareArgList += pendingChainObj_;
                pendingChainObj_.clear();
            }

            if (calleeHasPA) {
                // ParamArray: pass NULL SAFEARRAY*
                if (!bareArgList.empty()) bareArgList += ", ";
                bareArgList += "NULL";
            } else if (calleeParams.size() > 0 && !calleeIsBuiltin) {
                // Pad all params with default values (bare call = 0 args)
                // Fix 030b: builtin 跳过 padding/IsMissing 路径 (RTL C 签名不接受尾叜)
                for (size_t i = 0; i < calleeParams.size(); i++) {
                    const auto& param = calleeParams[i];
                    if (!bareArgList.empty()) bareArgList += ", ";
                    std::string defVal;
                    if (param.hasDefaultValue && !param.defaultValueExpr.empty()) {
                        defVal = param.defaultValueExpr;
                    } else {
                        defVal = defaultValue(param.type);
                    }
                    if (param.isByVal) {
                        bareArgList += defVal;
                    } else {
                        // ByRef: pass address of compound literal
                        std::string cType = mapType(param.type);
                        if (param.type == Vb6Type::Variant || param.type == Vb6Type::Empty ||
                            param.type == Vb6Type::Null || param.type == Vb6Type::Object) {
                            bareArgList += "&(" + cType + "){0}";
                        } else {
                            bareArgList += "&(" + cType + "){" + defVal + "}";
                        }
                    }
                }
                // Append _has_ flags for Optional params (all 0 since none passed)
                for (size_t i = 0; i < calleeParams.size(); i++) {
                    if (calleeParams[i].isOptional && !calleeParams[i].isParamArray) {
                        if (!bareArgList.empty()) bareArgList += ", ";
                        bareArgList += "0";
                    }
                }
            }
            // Fix 034: Builtin Sub statements with Optional ByVal 参 — 硬编码补默认值.
            // calleeParams 未注册的 builtin (如 Randomize), bare-call 无参时补默认值.
            // Randomize([seed]) — RTL vb6_Randomize(double seed); 不传参时 seed=0.0.
            if (callExpr == "vb6_Randomize" && bareArgList.empty()) {
                bareArgList = "0.0";
            }
            callExpr += "(" + bareArgList + ")";
        }

        // Fix 090g: fallback 合成 (visit MemberAccessExpr asCallCallee_ → func(obj)
        // 完整调用) 产生的单 this 语句调用 — 无括号 MAE 语句 (如 Response.State403,
        // State403 声明 (Optional Say As String) → C 签名 (me, BSTR*, int _has_Say)),
        // callExpr 只含 this → C2198 参数太少. 检测括号内无顶层逗号 (单 this
        // 实参, 用户实参由 IndexOrCallExpr 承载不会以 MAE 形态到此) 后按形参表
        // 重建: this + 各形参默认值 + Optional _has_ 标志 (同 bare-call padding).
        if (node.callee && node.callee->kind == ASTNodeKind::MemberAccessExpr) {
            auto& maExpr090g = static_cast<MemberAccessExpr&>(*node.callee);
            std::string cls090g = inferClassTypeOfExpr(*maExpr090g.object);
            std::vector<ParameterInfo> params090g;
            bool builtin090g = false;
            if (!cls090g.empty()
                && findClassMemberCallParams(cls090g, maExpr090g.memberName,
                                             params090g, builtin090g)
                && !params090g.empty() && !builtin090g) {
                size_t p090g = callExpr.find('(');
                if (p090g != std::string::npos && callExpr.size() >= 3
                    && callExpr.back() == ')') {
                    bool topComma090g = false;
                    int depth090g = 0;
                    for (size_t k090g = p090g; k090g < callExpr.size(); k090g++) {
                        char ch090g = callExpr[k090g];
                        if (ch090g == '(') depth090g++;
                        else if (ch090g == ')') {
                            depth090g--;
                            if (depth090g == 0) break;
                        } else if (ch090g == ',' && depth090g == 1) {
                            topComma090g = true;
                            break;
                        }
                    }
                    if (!topComma090g) {
                        std::string thisArg090g = callExpr.substr(p090g + 1,
                                                                  callExpr.size() - p090g - 2);
                        std::string fullArg090g = thisArg090g;
                        bool anyPad090g = false;
                        for (size_t i090g = 0; i090g < params090g.size(); i090g++) {
                            const auto& prm090g = params090g[i090g];
                            if (prm090g.isParamArray) continue;
                            std::string defVal090g;
                            if (prm090g.hasDefaultValue && !prm090g.defaultValueExpr.empty()) {
                                defVal090g = prm090g.defaultValueExpr;
                            } else {
                                defVal090g = defaultValue(prm090g.type);
                            }
                            if (prm090g.isByVal) {
                                fullArg090g += ", " + defVal090g;
                            } else {
                                std::string cT090g = mapType(prm090g.type);
                                if (prm090g.type == Vb6Type::Variant
                                    || prm090g.type == Vb6Type::Empty
                                    || prm090g.type == Vb6Type::Null
                                    || prm090g.type == Vb6Type::Object) {
                                    fullArg090g += ", &(" + cT090g + "){0}";
                                } else {
                                    fullArg090g += ", &(" + cT090g + "){" + defVal090g + "}";
                                }
                            }
                            anyPad090g = true;
                        }
                        for (size_t i090g = 0; i090g < params090g.size(); i090g++) {
                            if (params090g[i090g].isOptional
                                && !params090g[i090g].isParamArray) {
                                fullArg090g += ", 0";
                            }
                        }
                        if (anyPad090g) {
                            callExpr = callExpr.substr(0, p090g) + "("
                                       + fullArg090g + ")";
                        }
                    }
                }
            }
        }

        if (callExpr.find("vb6_ComCall(") == 0) {
            // ComCall返回可能含对象的VARIANT*, 用VarFree避免Release对象
            c_.emitLine("vb6_ComVarFree((void*)" + callExpr + ");  /* COM call, discard result */");
        } else if (callExpr.find("vb6_ComGetProp(") == 0) {
            c_.emitLine("vb6_ComVarClear((void*)" + callExpr + ");  /* COM prop get, discard result */");
        } else {
            c_.emitLine(callExpr + ";");
        }

        // M22: ANSI临时变量释放由emitStmtList统一处理, 此处不再单独清理
    }
}

void CCodeGen::visit(ReDimStmt& node) {
    // Fix 084y-5: ReDim 目标含成员访问 (ByRef UDT 参数数组字段 uOutput.Buffer,
    // With 块成员 .Field) 时按成员访问展开, 避免 cIdent 把 '.' 替换成 '_'
    std::string cName = resolveArrayTargetIdent(node.varName);
    // Fix 010r: Add me-> prefix for class member arrays
    std::string lowerVar = node.varName;
    std::transform(lowerVar.begin(), lowerVar.end(), lowerVar.begin(), ::tolower);
    // Fix 086: ReDim 目标是当前 Function/PropertyGet 自身名 → 返回值变量
    // (VB6: 数组返回函数内 ReDim Preserve FuncName(...) 重设返回数组.
    //  此前生成裸函数名 → C2065, 如 cMemoryStream.Contents / cImage.PictureToBytes)
    if (currentProc_ && !currentReturnVar_.empty()
        && (currentProc_->kind == SymbolKind::Function
            || currentProc_->kind == SymbolKind::PropertyGet)
        && Symbol::toLower(currentProc_->name) == lowerVar
        && !knownLocalVars_.count(lowerVar)) {
        cName = currentReturnVar_;
        lowerVar = currentReturnVar_;
    }
    if (isClassModule_ && classMemberVars_.count(lowerVar) && !knownLocalVars_.count(lowerVar)) {
        cName = "me->" + cName;
    }
    // Fix 010r-6 rev2: ByRef array param in ReDim needs (*name) since it's vb6_SafeArray1D**
    // Fix 084o-6: ByRef Variant 参数也是 vb6_VARIANT*, 同样需要 (*name)
    // (VB6 允许 As Variant 参数后接 ReDim 变数组, 如 cZipArchive.Extract 的
    // OutputTarget As Variant → ReDim OutputTarget(...) As Byte)
    if (currentProc_) {
        for (auto& param : currentProc_->params) {
            std::string paramLower = param.name;
            std::transform(paramLower.begin(), paramLower.end(), paramLower.begin(), ::tolower);
            if (paramLower == lowerVar && !param.isByVal
                && ((static_cast<uint16_t>(param.type) & static_cast<uint16_t>(Vb6Type::Array))
                    || param.type == Vb6Type::Variant)) {
                cName = "(*" + cName + ")";
                break;
            }
        }
    }
    // Fix 061: With块内 ReDim .Data(...) → _vb6_with_N.Data
    // parser 在 varName 前加了 '.' 前缀表示 With 成员引用 (e.g. ".Data")
    if (node.varName.size() > 1 && node.varName[0] == '.'
        && !withObjectVars_.empty() && !withObjectInfoStack_.empty()) {
        std::string memberName = node.varName.substr(1);  // strip leading '.'
        const auto& info = withObjectInfoStack_.back();
        if (info.kind == WithObjKind::Unknown) {
            // Fix 081j-2: UDT With block 临时变量是指针，用 -> 访问成员
            cName = withObjectVars_.back() + "->" + cIdent(memberName);
        } else if (info.kind == WithObjKind::ClassInstance) {
            // Class With block: _vb6_with_N->member
            cName = withObjectVars_.back() + "->" + cIdent(memberName);
        }
    }
    Vb6Type elemType = resolveArrayElemType(node.asType.get());
    std::string saElemType = mapSaElemType(elemType);
    // Bug4-Fix: UDT数组需使用vb6_SafeArrayReDim1D_Udt
    std::string udtCType = resolveArrayUdtElemCType(node.asType.get());
    bool isUdtArray = !udtCType.empty();

    // Fix 084a/090m: ReDim 目标是否为 Variant 数组 — 逻辑见成员函数 isVariantArrayTarget
    auto isVariantArrayVar = [&](const std::string& nm) -> bool { return isVariantArrayTarget(nm); };

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
            // Fix 084a: Variant 数组 ReDim Preserve — 实参提取 SafeArray, 结果包装回 Variant
            std::string callArg = cName;
            std::string assignVal;
            if (isVariantArrayVar(cName)) {
                callArg = "vb6_VariantToSafeArray1D(" + cName + ")";
                assignVal = "vb6_VariantFromValue(vb6_SafeArrayReDimPreserve1D(" + callArg + ", " + lBound + ", " + uBound + "))";
            } else {
                assignVal = "vb6_SafeArrayReDimPreserve1D(" + callArg + ", " + lBound + ", " + uBound + ")";
            }
            c_.emitLine(cName + " = " + assignVal + ";");
        } else {
            // Fix 084a: Variant 数组非 preserve ReDim — 先销毁提取出的 SafeArray, 再包装新数组回 Variant
            std::string destroyArg = cName;
            if (isVariantArrayVar(cName)) destroyArg = "vb6_VariantToSafeArray1D(" + cName + ")";
            c_.emitLine("vb6_SafeArrayDestroy1D(" + destroyArg + ");");
            std::string newVal;
            if (isUdtArray) {
                // Bug4-Fix: UDT数组使用_Udt版本，传入sizeof(UDT类型)
                newVal = "vb6_SafeArrayReDim1D_Udt((int32_t)sizeof(" + udtCType + "), " + lBound + ", " + uBound + ")";
            } else {
                newVal = "vb6_SafeArrayReDim1D(" + saElemType + ", " + lBound + ", " + uBound + ")";
            }
            if (isVariantArrayVar(cName)) newVal = "vb6_VariantFromValue(" + newVal + ")";
            c_.emitLine(cName + " = " + newVal + ";");
        }
    } else {
        // P8.1: 多维 ReDim
        // Use raw variable name for boundsVar (must be a valid C identifier)
        std::string rawCName = cIdent(node.varName);
        if (isClassModule_ && classMemberVars_.count(lowerVar) && !knownLocalVars_.count(lowerVar)) {
            rawCName = "me_" + rawCName;
        }
        std::string boundsVar = "_redim_bounds_" + rawCName;
        c_.emitLine("vb6_SafeArrayBound " + boundsVar + "[] = {");
        c_.indent();
        for (int d = 0; d < dimCount; d++) {
            auto& dim = node.dimensions[d];
            std::string lb = "0", ub = "0";
            if (dim.lower) { emitExpr(*dim.lower); lb = std::move(lastExpr_); }
            if (dim.upper) { emitExpr(*dim.upper); ub = std::move(lastExpr_); }
            std::string trailing = (d < dimCount - 1) ? "," : "";
            // vb6_SafeArrayBound = {lLbound, cElements}
            // cElements = uBound - lBound + 1 (VB6 "0 To 3" has 4 elements)
            c_.emitLine("{" + lb + ", (" + ub + " - " + lb + " + 1)}" + trailing);
        }
        c_.dedent();
        c_.emitLine("};");
        // Fix 084a: Variant 多维数组 ReDim — 实参提取 SafeArray, 结果包装回 Variant
        std::string redimArg = cName;
        std::string wrapBack = "";
        if (isVariantArrayVar(cName)) {
            redimArg = "vb6_VariantToSafeArray1D(" + cName + ")";
            wrapBack = "vb6_VariantFromValue(";
        }
        if (node.preserve) {
            std::string res = "vb6_SafeArrayReDimPreserveND((vb6_SafeArrayND*)" + redimArg + ", " + std::to_string(dimCount) + ", " + boundsVar + ")";
            if (!wrapBack.empty()) res = wrapBack + res + ")";
            c_.emitLine(cName + " = (vb6_SafeArray1D*)" + res + ";");
        } else {
            std::string destroyArg = redimArg;
            c_.emitLine("vb6_SafeArrayDestroyND((vb6_SafeArrayND*)" + destroyArg + ");");
            std::string newVal;
            if (isUdtArray) {
                // Bug4-Fix: UDT多维数组使用_Udt版本，传入sizeof(UDT类型)
                newVal = "vb6_SafeArrayReDimND_Udt((int32_t)sizeof(" + udtCType + "), " + std::to_string(dimCount) + ", " + boundsVar + ")";
            } else {
                newVal = "vb6_SafeArrayReDimND(" + saElemType + ", " + std::to_string(dimCount) + ", " + boundsVar + ")";
            }
            if (!wrapBack.empty()) newVal = wrapBack + newVal + ")";
            c_.emitLine(cName + " = (vb6_SafeArray1D*)" + newVal + ";");
        }

        // 鏇存柊鏁扮粍缁村害淇℃伅
        std::string lower = node.varName;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        arrayDimCounts_[lower] = dimCount;
    }
}

void CCodeGen::visit(EraseStmt& node) {
    for (auto& name : node.varNames) {
        // Fix 084y-5: Erase 目标含成员访问 (With 块成员 .Field, ByRef UDT 参数
        // 数组字段) 时按成员访问展开, 避免 cIdent 把 '.' 替换成 '_' 生成
        // 未声明的单标识符 (_RemoteLegacyNextTrafficKey / uOutput_Buffer → C2065)
        std::string cName = resolveArrayTargetIdent(name);
        // Fix 010r: Add me-> prefix for class member arrays
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (isClassModule_ && classMemberVars_.count(lower) && !knownLocalVars_.count(lower)) {
            cName = "me->" + cName;
        }
        // Fix 090p: UDT 的 As Variant 数组字段 (如 (*uFile).BufferArray) Erase —
        // 裸 vb6_SafeArrayDestroy1D((*uFile).BufferArray) 把 VARIANT 当 SafeArray* →
        // C2440; 用 vb6_VariantClear 释放数组并置 VT_EMPTY (cZipArchive pvVfsSetEof)
        if (isVariantArrayTarget(name)) {
            c_.emitLine("vb6_VariantClear(&" + cName + ");");
            continue;
        }
        // P8.1: 根据维度数选择1D/ND销毁
        auto it = arrayDimCounts_.find(lower);
        if (it != arrayDimCounts_.end() && it->second > 1) {
            c_.emitLine("vb6_SafeArrayDestroyND((vb6_SafeArrayND*)" + cName + "); " + cName + " = NULL;");
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
    // M22-Issue6: BSTR expression detection shared by both Print modes
    static const std::vector<std::string> bstrFuncs = {
        "vb6_BSTR_FromStr", "vb6_Left", "vb6_Right", "vb6_Mid",
        "vb6_UCase", "vb6_LCase", "vb6_UCase_str", "vb6_LCase_str",
        "vb6_Trim", "vb6_LTrim", "vb6_RTrim", "vb6_Chr",
        "vb6_Str", "vb6_CStr", "vb6_Format", "vb6_Hex", "vb6_Oct",
        "vb6_Replace", "vb6_Space", "vb6_String", "vb6_StrReverse",
        "vb6_BSTR_Concat", "vb6_BSTR_Empty", "vb6_App_Path", "vb6_App_EXEName", "vb6_App_HelpFile", "vb6_Command", "vb6_CurDir", "vb6_Environ", "vb6_Dir", "vb6_IIfBSTR", "vb6_GetControlText", "vb6_GetControlCaption"
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

    if (node.isFormPrint) {
        // M22-Issue6: Form surface Print - "Print expr" in form module
        // Generate: vb6_Form_Print(formHwnd, bstrExpr) for each output item
        std::string formHwnd = isFormModule_ ? ("vb6_hwnd_" + cIdent(formName_)) : "NULL";
        for (auto& expr : node.outputList) {
            emitExpr(*expr);
            std::string val = lastExpr_;
            if (isBstrExpr(val)) {
                c_.emitLine("vb6_Form_Print(" + formHwnd + ", " + val + ");");
            } else {
                // Use wrapToBSTR for proper type conversion (Date→CStrDate, etc.)
                std::string bstrVal = wrapToBSTR(val, *expr);
                c_.emitLine("vb6_Form_Print(" + formHwnd + ", " + bstrVal + ");");
            }
        }
        return;
    }

    // File I/O Print: Print #fnum, expr1; expr2; ...
    emitExpr(*node.fileNumber);
    std::string fnum = std::move(lastExpr_);

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
        "vb6_BSTR_Concat", "vb6_BSTR_Empty", "vb6_App_Path", "vb6_App_EXEName", "vb6_App_HelpFile", "vb6_Command", "vb6_CurDir", "vb6_Environ", "vb6_Dir", "vb6_IIfBSTR", "vb6_GetControlText", "vb6_GetControlCaption"
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
    c_.emitLine("vb6_SeekStmt(" + fnum + ", " + lastExpr_ + ");");
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
    emitExpr(*node.fileNumber);
    std::string fnum = std::move(lastExpr_);
    emitExpr(*node.width);
    std::string w = std::move(lastExpr_);
    c_.emitLine("vb6_Width((int32_t)(" + fnum + "), (int32_t)(" + w + "));");
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
    // Fix 086: For循环方向拆分会发射两份循环体, 第二份的标签加 _d2 后缀,
    // 与对应的 goto 配对, 避免同一函数内标签重定义 (C2045).
    std::string lblSuffix = (labelCopyIdx_ > 0) ? ("_d" + std::to_string(labelCopyIdx_ + 1)) : "";
    c_.emitLine("vb6_label_" + cIdent(node.labelName) + lblSuffix + ":;");
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
    // Fix 086: 已在过程序言处提升声明的节点, 原位置跳过
    if (hoistedLocalDeclSet_.count(&node)) return;
    emitLocalDeclCode(node);
}

void CCodeGen::emitLocalDeclCode(LocalDeclStmt& node) {
    if (!node.decl) return;

    switch (node.decl->kind) {
        case ASTNodeKind::VariableDecl: {
            auto& var = static_cast<VariableDecl&>(*node.decl);
            std::string cName = cIdent(var.name);

            // P8.1: 局部数组声明 (支持多维)
            if (!var.dimensions.empty()) {
                Vb6Type elemType = resolveArrayElemType(var.asType.get());
                std::string saElemType = mapSaElemType(elemType);
                // Bug4-Fix: UDT数组
                std::string udtCType = resolveArrayUdtElemCType(var.asType.get());
                bool isUdtArr = !udtCType.empty();
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
                    std::string initCode;
                    if (isUdtArr) {
                        initCode = "vb6_SafeArrayReDim1D_Udt((int32_t)sizeof(" + udtCType + "), " + lBound + ", " + uBound + ")";
                    } else {
                        initCode = "vb6_SafeArrayCreate1D(" + saElemType + ", " + lBound + ", " + uBound + ")";
                    }
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
                        // vb6_SafeArrayBound = {lLbound, cElements}
                        // cElements = uBound - lBound + 1 (VB6 "0 To 3" has 4 elements)
                        c_.emitLine("{" + lb + ", (" + ub + " - " + lb + " + 1)}" + trailing);
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
                if (elemType == Vb6Type::Byte) knownByteArrayVars_.insert(lower);
                // Fix 055: 注册UDT数组元素C类型
                {
                    std::string udtCType = resolveArrayUdtElemCType(var.asType.get());
                    if (!udtCType.empty()) arrayUdtElemTypes_[lower] = udtCType;
                }
                knownLocalVars_.insert(lower);
                break;
            }

            // P8.1: 动态数组声明: Dim arr() As Long → 默认1D, ReDim时可能升级
            if (var.isDynamicArray) {
                Vb6Type elemType = resolveArrayElemType(var.asType.get());
                // Fix 084aa: #undef 防宏污染 — 模块常量被生成 #define 宏 (如 cStartUp 的
                // #define K (vb6_BSTR_FromStr(...))), 同名局部变量声明会被宏展开破坏.
                // 局部变量总是遮蔽模块常量, #undef 是安全且正确的.
                c_.emitLine("#undef " + cName);
                c_.emitLine("vb6_SafeArray1D* " + cName + " = NULL;");

                // 注册到已知数组集合
                std::string lower = var.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                knownArrays_.insert(lower);
                arrayElemTypes_[lower] = elemType;
                arrayDimCounts_[lower] = 1;  // 动态数组默认1D
                if (elemType == Vb6Type::Byte) knownByteArrayVars_.insert(lower);
                // Fix 055: 注册UDT数组元素C类型
                {
                    std::string udtCType = resolveArrayUdtElemCType(var.asType.get());
                    if (!udtCType.empty()) arrayUdtElemTypes_[lower] = udtCType;
                }
                knownLocalVars_.insert(lower);
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
            } else if (cType == "intptr_t") {
                // Bug #2 fix: LongPtr变量注册到独立集合
                std::string lower = var.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                knownLongPtrVars_.insert(lower);
            } else if (cType.find("vb6_ComIface_") == 0 || cType.find("vb6_ComIface_") != std::string::npos) {
                // Fix 082: COM interface pointer variables are also pointer-sized on x64
                std::string lower = var.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                knownLongPtrVars_.insert(lower);
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
                auto* comSym = lookupModuleDotted(simple.name);
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
            bool isLocalEnumType = false;  // Fix 010q
            bool isLocalComIfaceType = false;
            bool isLocalVb6IfaceType = false;  // P6.4: VB6接口引用
            if (var.asType && var.asType->kind == ASTNodeKind::SimpleTypeRef) {
                auto& simple = static_cast<SimpleTypeRef&>(*var.asType);
                auto* clsSym = lookupModuleDotted(simple.name);
                if (clsSym && clsSym->kind == SymbolKind::Class) {
                    std::string lower = var.name;
                    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                    // P6.4: 接口类 → knownIfaceVars_ (而非 knownClassVars_)
                    if (clsSym->isInterface) {
                        knownIfaceVars_[lower] = clsSym->name;
                        isLocalVb6IfaceType = true;
                    } else {
                        // Fix 010r-10: map赋值, 存储类名以便方法分发时查找
                        knownClassVars_[lower] = clsSym->name;
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
                auto* udtSym = lookupDotted(simple.name);
                if (udtSym && udtSym->kind == SymbolKind::UserDefinedType) {
                    isLocalUdtType = true;
                    // M22-fix: 注册到knownUdtVars_，防止成员访问被误判为模块名限定
                    std::string udtLower = var.name;
                    std::transform(udtLower.begin(), udtLower.end(), udtLower.begin(), ::tolower);
                    knownUdtVars_[udtLower] = "vb6_type_" + cIdent(simple.name);
                }
                // Fix 010q: 检查是否是Enum类型 (mapTypeRef映射为int32_t, 但defaultValue返回vb6_VariantEmpty())
                if (udtSym && udtSym->kind == SymbolKind::EnumType) {
                    isLocalEnumType = true;
                }
            }

            // VB6 Static变量: 跨调用持久化 → C static局部变量
            // 包括: 显式Static声明 或 Static Sub/Function内的所有局部变量
            std::string storageClass = (var.isStatic || inStaticProc_) ? "static " : "";

            // Fix 010r-12c: Register non-array local variable to knownLocalVars_
            // so it shadows cross-module external Public symbols with the same name.
            // (Array/dynamic-array cases already insert above; this covers all other types)
            {
                std::string lower = var.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                knownLocalVars_.insert(lower);
            }

            if (var.initializer) {
                // Fix 084aa: #undef 防宏污染 (见 P8.1 动态数组处注释)
                c_.emitLine("#undef " + cName);
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
                } else if (isLocalEnumType) {  // Fix 010q
                    initVal = "0";
                } else if (var.asType && var.asType->kind == ASTNodeKind::FixedStringTypeRef) {
                    // String * N: 初始化为N个空格的BSTR, LSet/RSet使用固定长度
                    auto& fs = static_cast<FixedStringTypeRef&>(*var.asType);
                    emitExpr(*fs.length);
                    std::string fsLen = lastExpr_;
                    initVal = "vb6_BSTR_FixedSTR(" + fsLen + ")";
                    // 注册定长字符串变量名→长度
                    std::string fsLower = var.name;
                    std::transform(fsLower.begin(), fsLower.end(), fsLower.begin(), ::tolower);
                    knownFixedStringLen_[fsLower] = fsLen;
                } else {
                    initVal = defaultValue(
                        var.asType && var.asType->kind == ASTNodeKind::SimpleTypeRef
                            ? typeSys_.resolveTypeName(static_cast<SimpleTypeRef*>(var.asType.get())->name)
                            : Vb6Type::Variant
                    );
                }
                // Fix 084aa: 静态局部变量初始化必须是编译期常量 (C2099).
                // vb6_VariantEmpty()/vb6_BSTR_Empty() 是函数调用, 静态初始化会报错.
                // {0} (vt=0=VT_EMPTY) 与 vb6_VariantEmpty() 语义一致; NULL 即空BSTR.
                if (!storageClass.empty()) {
                    if (initVal == "vb6_VariantEmpty()") initVal = "{0}";
                    if (initVal == "vb6_BSTR_Empty()") initVal = "NULL";
                }
                // Fix 084aa: #undef 防宏污染 (见 P8.1 动态数组处注释)
                c_.emitLine("#undef " + cName);
                c_.emitLine(storageClass + cType + " " + cName + " = " + initVal + ";");
            }
            break;
        }
        case ASTNodeKind::ConstDecl: {
            auto& con = static_cast<ConstDecl&>(*node.decl);
            std::string cType = mapTypeRef(con.asType.get());
            // Fix 091d: 无 As 类型常量按字面量推断 C 类型. 此前一律 vb6_VARIANT →
            // `const vb6_VARIANT SW_SHOWNORMAL = 1;` 非法初始化 → C2440
            // (cToolsSystem.c 11/13); 且 Variant 常量参与位运算时操作数被包装
            // vb6_VariantToLong(<字面量>) → C2440 (cDialog.c 36 BIF_USENEWUI).
            if (!con.asType && con.value
                && con.value->kind == ASTNodeKind::LiteralExpr) {
                auto& lit091d = static_cast<LiteralExpr&>(*con.value);
                switch (lit091d.literalKind) {
                    case LiteralKind::Integer:
                    case LiteralKind::Long:
                        cType = "int32_t";
                        break;
                    case LiteralKind::Single:
                    case LiteralKind::Double:
                        cType = "double";
                        break;
                    case LiteralKind::String:
                        cType = "BSTR";
                        break;
                    case LiteralKind::Boolean:
                        cType = "VBABOOL";
                        break;
                    default:
                        break;
                }
            }
            std::string cName = cIdent(con.name);
            // Fix 010r-12c: Register local constant to knownLocalVars_
            {
                std::string lower = con.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                knownLocalVars_.insert(lower);
            }
            // Fix 049: Register local constant to type-specific known*Vars_ sets.
            // Same logic as Dim (cgen_decl.cpp:749-767). Without this, inferExprType
            // falls back to Variant for unknown identifiers, causing wrapToBSTR to
            // generate vb6_CStr(BSTR_const) which triggers C2440 (BSTR→VARIANT).
            {
                std::string lower = con.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if (cType == "BSTR") {
                    knownBstrVars_.insert(lower);
            } else if (cType == "int32_t" || cType == "int16_t" || cType == "VBABOOL") {
                    knownLongVars_.insert(lower);
                } else if (cType == "intptr_t") {
                    // Bug #2 fix: LongPtr局部const变量注册到独立集合
                    knownLongPtrVars_.insert(lower);
                } else if (cType.find("vb6_ComIface_") != std::string::npos) {
                    // Fix 082: COM interface pointer types are pointer-sized on x64
                    knownLongPtrVars_.insert(lower);
                } else if (cType == "double" || cType == "float") {
                    knownDoubleVars_.insert(lower);
                } else if (cType == "vb6_VARIANT") {
                    knownVariantVars_.insert(lower);
                }
            }
            if (con.value) {
                emitExpr(*con.value);
                // Fix 010r-13: Local Const redefining Windows API macro? #undef first.
                // 必须在声明 const 变量之前 #undef, 防止名字被 <windows.h> 等头文件中的
                // 宏展开 (例: WHITE_BRUSH、MEM_COMMIT、CP_UTF8、SW_SHOWNORMAL 等
                // 都是 windows.h 中的 #define, 否则 `const int32_t WHITE_BRUSH = 0;`
                // 会被宏展开为 `const int32_t 0 = 0;` 引发 C2106).
                // #undef 对没有定义为宏的名字是空操作, 无副作用.
                c_.emitLine("#undef " + cName);
                c_.emitLine("const " + cType + " " + cName + " = " + lastExpr_ + ";");
            }
            break;
        }
        default:
            c_.emitLine("/* unhandled LocalDeclStmt: " + std::string(node.decl->kindName()) + " */");
            break;
    }
}

// ---- Fix 086: 局部声明过程级作用域提升 ----

void CCodeGen::collectLocalDeclStmts(StmtList& stmts, std::vector<LocalDeclStmt*>& out) {
    for (auto& stmt : stmts) {
        if (!stmt) continue;
        switch (stmt->kind) {
            case ASTNodeKind::LocalDeclStmt:
                out.push_back(static_cast<LocalDeclStmt*>(stmt.get()));
                break;
            case ASTNodeKind::Block:
                collectLocalDeclStmts(static_cast<Block&>(*stmt).stmts, out);
                break;
            case ASTNodeKind::IfStmt: {
                auto& n = static_cast<IfStmt&>(*stmt);
                collectLocalDeclStmts(n.thenBody, out);
                for (auto& ei : n.elseIfs)
                    if (ei) collectLocalDeclStmts(ei->body, out);
                collectLocalDeclStmts(n.elseBody, out);
                break;
            }
            case ASTNodeKind::ForStmt:
                collectLocalDeclStmts(static_cast<ForStmt&>(*stmt).body, out);
                break;
            case ASTNodeKind::ForEachStmt:
                collectLocalDeclStmts(static_cast<ForEachStmt&>(*stmt).body, out);
                break;
            case ASTNodeKind::DoLoopStmt:
                collectLocalDeclStmts(static_cast<DoLoopStmt&>(*stmt).body, out);
                break;
            case ASTNodeKind::WhileWendStmt:
                collectLocalDeclStmts(static_cast<WhileWendStmt&>(*stmt).body, out);
                break;
            case ASTNodeKind::SelectCaseStmt: {
                auto& n = static_cast<SelectCaseStmt&>(*stmt);
                for (auto& cc : n.cases)
                    if (cc) collectLocalDeclStmts(cc->body, out);
                collectLocalDeclStmts(n.elseCase, out);
                break;
            }
            case ASTNodeKind::WithStmt:
                collectLocalDeclStmts(static_cast<WithStmt&>(*stmt).body, out);
                break;
            default:
                break;
        }
    }
}

// Fix 090o: 递归收集语句序列内定义的标签名 (VB6 过程内标签唯一; 用于判定
// GoTo 目标是否在 For 方向拆分的 body 内, 决定第二份副本 goto 是否加 _dN 后缀)
void CCodeGen::collectForBodyLabels(const StmtList& stmts, std::unordered_set<std::string>& out) {
    for (auto& stmt : stmts) {
        if (!stmt) continue;
        switch (stmt->kind) {
            case ASTNodeKind::LabelStmt:
                out.insert(Symbol::toLower(static_cast<LabelStmt&>(*stmt).labelName));
                break;
            case ASTNodeKind::Block:
                collectForBodyLabels(static_cast<Block&>(*stmt).stmts, out);
                break;
            case ASTNodeKind::IfStmt: {
                auto& n = static_cast<IfStmt&>(*stmt);
                collectForBodyLabels(n.thenBody, out);
                for (auto& ei : n.elseIfs)
                    if (ei) collectForBodyLabels(ei->body, out);
                collectForBodyLabels(n.elseBody, out);
                break;
            }
            case ASTNodeKind::ForStmt:
                collectForBodyLabels(static_cast<ForStmt&>(*stmt).body, out);
                break;
            case ASTNodeKind::ForEachStmt:
                collectForBodyLabels(static_cast<ForEachStmt&>(*stmt).body, out);
                break;
            case ASTNodeKind::DoLoopStmt:
                collectForBodyLabels(static_cast<DoLoopStmt&>(*stmt).body, out);
                break;
            case ASTNodeKind::WhileWendStmt:
                collectForBodyLabels(static_cast<WhileWendStmt&>(*stmt).body, out);
                break;
            case ASTNodeKind::SelectCaseStmt: {
                auto& n = static_cast<SelectCaseStmt&>(*stmt);
                for (auto& cc : n.cases)
                    if (cc) collectForBodyLabels(cc->body, out);
                collectForBodyLabels(n.elseCase, out);
                break;
            }
            case ASTNodeKind::WithStmt:
                collectForBodyLabels(static_cast<WithStmt&>(*stmt).body, out);
                break;
            default:
                break;
        }
    }
}

// Fix 090m/090p: ReDim/Erase 目标是 Variant 数组判定 — 顶层 As Variant 变量
// 或 UDT 的 As Variant 字段 ((*uFile).BufferArray / vb6_ret_X.BufferArray)。
// 090m 修复 ReDim Preserve; 090p 使 Erase 复用同一判定 (cZipArchive pvVfsSetEof:
// Erase uFile.BufferArray 生成了裸 SafeArrayDestroy1D((*uFile).BufferArray) → C2440)
bool CCodeGen::isVariantArrayTarget(const std::string& name) {
    std::string checkName = name;
    if (checkName.substr(0, 4) == "me->") checkName = checkName.substr(4);
    if (checkName.size() > 4 && checkName[0] == '(' && checkName[1] == '*'
        && checkName.back() == ')') {
        checkName = checkName.substr(2, checkName.size() - 3);
    }
    std::string lower = checkName;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (knownVariantVars_.count(lower) > 0) return true;
    // Fix 090m: UDT 字段目标 — (*uFile).BufferArray / vb6_ret_X.BufferArray:
    // 对象是 UDT (ByRef 参数/局部/返回变量), 字段声明 As Variant 时按
    // Variant 数组处理 (cZipArchive pvVfsWrite: ReDim Preserve
    // uFile.BufferArray(...) As Byte → C2440 直接把 VARIANT 当 SafeArray*).
    size_t dotP090m = name.rfind('.');
    size_t arrowP090m = name.rfind("->");
    size_t sepP090m = (dotP090m == std::string::npos) ? arrowP090m
                    : (arrowP090m == std::string::npos) ? dotP090m : std::max(dotP090m, arrowP090m);
    if (sepP090m != std::string::npos && sepP090m + 1 < name.size()) {
        std::string objTxt090m = name.substr(0, sepP090m);
        std::string fieldTxt090m = name.substr(sepP090m + ((sepP090m >= 1 && name[sepP090m - 1] == '-') ? 2 : 1));
        if (objTxt090m.size() > 2 && objTxt090m.rfind("(*", 0) == 0
            && objTxt090m.back() == ')') {
            objTxt090m = objTxt090m.substr(2, objTxt090m.size() - 3);
        }
        std::string objLower090m = objTxt090m;
        std::transform(objLower090m.begin(), objLower090m.end(), objLower090m.begin(), ::tolower);
        auto itUdt090m = knownUdtVars_.find(objLower090m);
        if (itUdt090m != knownUdtVars_.end() && itUdt090m->second.rfind("vb6_type_", 0) == 0) {
            std::string udtName090m = itUdt090m->second.substr(8);
            // Fix 090m: knownUdtVars_ 值 = "vb6_type_" + cIdent(UDT名), cIdent 给
            // 私有 UDT 名加前导 '_' (vb6_type__ZipVfsType) → lookupModule 前需去 _
            if (udtName090m.size() > 1 && udtName090m[0] == '_') udtName090m = udtName090m.substr(1);
            Symbol* udtSym090m = symTab_.lookupModule(udtName090m);
            if (udtSym090m && udtSym090m->kind == SymbolKind::UserDefinedType) {
                std::string fieldLower090m = fieldTxt090m;
                std::transform(fieldLower090m.begin(), fieldLower090m.end(), fieldLower090m.begin(), ::tolower);
                for (auto& mi090m : udtSym090m->udtMembers) {
                    std::string miLower090m = mi090m.name;
                    std::transform(miLower090m.begin(), miLower090m.end(), miLower090m.begin(), ::tolower);
                    if (miLower090m == fieldLower090m) {
                        return mi090m.type == Vb6Type::Variant;
                    }
                }
            }
        }
    }
    return false;
}

void CCodeGen::hoistLocalDecls(StmtList& body) {
    if (const char* dis = std::getenv("C3_NO_HOIST")) {
        (void)dis;
        hoistedLocalDeclSet_.clear();
        return;
    }
    hoistedLocalDeclSet_.clear();
    std::vector<LocalDeclStmt*> decls;
    collectLocalDeclStmts(body, decls);
    for (auto* d : decls) {
        if (!d || !d->decl) continue;
        if (hoistedLocalDeclSet_.count(d)) continue;
        bool hoistable = false;
        if (d->decl->kind == ASTNodeKind::VariableDecl) {
            auto& var = static_cast<VariableDecl&>(*d->decl);
            // 固定边界数组保留原位 (边界表达式可能依赖执行到该点时的状态)
            hoistable = var.dimensions.empty();
        } else if (d->decl->kind == ASTNodeKind::ConstDecl) {
            hoistable = true;
        }
        if (hoistable) {
            hoistedLocalDeclSet_.insert(d);
            emitLocalDeclCode(*d);
        }
    }
}

} // namespace vb6c3
