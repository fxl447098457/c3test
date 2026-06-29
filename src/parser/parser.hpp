#pragma once
// vb6c3 - Visual Basic 6.0 Compiler
// 递归下降解析器 + Pratt 表达式解析
// 参考: FreeBASIC parser.bis, RustASP Pratt (l_bp, r_bp) 设计, VB6.g4 ANTLR

#include "preprocessor/preprocessor.hpp"
#include "ast/ast.hpp"
#include "common/diagnostics.hpp"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace vb6c3 {

// ============================================================
// Pratt 优先级表
// VB6 运算符优先级 (从低到高, 14级)
// l_bp = 左绑定力, r_bp = 右绑定力
// 左结合: l_bp < r_bp (如 +: 5,6)
// 右结合: l_bp > r_bp (如 ^: 12,11)
// ============================================================
struct BindingPower {
    int l_bp;  // 左绑定力 (null denotation 时为 0)
    int r_bp;  // 右绑定力
};

class Parser {
public:
    // 使用预处理器 (含条件编译支持)
    Parser(std::shared_ptr<SourceBuffer> buffer, Diagnostics& diag,
           const PreprocessOptions& ppOpts = PreprocessOptions());

    // 主入口: 解析整个模块
    // isClassModule: true=.cls类模块, false=.bas标准模块/.frm窗体模块
    std::unique_ptr<Module> parseModule(bool isClassModule = false);

    // 诊断查询
    bool hasErrors() const { return diag_.hasErrors(); }

private:
    // ============================================================
    // Token 消费接口
    // ============================================================

    // 前瞻 (不消费)
    const Token& peek() const;
    const Token& peek2() const;

    // 消费当前 token 并返回 (自动跳过 Comment)
    Token advance();

    // 从词法器获取下一个非 Comment token
    Token fetchNextToken();

    // 消费当前 token, 断言类型匹配; 不匹配则报错并返回当前 token
    Token expect(TokenKind kind, DiagnosticID diagId, const std::string& msg);
    Token expect(TokenKind kind, const std::string& msg);

    // 如果当前 token 匹配则消费, 否则返回 false
    bool match(TokenKind kind);

    // 如果当前 token 匹配则消费, 否则报错
    // 返回是否成功匹配
    bool expectOrSkip(TokenKind kind, DiagnosticID diagId, const std::string& msg);

    // 当前 token 类型判断
    bool check(TokenKind kind) const;
    bool checkAny(std::initializer_list<TokenKind> kinds) const;

    // 判断是不是语句开始 token
    bool isStatementStart() const;
    bool isDeclarationStart() const;

    // 新行处理: VB6 新行是语句分隔符
    // 跳过零或多个 NewLine (同时跳过 Comment)
    void skipNewLines();
    // 期望至少一个 NewLine (语句终止), 变体: 也接受冒号
    bool expectEndOfStatement();

    // 标识符解析: 接受 Identifier 或软关键字 (VB6 允许 Name/String/Step 等作为标识符)
    bool isSoftKeyword(TokenKind kind) const;
    bool canBeName(TokenKind kind) const;
    Token expectName(const std::string& msg);

    // 获取当前源码位置
    SourceLocation currentLoc() const;

    // ============================================================
    // 模块解析 (最高层)
    // ============================================================
    void parseModuleBody(Module& mod);
    std::unique_ptr<OptionStmt> parseOption();
    std::unique_ptr<ImplementsStmt> parseImplements();
    std::unique_ptr<DefTypeStmt> parseDefType();
    std::unique_ptr<AttributeStmt> parseAttribute();

    // ============================================================
    // 声明解析 (parser_decl.cpp)
    // ============================================================
    DeclPtr parseDeclaration();
    std::unique_ptr<SubDecl> parseSubDecl(AccessLevel access, bool isStatic);
    std::unique_ptr<FunctionDecl> parseFunctionDecl(AccessLevel access, bool isStatic);
    std::unique_ptr<PropertyDecl> parsePropertyDecl(AccessLevel access);
    std::unique_ptr<TypeDecl> parseTypeDecl(AccessLevel access);
    std::unique_ptr<EnumDecl> parseEnumDecl(AccessLevel access);
    std::unique_ptr<DeclareDecl> parseDeclareDecl(AccessLevel access);
    std::unique_ptr<EventDecl> parseEventDecl(AccessLevel access);
    std::unique_ptr<ConstDecl> parseConstDecl(AccessLevel access);
    std::unique_ptr<VariableDecl> parseVariableDecl(AccessLevel access, bool isStatic);

    // 参数列表
    std::vector<std::unique_ptr<ParameterDecl>> parseParameterList();
    std::unique_ptr<ParameterDecl> parseParameter();

    // 类型引用
    TypeRefPtr parseTypeRef();

    // ============================================================
    // 语句解析 (parser_stmt.cpp)
    // ============================================================
    StmtPtr parseStatement();
    StmtList parseBlock(TokenKind endKind1, TokenKind endKind2 = TokenKind::EndOfFile);
    StmtList parseBlockUntil(std::initializer_list<TokenKind> endKinds);

    // 块语句
    std::unique_ptr<IfStmt> parseIfStmt();
    StmtPtr parseForOrForEach();
    std::unique_ptr<ForStmt> parseForStmt();
    std::unique_ptr<ForEachStmt> parseForEachStmt();
    std::unique_ptr<DoLoopStmt> parseDoLoopStmt();
    std::unique_ptr<WhileWendStmt> parseWhileWendStmt();
    std::unique_ptr<SelectCaseStmt> parseSelectCaseStmt();
    std::unique_ptr<WithStmt> parseWithStmt();

