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
