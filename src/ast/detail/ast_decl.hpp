#pragma once
// ast_decl.hpp - 第九节 声明节点 + 第十节 模块节点
// 由 src/ast/ast.hpp 拆出（2026-09-17），内容与原文件对应区间逐字节相同。

#include "ast/detail/ast_stmt_io.hpp"

namespace vb6c3 {

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

// 成员级 Implements 尾子句 (tB 扩展; ai/022 D5, 批次 B02b):
//   Private Sub OpenFile(s As String) Implements IStorage.Open, IStream.Read
// 一个成员可同时绑定多个接口槽 (逗号列表), ifaceName/memberName 均留原样大小写,
// 由语义层按小写键解到具体槽. 只有 Sub/Function/Property 三种节点持有该列表.
struct ImplementsClause {
    std::string ifaceName;
    std::string memberName;
    SourceLocation loc;
};

// Sub 声明: [Public|Private] Sub name(params) ... End Sub
class SubDecl : public Decl {
public:
    AccessLevel access;
    std::string name;
    std::vector<std::unique_ptr<ParameterDecl>> params;
    StmtList body;
    bool isStatic = false;  // Static Sub
    // 泛型 (tB 扩展): 非空 = 泛型模板 (声明位 (Of T[,U]) 的类型参数名表).
    // 模板声明不进符号表, 由泛型器 (driver_generics) 按使用点特化克隆.
    std::vector<std::string> typeParams;
    std::vector<ImplementsClause> implementsClauses;  // B02b, 空 = 无显式绑定

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
    // 泛型 (tB 扩展): 见 SubDecl::typeParams 注释
    std::vector<std::string> typeParams;
    std::vector<ImplementsClause> implementsClauses;  // B02b, 见 SubDecl

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
    // 泛型 (tB 扩展): 见 SubDecl::typeParams
    std::vector<std::string> typeParams;
    std::vector<ImplementsClause> implementsClauses;  // B02b, 见 SubDecl

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
    // Fix 037: 动态数组成员标记 (`memberName() As Type`). arraySize 与 isArrayDynamic
    // 互斥: 定长数组 arraySize!=nullptr 且 isArrayDynamic=false; 动态数组 arraySize=nullptr
    // 且 isArrayDynamic=true; 标量字段两者皆 false/nullptr. 用于区分 UDT 动态数组字段
    // (emit `vb6_SafeArray1D* Member`) 与普通标量字段 (`type Member`).
    bool isArrayDynamic = false;

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
    // 泛型 (tB 扩展): 见 SubDecl::typeParams
    std::vector<std::string> typeParams;

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

// Delegate 声明 (tB 扩展): [Public|Private] Delegate Sub/Function name [CDecl] (params) [As Type]
// 声明一个具名函数指针类型; 委托值与 LongPtr 位兼容, 赋值/传参/调用时做签名检查.
class DelegateDecl : public Decl {
public:
    AccessLevel access;
    ProcKind procKind;      // Sub 或 Function
    std::string name;
    CallConv callingConv;   // 默认 StdCall, 尾置 CDecl 关键字切换
    std::vector<std::unique_ptr<ParameterDecl>> params;
    TypeRefPtr returnType;  // Function 返回类型 (可为nullptr)

