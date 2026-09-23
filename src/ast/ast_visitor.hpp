#pragma once
// vb6c - AST Visitor 模式接口
// 用于遍历和操作AST节点

#include "ast/ast.hpp"

namespace vb6c3 {

// AST Visitor 基类 - 返回void的简单访问者
// 子类override需要的visit方法即可, 无需实现所有
class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;

    // --- 模块 ---
    virtual void visit(Module& node) {}

    // --- 声明 ---
    virtual void visit(SubDecl& node) {}
    virtual void visit(FunctionDecl& node) {}
    virtual void visit(PropertyDecl& node) {}
    virtual void visit(TypeDecl& node) {}
    virtual void visit(TypeMember& node) {}
    virtual void visit(EnumDecl& node) {}
    virtual void visit(EnumMember& node) {}
    virtual void visit(DeclareDecl& node) {}
    virtual void visit(EventDecl& node) {}
    virtual void visit(DelegateDecl& node) {}
    virtual void visit(ConstDecl& node) {}
    virtual void visit(VariableDecl& node) {}
    virtual void visit(ParameterDecl& node) {}

    // --- 语句 ---
    virtual void visit(Block& node) {}
    virtual void visit(AssignmentStmt& node) {}
    virtual void visit(SetStmt& node) {}
    virtual void visit(LetStmt& node) {}
    virtual void visit(IfStmt& node) {}
    virtual void visit(ElseIfClause& node) {}
    virtual void visit(ForStmt& node) {}
    virtual void visit(ForEachStmt& node) {}
    virtual void visit(DoLoopStmt& node) {}
    virtual void visit(WhileWendStmt& node) {}
    virtual void visit(SelectCaseStmt& node) {}
    virtual void visit(CaseClause& node) {}
    virtual void visit(WithStmt& node) {}
    virtual void visit(GoToStmt& node) {}
    virtual void visit(GoSubStmt& node) {}
    virtual void visit(ReturnStmt& node) {}
    virtual void visit(OnErrorStmt& node) {}
    virtual void visit(ResumeStmt& node) {}
    virtual void visit(ErrorStmt& node) {}
    virtual void visit(OnGoToStmt& node) {}
    virtual void visit(OnGoSubStmt& node) {}
    virtual void visit(MidStmt& node) {}  // P18-A
    virtual void visit(ExitStmt& node) {}
    virtual void visit(StopStmt& node) {}
    virtual void visit(EndStmt& node) {}
    virtual void visit(CallStmt& node) {}
    virtual void visit(ReDimStmt& node) {}
    virtual void visit(EraseStmt& node) {}
    virtual void visit(LabelStmt& node) {}
    virtual void visit(RaiseEventStmt& node) {}
    virtual void visit(OpenStmt& node) {}
    virtual void visit(CloseStmt& node) {}
    virtual void visit(GetStmt& node) {}
    virtual void visit(PutStmt& node) {}
    virtual void visit(InputStmt& node) {}
    virtual void visit(PrintStmt& node) {}
    virtual void visit(WriteStmt& node) {}
    virtual void visit(LineInputStmt& node) {}
    virtual void visit(WidthStmt& node) {}
    virtual void visit(SeekStmt& node) {}
    virtual void visit(LockStmt& node) {}
    virtual void visit(UnlockStmt& node) {}
    virtual void visit(ResetStmt& node) {}
    virtual void visit(NameStmt& node) {}
    virtual void visit(FileCopyStmt& node) {}
    virtual void visit(KillStmt& node) {}
    virtual void visit(MkDirStmt& node) {}
    virtual void visit(RmDirStmt& node) {}
    virtual void visit(ChDirStmt& node) {}
    virtual void visit(ChDriveStmt& node) {}
    virtual void visit(BeepStmt& node) {}
    virtual void visit(DoEventsStmt& node) {}
    virtual void visit(AttributeStmt& node) {}
    virtual void visit(OptionStmt& node) {}
    virtual void visit(ImplementsStmt& node) {}
    virtual void visit(DefTypeStmt& node) {}
    virtual void visit(LocalDeclStmt& node) {}

    // --- 表达式 ---
    virtual void visit(BinaryExpr& node) {}
    virtual void visit(UnaryExpr& node) {}
    virtual void visit(LiteralExpr& node) {}
    virtual void visit(IdentifierExpr& node) {}
    virtual void visit(MemberAccessExpr& node) {}
    virtual void visit(DictionaryAccessExpr& node) {}
    virtual void visit(IndexOrCallExpr& node) {}
    virtual void visit(NewExpr& node) {}
    virtual void visit(TypeOfExpr& node) {}
    virtual void visit(AddressOfExpr& node) {}
    virtual void visit(MeExpr& node) {}
    virtual void visit(WithMemberExpr& node) {}

    // --- 类型引用 ---
    virtual void visit(SimpleTypeRef& node) {}
    virtual void visit(ArrayTypeRef& node) {}
    virtual void visit(FixedStringTypeRef& node) {}
};

// 递归遍历AST的辅助函数
// 对每个节点调用visitor对应的visit方法, 然后递归子节点
void traverseAST(ASTNode& node, ASTVisitor& visitor);

// 递归遍历表达式
void traverseExpr(Expr& expr, ASTVisitor& visitor);

// 递归遍历语句
void traverseStmt(Stmt& stmt, ASTVisitor& visitor);

// 递归遍历声明
void traverseDecl(Decl& decl, ASTVisitor& visitor);

} // namespace vb6c3
