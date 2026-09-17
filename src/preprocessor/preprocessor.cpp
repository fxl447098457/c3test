#include "preprocessor/preprocessor.hpp"
#include <cctype>
#include <algorithm>

namespace vb6c3 {

// --- preprocessor.cpp: 构造 + token 流原语 + 主处理循环 ---

// ============================================================
// 构造
// ============================================================

Preprocessor::Preprocessor(std::shared_ptr<SourceBuffer> buffer, Diagnostics& diag,
                           const PreprocessOptions& options)
    : lexer_(std::move(buffer), diag)
    , diag_(diag)
{
    // VB6内置条件编译常量 (全部小写键, 与查找一致)
    // Win16, Win32, Win64, VBA6, VBA7, Mac, Win [按平台]
    // 默认: Win32=True, VBA6=True, VBA7=True (我们编译器兼容VB6/VBA7)
    constants_["win32"] = CondCompileValue::fromBool(true);
    constants_["vba6"] = CondCompileValue::fromBool(true);
    constants_["vba7"] = CondCompileValue::fromBool(true);
    constants_["win"] = CondCompileValue::fromBool(true);
    // Fix 081h: Win64 根据 --arch x64 自动设置
    constants_["win64"] = CondCompileValue::fromBool(options.is64Bit);
    constants_["win16"] = CondCompileValue::fromBool(false);
    constants_["mac"] = CondCompileValue::fromBool(false);

    // 命令行 -D 定义 (在内置常量之后, 可覆盖)
    for (const auto& [name, value] : options.defines) {
        constants_[name] = value;
    }

    // 预读第一个token
    rawCur_ = lexer_.nextToken();
}

// ============================================================
// Lexer接口
// ============================================================

Token Preprocessor::nextToken() {
    // 安全限制: 超过token上限直接返回EOF
    if (hitTokenLimit_) {
        Token eof;
        eof.kind = TokenKind::EndOfFile;
        return eof;
    }
    if (!lookahead_.empty()) {
        Token tok = std::move(lookahead_.front());
        lookahead_.pop_front();
        return tok;
    }
    return processToken();
}

const Token& Preprocessor::peekToken() {
    if (lookahead_.empty()) {
        lookahead_.push_back(processToken());
    }
    return lookahead_.front();
}

const Token& Preprocessor::peekToken2() {
    while (lookahead_.size() < 2) {
        lookahead_.push_back(processToken());
    }
    return lookahead_[1];
}

bool Preprocessor::isActive() const {
    if (condStack_.empty()) return true;
    return condStack_.back().currentActive;
}

// ============================================================
// 内部token获取
// ============================================================

Token Preprocessor::fetchRaw() {
    Token tok = std::move(rawCur_);
    rawCur_ = lexer_.nextToken();
    tokenCount_++;
    return tok;
}

SourceLocation Preprocessor::currentLoc() const {
    return SourceLocation{std::string(rawCur_.text), rawCur_.line, rawCur_.column};
}

// ============================================================
// 预处理核心
// ============================================================

Token Preprocessor::processToken() {
    while (true) {
        // 安全限制检查
        if (tokenCount_ >= MAX_TOKENS) {
            hitTokenLimit_ = true;
            SourceLocation loc{"", rawCur_.line, rawCur_.column};
            diag_.error(DiagnosticID::PreprocInvalidDirective, loc,
                "预处理token数量超过上限(" + std::to_string(MAX_TOKENS) + "), 可能存在无限循环");
            Token eof;
            eof.kind = TokenKind::EndOfFile;
            return eof;
        }

        // 文件结束: 检查未闭合的条件块
        if (rawCur_.kind == TokenKind::EndOfFile) {
            if (!condStack_.empty()) {
                SourceLocation loc{"", rawCur_.line, rawCur_.column};
                diag_.error(DiagnosticID::ParseMismatchedBlock, loc,
                    "条件编译块未闭合: 缺少 #End If");
                condStack_.clear();
            }
            return fetchRaw();
        }

        // 条件编译指令始终需要处理, 即使在非活跃分支中
        // (因为需要追踪嵌套深度)
        switch (rawCur_.kind) {
            case TokenKind::HashConst:
                handleHashConst();
                continue; // 处理完后继续取下一个token

            case TokenKind::HashIf:
                handleHashIf();
                continue;

            case TokenKind::HashElseIf:
                handleHashElseIf();
                continue;

            case TokenKind::HashElse:
                handleHashElse();
                continue;

            case TokenKind::HashEnd:
                handleHashEndIf();
                continue;

            default:
                break;
        }

        // 非预处理指令: 根据活跃状态决定是否传递
        if (isActive()) {
            return fetchRaw();
        } else {
            // 跳过非活跃分支中的token
            fetchRaw();
            continue;
        }
    }
}

} // namespace vb6c3
