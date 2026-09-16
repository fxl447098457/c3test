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
                // 项目类实例 (Dim b As Button / Dim WithEvents b As Button) 的 C 类型同样
                // 是 void*, 但它不是 COM 后期绑定对象, 方法调用必须走直接分发
                // (vb6_Button_DoClick(...)). 若误注册进 knownObjectVars_, cgen_expr 的
                // "优先级1" 会把它当 IDispatch 处理, 生成 vb6_ComCall(b, L"DoClick", ...)
                // → 对纯 C 结构体解引用 vtable → 运行期 0xC0000005.
                // 注意: 本处先于下方 knownClassVars_ 注册, 故直接查类符号判断.
                bool isProjectClassVar = false;
                if (var.asType && var.asType->kind == ASTNodeKind::SimpleTypeRef) {
                    auto& st = static_cast<SimpleTypeRef&>(*var.asType);
                    auto* clsSym = lookupModuleDotted(st.name);
                    if (clsSym && clsSym->kind == SymbolKind::Class) isProjectClassVar = true;
                }
                if (!isProjectClassVar) {
                    knownObjectVars_.insert(lower);
                }
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
