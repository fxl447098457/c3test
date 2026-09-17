#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>

namespace vb6c3 {

// --- cgen_localdecl_hoist.cpp: 局部声明过程级作用域提升与收集 ---
// 由 src/backend/decl/cgen_localdecl.cpp 拆出（2026-09-17），纯搬移、零行为改动：
//   collectLocalDeclStmts / collectForBodyLabels / isVariantArrayTarget / hoistLocalDecls


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
