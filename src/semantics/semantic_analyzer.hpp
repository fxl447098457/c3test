#pragma once
// VB6语义分析器 - 符号表构建 + 类型检查
// 两遍扫描: Pass1收集声明, Pass2分析过程体

#include "ast/ast.hpp"
#include "ast/ast_visitor.hpp"
#include "semantics/symbol_table.hpp"
#include "semantics/type_system.hpp"
#include "common/diagnostics.hpp"
#include <string>
#include <vector>
#include <iostream>

namespace vb6c3 {

// ============================================================
// 语义分析器
// ============================================================

class SemanticAnalyzer : public ASTVisitor {
public:
    SemanticAnalyzer(Diagnostics& diag, bool verbose = false);

    // 分析整个模块 (两遍扫描)
    bool analyze(Module& module);

    // 打印符号表
    void dumpSymbols(std::ostream& os) const;

    // 获取符号表 (供后续阶段使用)
    SymbolTable& symbolTable() { return symTab_; }
    const SymbolTable& symbolTable() const { return symTab_; }

    // 获取类型系统
    TypeSystem& typeSystem() { return typeSys_; }

    // --- ASTVisitor overrides ---
    // 声明
    void visit(SubDecl& node) override;
    void visit(FunctionDecl& node) override;
    void visit(PropertyDecl& node) override;
    void visit(TypeDecl& node) override;
    void visit(TypeMember& node) override;
    void visit(EnumDecl& node) override;
    void visit(EnumMember& node) override;
    void visit(DeclareDecl& node) override;
    void visit(EventDecl& node) override;
    void visit(ConstDecl& node) override;
    void visit(VariableDecl& node) override;
    void visit(ParameterDecl& node) override;

    // 语句
    void visit(Block& node) override;
    void visit(AssignmentStmt& node) override;
    void visit(SetStmt& node) override;
    void visit(LetStmt& node) override;
    void visit(IfStmt& node) override;
    void visit(ElseIfClause& node) override;
    void visit(ForStmt& node) override;
    void visit(ForEachStmt& node) override;
    void visit(DoLoopStmt& node) override;
    void visit(WhileWendStmt& node) override;
    void visit(SelectCaseStmt& node) override;
    void visit(CaseClause& node) override;
    void visit(WithStmt& node) override;
    void visit(GoToStmt& node) override;
    void visit(GoSubStmt& node) override;
    void visit(ReturnStmt& node) override;
    void visit(OnErrorStmt& node) override;
    void visit(ExitStmt& node) override;
    void visit(CallStmt& node) override;
    void visit(ReDimStmt& node) override;
    void visit(LabelStmt& node) override;
    void visit(OptionStmt& node) override;
    void visit(LocalDeclStmt& node) override;
    // 文件I/O
    void visit(OpenStmt& node) override;
    void visit(GetStmt& node) override;
    void visit(PutStmt& node) override;

    // 表达式
    void visit(BinaryExpr& node) override;
    void visit(UnaryExpr& node) override;
    void visit(LiteralExpr& node) override;
    void visit(IdentifierExpr& node) override;
    void visit(MemberAccessExpr& node) override;
    void visit(DictionaryAccessExpr& node) override;
    void visit(IndexOrCallExpr& node) override;
    void visit(NewExpr& node) override;
    void visit(TypeOfExpr& node) override;
    void visit(AddressOfExpr& node) override;
    void visit(MeExpr& node) override;
    void visit(WithMemberExpr& node) override;

    // 类型引用
    void visit(SimpleTypeRef& node) override;
    void visit(ArrayTypeRef& node) override;
    void visit(FixedStringTypeRef& node) override;

    // 模块 (两遍入口)
    void visit(Module& node) override;

private:
    Diagnostics& diag_;
    SymbolTable symTab_;
    TypeSystem typeSys_;
    bool verbose_;

    // 分析状态
    int pass_ = 0;              // 当前遍次 (1=声明收集, 2=体分析)
    Module* currentModule_ = nullptr;
    Symbol* currentProc_ = nullptr;  // 当前过程符号

    // 表达式类型推导结果 (最近一次表达式访问的推导类型)
    Vb6Type lastExprType_ = Vb6Type::Unknown;

    // Option Explicit 标志
    bool optionExplicit_ = false;

    // With 语句对象类型栈
    std::vector<Vb6Type> withStack_;

    // 已声明的标签 (用于GoTo检查)
    std::vector<std::string> declaredLabels_;
        std::vector<std::pair<std::string, SourceLocation>> gosubTargetLabels_;  // P12.5: GoSub引用的标签+位置

    // ---- 内部辅助 ----

    // 解析TypeRefPtr为Vb6Type
    Vb6Type resolveTypeRef(ASTNode* typeRef);

    // 注册单个声明到符号表 (Pass1)
    void registerDecl(Decl& decl);

    // 注册VariableDecl
    void registerVariable(VariableDecl& decl);

    // 注册ConstDecl
    void registerConstant(ConstDecl& decl);

    // 分析语句列表
    void analyzeStmtList(StmtList& stmts);

    // 分析表达式并返回推导类型
    Vb6Type analyzeExpr(Expr& expr);

    // 检查赋值类型兼容性
    void checkAssignment(Vb6Type targetType, Vb6Type valueType,
                         const SourceLocation& loc, const std::string& context);

    // 检查过程调用参数
    void checkCallArgs(Symbol* procSym, IndexOrCallExpr& callNode);

    // 标记符号为已引用
    void markReferenced(const std::string& name);

    // 检查未引用的符号 (Option Explicit时发出警告)
    void checkUnreferencedSymbols();

    // 注册内置对象、函数和常量
    void registerBuiltins();

    // 生成唯一内部名称
    static std::string makeInternalName(const std::string& prefix, const std::string& name);
};

} // namespace vb6c3
