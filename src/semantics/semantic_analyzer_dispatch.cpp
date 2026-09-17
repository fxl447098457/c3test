#include "semantics/semantic_analyzer.hpp"
#include <algorithm>
#include <cctype>
#include <tuple>
#include <initializer_list>
#include "semantics/semantic_analyzer_internal.h"

namespace vb6c3 {

// --- semantic_analyzer_dispatch.cpp: 手动分派辅助 (AST节点无accept, 按kind分派到visitor) ---


// ============================================================
// 手动分派辅助 (AST节点无accept, 按kind分派到visitor)
// ============================================================

// 声明节点分派
void dispatchDecl(Decl& decl, SemanticAnalyzer& analyzer) {
    switch (decl.kind) {
        case ASTNodeKind::SubDecl:       analyzer.visit(static_cast<SubDecl&>(decl)); break;
        case ASTNodeKind::FunctionDecl:  analyzer.visit(static_cast<FunctionDecl&>(decl)); break;
        case ASTNodeKind::PropertyDecl:  analyzer.visit(static_cast<PropertyDecl&>(decl)); break;
        case ASTNodeKind::TypeDecl:      analyzer.visit(static_cast<TypeDecl&>(decl)); break;
        case ASTNodeKind::TypeMember:    analyzer.visit(static_cast<TypeMember&>(decl)); break;
        case ASTNodeKind::EnumDecl:      analyzer.visit(static_cast<EnumDecl&>(decl)); break;
        case ASTNodeKind::EnumMember:    analyzer.visit(static_cast<EnumMember&>(decl)); break;
        case ASTNodeKind::DeclareDecl:   analyzer.visit(static_cast<DeclareDecl&>(decl)); break;
        case ASTNodeKind::EventDecl:     analyzer.visit(static_cast<EventDecl&>(decl)); break;
        case ASTNodeKind::ConstDecl:     analyzer.visit(static_cast<ConstDecl&>(decl)); break;
        case ASTNodeKind::VariableDecl:  analyzer.visit(static_cast<VariableDecl&>(decl)); break;
        case ASTNodeKind::ParameterDecl: analyzer.visit(static_cast<ParameterDecl&>(decl)); break;
        default: break;
    }
}

// 语句节点分派
void dispatchStmt(Stmt& stmt, SemanticAnalyzer& analyzer) {
    switch (stmt.kind) {
        case ASTNodeKind::Block:            analyzer.visit(static_cast<Block&>(stmt)); break;
        case ASTNodeKind::AssignmentStmt:   analyzer.visit(static_cast<AssignmentStmt&>(stmt)); break;
        case ASTNodeKind::SetStmt:          analyzer.visit(static_cast<SetStmt&>(stmt)); break;
        case ASTNodeKind::LetStmt:          analyzer.visit(static_cast<LetStmt&>(stmt)); break;
        case ASTNodeKind::IfStmt:           analyzer.visit(static_cast<IfStmt&>(stmt)); break;
        case ASTNodeKind::ElseIfClause:     analyzer.visit(static_cast<ElseIfClause&>(stmt)); break;
        case ASTNodeKind::ForStmt:          analyzer.visit(static_cast<ForStmt&>(stmt)); break;
        case ASTNodeKind::ForEachStmt:      analyzer.visit(static_cast<ForEachStmt&>(stmt)); break;
        case ASTNodeKind::DoLoopStmt:       analyzer.visit(static_cast<DoLoopStmt&>(stmt)); break;
        case ASTNodeKind::WhileWendStmt:    analyzer.visit(static_cast<WhileWendStmt&>(stmt)); break;
        case ASTNodeKind::SelectCaseStmt:   analyzer.visit(static_cast<SelectCaseStmt&>(stmt)); break;
        case ASTNodeKind::CaseClause:       analyzer.visit(static_cast<CaseClause&>(stmt)); break;
        case ASTNodeKind::WithStmt:         analyzer.visit(static_cast<WithStmt&>(stmt)); break;
        case ASTNodeKind::GoToStmt:         analyzer.visit(static_cast<GoToStmt&>(stmt)); break;
        case ASTNodeKind::OnErrorStmt:      analyzer.visit(static_cast<OnErrorStmt&>(stmt)); break;
        case ASTNodeKind::ResumeStmt:       analyzer.visit(static_cast<ResumeStmt&>(stmt)); break;
        case ASTNodeKind::ErrorStmt:        analyzer.visit(static_cast<ErrorStmt&>(stmt)); break;
        case ASTNodeKind::ExitStmt:         analyzer.visit(static_cast<ExitStmt&>(stmt)); break;
        case ASTNodeKind::CallStmt:         analyzer.visit(static_cast<CallStmt&>(stmt)); break;
        case ASTNodeKind::ReDimStmt:        analyzer.visit(static_cast<ReDimStmt&>(stmt)); break;
        case ASTNodeKind::LabelStmt:        analyzer.visit(static_cast<LabelStmt&>(stmt)); break;
        case ASTNodeKind::OptionStmt:       analyzer.visit(static_cast<OptionStmt&>(stmt)); break;
        case ASTNodeKind::LocalDeclStmt:    analyzer.visit(static_cast<LocalDeclStmt&>(stmt)); break;
        case ASTNodeKind::RaiseEventStmt:   /* P6.5: RaiseEvent在Pass2语义分析中验证事件存在性 */ break;
        // 文件I/O语句
        case ASTNodeKind::OpenStmt:        analyzer.visit(static_cast<OpenStmt&>(stmt)); break;
        case ASTNodeKind::GetStmt:         analyzer.visit(static_cast<GetStmt&>(stmt)); break;
        case ASTNodeKind::PutStmt:         analyzer.visit(static_cast<PutStmt&>(stmt)); break;
        case ASTNodeKind::GoSubStmt:        analyzer.visit(static_cast<GoSubStmt&>(stmt)); break;
case ASTNodeKind::OnGoSubStmt:      /* P17.4: OnGoSub labels validated in GoSub validation pass */ break;
        case ASTNodeKind::OnGoToStmt:      /* P18-A: OnGoTo labels validated in GoSub validation pass */ break;
        case ASTNodeKind::MidStmt:        /* P18-A: Mid$ statement */ break;
        case ASTNodeKind::ReturnStmt:       analyzer.visit(static_cast<ReturnStmt&>(stmt)); break;
        // 其他语句暂不处理
        default: break;
    }
}

// 表达式节点分派
void dispatchExpr(Expr& expr, SemanticAnalyzer& analyzer) {
    switch (expr.kind) {
        case ASTNodeKind::BinaryExpr:          analyzer.visit(static_cast<BinaryExpr&>(expr)); break;
        case ASTNodeKind::UnaryExpr:           analyzer.visit(static_cast<UnaryExpr&>(expr)); break;
        case ASTNodeKind::LiteralExpr:         analyzer.visit(static_cast<LiteralExpr&>(expr)); break;
        case ASTNodeKind::IdentifierExpr:      analyzer.visit(static_cast<IdentifierExpr&>(expr)); break;
        case ASTNodeKind::MemberAccessExpr:    analyzer.visit(static_cast<MemberAccessExpr&>(expr)); break;
        case ASTNodeKind::DictionaryAccessExpr:analyzer.visit(static_cast<DictionaryAccessExpr&>(expr)); break;
        case ASTNodeKind::IndexOrCallExpr:     analyzer.visit(static_cast<IndexOrCallExpr&>(expr)); break;
        case ASTNodeKind::NewExpr:             analyzer.visit(static_cast<NewExpr&>(expr)); break;
        case ASTNodeKind::TypeOfExpr:          analyzer.visit(static_cast<TypeOfExpr&>(expr)); break;
        case ASTNodeKind::AddressOfExpr:       analyzer.visit(static_cast<AddressOfExpr&>(expr)); break;
        case ASTNodeKind::MeExpr:              analyzer.visit(static_cast<MeExpr&>(expr)); break;
        case ASTNodeKind::WithMemberExpr:      analyzer.visit(static_cast<WithMemberExpr&>(expr)); break;
        default: break;
    }
}
} // namespace vb6c3
