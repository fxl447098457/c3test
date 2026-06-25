#include "preprocessor/preprocessor.hpp"
#include <cctype>
#include <algorithm>

namespace vb6c3 {

// ============================================================
// 构造
// ============================================================

Preprocessor::Preprocessor(std::shared_ptr<SourceBuffer> buffer, Diagnostics& diag,
                           const PreprocessOptions& options)
    : lexer_(std::move(buffer), diag)
    , diag_(diag)
{
    // 初始化命令行定义
    for (const auto& [name, value] : options.defines) {
        constants_[name] = value;
    }

    // VB6内置条件编译常量 (全部小写键, 与查找一致)
    // Win16, Win32, Win64, VBA6, VBA7, Mac, Win [按平台]
    // 默认: Win32=True, VBA6=True, VBA7=True (我们编译器兼容VB6/VBA7)
    constants_["win32"] = CondCompileValue::fromBool(true);
    constants_["vba6"] = CondCompileValue::fromBool(true);
    constants_["vba7"] = CondCompileValue::fromBool(true);
    constants_["win"] = CondCompileValue::fromBool(true);
    // Win64默认False, 除非--target是x64
    constants_["win64"] = CondCompileValue::fromBool(false);
    constants_["win16"] = CondCompileValue::fromBool(false);
    constants_["mac"] = CondCompileValue::fromBool(false);

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

// ============================================================
// #Const name = expression
// ============================================================

void Preprocessor::handleHashConst() {
    auto loc = currentLoc();
    fetchRaw(); // 消费 #Const

    // 在非活跃分支中, #Const 不生效, 跳过整行
    if (!isActive()) {
        skipToEndOfLine();
        return;
    }

    // 读取常量名
    if (rawCur_.kind != TokenKind::Identifier) {
        diag_.error(DiagnosticID::ParseExpectedToken, loc,
            "#Const 后面需要标识符");
        skipToEndOfLine();
        return;
    }
    std::string name = rawCur_.text;
    std::string nameLower = name;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
    fetchRaw(); // 消费标识符

    // 期望 =
    if (rawCur_.kind != TokenKind::Equals) {
        diag_.error(DiagnosticID::ParseExpectedToken, loc,
            "#Const 后面需要 '='");
        skipToEndOfLine();
        return;
    }
    fetchRaw(); // 消费 =

    // 求值表达式
    CondCompileValue value = evalCondition();

    // 存储 (命令行定义优先, 不可覆盖)
    if (constants_.find(nameLower) == constants_.end()) {
        constants_[nameLower] = value;
    }
}

// ============================================================
// #If condition Then
// ============================================================

void Preprocessor::handleHashIf() {
    fetchRaw(); // 消费 #If

    bool outerActive = isActive();

    if (!outerActive) {
        // 外层非活跃, 本块整体非活跃
        // 仍需压栈以追踪嵌套
        condStack_.push_back({false, false, false, false});
        skipToEndOfLine(); // 跳过 Then
        return;
    }

    // 求值条件
    CondCompileValue condVal = evalCondition();

    // 期望 Then
    if (rawCur_.kind == TokenKind::Then) {
        fetchRaw(); // 消费 Then
    } else {
        SourceLocation loc{"", rawCur_.line, rawCur_.column};
        diag_.error(DiagnosticID::ParseExpectedToken, loc,
            "#If 条件后需要 Then");
    }

    bool condBool = condVal.toBool();
    condStack_.push_back({true, condBool, condBool, false});
}

// ============================================================
// #ElseIf condition Then
// ============================================================

void Preprocessor::handleHashElseIf() {
    fetchRaw(); // 消费 #ElseIf

    if (condStack_.empty()) {
        SourceLocation loc{"", rawCur_.line, rawCur_.column};
        diag_.error(DiagnosticID::ParseMismatchedBlock, loc,
            "#ElseIf 没有匹配的 #If");
        skipToEndOfLine();
        return;
    }

    auto& block = condStack_.back();

    if (block.hasElse) {
        SourceLocation loc{"", rawCur_.line, rawCur_.column};
        diag_.error(DiagnosticID::ParseMismatchedBlock, loc,
            "#ElseIf 出现在 #Else 之后");
        skipToEndOfLine();
        return;
    }

    if (!block.outerActive) {
        // 外层非活跃, 直接跳过
        skipToEndOfLine();
        return;
    }

    if (block.anyBranchTaken) {
        // 已有分支被选中, 本分支不活跃
        block.currentActive = false;
        skipToEndOfLine();
        return;
    }

    // 求值条件
    CondCompileValue condVal = evalCondition();

    // 期望 Then
    if (rawCur_.kind == TokenKind::Then) {
        fetchRaw(); // 消费 Then
    }

    bool condBool = condVal.toBool();
    block.currentActive = condBool;
    if (condBool) {
        block.anyBranchTaken = true;
    }
}

// ============================================================
// #Else
// ============================================================

void Preprocessor::handleHashElse() {
    fetchRaw(); // 消费 #Else

    if (condStack_.empty()) {
        SourceLocation loc{"", rawCur_.line, rawCur_.column};
        diag_.error(DiagnosticID::ParseMismatchedBlock, loc,
            "#Else 没有匹配的 #If");
        return;
    }

    auto& block = condStack_.back();
    block.hasElse = true;

    if (!block.outerActive) {
        return;
    }

    // 如果前面已有分支被选中, #Else 不活跃
    block.currentActive = !block.anyBranchTaken;
    if (block.currentActive) {
        block.anyBranchTaken = true;
    }
}

// ============================================================
// #End If
// ============================================================

void Preprocessor::handleHashEndIf() {
    fetchRaw(); // 消费 #End

    // 期望 If
    if (rawCur_.kind == TokenKind::If) {
        fetchRaw(); // 消费 If
    } else {
        SourceLocation loc{"", rawCur_.line, rawCur_.column};
        diag_.error(DiagnosticID::ParseMismatchedBlock, loc,
            "#End 后面需要 If");
        return;
    }

    if (condStack_.empty()) {
        SourceLocation loc{"", rawCur_.line, rawCur_.column};
        diag_.error(DiagnosticID::ParseMismatchedBlock, loc,
            "#End If 没有匹配的 #If");
        return;
    }

    condStack_.pop_back();
}

// ============================================================
// 跳过到行尾
// ============================================================

void Preprocessor::skipToEndOfLine() {
    // 跳过当前行剩余token直到NewLine或EOF
    // 注意: 需要正确处理嵌套的条件编译
    while (rawCur_.kind != TokenKind::NewLine &&
           rawCur_.kind != TokenKind::EndOfFile) {
        fetchRaw();
    }
    // 也跳过NewLine本身
    if (rawCur_.kind == TokenKind::NewLine) {
        fetchRaw();
    }
}

// ============================================================
// 条件表达式求值
// ============================================================

Token Preprocessor::condAdvance() {
    if (condPos_ < condTokens_.size()) {
        return condTokens_[condPos_++];
    }
    // 返回一个EOF标记
    Token tok;
    tok.kind = TokenKind::EndOfFile;
    return tok;
}

const Token& Preprocessor::condPeek() const {
    if (condPos_ < condTokens_.size()) {
        return condTokens_[condPos_];
    }
    static const Token eof = []{ Token t; t.kind = TokenKind::EndOfFile; return t; }();
    return eof;
}

CondCompileValue Preprocessor::evalCondition() {
    // 先收集条件表达式的所有token, 直到Then或NewLine
    condTokens_.clear();
    condPos_ = 0;

    while (rawCur_.kind != TokenKind::EndOfFile) {
        // Then关键字标志条件表达式结束
        if (rawCur_.kind == TokenKind::Then) {
            break; // Then不属于表达式
        }
        if (rawCur_.kind == TokenKind::NewLine) {
            fetchRaw(); // 消费换行
            break;
        }

        condTokens_.push_back(rawCur_);
        fetchRaw();
    }

    // 递归下降求值
    auto result = evalOrExpr();
    return result;
}

// Or / Xor (最低优先级)
CondCompileValue Preprocessor::evalOrExpr() {
    auto left = evalAndExpr();

    while (condPeek().kind == TokenKind::Or ||
           condPeek().kind == TokenKind::Xor) {
        auto op = condAdvance().kind;
        auto right = evalAndExpr();

        if (op == TokenKind::Or) {
            left = CondCompileValue::fromBool(left.toBool() || right.toBool());
        } else {
            // Xor: 两者不同时为True
            left = CondCompileValue::fromBool(left.toBool() != right.toBool());
        }
    }
    return left;
}

// And
CondCompileValue Preprocessor::evalAndExpr() {
    auto left = evalNotExpr();

    while (condPeek().kind == TokenKind::And) {
        condAdvance(); // 消费 And
        auto right = evalNotExpr();
        left = CondCompileValue::fromBool(left.toBool() && right.toBool());
    }

    // Eqv / Imp (VB6逻辑等价/蕴含)
    while (condPeek().kind == TokenKind::Eqv ||
           condPeek().kind == TokenKind::Imp) {
        auto op = condAdvance().kind;
        auto right = evalNotExpr();
        if (op == TokenKind::Eqv) {
            // Eqv: 两者相同时为True
            left = CondCompileValue::fromBool(left.toBool() == right.toBool());
        } else {
            // Imp: 左False或右True时为True
            left = CondCompileValue::fromBool(!left.toBool() || right.toBool());
        }
    }
    return left;
}

// Not
CondCompileValue Preprocessor::evalNotExpr() {
    if (condPeek().kind == TokenKind::Not) {
        condAdvance(); // 消费 Not
        auto val = evalNotExpr(); // 右结合
        return CondCompileValue::fromBool(!val.toBool());
    }
    return evalComparison();
}

// 比较运算: =, <>, <, >, <=, >=
CondCompileValue Preprocessor::evalComparison() {
    auto left = evalAddExpr();

    auto kind = condPeek().kind;
    if (kind == TokenKind::Equals || kind == TokenKind::NotEquals ||
        kind == TokenKind::LessThan || kind == TokenKind::GreaterThan ||
        kind == TokenKind::LessEqual || kind == TokenKind::GreaterEqual) {
        condAdvance(); // 消费比较运算符
        auto right = evalAddExpr();

        int64_t lv = left.kind == CondCompileValue::Long ? left.longValue : (left.toBool() ? 1 : 0);
        int64_t rv = right.kind == CondCompileValue::Long ? right.longValue : (right.toBool() ? 1 : 0);

        bool result = false;
        switch (kind) {
            case TokenKind::Equals:      result = (lv == rv); break;
            case TokenKind::NotEquals:   result = (lv != rv); break;
            case TokenKind::LessThan:    result = (lv < rv);  break;
            case TokenKind::GreaterThan: result = (lv > rv);  break;
            case TokenKind::LessEqual:   result = (lv <= rv); break;
            case TokenKind::GreaterEqual:result = (lv >= rv); break;
            default: break;
        }
        return CondCompileValue::fromBool(result);
    }
    return left;
}

// 加减: +, -
CondCompileValue Preprocessor::evalAddExpr() {
    auto left = evalMulExpr();

    while (condPeek().kind == TokenKind::Plus || condPeek().kind == TokenKind::Minus) {
        auto op = condAdvance().kind;
        auto right = evalMulExpr();

        int64_t lv = left.kind == CondCompileValue::Long ? left.longValue : (left.toBool() ? -1 : 0);
        int64_t rv = right.kind == CondCompileValue::Long ? right.longValue : (right.toBool() ? -1 : 0);

        if (op == TokenKind::Plus) {
            left = CondCompileValue::fromLong(lv + rv);
        } else {
            left = CondCompileValue::fromLong(lv - rv);
        }
    }
    return left;
}

// 乘除模: *, /, \, Mod
CondCompileValue Preprocessor::evalMulExpr() {
    auto left = evalUnaryExpr();

    while (condPeek().kind == TokenKind::Star ||
           condPeek().kind == TokenKind::Slash ||
           condPeek().kind == TokenKind::BackSlash ||
           condPeek().kind == TokenKind::Mod) {
        auto op = condAdvance().kind;
        auto right = evalUnaryExpr();

        int64_t lv = left.kind == CondCompileValue::Long ? left.longValue : (left.toBool() ? -1 : 0);
        int64_t rv = right.kind == CondCompileValue::Long ? right.longValue : (right.toBool() ? -1 : 0);

        if (rv == 0) {
            left = CondCompileValue::fromLong(0); // 除零保护
        } else if (op == TokenKind::Star) {
            left = CondCompileValue::fromLong(lv * rv);
        } else if (op == TokenKind::Slash) {
            left = CondCompileValue::fromLong(lv / rv); // 浮点除转整
        } else if (op == TokenKind::BackSlash) {
            left = CondCompileValue::fromLong(lv / rv); // 整除
        } else { // Mod
            left = CondCompileValue::fromLong(lv % rv);
        }
    }
    return left;
}

// 一元 +/-
CondCompileValue Preprocessor::evalUnaryExpr() {
    if (condPeek().kind == TokenKind::Minus) {
        condAdvance();
        auto val = evalPrimaryExpr();
        if (val.kind == CondCompileValue::Long) {
            return CondCompileValue::fromLong(-val.longValue);
        }
        return CondCompileValue::fromBool(!val.toBool());
    }
    if (condPeek().kind == TokenKind::Plus) {
        condAdvance(); // 一元+, 忽略
    }
    return evalPrimaryExpr();
}

// 原子: True, False, 整数, 标识符(常量名), (expr)
CondCompileValue Preprocessor::evalPrimaryExpr() {
    auto tok = condPeek();

    // 布尔常量
    if (tok.kind == TokenKind::TrueKeyword) {
        condAdvance();
        return CondCompileValue::fromBool(true);
    }
    if (tok.kind == TokenKind::FalseKeyword) {
        condAdvance();
        return CondCompileValue::fromBool(false);
    }

    // 整数 (从文本解析, 因为Lexer的makeToken未设置intValue)
    if (tok.kind == TokenKind::IntegerLiteral) {
        condAdvance();
        try {
            // 处理 &H十六进制等
            if (tok.text.size() > 2 && tok.text[0] == '&') {
                char base = static_cast<char>(std::tolower(static_cast<unsigned char>(tok.text[1])));
                if (base == 'h') return CondCompileValue::fromLong(std::stoll(tok.text.substr(2), nullptr, 16));
                if (base == 'o') return CondCompileValue::fromLong(std::stoll(tok.text.substr(2), nullptr, 8));
                if (base == 'b') return CondCompileValue::fromLong(std::stoll(tok.text.substr(2), nullptr, 2));
            }
            return CondCompileValue::fromLong(std::stoll(tok.text));
        } catch (...) {
            return CondCompileValue::fromLong(0);
        }
    }
    if (tok.kind == TokenKind::LongLiteral) {
        condAdvance();
        try {
            std::string text = tok.text;
            // 去掉后缀 &
            if (!text.empty() && text.back() == '&') text.pop_back();
            if (text.size() > 2 && text[0] == '&') {
                char base = static_cast<char>(std::tolower(static_cast<unsigned char>(text[1])));
                if (base == 'h') return CondCompileValue::fromLong(std::stoll(text.substr(2), nullptr, 16));
                if (base == 'o') return CondCompileValue::fromLong(std::stoll(text.substr(2), nullptr, 8));
                if (base == 'b') return CondCompileValue::fromLong(std::stoll(text.substr(2), nullptr, 2));
            }
            return CondCompileValue::fromLong(std::stoll(text));
        } catch (...) {
            return CondCompileValue::fromLong(0);
        }
    }
    if (tok.kind == TokenKind::FloatLiteral) {
        condAdvance();
        try {
            return CondCompileValue::fromLong(static_cast<int64_t>(std::stod(tok.text)));
        } catch (...) {
            return CondCompileValue::fromLong(0);
        }
    }

    // 括号
    if (tok.kind == TokenKind::LeftParen) {
        condAdvance(); // 消费 (
        auto val = evalOrExpr();
        if (condPeek().kind == TokenKind::RightParen) {
            condAdvance(); // 消费 )
        }
        return val;
    }

    // 标识符: 查找条件编译常量
    if (tok.kind == TokenKind::Identifier) {
        condAdvance();
        std::string nameLower = tok.text;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
        auto it = constants_.find(nameLower);
        if (it != constants_.end()) {
            return it->second;
        }
        // 未定义的常量视为False (VB6行为)
        return CondCompileValue::fromBool(false);
    }

    // 软关键字也可能作为常量名
    // (如 Win32, Debug 等可能被词法器识别为关键字)
    if (tok.isKeyword()) {
        condAdvance();
        std::string nameLower = tok.text;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
        auto it = constants_.find(nameLower);
        if (it != constants_.end()) {
            return it->second;
        }
        // 特殊: 检查一些常见条件编译常量名的关键字形式
        // VB6中 Win32 等在条件编译中是标识符, 但可能被词法器误判
        return CondCompileValue::fromBool(false);
    }

    // 无法识别, 跳过
    condAdvance();
    return CondCompileValue::fromBool(false);
}

} // namespace vb6c3
