#pragma once
// vb6c - Visual Basic 6.0 Compiler
// AST节点体系 - 覆盖VB6全部语法结构
// P1.1 定义

#include "common/diagnostics.hpp"  // SourceLocation
#include "common/types.hpp"        // Vb6Type, CallConv, AccessLevel, ProcKind
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace vb6c3 {

// ============================================================
// 第一节 枚举定义
// ============================================================

// 二元运算符 (与Pratt解析器的14级优先级对应)
enum class BinaryOp : uint8_t {
    // 优先级1: 逻辑或
    Or, Xor,
    // 优先级2: 逻辑与
    And,
    // 优先级4: 逻辑等价
    Eqv, Imp,
    // 优先级5: 比较
    Eq, Neq, Lt, Gt, Le, Ge,
    // 优先级6: 字符串连接
    Concat,       // &
    // 优先级7: 加减
    Add, Sub,
    // 优先级8: 取模
    Mod,
    // 优先级9: 整除
    IntDiv,       // \
    // 优先级10: 乘除
    Mul, Div,
    // 优先级11: 幂 (右结合)
    Pow,          // ^
    // 优先级13: 对象比较
    Like, Is,
};

// 一元运算符
enum class UnaryOp : uint8_t {
    Negate,   // - (一元负)
    Not,      // Not
};

// 字面量类型
enum class LiteralKind : uint8_t {
    Integer, Long, Single, Double, Currency, Decimal,
    String, Date, Boolean,
    Nothing, Empty, Null,
};

// 语句块类型
enum class ExitKind : uint8_t {
    Do, For, Sub, Function, Property,
};

// On Error 处理类型
enum class OnErrorKind : uint8_t {
    GoToLabel,     // On Error GoTo label
    ResumeNext,    // On Error Resume Next
    GoToZero,      // On Error GoTo 0 (关闭错误处理)
};

// Do Loop 变体
enum class DoLoopKind : uint8_t {
    DoWhileLoop,   // Do While ... Loop
    DoUntilLoop,   // Do Until ... Loop
    DoLoopWhile,   // Do ... Loop While
    DoLoopUntil,   // Do ... Loop Until
    DoLoop,        // Do ... Loop (无条件)
};

// Option 语句类型
enum class OptionKind : uint8_t {
    Explicit,      // Option Explicit
    CompareText,   // Option Compare Text
    CompareBinary, // Option Compare Binary
    BaseZero,      // Option Base 0
    BaseOne,       // Option Base 1
    PrivateModule, // Option Private Module
};

// Open 语句访问模式
enum class OpenMode : uint8_t {
    Input, Output, Append, Binary, Random,
};

// Open 语句访问权限
enum class OpenAccess : uint8_t {
    Read, Write, ReadWrite, Default,
};

// Open 语句锁类型
enum class LockType : uint8_t {
    Shared, LockRead, LockWrite, LockReadWrite, Default,
};

// DefType 语句类型 (DefInt, DefStr等)
enum class DefTypeKind : uint8_t {
    Bool, Byte, Int, Lng, Cur, Sng, Dbl, Date, Str, Obj, Var,
};

// ============================================================
// 第二节 节点类型枚举
// ============================================================

enum class ASTNodeKind : uint16_t {
    // --- 模块 ---
    Module,

    // --- 声明 ---
    SubDecl, FunctionDecl, PropertyDecl,
    TypeDecl, TypeMember,
    EnumDecl, EnumMember,
    DeclareDecl, EventDecl,
    ConstDecl, VariableDecl,
    ParameterDecl,

    // --- 语句 ---
    Block,
    AssignmentStmt,
    SetStmt,
    LetStmt,
    IfStmt,
    ElseIfClause,
    ForStmt,
    ForEachStmt,
    DoLoopStmt,
    WhileWendStmt,
    SelectCaseStmt,
    CaseClause,
    WithStmt,
    GoToStmt,
    GoSubStmt,
    ReturnStmt,
    OnErrorStmt,
    OnGoToStmt,
    OnGoSubStmt,
    ExitStmt,
    StopStmt,
    EndStmt,
    CallStmt,
    ReDimStmt,
    EraseStmt,
    LabelStmt,
    RaiseEventStmt,

    // 文件I/O语句
    OpenStmt, CloseStmt, GetStmt, PutStmt,
    InputStmt, PrintStmt, WriteStmt,
    LineInputStmt, WidthStmt,
    SeekStmt, LockStmt, UnlockStmt,
    NameStmt,
    FileCopyStmt, KillStmt, MkDirStmt, RmDirStmt,
    ChDirStmt, ChDriveStmt,

    // 杂项语句
    BeepStmt, DoEventsStmt,
    AttributeStmt,
    OptionStmt,
    ImplementsStmt,
    DefTypeStmt,
    LocalDeclStmt,  // 过程体内的局部声明包装 (Dim/Const/Static/Public/Private)

    // --- 表达式 ---
    BinaryExpr,
    UnaryExpr,
    LiteralExpr,
    IdentifierExpr,
    MemberAccessExpr,
    DictionaryAccessExpr,
    IndexOrCallExpr,
    NewExpr,
    TypeOfExpr,
    AddressOfExpr,
    MeExpr,
    WithMemberExpr,

    // --- 类型引用 ---
    SimpleTypeRef,
    ArrayTypeRef,
    FixedStringTypeRef,
};

// ============================================================
// 第三节 前向声明
// ============================================================

class ASTNode;
class Expr;
class Stmt;
class Decl;

// 具体表达式
class BinaryExpr;
class UnaryExpr;
class LiteralExpr;
class IdentifierExpr;
class MemberAccessExpr;
class DictionaryAccessExpr;
class IndexOrCallExpr;
class NewExpr;
class TypeOfExpr;
class AddressOfExpr;
class MeExpr;
class WithMemberExpr;

// 具体语句
class Block;
class AssignmentStmt;
class SetStmt;
class LetStmt;
class IfStmt;
class ElseIfClause;
class ForStmt;
class ForEachStmt;
class DoLoopStmt;
class WhileWendStmt;
class SelectCaseStmt;
class CaseClause;
class WithStmt;
class GoToStmt;
class GoSubStmt;
class ReturnStmt;
class OnErrorStmt;
class OnGoToStmt;
class OnGoSubStmt;
class ExitStmt;
class StopStmt;
class EndStmt;
class CallStmt;
class ReDimStmt;
class EraseStmt;
class LabelStmt;
class RaiseEventStmt;
class OpenStmt;
class CloseStmt;
class GetStmt;
class PutStmt;
class InputStmt;
class PrintStmt;
class WriteStmt;
class LineInputStmt;
class WidthStmt;
class SeekStmt;
class LockStmt;
class UnlockStmt;
class NameStmt;
class FileCopyStmt;
class KillStmt;
class MkDirStmt;
class RmDirStmt;
class ChDirStmt;
class ChDriveStmt;
class BeepStmt;
class DoEventsStmt;
class AttributeStmt;
class OptionStmt;
class ImplementsStmt;
class DefTypeStmt;
class LocalDeclStmt;

// 具体声明
class SubDecl;
class FunctionDecl;
class PropertyDecl;
class TypeDecl;
class TypeMember;
class EnumDecl;
class EnumMember;
class DeclareDecl;
class EventDecl;
class ConstDecl;
class VariableDecl;
class ParameterDecl;

// 模块
class Module;

// 类型引用
class SimpleTypeRef;
class ArrayTypeRef;
class FixedStringTypeRef;

// ============================================================
// 第四节 智能指针别名
// ============================================================

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;
using DeclPtr = std::unique_ptr<Decl>;
using ExprList = std::vector<ExprPtr>;
using StmtList = std::vector<StmtPtr>;
using DeclList = std::vector<DeclPtr>;

// ============================================================
// 第五节 AST 基类
// ============================================================

class ASTNode {
public:
    ASTNodeKind kind;
    SourceLocation loc;

    ASTNode(ASTNodeKind k, SourceLocation l) : kind(k), loc(l) {}
    virtual ~ASTNode() = default;

    // 禁止拷贝，允许移动
    ASTNode(const ASTNode&) = delete;
    ASTNode& operator=(const ASTNode&) = delete;
    ASTNode(ASTNode&&) = default;
    ASTNode& operator=(ASTNode&&) = default;

    const char* kindName() const;
};

// ============================================================
// 第六节 类型引用节点
// ============================================================

// 简单类型引用: Long, String, MyClass 等
class SimpleTypeRef : public ASTNode {
public:
    std::string name;  // 类型名 (不区分大小写)

    SimpleTypeRef(SourceLocation loc, std::string n)
        : ASTNode(ASTNodeKind::SimpleTypeRef, loc), name(std::move(n)) {}
};

// 数组类型引用: Long(), String(10), Integer(1 To 100)
class ArrayTypeRef : public ASTNode {
public:
    std::unique_ptr<ASTNode> elementType;  // 元素类型 (SimpleTypeRef 或嵌套 ArrayTypeRef)

    // 数组维度: 每维为 (lower, upper) 对; lower 默认由 Option Base 决定
    struct Dimension {
        ExprPtr lower;  // 可为 nullptr (使用 Option Base)
        ExprPtr upper;
    };
    std::vector<Dimension> dimensions;

    ArrayTypeRef(SourceLocation loc, std::unique_ptr<ASTNode> elemType,
                 std::vector<Dimension> dims)
        : ASTNode(ASTNodeKind::ArrayTypeRef, loc),
          elementType(std::move(elemType)), dimensions(std::move(dims)) {}
};

// 定长字符串类型引用: String * N
class FixedStringTypeRef : public ASTNode {
public:
    ExprPtr length;  // N 表达式

    FixedStringTypeRef(SourceLocation loc, ExprPtr len)
        : ASTNode(ASTNodeKind::FixedStringTypeRef, loc), length(std::move(len)) {}
};

using TypeRefPtr = std::unique_ptr<ASTNode>;

// ============================================================
// 第七节 表达式节点
// ============================================================

class Expr : public ASTNode {
public:
    Expr(ASTNodeKind k, SourceLocation loc) : ASTNode(k, loc) {}
};

// 二元表达式: a + b, x And y 等
class BinaryExpr : public Expr {
public:
    BinaryOp op;
    ExprPtr left;
    ExprPtr right;

    BinaryExpr(SourceLocation loc, BinaryOp op, ExprPtr l, ExprPtr r)
        : Expr(ASTNodeKind::BinaryExpr, loc),
          op(op), left(std::move(l)), right(std::move(r)) {}
};

// 一元表达式: -x, Not y
class UnaryExpr : public Expr {
public:
    UnaryOp op;
    ExprPtr operand;

    UnaryExpr(SourceLocation loc, UnaryOp op, ExprPtr operand)
        : Expr(ASTNodeKind::UnaryExpr, loc),
          op(op), operand(std::move(operand)) {}
};

// 字面量: 42, 3.14#, "hello", #1/1/2026#, True, Nothing 等
class LiteralExpr : public Expr {
public:
    LiteralKind literalKind;
    std::string rawText;  // 原始文本 (保留用户写法)
    union {
        int32_t intValue = 0;
        int64_t longValue;
        float floatValue;
        double doubleValue;
        bool boolValue;
    };

    LiteralExpr(SourceLocation loc, LiteralKind k, std::string raw)
        : Expr(ASTNodeKind::LiteralExpr, loc),
          literalKind(k), rawText(std::move(raw)), intValue(0) {}
};

// 标识符: x, MyVar, [带空格的名称]
class IdentifierExpr : public Expr {
public:
    std::string name;        // 标识符名称 (保留原始大小写)
    bool bracketed = false;  // 是否使用方括号 [name]

    IdentifierExpr(SourceLocation loc, std::string n, bool br = false)
        : Expr(ASTNodeKind::IdentifierExpr, loc),
          name(std::move(n)), bracketed(br) {}
};

// 成员访问: obj.member
class MemberAccessExpr : public Expr {
public:
    ExprPtr object;
    std::string memberName;

    MemberAccessExpr(SourceLocation loc, ExprPtr obj, std::string member)
        : Expr(ASTNodeKind::MemberAccessExpr, loc),
          object(std::move(obj)), memberName(std::move(member)) {}
};

// 字典访问: obj!key (等价于 obj.Fields("key"))
class DictionaryAccessExpr : public Expr {
public:
    ExprPtr object;
    std::string key;

    DictionaryAccessExpr(SourceLocation loc, ExprPtr obj, std::string k)
        : Expr(ASTNodeKind::DictionaryAccessExpr, loc),
          object(std::move(obj)), key(std::move(k)) {}
};

// 命名参数
struct NamedArg {
    std::string name;   // 参数名
    ExprPtr value;      // 参数值
};

// 索引/调用: arr(i), func(x, y), call with named args
// VB6 在解析阶段不区分数组索引和函数调用, 统一为此节点
class IndexOrCallExpr : public Expr {
public:
    ExprPtr callee;                    // 被调用者 (IdentifierExpr 或 MemberAccessExpr)
    std::vector<ExprPtr> positional;   // 位置参数
    std::vector<NamedArg> named;       // 命名参数

    IndexOrCallExpr(SourceLocation loc, ExprPtr callee)
        : Expr(ASTNodeKind::IndexOrCallExpr, loc),
          callee(std::move(callee)) {}
};

// New 表达式: New ClassName
class NewExpr : public Expr {
public:
    std::string className;

    NewExpr(SourceLocation loc, std::string cls)
        : Expr(ASTNodeKind::NewExpr, loc), className(std::move(cls)) {}
};

// TypeOf 表达式: TypeOf obj Is ClassName
class TypeOfExpr : public Expr {
public:
    ExprPtr object;
    std::string typeName;

    TypeOfExpr(SourceLocation loc, ExprPtr obj, std::string typeName)
        : Expr(ASTNodeKind::TypeOfExpr, loc),
          object(std::move(obj)), typeName(std::move(typeName)) {}
};

// AddressOf 表达式: AddressOf funcname (用于回调)
class AddressOfExpr : public Expr {
public:
    std::string funcName;

    AddressOfExpr(SourceLocation loc, std::string fn)
        : Expr(ASTNodeKind::AddressOfExpr, loc), funcName(std::move(fn)) {}
};

// Me 表达式: Me (当前对象实例)
class MeExpr : public Expr {
public:
    MeExpr(SourceLocation loc)
        : Expr(ASTNodeKind::MeExpr, loc) {}
};

// With 块中的隐式成员访问: .Property (等价于 WithVar.Property)
class WithMemberExpr : public Expr {
public:
    std::string memberName;

    WithMemberExpr(SourceLocation loc, std::string member)
        : Expr(ASTNodeKind::WithMemberExpr, loc), memberName(std::move(member)) {}
};

// ============================================================
// 第八节 语句节点
// ============================================================

class Stmt : public ASTNode {
public:
    Stmt(ASTNodeKind k, SourceLocation loc) : ASTNode(k, loc) {}
};

// 语句块 (过程体、If体、For体等)
class Block : public Stmt {
public:
    StmtList stmts;

    Block(SourceLocation loc, StmtList s)
        : Stmt(ASTNodeKind::Block, loc), stmts(std::move(s)) {}
};

// 赋值语句: x = expr (隐式Let)
class AssignmentStmt : public Stmt {
public:
    ExprPtr target;   // 左值
    ExprPtr value;    // 右值

    AssignmentStmt(SourceLocation loc, ExprPtr t, ExprPtr v)
        : Stmt(ASTNodeKind::AssignmentStmt, loc),
          target(std::move(t)), value(std::move(v)) {}
};

// Set 语句: Set obj = expr (对象赋值)
class SetStmt : public Stmt {
public:
    ExprPtr target;
    ExprPtr value;

    SetStmt(SourceLocation loc, ExprPtr t, ExprPtr v)
        : Stmt(ASTNodeKind::SetStmt, loc),
          target(std::move(t)), value(std::move(v)) {}
};

// Let 语句: Let x = expr (显式Let, 极少使用)
class LetStmt : public Stmt {
public:
    ExprPtr target;
    ExprPtr value;

    LetStmt(SourceLocation loc, ExprPtr t, ExprPtr v)
        : Stmt(ASTNodeKind::LetStmt, loc),
          target(std::move(t)), value(std::move(v)) {}
};

// ElseIf 子句
class ElseIfClause : public Stmt {
public:
    ExprPtr condition;
    StmtList body;

    ElseIfClause(SourceLocation loc, ExprPtr cond, StmtList body)
        : Stmt(ASTNodeKind::ElseIfClause, loc),
          condition(std::move(cond)), body(std::move(body)) {}
};

// If 语句: If...Then...ElseIf...Else...End If
class IfStmt : public Stmt {
public:
    ExprPtr condition;
    StmtList thenBody;
    std::vector<std::unique_ptr<ElseIfClause>> elseIfs;
    StmtList elseBody;
    bool singleLine = false;  // 单行 If...Then...Else

    IfStmt(SourceLocation loc, ExprPtr cond, StmtList then,
           std::vector<std::unique_ptr<ElseIfClause>> elseifs,
           StmtList elseBody, bool single)
        : Stmt(ASTNodeKind::IfStmt, loc),
          condition(std::move(cond)), thenBody(std::move(then)),
          elseIfs(std::move(elseifs)), elseBody(std::move(elseBody)),
          singleLine(single) {}
};

// For 语句: For i = 1 To 10 Step 2
class ForStmt : public Stmt {
public:
    std::string varName;    // 循环变量名
    ExprPtr start;          // 起始值
    ExprPtr end;            // 终止值
    ExprPtr step;           // 步长 (可为nullptr, 默认1)
    StmtList body;

    ForStmt(SourceLocation loc, std::string var, ExprPtr s, ExprPtr e,
            ExprPtr step, StmtList body)
        : Stmt(ASTNodeKind::ForStmt, loc),
          varName(std::move(var)), start(std::move(s)), end(std::move(e)),
          step(std::move(step)), body(std::move(body)) {}
};

// For Each 语句: For Each item In collection
class ForEachStmt : public Stmt {
public:
    std::string varName;
    ExprPtr collection;
    StmtList body;

    ForEachStmt(SourceLocation loc, std::string var, ExprPtr coll, StmtList body)
        : Stmt(ASTNodeKind::ForEachStmt, loc),
          varName(std::move(var)), collection(std::move(coll)), body(std::move(body)) {}
};

// Do Loop 语句 (5种变体)
class DoLoopStmt : public Stmt {
public:
    DoLoopKind loopKind;
    ExprPtr condition;  // 可为nullptr (Do...Loop 无条件变体)
    StmtList body;

    DoLoopStmt(SourceLocation loc, DoLoopKind kind, ExprPtr cond, StmtList body)
        : Stmt(ASTNodeKind::DoLoopStmt, loc),
          loopKind(kind), condition(std::move(cond)), body(std::move(body)) {}
};

// While...Wend 语句 (VB6旧式, 等价于Do While...Loop)
class WhileWendStmt : public Stmt {
public:
    ExprPtr condition;
    StmtList body;

    WhileWendStmt(SourceLocation loc, ExprPtr cond, StmtList body)
        : Stmt(ASTNodeKind::WhileWendStmt, loc),
          condition(std::move(cond)), body(std::move(body)) {}
};

// Case 子句: Case value1, value2, Case Is > 0, Case 1 To 10
class CaseClause : public Stmt {
public:
    // Case 值: 可以是单个值、Is比较、范围或多个值
    struct CaseValue {
        ExprPtr value;       // 单个值或范围的起始值
        ExprPtr toValue;     // To 范围的终止值 (可为nullptr)
        bool isIsClause = false;  // 是否是 Case Is > 0 形式
    };
    std::vector<CaseValue> values;  // Case val1, val2, val3
    StmtList body;

    CaseClause(SourceLocation loc, std::vector<CaseValue> vals, StmtList body)
        : Stmt(ASTNodeKind::CaseClause, loc),
          values(std::move(vals)), body(std::move(body)) {}
};

// Select Case 语句
class SelectCaseStmt : public Stmt {
public:
    ExprPtr testExpr;
    std::vector<std::unique_ptr<CaseClause>> cases;
    StmtList elseCase;  // Case Else

    SelectCaseStmt(SourceLocation loc, ExprPtr test,
                   std::vector<std::unique_ptr<CaseClause>> cases,
                   StmtList elseBody)
        : Stmt(ASTNodeKind::SelectCaseStmt, loc),
          testExpr(std::move(test)), cases(std::move(cases)),
          elseCase(std::move(elseBody)) {}
};

// With 语句: With obj ... End With
class WithStmt : public Stmt {
public:
    ExprPtr object;
    StmtList body;

    WithStmt(SourceLocation loc, ExprPtr obj, StmtList body)
        : Stmt(ASTNodeKind::WithStmt, loc),
          object(std::move(obj)), body(std::move(body)) {}
};

// GoTo 语句
class GoToStmt : public Stmt {
public:
    std::string labelName;

    GoToStmt(SourceLocation loc, std::string label)
        : Stmt(ASTNodeKind::GoToStmt, loc), labelName(std::move(label)) {}
};

// GoSub 语句 (VB6遗留, 类似GoTo但可Return)
class GoSubStmt : public Stmt {
public:
    std::string labelName;

    GoSubStmt(SourceLocation loc, std::string label)
        : Stmt(ASTNodeKind::GoSubStmt, loc), labelName(std::move(label)) {}
};

// Return 语句 (从GoSub返回)
class ReturnStmt : public Stmt {
public:
    ReturnStmt(SourceLocation loc)
        : Stmt(ASTNodeKind::ReturnStmt, loc) {}
};

// On Error 语句
class OnErrorStmt : public Stmt {
public:
    OnErrorKind errorKind;
    std::string labelName;  // GoTo label时使用

    OnErrorStmt(SourceLocation loc, OnErrorKind kind, std::string label = "")
        : Stmt(ASTNodeKind::OnErrorStmt, loc),
          errorKind(kind), labelName(std::move(label)) {}
};

// On...GoTo: On x GoTo label1, label2, label3
class OnGoToStmt : public Stmt {
public:
    ExprPtr index;
    std::vector<std::string> labels;

    OnGoToStmt(SourceLocation loc, ExprPtr idx, std::vector<std::string> labels)
        : Stmt(ASTNodeKind::OnGoToStmt, loc),
          index(std::move(idx)), labels(std::move(labels)) {}
};

// On...GoSub: On x GoSub label1, label2, label3
class OnGoSubStmt : public Stmt {
public:
    ExprPtr index;
    std::vector<std::string> labels;

    OnGoSubStmt(SourceLocation loc, ExprPtr idx, std::vector<std::string> labels)
        : Stmt(ASTNodeKind::OnGoSubStmt, loc),
          index(std::move(idx)), labels(std::move(labels)) {}
};

// Exit 语句: Exit Sub/Function/Property/Do/For
class ExitStmt : public Stmt {
public:
    ExitKind exitKind;

    ExitStmt(SourceLocation loc, ExitKind k)
        : Stmt(ASTNodeKind::ExitStmt, loc), exitKind(k) {}
};

// Stop 语句 (进入断点中断)
class StopStmt : public Stmt {
public:
    StopStmt(SourceLocation loc)
        : Stmt(ASTNodeKind::StopStmt, loc) {}
};

// End 语句 (终止程序)
class EndStmt : public Stmt {
public:
    EndStmt(SourceLocation loc)
        : Stmt(ASTNodeKind::EndStmt, loc) {}
};

// Call 语句: Call proc(args) 或 proc args
class CallStmt : public Stmt {
public:
    ExprPtr callee;  // 通常是 IdentifierExpr 或 MemberAccessExpr

    CallStmt(SourceLocation loc, ExprPtr callee)
        : Stmt(ASTNodeKind::CallStmt, loc), callee(std::move(callee)) {}
};

// ReDim 语句: ReDim arr(10) / ReDim Preserve arr(20)
class ReDimStmt : public Stmt {
public:
    bool preserve = false;
    std::string varName;
    // ReDim 每维边界
    struct Dimension {
        ExprPtr lower;  // 可为nullptr
        ExprPtr upper;
    };
    std::vector<Dimension> dimensions;
    TypeRefPtr asType;  // As Type (可为nullptr)

    ReDimStmt(SourceLocation loc, bool pres, std::string var,
              std::vector<Dimension> dims, TypeRefPtr type)
        : Stmt(ASTNodeKind::ReDimStmt, loc),
          preserve(pres), varName(std::move(var)),
          dimensions(std::move(dims)), asType(std::move(type)) {}
};

// Erase 语句: Erase arr1, arr2
class EraseStmt : public Stmt {
public:
    std::vector<std::string> varNames;

    EraseStmt(SourceLocation loc, std::vector<std::string> names)
        : Stmt(ASTNodeKind::EraseStmt, loc), varNames(std::move(names)) {}
};

// 行标签: LabelName:
class LabelStmt : public Stmt {
public:
    std::string labelName;

    LabelStmt(SourceLocation loc, std::string name)
        : Stmt(ASTNodeKind::LabelStmt, loc), labelName(std::move(name)) {}
};

// RaiseEvent 语句: RaiseEvent eventName(arg1, arg2)
class RaiseEventStmt : public Stmt {
public:
    std::string eventName;
    std::vector<ExprPtr> args;

    RaiseEventStmt(SourceLocation loc, std::string name, std::vector<ExprPtr> args)
        : Stmt(ASTNodeKind::RaiseEventStmt, loc),
          eventName(std::move(name)), args(std::move(args)) {}
};

// ---- 文件I/O语句 ----

// Open 语句: Open pathname For mode [Access access] [lock] As [#]filenumber [Len=reclength]
class OpenStmt : public Stmt {
public:
    ExprPtr pathName;
    OpenMode mode;
    OpenAccess access;
    LockType lock;
    ExprPtr fileNumber;
    ExprPtr recordLength;  // Len=reclength (可为nullptr)

    OpenStmt(SourceLocation loc, ExprPtr path, OpenMode m, OpenAccess a,
             LockType l, ExprPtr fn, ExprPtr recLen)
        : Stmt(ASTNodeKind::OpenStmt, loc),
          pathName(std::move(path)), mode(m), access(a), lock(l),
          fileNumber(std::move(fn)), recordLength(std::move(recLen)) {}
};

// Close 语句: Close [#]filenumber, [#]filenumber2, ...
class CloseStmt : public Stmt {
public:
    std::vector<ExprPtr> fileNumbers;  // 空列表=Close All

    CloseStmt(SourceLocation loc, std::vector<ExprPtr> fns)
        : Stmt(ASTNodeKind::CloseStmt, loc), fileNumbers(std::move(fns)) {}
};

// Get 语句: Get [#]filenumber, [recnumber], varname
class GetStmt : public Stmt {
public:
    ExprPtr fileNumber;
    ExprPtr recordNumber;  // 可为nullptr (顺序读)
    ExprPtr varName;

    GetStmt(SourceLocation loc, ExprPtr fn, ExprPtr rec, ExprPtr var)
        : Stmt(ASTNodeKind::GetStmt, loc),
          fileNumber(std::move(fn)), recordNumber(std::move(rec)),
          varName(std::move(var)) {}
};

// Put 语句: Put [#]filenumber, [recnumber], varname
class PutStmt : public Stmt {
public:
    ExprPtr fileNumber;
    ExprPtr recordNumber;
    ExprPtr varName;

    PutStmt(SourceLocation loc, ExprPtr fn, ExprPtr rec, ExprPtr var)
        : Stmt(ASTNodeKind::PutStmt, loc),
          fileNumber(std::move(fn)), recordNumber(std::move(rec)),
          varName(std::move(var)) {}
};

// Input 语句: Input #filenumber, varlist
class InputStmt : public Stmt {
public:
    ExprPtr fileNumber;
    std::vector<ExprPtr> varList;

    InputStmt(SourceLocation loc, ExprPtr fn, std::vector<ExprPtr> vars)
        : Stmt(ASTNodeKind::InputStmt, loc),
          fileNumber(std::move(fn)), varList(std::move(vars)) {}
};

// Print 语句: Print #filenumber, [outputlist]
class PrintStmt : public Stmt {
public:
    ExprPtr fileNumber;
    std::vector<ExprPtr> outputList;

    PrintStmt(SourceLocation loc, ExprPtr fn, std::vector<ExprPtr> output)
        : Stmt(ASTNodeKind::PrintStmt, loc),
          fileNumber(std::move(fn)), outputList(std::move(output)) {}
};

// Write 语句: Write #filenumber, [outputlist]
class WriteStmt : public Stmt {
public:
    ExprPtr fileNumber;
    std::vector<ExprPtr> outputList;

    WriteStmt(SourceLocation loc, ExprPtr fn, std::vector<ExprPtr> output)
        : Stmt(ASTNodeKind::WriteStmt, loc),
          fileNumber(std::move(fn)), outputList(std::move(output)) {}
};

// Line Input 语句: Line Input #filenumber, varname
class LineInputStmt : public Stmt {
public:
    ExprPtr fileNumber;
    ExprPtr varName;

    LineInputStmt(SourceLocation loc, ExprPtr fn, ExprPtr var)
        : Stmt(ASTNodeKind::LineInputStmt, loc),
          fileNumber(std::move(fn)), varName(std::move(var)) {}
};

// Width 语句: Width #filenumber, width
class WidthStmt : public Stmt {
public:
    ExprPtr fileNumber;
    ExprPtr width;

    WidthStmt(SourceLocation loc, ExprPtr fn, ExprPtr w)
        : Stmt(ASTNodeKind::WidthStmt, loc),
          fileNumber(std::move(fn)), width(std::move(w)) {}
};

// Seek 语句: Seek [#]filenumber, position
class SeekStmt : public Stmt {
public:
    ExprPtr fileNumber;
    ExprPtr position;

    SeekStmt(SourceLocation loc, ExprPtr fn, ExprPtr pos)
        : Stmt(ASTNodeKind::SeekStmt, loc),
          fileNumber(std::move(fn)), position(std::move(pos)) {}
};

// Lock/Unlock 语句
class LockStmt : public Stmt {
public:
    ExprPtr fileNumber;
    ExprPtr start;    // 可为nullptr
    ExprPtr end;      // 可为nullptr

    LockStmt(SourceLocation loc, ExprPtr fn, ExprPtr s, ExprPtr e)
        : Stmt(ASTNodeKind::LockStmt, loc),
          fileNumber(std::move(fn)), start(std::move(s)), end(std::move(e)) {}
};

class UnlockStmt : public Stmt {
public:
    ExprPtr fileNumber;
    ExprPtr start;
    ExprPtr end;

    UnlockStmt(SourceLocation loc, ExprPtr fn, ExprPtr s, ExprPtr e)
        : Stmt(ASTNodeKind::UnlockStmt, loc),
          fileNumber(std::move(fn)), start(std::move(s)), end(std::move(e)) {}
};

// Name 语句: Name oldpathname As newpathname
class NameStmt : public Stmt {
public:
    ExprPtr oldPath;
    ExprPtr newPath;

    NameStmt(SourceLocation loc, ExprPtr oldP, ExprPtr newP)
        : Stmt(ASTNodeKind::NameStmt, loc),
          oldPath(std::move(oldP)), newPath(std::move(newP)) {}
};

// 简单文件系统语句 (只有路径参数)
class FileCopyStmt : public Stmt {
public:
    ExprPtr source;
    ExprPtr destination;

    FileCopyStmt(SourceLocation loc, ExprPtr src, ExprPtr dest)
        : Stmt(ASTNodeKind::FileCopyStmt, loc),
          source(std::move(src)), destination(std::move(dest)) {}
};

class KillStmt : public Stmt {
public:
    ExprPtr pathName;

    KillStmt(SourceLocation loc, ExprPtr path)
        : Stmt(ASTNodeKind::KillStmt, loc), pathName(std::move(path)) {}
};

class MkDirStmt : public Stmt {
public:
    ExprPtr pathName;

    MkDirStmt(SourceLocation loc, ExprPtr path)
        : Stmt(ASTNodeKind::MkDirStmt, loc), pathName(std::move(path)) {}
};

class RmDirStmt : public Stmt {
public:
    ExprPtr pathName;

    RmDirStmt(SourceLocation loc, ExprPtr path)
        : Stmt(ASTNodeKind::RmDirStmt, loc), pathName(std::move(path)) {}
};

class ChDirStmt : public Stmt {
public:
    ExprPtr pathName;

    ChDirStmt(SourceLocation loc, ExprPtr path)
        : Stmt(ASTNodeKind::ChDirStmt, loc), pathName(std::move(path)) {}
};

class ChDriveStmt : public Stmt {
public:
    ExprPtr drive;

    ChDriveStmt(SourceLocation loc, ExprPtr d)
        : Stmt(ASTNodeKind::ChDriveStmt, loc), drive(std::move(d)) {}
};

// ---- 杂项语句 ----

class BeepStmt : public Stmt {
public:
    BeepStmt(SourceLocation loc) : Stmt(ASTNodeKind::BeepStmt, loc) {}
};

class DoEventsStmt : public Stmt {
public:
    DoEventsStmt(SourceLocation loc) : Stmt(ASTNodeKind::DoEventsStmt, loc) {}
};

// Attribute 语句: Attribute VB_Name = "ModuleName"
class AttributeStmt : public Stmt {
public:
    std::string attrName;
    ExprPtr value;

    AttributeStmt(SourceLocation loc, std::string name, ExprPtr val)
        : Stmt(ASTNodeKind::AttributeStmt, loc),
          attrName(std::move(name)), value(std::move(val)) {}
};

// Option 语句
class OptionStmt : public Stmt {
public:
    OptionKind optionKind;

    OptionStmt(SourceLocation loc, OptionKind k)
        : Stmt(ASTNodeKind::OptionStmt, loc), optionKind(k) {}
};

// Implements 语句: Implements InterfaceName
class ImplementsStmt : public Stmt {
public:
    std::string interfaceName;

    ImplementsStmt(SourceLocation loc, std::string name)
        : Stmt(ASTNodeKind::ImplementsStmt, loc), interfaceName(std::move(name)) {}
};

// DefType 语句: DefInt A-C, DefStr D-F
class DefTypeStmt : public Stmt {
public:
    DefTypeKind defKind;
    struct LetterRange {
        char from;
        char to;  // 相同时表示单个字母
    };
    std::vector<LetterRange> ranges;

    DefTypeStmt(SourceLocation loc, DefTypeKind k, std::vector<LetterRange> r)
        : Stmt(ASTNodeKind::DefTypeStmt, loc),
          defKind(k), ranges(std::move(r)) {}
};

// 过程体内的局部声明包装: Dim x As Long, Const k = 0, Static s As String
// VB6 允许声明出现在语句位置, 但 AST 中 Decl 不继承 Stmt
// 使用此包装节点将 Decl 存入 StmtList
class LocalDeclStmt : public Stmt {
public:
    DeclPtr decl;  // 被包装的声明

    LocalDeclStmt(SourceLocation loc, DeclPtr d)
        : Stmt(ASTNodeKind::LocalDeclStmt, loc), decl(std::move(d)) {}
};

// ============================================================
// 第九节 声明节点
// ============================================================

class Decl : public ASTNode {
public:
    Decl(ASTNodeKind k, SourceLocation loc) : ASTNode(k, loc) {}
};

// 参数声明: [Optional] [ByVal|ByRef] varName [As Type] [= default]
class ParameterDecl : public Decl {
public:
    std::string name;
    bool isOptional = false;
    bool isByVal = false;
    bool isParamArray = false;
    TypeRefPtr asType;      // As Type (可为nullptr = Variant)
    ExprPtr defaultValue;   // Optional参数的默认值 (可为nullptr)

    ParameterDecl(SourceLocation loc, std::string n, bool opt, bool byval,
                  bool paramArray, TypeRefPtr type, ExprPtr defVal)
        : Decl(ASTNodeKind::ParameterDecl, loc),
          name(std::move(n)), isOptional(opt), isByVal(byval),
          isParamArray(paramArray), asType(std::move(type)),
          defaultValue(std::move(defVal)) {}
};

// Sub 声明: [Public|Private] Sub name(params) ... End Sub
class SubDecl : public Decl {
public:
    AccessLevel access;
    std::string name;
    std::vector<std::unique_ptr<ParameterDecl>> params;
    StmtList body;
    bool isStatic = false;  // Static Sub

    SubDecl(SourceLocation loc, AccessLevel acc, std::string n,
            std::vector<std::unique_ptr<ParameterDecl>> p, StmtList b,
            bool isStatic = false)
        : Decl(ASTNodeKind::SubDecl, loc),
          access(acc), name(std::move(n)), params(std::move(p)),
          body(std::move(b)), isStatic(isStatic) {}
};

// Function 声明: [Public|Private] Function name(params) As Type ... End Function
class FunctionDecl : public Decl {
public:
    AccessLevel access;
    std::string name;
    std::vector<std::unique_ptr<ParameterDecl>> params;
    TypeRefPtr returnType;  // As Type (可为nullptr = Variant)
    StmtList body;
    bool isStatic = false;

    FunctionDecl(SourceLocation loc, AccessLevel acc, std::string n,
                 std::vector<std::unique_ptr<ParameterDecl>> p,
                 TypeRefPtr ret, StmtList b, bool isStatic = false)
        : Decl(ASTNodeKind::FunctionDecl, loc),
          access(acc), name(std::move(n)), params(std::move(p)),
          returnType(std::move(ret)), body(std::move(b)), isStatic(isStatic) {}
};

// Property 声明: [Public|Private] Property Get/Let/Set name(params) [As Type] ... End Property
class PropertyDecl : public Decl {
public:
    AccessLevel access;
    ProcKind propKind;      // Get/Let/Set
    std::string name;
    std::vector<std::unique_ptr<ParameterDecl>> params;
    TypeRefPtr returnType;  // Property Get 返回类型 (可为nullptr)
    StmtList body;
    bool isDefault = false;  // 是否为默认属性

    PropertyDecl(SourceLocation loc, AccessLevel acc, ProcKind kind,
                 std::string n, std::vector<std::unique_ptr<ParameterDecl>> p,
                 TypeRefPtr ret, StmtList b)
        : Decl(ASTNodeKind::PropertyDecl, loc),
          access(acc), propKind(kind), name(std::move(n)),
          params(std::move(p)), returnType(std::move(ret)),
          body(std::move(b)) {}
};

// Type 成员: memberName As Type
class TypeMember : public Decl {
public:
    std::string name;
    TypeRefPtr type;
    ExprPtr arraySize;  // 定长数组的上界 (可为nullptr)

    TypeMember(SourceLocation loc, std::string n, TypeRefPtr t, ExprPtr arrSize)
        : Decl(ASTNodeKind::TypeMember, loc),
          name(std::move(n)), type(std::move(t)), arraySize(std::move(arrSize)) {}
};

// Type 声明 (用户自定义类型/UDT): [Public|Private] Type name ... End Type
class TypeDecl : public Decl {
public:
    AccessLevel access;
    std::string name;
    std::vector<std::unique_ptr<TypeMember>> members;

    TypeDecl(SourceLocation loc, AccessLevel acc, std::string n,
             std::vector<std::unique_ptr<TypeMember>> m)
        : Decl(ASTNodeKind::TypeDecl, loc),
          access(acc), name(std::move(n)), members(std::move(m)) {}
};

// Enum 成员: MemberName [= value]
class EnumMember : public Decl {
public:
    std::string name;
    ExprPtr value;  // 可为nullptr (自动递增)

    EnumMember(SourceLocation loc, std::string n, ExprPtr v)
        : Decl(ASTNodeKind::EnumMember, loc),
          name(std::move(n)), value(std::move(v)) {}
};

// Enum 声明: [Public|Private] Enum name ... End Enum
class EnumDecl : public Decl {
public:
    AccessLevel access;
    std::string name;
    std::vector<std::unique_ptr<EnumMember>> members;

    EnumDecl(SourceLocation loc, AccessLevel acc, std::string n,
             std::vector<std::unique_ptr<EnumMember>> m)
        : Decl(ASTNodeKind::EnumDecl, loc),
          access(acc), name(std::move(n)), members(std::move(m)) {}
};

// Declare 声明: Declare [PtrSafe] Sub/Function name Lib "lib" [Alias "alias"] (params)
class DeclareDecl : public Decl {
public:
    AccessLevel access;
    ProcKind procKind;      // Sub 或 Function
    std::string name;
    std::string libName;
    std::string aliasName;  // 可为空
    CallConv callingConv;
    bool isPtrSafe = false;  // PtrSafe关键字 (64位兼容)
    std::vector<std::unique_ptr<ParameterDecl>> params;
    TypeRefPtr returnType;  // Function返回类型 (可为nullptr)

    DeclareDecl(SourceLocation loc, AccessLevel acc, ProcKind kind,
                std::string n, std::string lib, std::string alias,
                CallConv conv, bool ptrSafe,
                std::vector<std::unique_ptr<ParameterDecl>> p,
                TypeRefPtr ret)
        : Decl(ASTNodeKind::DeclareDecl, loc),
          access(acc), procKind(kind), name(std::move(n)),
          libName(std::move(lib)), aliasName(std::move(alias)),
          callingConv(conv), isPtrSafe(ptrSafe),
          params(std::move(p)), returnType(std::move(ret)) {}
};

// Event 声明: [Public] Event name(params)
class EventDecl : public Decl {
public:
    AccessLevel access;
    std::string name;
    std::vector<std::unique_ptr<ParameterDecl>> params;

    EventDecl(SourceLocation loc, AccessLevel acc, std::string n,
              std::vector<std::unique_ptr<ParameterDecl>> p)
        : Decl(ASTNodeKind::EventDecl, loc),
          access(acc), name(std::move(n)), params(std::move(p)) {}
};

// Const 声明: [Public|Private] Const name As Type = value
class ConstDecl : public Decl {
public:
    AccessLevel access;
    std::string name;
    TypeRefPtr asType;   // As Type (可为nullptr, 由值推导)
    ExprPtr value;

    ConstDecl(SourceLocation loc, AccessLevel acc, std::string n,
              TypeRefPtr type, ExprPtr val)
        : Decl(ASTNodeKind::ConstDecl, loc),
          access(acc), name(std::move(n)),
          asType(std::move(type)), value(std::move(val)) {}
};

// Variable 声明: Dim/Public/Private/Static name [As Type] [= value]
// 也支持: Dim name(bounds) As Type (数组声明)
class VariableDecl : public Decl {
public:
    AccessLevel access;
    std::string name;
    bool isWithEvents = false;
    bool isStatic = false;
    bool isNew = false;         // Dim x As New ClassName
    TypeRefPtr asType;          // As Type (可为nullptr = Variant)
    ExprPtr initializer;        // = value (可为nullptr)

    // 数组边界 (Dim a(1 To 10, 1 To 20) As Long)
    struct Dimension {
        ExprPtr lower;  // 可为nullptr (Option Base决定)
        ExprPtr upper;  // 可为nullptr (ReDim时留空)
    };
    std::vector<Dimension> dimensions;  // 非空=固定大小数组声明
    bool isDynamicArray = false;         // Dim arr() 动态数组 (空括号)

    VariableDecl(SourceLocation loc, AccessLevel acc, std::string n,
                 bool withEvents, bool isStatic, bool isNew,
                 TypeRefPtr type, ExprPtr init,
                 std::vector<Dimension> dims, bool isDynArr = false)
        : Decl(ASTNodeKind::VariableDecl, loc),
          access(acc), name(std::move(n)), isWithEvents(withEvents),
          isStatic(isStatic), isNew(isNew), asType(std::move(type)),
          initializer(std::move(init)), dimensions(std::move(dims)),
          isDynamicArray(isDynArr) {}
};

// ============================================================
// 第十节 模块节点
// ============================================================

// VBInstancing 已移至 common/types.hpp

// Module: VB6编译单元 (.bas/.cls/.frm 的代码部分)
class Module : public ASTNode {
public:
    std::string filename;           // 源文件路径
    std::string moduleName;         // 模块名 (通常来自Attribute VB_Name)

    // 模块类别
    bool isClassModule = false;     // true = .cls类模块, false = .bas标准模块/.frm窗体模块
    VBInstancing instancing = VBInstancing::Private;  // 类Instancing属性 (仅类模块)

    // Option 语句
    std::vector<std::unique_ptr<OptionStmt>> options;

    // Implements 语句
    std::vector<std::unique_ptr<ImplementsStmt>> implements;

    // DefType 语句
    std::vector<std::unique_ptr<DefTypeStmt>> defTypes;

    // 声明 (按源码顺序)
    DeclList declarations;

    // Attribute 语句
    std::vector<std::unique_ptr<AttributeStmt>> attributes;

    Module(SourceLocation loc, std::string fname)
        : ASTNode(ASTNodeKind::Module, loc), filename(std::move(fname)) {}
};

// ============================================================
// 第十一节 工具函数
// ============================================================

// 判断节点是否为表达式
inline bool isExpr(ASTNodeKind k) {
    return k >= ASTNodeKind::BinaryExpr && k <= ASTNodeKind::WithMemberExpr;
}

// 判断节点是否为语句
inline bool isStmt(ASTNodeKind k) {
    return k >= ASTNodeKind::Block && k <= ASTNodeKind::LocalDeclStmt;
}

// 判断节点是否为声明
inline bool isDecl(ASTNodeKind k) {
    return k >= ASTNodeKind::SubDecl && k <= ASTNodeKind::ParameterDecl;
}

// BinaryOp 转字符串
const char* binaryOpToString(BinaryOp op);

// UnaryOp 转字符串
const char* unaryOpToString(UnaryOp op);

// LiteralKind 转字符串
const char* literalKindToString(LiteralKind k);

// ExitKind 转字符串
const char* exitKindToString(ExitKind k);

// ProcKind 转字符串 (复用types.hpp中的ProcKind)
const char* procKindToString(ProcKind k);

} // namespace vb6c3
