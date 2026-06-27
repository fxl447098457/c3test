#pragma once
// VB6符号表 - 作用域层次 + 符号查找
// 支持: 模块级/过程级/块级作用域, VB6不区分大小写

#include "common/types.hpp"
#include "common/diagnostics.hpp"
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <optional>

namespace vb6c3 {

class TypeSystem;  // 前向声明

// ============================================================
// 符号类别
// ============================================================

enum class SymbolKind : uint8_t {
    Variable,       // Dim/Private/Public/Static 变量
    Constant,       // Const 常量
    Sub,            // Sub 过程
    Function,       // Function 过程
    PropertyGet,    // Property Get
    PropertyLet,    // Property Let
    PropertySet,    // Property Set
    Parameter,      // 过程参数
    UserDefinedType,// Type ... End Type
    EnumType,       // Enum ... End Enum
    EnumMember,     // Enum 成员
    DeclareSub,     // Declare Sub (外部)
    DeclareFunc,    // Declare Function (外部)
    Event,          // Event 声明
    Class,          // 类模块 (.cls)
    Label,          // 行标签
    ComClass,       // COM coclass (来自TypeLib, 前期绑定)
    ComInterface,   // COM 接口 (来自TypeLib, 前期绑定)
};

// 判断是否为Property类型
inline bool isPropertyKind(SymbolKind k) {
    return k == SymbolKind::PropertyGet ||
           k == SymbolKind::PropertyLet ||
           k == SymbolKind::PropertySet;
}

// ============================================================
// 符号
// ============================================================

struct ParameterInfo {
    std::string name;
    Vb6Type type = Vb6Type::Variant;
    bool isByVal = false;
    bool isOptional = false;
    bool isParamArray = false;
};

struct Symbol {
    std::string name;           // 原始名称 (保留大小写)
    std::string lowerName;      // 小写名称 (用于查找)
    SymbolKind kind;
    Vb6Type type = Vb6Type::Empty;  // 符号类型

    // 位置
    SourceLocation location;

    // 访问级别
    AccessLevel access = AccessLevel::Public;

    // 过程相关
    std::vector<ParameterInfo> params;
    bool isStatic = false;      // Static Sub/Function
    bool isArray = false;       // 数组变量
    int32_t dimCount = 0;       // 数组维度数 (0=非数组, 1=一维, 2+=多维)

    // 常量值 (仅Constant)
    bool hasConstValue = false;
    int64_t constIntValue = 0;
    double constFloatValue = 0.0;
    std::string constStringValue;
    bool constBoolValue = false;
    Vb6Type constType = Vb6Type::Empty;  // 常量值的实际类型

    // 是否已被引用 (用于未使用变量警告)
    bool isReferenced = false;

    // 是否为内置符号 (由编译器预注册, 非用户代码)
    bool isBuiltin = false;

    // --- 跨模块符号解析 ---
    // isExternal=true 表示该符号定义在其他模块中（Public符号被当前模块引用）
    bool isExternal = false;
    // sourceModule 记录符号定义所在的模块基名（如 "MathUtils"）
    // 仅当 isExternal=true 时有效
    std::string sourceModule;

    // --- 类相关 (仅SymbolKind::Class) ---
    VBInstancing instancing = VBInstancing::Private;  // Instancing属性
    std::vector<std::string> memberNames;              // 类成员名称列表(方法+属性+事件)
    bool isInterface = false;                          // 是否为接口类(纯抽象,无实现)
    std::vector<std::string> implementsNames;          // Implements列表: 该类实现的接口名
    // 接口方法(仅isInterface=true时有意义): 必须被实现类覆盖的方法签名
    std::vector<ParameterInfo> interfaceMethodParams;  // 备用: 接口方法参数信息
    std::vector<std::string> interfaceMethodNames;     // 接口方法名列表(小写)

    // --- P6.5 事件相关 ---
    bool isWithEvents = false;                         // 变量是否声明为WithEvents
    std::vector<std::string> eventNames;               // 类声明的事件名列表(仅Class)
    std::string withEventsSourceClass;                 // WithEvents变量的源类名

    // --- COM前期绑定相关 (P6.3, SymbolKind::ComClass/ComInterface) ---
    std::string comIidStr;            // 接口IID字符串 (如 "{2A0A3E20-...}")
    std::string comClsidStr;          // coclass CLSID字符串
    std::string comProgId;            // ProgID (如 "Scripting.FileSystemObject")
    std::string comDefaultIfaceName;  // 默认接口名 (ComClass用)
    int32_t comVtblBase = 7;         // vtable起始偏移 (IDispatch=7, IUnknown=3)
    bool comIsDual = false;           // 双重接口 (dispinterface + vtable)

    // COM方法签名 (ComInterface用, 方法名小写→签名)
    struct ComMethodSig {
        std::string realName;         // 原始名称(保留大小写)
        int32_t memid = 0;           // DISPID
        int32_t vtableIndex = -1;    // vtable偏移
        Vb6Type returnType = Vb6Type::Void;  // 返回类型
        bool isPropertyGet = false;
        bool isPropertyPut = false;
        bool isPropertyPutRef = false;
        std::vector<ParameterInfo> params;  // 参数列表(含方向/类型)
    };
    std::unordered_map<std::string, ComMethodSig> comMethods;  // key=小写方法名

    Symbol() = default;
    Symbol(SymbolKind k, const std::string& n, Vb6Type t,
           SourceLocation loc, AccessLevel acc = AccessLevel::Public)
        : name(n), lowerName(toLower(n)), kind(k), type(t)
        , location(loc), access(acc) {}

