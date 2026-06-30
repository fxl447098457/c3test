#include "ast/ast_visitor.hpp"

namespace vb6c3 {

// 递归遍历表达式 - 先visit当前节点, 再递归子节点
void traverseExpr(Expr& expr, ASTVisitor& visitor) {
    switch (expr.kind) {
    case ASTNodeKind::BinaryExpr: {
        auto& e = static_cast<BinaryExpr&>(expr);
        visitor.visit(e);
        traverseExpr(*e.left, visitor);
        traverseExpr(*e.right, visitor);
        break;
    }
    case ASTNodeKind::UnaryExpr: {
        auto& e = static_cast<UnaryExpr&>(expr);
        visitor.visit(e);
        traverseExpr(*e.operand, visitor);
        break;
    }
    case ASTNodeKind::LiteralExpr:
        visitor.visit(static_cast<LiteralExpr&>(expr));
        break;
    case ASTNodeKind::IdentifierExpr:
        visitor.visit(static_cast<IdentifierExpr&>(expr));
        break;
    case ASTNodeKind::MemberAccessExpr: {
        auto& e = static_cast<MemberAccessExpr&>(expr);
        visitor.visit(e);
        traverseExpr(*e.object, visitor);
        break;
    }
    case ASTNodeKind::DictionaryAccessExpr: {
        auto& e = static_cast<DictionaryAccessExpr&>(expr);
        visitor.visit(e);
        traverseExpr(*e.object, visitor);
        break;
    }
    case ASTNodeKind::IndexOrCallExpr: {
        auto& e = static_cast<IndexOrCallExpr&>(expr);
        visitor.visit(e);
        traverseExpr(*e.callee, visitor);
        for (auto& arg : e.positional) traverseExpr(*arg, visitor);
        for (auto& na : e.named) traverseExpr(*na.value, visitor);
        break;
    }
    case ASTNodeKind::NewExpr:
        visitor.visit(static_cast<NewExpr&>(expr));
        break;
    case ASTNodeKind::TypeOfExpr: {
        auto& e = static_cast<TypeOfExpr&>(expr);
        visitor.visit(e);
        traverseExpr(*e.object, visitor);
        break;
    }
    case ASTNodeKind::AddressOfExpr:
        visitor.visit(static_cast<AddressOfExpr&>(expr));
        break;
    case ASTNodeKind::MeExpr:
        visitor.visit(static_cast<MeExpr&>(expr));
        break;
    case ASTNodeKind::WithMemberExpr:
        visitor.visit(static_cast<WithMemberExpr&>(expr));
        break;
    default:
        break;
    }
}

// 递归遍历语句块
static void traverseStmtList(StmtList& stmts, ASTVisitor& visitor) {
    for (auto& s : stmts) traverseStmt(*s, visitor);
}

// 递归遍历参数列表
static void traverseParamList(std::vector<std::unique_ptr<ParameterDecl>>& params,
                               ASTVisitor& visitor) {
    for (auto& p : params) traverseDecl(*p, visitor);
}

// 递归遍历表达式列表
static void traverseExprList(std::vector<ExprPtr>& exprs, ASTVisitor& visitor) {
    for (auto& e : exprs) traverseExpr(*e, visitor);
}

// 递归遍历语句
void traverseStmt(Stmt& stmt, ASTVisitor& visitor) {
    switch (stmt.kind) {
    case ASTNodeKind::Block: {
        auto& s = static_cast<Block&>(stmt);
        visitor.visit(s);
        traverseStmtList(s.stmts, visitor);
        break;
    }
    case ASTNodeKind::AssignmentStmt: {
        auto& s = static_cast<AssignmentStmt&>(stmt);
        visitor.visit(s);
        traverseExpr(*s.target, visitor);
        traverseExpr(*s.value, visitor);
        break;
    }
    case ASTNodeKind::SetStmt: {
        auto& s = static_cast<SetStmt&>(stmt);
        visitor.visit(s);
        traverseExpr(*s.target, visitor);
        traverseExpr(*s.value, visitor);
        break;
    }
    case ASTNodeKind::LetStmt: {
        auto& s = static_cast<LetStmt&>(stmt);
        visitor.visit(s);
        traverseExpr(*s.target, visitor);
        traverseExpr(*s.value, visitor);
        break;
    }
    case ASTNodeKind::IfStmt: {
        auto& s = static_cast<IfStmt&>(stmt);
        visitor.visit(s);
        traverseExpr(*s.condition, visitor);
        traverseStmtList(s.thenBody, visitor);
        for (auto& elif : s.elseIfs) {
            traverseExpr(*elif->condition, visitor);
            traverseStmtList(elif->body, visitor);
        }
        traverseStmtList(s.elseBody, visitor);
        break;
    }
    case ASTNodeKind::ForStmt: {
        auto& s = static_cast<ForStmt&>(stmt);
        visitor.visit(s);
        traverseExpr(*s.start, visitor);
        traverseExpr(*s.end, visitor);
        if (s.step) traverseExpr(*s.step, visitor);
        traverseStmtList(s.body, visitor);
        break;
    }
    case ASTNodeKind::ForEachStmt: {
        auto& s = static_cast<ForEachStmt&>(stmt);
        visitor.visit(s);
        traverseExpr(*s.collection, visitor);
        traverseStmtList(s.body, visitor);
        break;
    }
    case ASTNodeKind::DoLoopStmt: {
        auto& s = static_cast<DoLoopStmt&>(stmt);
        visitor.visit(s);
        if (s.condition) traverseExpr(*s.condition, visitor);
        traverseStmtList(s.body, visitor);
        break;
    }
    case ASTNodeKind::WhileWendStmt: {
        auto& s = static_cast<WhileWendStmt&>(stmt);
        visitor.visit(s);
        traverseExpr(*s.condition, visitor);
        traverseStmtList(s.body, visitor);
        break;
    }
    case ASTNodeKind::SelectCaseStmt: {
        auto& s = static_cast<SelectCaseStmt&>(stmt);
        visitor.visit(s);
        traverseExpr(*s.testExpr, visitor);
        for (auto& c : s.cases) traverseStmt(*c, visitor);
        traverseStmtList(s.elseCase, visitor);
        break;
    }
    case ASTNodeKind::CaseClause: {
        auto& s = static_cast<CaseClause&>(stmt);
        visitor.visit(s);
        for (auto& cv : s.values) {
            if (cv.value) traverseExpr(*cv.value, visitor);
            if (cv.toValue) traverseExpr(*cv.toValue, visitor);
        }
        traverseStmtList(s.body, visitor);
        break;
    }
    case ASTNodeKind::WithStmt: {
        auto& s = static_cast<WithStmt&>(stmt);
        visitor.visit(s);
        traverseExpr(*s.object, visitor);
        traverseStmtList(s.body, visitor);
        break;
    }
    case ASTNodeKind::OnErrorStmt:
        visitor.visit(static_cast<OnErrorStmt&>(stmt));
        break;
    case ASTNodeKind::ExitStmt:
        visitor.visit(static_cast<ExitStmt&>(stmt));
        break;
    case ASTNodeKind::CallStmt: {
        auto& s = static_cast<CallStmt&>(stmt);
        visitor.visit(s);
        traverseExpr(*s.callee, visitor);
        break;
    }
    case ASTNodeKind::ReDimStmt: {
        auto& s = static_cast<ReDimStmt&>(stmt);
        visitor.visit(s);
        for (auto& d : s.dimensions) {
            if (d.lower) traverseExpr(*d.lower, visitor);
            if (d.upper) traverseExpr(*d.upper, visitor);
        }
        break;
    }
    case ASTNodeKind::RaiseEventStmt: {
        auto& s = static_cast<RaiseEventStmt&>(stmt);
        visitor.visit(s);
        traverseExprList(s.args, visitor);
        break;
    }
    // 简单语句 (无子节点)
    case ASTNodeKind::GoToStmt:
    case ASTNodeKind::GoSubStmt:
    case ASTNodeKind::ReturnStmt:
    case ASTNodeKind::OnGoToStmt:
    case ASTNodeKind::OnGoSubStmt:
    case ASTNodeKind::MidStmt:  // P18-A
    case ASTNodeKind::StopStmt:
    case ASTNodeKind::EndStmt:
    case ASTNodeKind::EraseStmt:
    case ASTNodeKind::LabelStmt:
    case ASTNodeKind::BeepStmt:
    case ASTNodeKind::DoEventsStmt:
    case ASTNodeKind::OptionStmt:
    case ASTNodeKind::ImplementsStmt:
    case ASTNodeKind::DefTypeStmt:
        // 通用dispatch
        switch (stmt.kind) {
        case ASTNodeKind::GoToStmt:     visitor.visit(static_cast<GoToStmt&>(stmt)); break;
        case ASTNodeKind::GoSubStmt:    visitor.visit(static_cast<GoSubStmt&>(stmt)); break;
        case ASTNodeKind::ReturnStmt:   visitor.visit(static_cast<ReturnStmt&>(stmt)); break;
        case ASTNodeKind::OnGoToStmt:   visitor.visit(static_cast<OnGoToStmt&>(stmt)); break;
        case ASTNodeKind::OnGoSubStmt:  visitor.visit(static_cast<OnGoSubStmt&>(stmt)); break;
        case ASTNodeKind::MidStmt:       visitor.visit(static_cast<MidStmt&>(stmt)); break;  // P18-A
        case ASTNodeKind::StopStmt:     visitor.visit(static_cast<StopStmt&>(stmt)); break;
        case ASTNodeKind::EndStmt:      visitor.visit(static_cast<EndStmt&>(stmt)); break;
        case ASTNodeKind::EraseStmt:    visitor.visit(static_cast<EraseStmt&>(stmt)); break;
        case ASTNodeKind::LabelStmt:    visitor.visit(static_cast<LabelStmt&>(stmt)); break;
        case ASTNodeKind::BeepStmt:     visitor.visit(static_cast<BeepStmt&>(stmt)); break;
        case ASTNodeKind::DoEventsStmt: visitor.visit(static_cast<DoEventsStmt&>(stmt)); break;
        case ASTNodeKind::OptionStmt:   visitor.visit(static_cast<OptionStmt&>(stmt)); break;
        case ASTNodeKind::ImplementsStmt: visitor.visit(static_cast<ImplementsStmt&>(stmt)); break;
        case ASTNodeKind::DefTypeStmt:  visitor.visit(static_cast<DefTypeStmt&>(stmt)); break;
        default: break;
        }
        break;
    // 有表达式的文件I/O语句 - 简化处理
    default:
        // 其他语句暂时只调用visitor, 不递归子表达式
        break;
    }
}

// 递归遍历声明
void traverseDecl(Decl& decl, ASTVisitor& visitor) {
    switch (decl.kind) {
    case ASTNodeKind::SubDecl: {
        auto& d = static_cast<SubDecl&>(decl);
        visitor.visit(d);
        traverseParamList(d.params, visitor);
        traverseStmtList(d.body, visitor);
        break;
    }
    case ASTNodeKind::FunctionDecl: {
        auto& d = static_cast<FunctionDecl&>(decl);
        visitor.visit(d);
        traverseParamList(d.params, visitor);
        traverseStmtList(d.body, visitor);
        break;
    }
    case ASTNodeKind::PropertyDecl: {
        auto& d = static_cast<PropertyDecl&>(decl);
        visitor.visit(d);
        traverseParamList(d.params, visitor);
        traverseStmtList(d.body, visitor);
        break;
    }
    case ASTNodeKind::TypeDecl: {
        auto& d = static_cast<TypeDecl&>(decl);
        visitor.visit(d);
        for (auto& m : d.members) traverseDecl(*m, visitor);
        break;
    }
    case ASTNodeKind::EnumDecl: {
        auto& d = static_cast<EnumDecl&>(decl);
        visitor.visit(d);
        for (auto& m : d.members) traverseDecl(*m, visitor);
        break;
    }
    case ASTNodeKind::ConstDecl: {
        auto& d = static_cast<ConstDecl&>(decl);
        visitor.visit(d);
        if (d.value) traverseExpr(*d.value, visitor);
        break;
    }
    case ASTNodeKind::VariableDecl: {
        auto& d = static_cast<VariableDecl&>(decl);
        visitor.visit(d);
        if (d.initializer) traverseExpr(*d.initializer, visitor);
        break;
    }
    case ASTNodeKind::ParameterDecl:
        visitor.visit(static_cast<ParameterDecl&>(decl));
        break;
    case ASTNodeKind::TypeMember:
        visitor.visit(static_cast<TypeMember&>(decl));
        break;
    case ASTNodeKind::EnumMember: {
        auto& d = static_cast<EnumMember&>(decl);
        visitor.visit(d);
        if (d.value) traverseExpr(*d.value, visitor);
        break;
    }
    default:
        break;
    }
}

// 根节点遍历入口
void traverseAST(ASTNode& node, ASTVisitor& visitor) {
    switch (node.kind) {
    case ASTNodeKind::Module: {
        auto& m = static_cast<Module&>(node);
        visitor.visit(m);
        for (auto& opt : m.options) traverseStmt(*opt, visitor);
        for (auto& imp : m.implements) traverseStmt(*imp, visitor);
        for (auto& def : m.defTypes) traverseStmt(*def, visitor);
        for (auto& decl : m.declarations) traverseDecl(*decl, visitor);
        break;
    }
    default:
        if (isExpr(node.kind)) traverseExpr(static_cast<Expr&>(node), visitor);
        else if (isStmt(node.kind)) traverseStmt(static_cast<Stmt&>(node), visitor);
        else if (isDecl(node.kind)) traverseDecl(static_cast<Decl&>(node), visitor);
        break;
    }
}

} // namespace vb6c3
