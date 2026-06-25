#pragma once
// vb6c3 - C代码生成器
// 将语义分析后的AST+符号表翻译为C代码，交由MSVC编译
// 设计参考: cfront, Nim, Zig早期均采用C代码生成路线

#include "ast/ast.hpp"
#include "ast/ast_visitor.hpp"
#include "semantics/symbol_table.hpp"
#include "semantics/type_system.hpp"
#include "common/diagnostics.hpp"
#include <string>
#include <vector>
#include <sstream>
#include <unordered_set>
#include <unordered_map>

namespace vb6c3 {

// ============================================================
// 代码输出辅助: 缩进管理、格式化
// ============================================================

class CodeEmitter {
public:
    // 输出一行（自动缩进+换行）
    void emitLine(const std::string& line = "");

    // 输出内容（不带换行）
    void emit(const std::string& text);

    // 输出空行
    void emitBlank();

    // 缩进控制
    void indent() { indentLevel_++; }
    void dedent() { if (indentLevel_ > 0) indentLevel_--; }

    // 获取生成的代码
    std::string str() const { return oss_.str(); }

    // 清空
    void clear() { oss_.str(""); oss_.clear(); indentLevel_ = 0; }

private:
    std::ostringstream oss_;
    int indentLevel_ = 0;
};

// ============================================================
// C代码生成器
//
// 输入: Module AST + SemanticAnalyzer的产出(符号表+类型系统)
// 输出: .h头文件 + .c源文件
//
// 架构:
//   1. generate() 主入口 → 生成 .h + .c
//   2. H文件: 类型定义、常量、变量声明、函数前向声明
//   3. C文件: #include ".h"、变量定义、函数实现
//   4. 各emit*方法按节点kind分派
// ============================================================

class CCodeGen : public ASTVisitor {
public:
    CCodeGen(Diagnostics& diag, const SymbolTable& symTab,
             const TypeSystem& typeSys, bool verbose = false);

    // 主入口: 生成C代码，返回是否成功
    bool generate(Module& module, const std::string& baseName);

    // 获取生成的代码
    const std::string& headerCode() const { return header_; }
    const std::string& sourceCode() const { return source_; }

    // --- 声明 ---
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

    // --- 语句 ---
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
    void visit(OnErrorStmt& node) override;
    void visit(ExitStmt& node) override;
    void visit(CallStmt& node) override;
    void visit(ReDimStmt& node) override;
    void visit(LabelStmt& node) override;
    void visit(OptionStmt& node) override;
    void visit(LocalDeclStmt& node) override;

    // --- 表达式 (返回C表达式字符串) ---
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

    // --- 类型引用 ---
    void visit(SimpleTypeRef& node) override;
    void visit(ArrayTypeRef& node) override;
    void visit(FixedStringTypeRef& node) override;

    // --- 模块 ---
    void visit(Module& node) override;

private:
    Diagnostics& diag_;
    const SymbolTable& symTab_;
    const TypeSystem& typeSys_;
    bool verbose_;

    // 代码输出
    CodeEmitter h_;   // .h 文件内容
    CodeEmitter c_;   // .c 文件内容
    std::string header_;
    std::string source_;
    std::string baseName_;  // 输出文件基名 (如 "hello")

    // 状态
    Module* currentModule_ = nullptr;
    Symbol* currentProc_ = nullptr;  // 当前过程符号
    std::string currentReturnVar_;   // Function返回值变量名 (如 "vb6_ret_CalculateSum")
    int labelCounter_ = 0;           // 标签计数器 (避免C标签冲突)
    int tempCounter_ = 0;            // 临时变量计数器

    // 表达式求值结果 (累加器模式: 每个emitExpr调用设置lastExpr_)
    std::string lastExpr_;           // 最近一次表达式生成的C代码字符串

    // 已生成的C标识符 (避免重复)
    std::unordered_set<std::string> emittedSymbols_;

    // With语句名称栈
    std::vector<std::string> withObjectVars_;

    // ---- 类型映射 ----

    // Vb6Type → C类型字符串
    std::string mapType(Vb6Type type) const;

    // TypeRefPtr → C类型字符串
    std::string mapTypeRef(ASTNode* typeRef) const;

    // VB6默认值 → C表达式
    std::string defaultValue(Vb6Type type) const;

    // ---- 标识符命名 ----

    // VB6标识符 → 安全C标识符 (处理关键字冲突、特殊字符)
    std::string cIdent(const std::string& vb6Name) const;

    // VB6标识符 → C全局函数名 (模块名_过程名)
    std::string cProcName(const std::string& procName, AccessLevel access) const;

    // ---- 表达式求值 ----

    // 生成C表达式字符串, 存入lastExpr_
    void emitExpr(Expr& expr);

    // ---- 语句生成 ----

    // 生成语句列表
    void emitStmtList(StmtList& stmts);

    // ---- 声明生成 ----

    // 生成模块级声明
    void emitModuleDecl(Decl& decl);

    // 生成函数签名 (不含函数体)
    std::string makeProcSignature(SubDecl& node);
    std::string makeProcSignature(FunctionDecl& node);

    // 生成参数列表
    std::string makeParamList(std::vector<std::unique_ptr<ParameterDecl>>& params);

    // ---- 二元运算符映射 ----
    std::string mapBinaryOp(BinaryOp op) const;
};

} // namespace vb6c3