    DelegateDecl(SourceLocation loc, AccessLevel acc, ProcKind kind,
                 std::string n, CallConv conv,
                 std::vector<std::unique_ptr<ParameterDecl>> p,
                 TypeRefPtr ret)
        : Decl(ASTNodeKind::DelegateDecl, loc),
          access(acc), procKind(kind), name(std::move(n)),
          callingConv(conv), params(std::move(p)), returnType(std::move(ret)) {}
};

// ============================================================
// Interface 声明块 (tB 扩展: 显式接口契约, 见 ai/022 设计记录 D1/D2)
// ============================================================

// 方括号属性行: [Name] / [Name("text")] / [Name(3)]
// 词法上整行是一个 Identifier token (文本含方括号), 由 parseBracketAttrLine 拆解;
// 不改 lexer 是为了保住 `[带空格的名字]` 这一 VB6 名称引用语法 (ai/022 D1).
struct InterfaceAttr {
    std::string name;       // 属性名, 原样大小写 (如 "InterfaceId")
    std::string strValue;   // 字符串实参 ("{GUID}" / 描述文本)
    int64_t numValue = 0;   // 整型实参 (如 [DispId(3)])
    bool hasStr = false;
    bool hasNum = false;
    SourceLocation loc;
};

// 接口成员: 签名节点 + 其上方的属性行 (属性挂在成员上, 不动通用 Decl 节点)
struct InterfaceMember {
    std::vector<InterfaceAttr> attributes;
    DeclPtr decl;  // SubDecl / FunctionDecl / PropertyDecl, body 恒空
};

// Interface Name [Extends Parent] ... End Interface
// 成员只允许 Sub / Function / Property Get|Let|Set 的**签名** (复用现有声明节点,
// body 恒空: 出现实现体在解析期即报错). 存进 Module::interfaces, 不进 declarations,
// 因此在语义/发码接手前对既有管线完全透明 (零回归).
class InterfaceDecl : public Decl {
public:
    std::string name;
    std::string extendsName;             // 空 = 无父接口
    std::vector<InterfaceAttr> attributes;
    std::vector<InterfaceMember> members;

    InterfaceDecl(SourceLocation loc, std::string n, std::string ext)
        : Decl(ASTNodeKind::InterfaceDecl, loc),
          name(std::move(n)), extendsName(std::move(ext)) {}
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

// 多变量声明: Dim a, b, c As Long (逗号分隔的多个声明)
// 仅用于解析阶段的传输, 在 parseModule 中展开为独立声明
class MultiDecl : public Decl {
public:
    DeclList declarations;

    MultiDecl(SourceLocation loc, DeclList decls)
        : Decl(ASTNodeKind::MultiDecl, loc), declarations(std::move(decls)) {}
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
    bool isFormModule = false;      // true = .frm窗体模块 (P7)
    // Interface 头行宿主 (tB 扩展, ai/022 D1/B03): VB6 "一文件一接口" 的 `.cls` 写法 —
    // 文件自身就叫 IFoo.cls, 体内唯一的 Interface 块与模块同名. 由 stage 2.7 识别置位
    // (模块名要到 driver_frontend 才定得下来), 供"接口名与模块名不算撞车"的放行与
    // 后续发码 (B04: 宿主不发类实例) 使用. 普通 `.bas` 里的接口块恒为 false.
    bool isInterfaceModule = false;
    VBInstancing instancing = VBInstancing::Private;  // 类Instancing属性 (仅类模块)
    // 泛型类模板 (tB 扩展, G4): .cls 头行 `Class Name(Of T[,U])` 的类型参数名表.
    // 非空 = 该类是"泛型模板类" — 不进符号表/不发码; 泛型器按使用点克隆整个
    // Module (moduleName 改为扁名) 注入 modules_, 走完整主管线发码 (显式实例化).
    std::vector<std::string> classTypeParams;

    // Option 语句
    std::vector<std::unique_ptr<OptionStmt>> options;

    // Implements 语句
    std::vector<std::unique_ptr<ImplementsStmt>> implements;

    // Interface 契约块 (tB 扩展): 与 declarations 分离存放, 语义/发码前不被遍历
    std::vector<std::unique_ptr<InterfaceDecl>> interfaces;

    // DefType 语句
    std::vector<std::unique_ptr<DefTypeStmt>> defTypes;

    // 声明 (按源码顺序)
    DeclList declarations;

    // Attribute 语句
    std::vector<std::unique_ptr<AttributeStmt>> attributes;

    Module(SourceLocation loc, std::string fname)
        : ASTNode(ASTNodeKind::Module, loc), filename(std::move(fname)) {}
};

} // namespace vb6c3
