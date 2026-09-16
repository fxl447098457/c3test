#include "preprocessor/preprocessor.hpp"
#include <cctype>
#include <algorithm>

namespace vb6c3 {

// --- preprocessor_expr.cpp: 条件编译表达式求值（Or/And/Not/比较/加减/乘除/一元/基本项） ---


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
