#pragma once
// VB6语义分析器 - 符号表构建 + 类型检查
// 两遍扫描: Pass1收集声明, Pass2分析过程体

#include "ast/ast.hpp"
#include "ast/ast_visitor.hpp"
#include "semantics/symbol_table.hpp"
#include "semantics/type_system.hpp"
#include "semantics/generics_registry.hpp"
#include "semantics/interfaces_registry.hpp"
#include "semantics/class_chain_registry.hpp"  // tB 类继承 (B07b)
#include "common/diagnostics.hpp"
#include <string>
#include <set>
#include <utility>
#include <vector>
#include <iostream>

namespace vb6c3 {

// 成员级 `Implements I.M[, I.N]` 尾子句的定位键 (tB 扩展, ai/022 B02b):
// (所属过程声明节点, 子句在该声明尾部列表中的序号).
using IfaceClauseRef = std::pair<const Decl*, size_t>;

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
    void visit(DelegateDecl& node) override;
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
    void visit(OnGoSubStmt& node) override;  // P17.4
    void visit(OnGoToStmt& node) override;  // P18-A
    void visit(MidStmt& node) override;  // P18-A
    void visit(ReturnStmt& node) override;
    void visit(OnErrorStmt& node) override;
    void visit(ResumeStmt& node) override;
    void visit(ErrorStmt& node) override;
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

    // 跨模块重载延后解析 (O3): 本模块分析时其它模块的 Public 组尚未注入
    // (runCrossModuleResolution 在其后), 故 visit(IndexOrCallExpr) 对当时查无
    // 此名的调用点记下 节点+实参类型; Driver 注入完成后调本方法逐点补跑
    // resolveOverload, 把 calleeOvlSuffix 写回 AST. 名称最终不属于任何重载组
    // 的点原样放过 (维持旧行为).
    void resolveDeferredCrossModuleOverloads();

    // 类继承 (tB, B07b): 这个名字是不是某个祖先类自己声明的成员 (字段或过程)?
    // 合并只发生在 Class 符号的成员表上, 模块作用域里没有它的过程符号 → 裸名会静默生成
    // 空调用, 所以发码前必须报错 (见 visit(IdentifierExpr) 的调用点)。
    bool declaredByAncestor(const std::string& name) const;
    // 虚方法 (tB, B08b): 本类体内对这个名字的调用必须走虚槽 (有后代 Overrides 了它)
    bool virtualCallNeedsDispatch(const std::string& name) const;

    // 类继承 (tB, B08c): `obj.<成员>` 的 Protected 越权判定, 命中即报错并返回 true。
    // 只在"接收者解析得出工程类 + 链上最近的声明者把它声明成 Protected + 当前模块不在那条
    // 家族链上"三者同时成立时报错; 任一不成立 (含解析不出接收者) 一律放过 —— 漏报可以补,
    // 把能编译的代码判成越权不可接受。红线同 D29-1: 绝不去 driver_crossmod 的逐字段成员表
    // 拷贝里按级别过滤, 那会把越权退化成运行期才炸的晚绑定 COM 调用。
    bool checkProtectedVisibility(const Expr& obj, const std::string& member,
                                  const SourceLocation& loc);
    // stage 2.8 登记表里"就是本模块"的那个类视图; 未登记 (泛型模板 / 接口宿主) 返回 nullptr。
    const ClassChainView* selfClassView() const;

    // 泛型 (tB, G3): 模板登记表只读视图 (driver 在逐模块分析前注入).
    void setGenericRegistry(const GenRegistry* reg) { genReg_ = reg; }
    // Interface 契约 (tB, B02): stage 2.7 建好的只读登记表, 供 Implements 分叉判定.
    void setInterfaceRegistry(const IfaceRegistry* reg) { ifaceReg_ = reg; }
    // 类继承 (tB, B07b): stage 2.8 链登记表 (祖先声明的只读视图), 供裸名继承成员判定.
    void setClassChainRegistry(const ClassChainRegistry* reg) { clsreg_ = reg; }
    // 推断成功的实例化请求 (驱动 fixpoint 物化) — 取空语义.
    struct GenInstRequest {
        std::string flat;                  // 小写扁名
        std::string base;                  // 模板名 (原大小写)
        std::vector<std::string> args;     // 类型实参名 (扁名)
    };
    std::vector<GenInstRequest> takeGenericRequests() {
        std::vector<GenInstRequest> out;
        out.swap(genericRequests_);
        return out;
    }
    // 物化器注入的特化过程增量分析 (模块常规分析已结束时的补注册路径):
    // 对给定声明按 pass1 注册 + pass2 体分析 走一遍 (与 visit(Module) 同构).
    void analyzeExtraDecls(const std::vector<Decl*>& decls);

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

    // P22: DefType mapping - letter -> implicit type
    Vb6Type defTypeMap_[26];  // index = toupper(letter) - 'A', default Variant
    bool defTypeActive_ = false;  // whether any DefType is declared

    // With 语句对象类型栈
    std::vector<Vb6Type> withStack_;

    // 已声明的标签 (用于GoTo检查)
    std::vector<std::string> declaredLabels_;