    // 单行语句
    StmtPtr parseOnStmt();
    std::unique_ptr<OnErrorStmt> parseOnErrorStmt();
    std::unique_ptr<ResumeStmt> parseResumeStmt();
    std::unique_ptr<ErrorStmt> parseErrorStmt();
    std::unique_ptr<OnGoToStmt> parseOnGoToStmt();
    std::unique_ptr<OnGoSubStmt> parseOnGoSubStmt();
    std::unique_ptr<ExitStmt> parseExitStmt();
    std::unique_ptr<GoToStmt> parseGoToStmt();
    std::unique_ptr<GoSubStmt> parseGoSubStmt();
    std::unique_ptr<ReturnStmt> parseReturnStmt();
    std::unique_ptr<ReDimStmt> parseReDimStmt();
    std::unique_ptr<EraseStmt> parseEraseStmt();
    std::unique_ptr<RaiseEventStmt> parseRaiseEventStmt();
    std::unique_ptr<EndStmt> parseEndStmt();
    std::unique_ptr<StopStmt> parseStopStmt();
    std::unique_ptr<SetStmt> parseSetStmt();
    std::unique_ptr<LetStmt> parseLetStmt();
    std::unique_ptr<CallStmt> parseCallStmt();
    StmtPtr parseDimStmt();
    StmtPtr parseConstStmtInBody();
    StmtPtr parseStaticStmtInBody();
    StmtPtr parseAccessDeclInBody();

    // 行标签/赋值/调用 (两可: label: 或 x = 1 或 proc args)
    StmtPtr parseLabelOrAssignmentOrCall();

    // 文件 I/O 语句
    std::unique_ptr<OpenStmt> parseOpenStmt();
    std::unique_ptr<CloseStmt> parseCloseStmt();
    std::unique_ptr<GetStmt> parseGetStmt();
    std::unique_ptr<PutStmt> parsePutStmt();
    std::unique_ptr<InputStmt> parseInputStmt();
    std::unique_ptr<PrintStmt> parsePrintStmt();
    std::unique_ptr<WriteStmt> parseWriteStmt();
    std::unique_ptr<LineInputStmt> parseLineInputStmt();
    std::unique_ptr<WidthStmt> parseWidthStmt();
    std::unique_ptr<SeekStmt> parseSeekStmt();
    std::unique_ptr<LockStmt> parseLockStmt();
    std::unique_ptr<UnlockStmt> parseUnlockStmt();
    std::unique_ptr<NameStmt> parseNameStmt();
    std::unique_ptr<FileCopyStmt> parseFileCopyStmt();
    std::unique_ptr<KillStmt> parseKillStmt();
    std::unique_ptr<MkDirStmt> parseMkDirStmt();
    std::unique_ptr<RmDirStmt> parseRmDirStmt();
    std::unique_ptr<ChDirStmt> parseChDirStmt();
    std::unique_ptr<ChDriveStmt> parseChDriveStmt();

    // 杂项语句
    StmtPtr parseBeepOrDoEvents();
    std::unique_ptr<AttributeStmt> parseAttributeInBody();

    // ============================================================
    // 表达式解析 (parser_expr.cpp) — Pratt 解析器
    // ============================================================
    ExprPtr parseExpression();
    ExprPtr parseExpression(int minBp);        // Pratt 核心: 最小绑定力
    ExprPtr parseNullDenotation();             // 前缀/原子表达式
    ExprPtr parseLeftDenotation(ExprPtr left, int& minBp);  // 中缀/后缀

    // 原子表达式辅助
    ExprPtr parseLiteral();
    ExprPtr parseIdentifierOrCall();
    ExprPtr parseParenthesizedExpr();
    ExprPtr parseNewExpr();
    ExprPtr parseTypeOfExpr();
    ExprPtr parseAddressOfExpr();
    ExprPtr parseMeExpr();
    ExprPtr parseWithMemberExpr();

    // 后缀: .member, !dict, (args)
    ExprPtr parsePostfix(ExprPtr expr);

    // Case 值解析 (Is > 0, 1 To 10, 等变体)
    CaseClause::CaseValue parseCaseValue();

    // ============================================================
    // 运算符优先级查找
    // ============================================================
    BindingPower getBindingPower(TokenKind kind) const;

    // TokenKind → BinaryOp 映射
    BinaryOp tokenToBinaryOp(TokenKind kind) const;

    // 判断 token 是否是中缀运算符
    bool isInfixOperator(TokenKind kind) const;

    // 判断 token 是否是前缀运算符
    bool isPrefixOperator(TokenKind kind) const;

    // ============================================================
    // 错误恢复
    // ============================================================
    // 跳过 token 直到遇到语句开始符号或同步点
    void synchronize();
    // 跳过到行尾
    void skipToNextLine();

    // ============================================================
    // 辅助
    // ============================================================
    // 标识符比较 (VB6 不区分大小写)
    bool identifierEquals(const std::string& a, const std::string& b) const;
    // Token 文本转小写
    std::string toLower(const std::string& s) const;

    // 判断 End 后面跟着什么 (End If, End Sub, End Function, ...)
    // 返回 true 如果 End + next 构成 End XXX 块终止符
    bool isEndBlock() const;

private:
    Preprocessor preproc_;
    Diagnostics& diag_;
    std::shared_ptr<SourceBuffer> buffer_;

    // 当前 token (lookahead 缓冲的第一个)
    Token cur_;
    // 前瞻 token
    Token next_;

    // With 嵌套深度 (>0 时 .member 为 WithMemberExpr)
    int withDepth_ = 0;

    // 安全限制: advance调用计数
    size_t advanceCount_ = 0;
    static constexpr size_t MAX_ADVANCES = 10'000'000;

    // 优先级表 (初始化一次)
    std::unordered_map<int, BindingPower> bpTable_;

    void initBindingPowers();
};

} // namespace vb6c3
