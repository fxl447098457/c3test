#pragma once
// 诊断系统 - 错误/警告/信息报告

#include <string>
#include <string_view>
#include <vector>
#include <sstream>
#include <cstdint>
#include <set>

namespace vb6c3 {

// 源码位置
struct SourceLocation {
    std::string filename;
    uint32_t line = 1;
    uint32_t column = 1;

    std::string toString() const {
        return filename + "(" + std::to_string(line) + "," + std::to_string(column) + ")";
    }
};

// 诊断级别
enum class DiagnosticLevel : uint8_t {
    Note,
    Warning,
    Error,
    Fatal,
};

// 诊断ID编号空间
// 1xxx: 词法阶段
// 2xxx: 语法阶段
// 3xxx: 语义阶段
// 4xxx: 代码生成阶段
// 5xxx: RTL/链接阶段
enum class DiagnosticID : uint16_t {
    // 词法 (1xxx)
    LexUnexpectedChar = 1001,
    LexUnterminatedString = 1002,
    LexInvalidNumber = 1003,
    LexInvalidCharLiteral = 1004,
    LexUnrecognizedToken = 1005,
    LexFileEncodingError = 1006,

    // 语法 (2xxx)
    ParseExpectedToken = 2001,
    ParseUnexpectedToken = 2002,
    ParseExpectedEndOfStatement = 2003,
    ParseExpectedExpression = 2004,
    ParseMismatchedBlock = 2005,
    ParseInvalidIf = 2006,
    ParseInvalidFor = 2007,
    ParseInvalidSelect = 2008,
    ParseDuplicateLabel = 2009,
    ParseUndeclaredLabel = 2010,
    ParseInvalidInterfaceMember = 2011,  // Interface 块内非法成员 (实现体/字段/可见性/事件)
    ParseUnknownAttribute = 2012,        // 无法识别的 [Xxx] 属性行

    // 语义 (3xxx)
    SemUndeclaredIdentifier = 3001,
    SemDuplicateDeclaration = 3002,
    SemTypeMismatch = 3003,
    SemMissingDefaultProperty = 3004,
    SemWrongNumberOfArguments = 3005,
    SemNamedArgNotFound = 3006,
    SemDuplicateNamedArg = 3007,
    SemOptionalParamAfterRequired = 3008,
    SemParamArrayMustBeLast = 3009,
    SemCantAssignToReadOnly = 3010,
    SemInvalidUseOfMe = 3011,
    SemInterfaceNotImplemented = 3012,
    SemCircularDependency = 3013,
    SemVariantOverflow = 3014,
    // Interface 契约 (tB 扩展, ai/022 B02). 文案一律 ASCII (D12).
    SemInterfaceUnknownParent = 3015,      // Extends 的父接口不存在
    SemInterfaceSlotConflict = 3016,       // 链上槽名冲突 / 接口内同名重载
    SemInterfaceSignatureMismatch = 3017,  // 实现成员签名与接口槽不符
    SemInterfaceNotSupported = 3018,       // v1 边界: 该处的 Interface 用法尚不支持
    SemInterfaceClauseUnbound = 3019,      // 成员级 Implements 子句没被任何契约比对接纳 (B02b)
    // 类继承 (tB 扩展, ai/022 D6, 批次 B07). 文案一律 ASCII (D12).
    SemInheritsUnknownBase = 3020,    // Inherits 的基名不是本工程内的类
    SemInheritsTooDeep = 3021,        // 继承链长度超上限
    SemInheritsNotSupported = 3022,   // v1 边界: 非类模块 / 泛型模板内 / 多条 Inherits
    // tB 类继承 (ai/022 D29-1, 批次 B08c): 家族外访问 Protected 成员。
    SemProtectedOutsideFamily = 3023,   // obj.<Protected 成员> 的接收者类不在当前类的家族里
    // 虚方法 (tB 扩展, ai/022 D30, 批次 B08b). 文案一律 ASCII (D12).
    SemOverrideTargetUnknown = 3024,    // Overrides 找不到同名的祖先可覆盖成员
    SemOverrideNotOverridable = 3025,   // 祖先成员存在但未标 Overridable (或显式 NotOverridable)
    SemOverrideSignatureMismatch = 3026, // Overrides 与祖先槽签名不符
    SemVirtualNotSupported = 3027,      // v1 边界: 该处的虚成员用法尚不支持 (含"需要类虚表")
    // tB 类继承 (ai/022 B09): `MyBase.<成员>` 显式基调用的可用性判定。
    SemMyBaseNotSupported = 3028,       // 无基类 / 基面上没有这个成员 / 基成员是 Private
    // tB 委托式实现 (ai/022 B10): `Implements I Via m_holder` 的可用性判定。
    SemViaTargetUnknown = 3029,         // Via 目标不是本类的对象持有字段 / 接口名不是新式 Interface
    SemViaHolderNotImplemented = 3030,  // 字段类型那个类没有实现被委托的接口 (v1 不接受再往下委托)