    // --- Interface 契约 (tB, B02) ---
    // 新式接口的严格契约比对 (error 级): 槽表来自 stage 2.7 登记表, 实现侧按
    // 同一套 ifaceSlotKey/ifaceSigFromDecl 规范函数取名 (源码签名口径).
    // 与 legacy VB6 Implements (warn 级 + IFace_M 命名约定) 互斥, 见 analyze() 分叉.
    void checkNewStyleInterface(const Module& module, const IfaceView& view,
                                const std::string& writtenName, const SourceLocation& loc,
                                std::set<IfaceClauseRef>& boundClauses);
    // 成员级 `Implements I.M[, I.N]` 子句 (tB, B02b) 的兜底诊断: 每条子句都必须被某个
    // 新式契约比对接纳, 否则它就是静默失效的摆设; 非类模块里的子句一并在此报错.
    void checkMemberImplementsClauses(const Module& module,
                                      const std::set<IfaceClauseRef>& boundClauses);
        std::vector<std::pair<std::string, SourceLocation>> gosubTargetLabels_;  // P12.5: GoSub引用的标签+位置

    // ---- 内部辅助 ----

    // 解析TypeRefPtr为Vb6Type
    Vb6Type resolveTypeRef(ASTNode* typeRef);
    Vb6Type resolveTypeOrDefault(const std::string& name, ASTNode* typeRef);  // P22: DefType-aware

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

    // --- Delegate 辅助 (semantic_analyzer_expr.cpp) ---
    // typeName 解析为 SymbolKind::Delegate 时返回其符号, 否则 nullptr.
    Symbol* lookupDelegateSym(const std::string& typeName);
    // 校验 proc 是否匹配 del 签名 (procKind/返回类型/逐参类型与ByVal/参数个数).
    bool checkDelegateSignature(Symbol* del, Symbol* proc, SourceLocation loc);
    // checkDelegateSignature 的无诊断版 (重载候选集筛选用)
    bool matchesDelegateSignature(Symbol* del, Symbol* proc);
    // 重载签名指纹 (tB 式, O1): 有资格分组 (非类模块的 Sub|Function、不含 ParamArray)
    // 时返回 "F|S : 每个参数 <type码><b|r>[o] : R<retType码>"，否则空串。
    std::string computeOverloadFp(const Symbol& sym) const;
    // 重载选择 (O2): 逐参打分 4=精确 / 2=隐式可转 / 1=Variant形参兜底 / 0=淘汰,
    // 求和取最高分唯一者; 平手报歧义、全淘汰报无匹配, 两者都回落 head 继续编译。
    // suffixOut = 选定变体键后缀 ("" = head/非重载), 供 cgen 定形 C 名。
    int ovlScoreParam(Vb6Type argT, const ParameterInfo& p);
    Symbol* resolveOverload(Symbol* head, const std::vector<Vb6Type>& argT,
                            SourceLocation loc, std::string& suffixOut);

    // 跨模块重载延后解析的登记项 (见 public resolveDeferredCrossModuleOverloads)
    struct DeferredXmodCallSite {
        IndexOrCallExpr* node;
        std::string identName;
        std::vector<Vb6Type> argTypes;
        // 泛型 (tB, G3): 逐位置实参"是否数组"标记 — 记录时局部作用域尚在,
        // 可靠查得数组符号 (argTypes 里数组变量只带**元素类型**不带 Array 位,
        // 且延后绑定期局部作用域已弹出无法回查). 专供 T() 形参位推断, 不污染
        // argTypes (后者兼作已发货的跨模块重载打分输入).
        std::vector<bool> argIsArray;
        SourceLocation loc;
    };
    std::vector<DeferredXmodCallSite> deferredXmodCalls_;
    // 泛型 (tB, G3): 调用点推断 (从模板登记表 AST 形参 + 延后点实参类型绑定)
    const GenRegistry* genReg_ = nullptr;
    const IfaceRegistry* ifaceReg_ = nullptr;  // Interface 契约 (tB, B02)
    const ClassChainRegistry* clsreg_ = nullptr;  // 类继承链 (tB, B07b)
    std::vector<GenInstRequest> genericRequests_;
    bool tryBindGenericCall(DeferredXmodCallSite& site, GenInstRequest& reqOut);
    // 若 valueExpr 是 AddressOf 且 typeName 是委托: 解析目标过程、签名校验,
    // 通过则在 AddressOfExpr 上打委托标记并登记 cgen 桩生成需求.
    void bindDelegateAddressOf(const std::string& typeName, Expr& valueExpr, SourceLocation loc);

    // 标记符号为已引用
    void markReferenced(const std::string& name);

    // 检查未引用的符号 (Option Explicit时发出警告)
    void checkUnreferencedSymbols();

    // 注册内置对象、函数和常量
    void registerBuiltins();

    // 生成唯一内部名称
    static std::string makeInternalName(const std::string& prefix, const std::string& name);

    // P14.1.4: 将Optional参数的默认值AST表达式转换为C表达式字符串
    std::string evalOptionalDefault(ASTNode* defaultValue, Vb6Type paramType);
};

} // namespace vb6c3