    static std::string toLower(const std::string& s) {
        std::string result = s;
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return result;
    }

    // 存储键: Property Get/Let/Set加后缀区分同名共存, 其他用lowerName
    std::string storageKey() const {
        if (isPropertyKind(kind)) {
            switch (kind) {
                case SymbolKind::PropertyGet:  return lowerName + "$pg";
                case SymbolKind::PropertyLet:  return lowerName + "$pl";
                case SymbolKind::PropertySet:  return lowerName + "$ps";
                default: break;
            }
        }
        return lowerName;
    }

    const char* kindName() const {
        switch (kind) {
            case SymbolKind::Variable:        return "Variable";
            case SymbolKind::Constant:        return "Constant";
            case SymbolKind::Sub:             return "Sub";
            case SymbolKind::Function:        return "Function";
            case SymbolKind::PropertyGet:     return "Property Get";
            case SymbolKind::PropertyLet:     return "Property Let";
            case SymbolKind::PropertySet:     return "Property Set";
            case SymbolKind::Parameter:       return "Parameter";
            case SymbolKind::UserDefinedType: return "Type";
            case SymbolKind::EnumType:        return "Enum";
            case SymbolKind::EnumMember:      return "Enum Member";
            case SymbolKind::DeclareSub:      return "Declare Sub";
            case SymbolKind::DeclareFunc:     return "Declare Function";
            case SymbolKind::Event:           return "Event";
            case SymbolKind::Class:           return "Class";
            case SymbolKind::Label:           return "Label";
            case SymbolKind::ComClass:        return "ComClass";
            case SymbolKind::ComInterface:    return "ComInterface";
        }
        return "Unknown";
    }
};

// ============================================================
// 作用域
// ============================================================

enum class ScopeKind : uint8_t {
    Module,     // 模块级 (全局)
    Procedure,  // 过程级 (Sub/Function/Property)
    Block,      // 块级 (If/For/Do/While/With/Select 内)
};

class Scope {
    friend class SymbolTable;  // SymbolTable需要直接访问symbols_
public:
    Scope(ScopeKind kind, Scope* parent = nullptr)
        : kind_(kind), parent_(parent) {}

    // 定义符号, 返回false如果已存在同名符号(Property Get/Let/Set允许同名共存)
    bool define(std::unique_ptr<Symbol> sym);

    // 按名称查找 (本作用域, 不递归) - 返回第一个匹配
    Symbol* lookupLocal(const std::string& name) const;

    // 按名称+类别查找 (本作用域, 不递归) - 精确匹配
    Symbol* lookupLocalByKind(const std::string& name, SymbolKind kind) const;

    // 按名称查找 (递归向上搜索所有祖先作用域)
    Symbol* lookup(const std::string& name) const;

    // 获取所有符号
    const std::unordered_map<std::string, std::unique_ptr<Symbol>>& symbols() const {
        return symbols_;
    }

    ScopeKind kind() const { return kind_; }
    Scope* parent() const { return parent_; }

private:
    ScopeKind kind_;
    Scope* parent_;
    std::unordered_map<std::string, std::unique_ptr<Symbol>> symbols_;  // key=storageKey()
};

// ============================================================
// 符号表
// ============================================================

class SymbolTable {
public:
    SymbolTable(Diagnostics& diag);

    // 作用域管理
    void pushScope(ScopeKind kind);
    void popScope();

    // 当前作用域
    Scope* currentScope() const { return current_; }

    // 模块级作用域
    Scope* moduleScope() const { return moduleScope_; }

    // 定义符号 (当前作用域)
    bool define(std::unique_ptr<Symbol> sym);

    // 查找符号 (从当前作用域向上递归)
    Symbol* lookup(const std::string& name) const;

    // 查找符号 (仅当前作用域)
    Symbol* lookupLocal(const std::string& name) const;

    // 查找符号 (按名称+类别, 仅当前作用域)
    Symbol* lookupLocalByKind(const std::string& name, SymbolKind kind) const;

    // 查找模块级符号
    Symbol* lookupModule(const std::string& name) const;

    // 查找模块级符号 (按名称+类别, 用于Property Get/Let精确查找)
    Symbol* lookupModuleByKind(const std::string& name, SymbolKind kind) const;

    // 当前作用域深度 (0=模块级)
    int scopeDepth() const;

    // 当前作用域类型
    ScopeKind currentScopeKind() const;

    // 是否在过程中
    bool inProcedure() const;

    // 获取诊断系统
    Diagnostics& diagnostics() { return diag_; }

    // --- 跨模块符号操作 ---

    // 注入一个跨模块外部符号（由Driver在跨模块解析pass中调用）
    // 在模块级作用域定义一个isExternal=true的符号
    void defineExternal(std::unique_ptr<Symbol> sym);

    // 获取所有模块级Public符号（供其他模块链接用）
    // 返回 name → Symbol* 的映射（仅Sub/Function/Variable/Constant, Public访问级别）
    std::vector<const Symbol*> getPublicSymbols() const;

    // 获取当前模块引用的所有外部模块名集合
    // 遍历模块级符号, 返回所有 isExternal=true 的 sourceModule
    std::unordered_set<std::string> getExternalModuleNames() const;

private:
    Diagnostics& diag_;
    Scope* moduleScope_;
    Scope* current_;
    std::vector<std::unique_ptr<Scope>> scopes_;
};

} // namespace vb6c3