    // 代码生成 (4xxx)
    CodeGenUnsupportedFeature = 4001,
    CodeGenLLVMError = 4002,
    CodeGenLinkerError = 4003,

    // RTL/链接 (5xxx)
    LinkUnresolvedExternal = 5001,
    LinkDuplicateSymbol = 5002,
    LinkMissingRTL = 5003,
    // 预处理 (6xxx)
    PreprocUndefinedConstant = 6001,
    PreprocInvalidDirective = 6002,
    PreprocConstRedefinition = 6003,
};

// 单条诊断信息
struct Diagnostic {
    DiagnosticLevel level;
    DiagnosticID id;
    SourceLocation location;
    std::string message;
    std::vector<SourceLocation> related; // 相关位置(如: 声明处)

    std::string toString() const {
        const char* levelStr = "";
        switch (level) {
            case DiagnosticLevel::Note:    levelStr = "note"; break;
            case DiagnosticLevel::Warning: levelStr = "warning"; break;
            case DiagnosticLevel::Error:   levelStr = "error"; break;
            case DiagnosticLevel::Fatal:   levelStr = "fatal error"; break;
        }
        std::string result = location.toString() + ": " + levelStr;
        result += " VB" + std::to_string(static_cast<uint16_t>(id)) + ": " + message;
        return result;
    }
};

// 诊断收集器
class Diagnostics {
public:
    void note(DiagnosticID id, const SourceLocation& loc, const std::string& msg) {
        diagnostics_.push_back({DiagnosticLevel::Note, id, loc, msg, {}});
    }

    void warn(DiagnosticID id, const SourceLocation& loc, const std::string& msg) {
        if (isSuppressed(id)) return;
        diagnostics_.push_back({DiagnosticLevel::Warning, id, loc, msg, {}});
        warningCount_++;
    }

    void error(DiagnosticID id, const SourceLocation& loc, const std::string& msg) {
        diagnostics_.push_back({DiagnosticLevel::Error, id, loc, msg, {}});
        errorCount_++;
    }

    void fatal(DiagnosticID id, const SourceLocation& loc, const std::string& msg) {
        diagnostics_.push_back({DiagnosticLevel::Fatal, id, loc, msg, {}});
        errorCount_++;
    }

    void addRelated(const SourceLocation& loc) {
        if (!diagnostics_.empty()) {
            diagnostics_.back().related.push_back(loc);
        }
    }

    // 查询
    bool hasErrors() const { return errorCount_ > 0; }
    bool hasWarnings() const { return warningCount_ > 0; }
    int errorCount() const { return errorCount_; }
    int warningCount() const { return warningCount_; }
    const std::vector<Diagnostic>& all() const { return diagnostics_; }

    // 输出所有诊断
    std::string toString() const {
        std::string result;
        for (const auto& d : diagnostics_) {
            result += d.toString() + "\n";
        }
        return result;
    }

    // 清空
    void clear() {
        diagnostics_.clear();
        errorCount_ = 0;
        warningCount_ = 0;
    }

    // 抑制指定ID的警告 (性能优化: 大型项目如vbman会打印上千条
    // VB3001/VB3003 宽松模式警告, 抑制后可减少日志I/O与输出膨胀)
    void suppress(DiagnosticID id) { suppressed_.insert(id); }
    bool isSuppressed(DiagnosticID id) const { return suppressed_.count(id) > 0; }

private:
    std::vector<Diagnostic> diagnostics_;
    std::set<DiagnosticID> suppressed_;
    int errorCount_ = 0;
    int warningCount_ = 0;
};

} // namespace vb6c3
