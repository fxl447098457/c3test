#include "lexer/lexer.hpp"

namespace vb6c3 {

// === 数字扫描 ===

Token Lexer::scanNumber() {
    uint32_t startLine = line_, startCol = column_;
    std::string text;
    bool isFloat = false;
    bool hasExponent = false;

    // 整数部分
    while (offset_ < content_.size() && isDigit(peek())) {
        text += advance();
    }

    // 小数部分
    if (peek() == '.' && !isAlpha(peekNext())) {
        // 需要区分: 123.456 (浮点) vs 123.Method (成员访问)
        char nextNext = peekNextNext();
        if (isDigit(peekNext()) || nextNext == 'e' || nextNext == 'E') {
            isFloat = true;
            text += advance(); // 消费 .
            while (offset_ < content_.size() && isDigit(peek())) {
                text += advance();
            }
        }
    }

    // 指数部分
    if (offset_ < content_.size() && (peek() == 'e' || peek() == 'E')) {
        isFloat = true;
        text += advance(); // 消费 E
        if (offset_ < content_.size() && (peek() == '+' || peek() == '-')) {
            text += advance();
        }
        while (offset_ < content_.size() && isDigit(peek())) {
            text += advance();
        }
    }

    // 类型后缀
    bool isLong = false;
    if (offset_ < content_.size()) {
        char c = peek();
        switch (c) {
            case '%': text += advance(); break;
            case '&': text += advance(); isLong = true; break;
            case '!': text += advance(); isFloat = true; break;
            case '#': text += advance(); isFloat = true; break;
            case '@': text += advance(); {
                Token tok = makeToken(TokenKind::DecimalLiteral, text, startLine, startCol);
                // 解析Decimal值 (暂用double)
                try { tok.doubleValue = std::stod(text); } catch (...) {}
                return tok;
            }
        }
    }

    if (isFloat) {
        Token tok = makeToken(TokenKind::FloatLiteral, text, startLine, startCol);
        try { tok.doubleValue = std::stod(text); } catch (...) {}
        return tok;
    }

    if (isLong) {
        Token tok = makeToken(TokenKind::LongLiteral, text, startLine, startCol);
        try { tok.longValue = std::stoll(text); } catch (...) {}
        return tok;
    }

    Token tok = makeToken(TokenKind::IntegerLiteral, text, startLine, startCol);
    try { tok.intValue = static_cast<int32_t>(std::stoll(text)); } catch (...) {}
    return tok;
}

Token Lexer::scanHexNumber() {
    uint32_t startLine = line_, startCol = column_;
    std::string text;
    text += advance(); // 消费 &
    text += advance(); // 消费 H/h

    if (offset_ >= content_.size() || !isHexDigit(peek())) {
        return errorToken("十六进制数字格式错误", startLine, startCol);
    }

    while (offset_ < content_.size() && isHexDigit(peek())) {
        text += advance();
    }

    // & 后缀表示 Long
    if (peek() == '&') {
        text += advance();
        Token tok = makeToken(TokenKind::LongLiteral, text, startLine, startCol);
        try { tok.longValue = std::stoll(text.substr(2), nullptr, 16); } catch (...) {}
        return tok;
    }

    Token tok = makeToken(TokenKind::IntegerLiteral, text, startLine, startCol);
    try { tok.intValue = static_cast<int32_t>(std::stoll(text.substr(2), nullptr, 16)); } catch (...) {}
    return tok;
}

Token Lexer::scanOctNumber() {
    uint32_t startLine = line_, startCol = column_;
    std::string text;
    text += advance(); // 消费 &
    text += advance(); // 消费 O/o

    while (offset_ < content_.size() && isOctDigit(peek())) {
        text += advance();
    }

    if (peek() == '&') text += advance();
    Token tok = makeToken(TokenKind::IntegerLiteral, text, startLine, startCol);
    try { tok.intValue = static_cast<int32_t>(std::stoll(text.substr(2), nullptr, 8)); } catch (...) {}
    return tok;
}

Token Lexer::scanBinNumber() {
    uint32_t startLine = line_, startCol = column_;
    std::string text;
    text += advance(); // 消费 &
    text += advance(); // 消费 B/b

    while (offset_ < content_.size() && isBinDigit(peek())) {
        text += advance();
    }

    if (peek() == '&') text += advance();
    Token tok = makeToken(TokenKind::IntegerLiteral, text, startLine, startCol);
    try { tok.intValue = static_cast<int32_t>(std::stoll(text.substr(2), nullptr, 2)); } catch (...) {}
    return tok;
}

} // namespace vb6c3
