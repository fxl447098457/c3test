#include "lexer/lexer.hpp"

namespace vb6c3 {

// === 字符串扫描 ===

Token Lexer::scanString() {
    uint32_t startLine = line_, startCol = column_;
    std::string text;
    text += advance(); // 消费开头 "

    while (offset_ < content_.size() && peek() != '"') {
        if (peek() == '\n') {
            // VB6不允许字符串跨行 (除非行续接)
            SourceLocation loc{std::string(buffer_->filePath()), startLine, startCol};
            diag_.error(DiagnosticID::LexUnterminatedString, loc, "字符串未闭合");
            break;
        }
        text += advance();
    }

    if (peek() == '"') {
        text += advance(); // 消费结尾 "
        // VB6中 """ 表示字符串内的一个引号
        // 如果紧跟另一个", 说明是转义引号, 继续扫描
        while (offset_ < content_.size() && peek() == '"') {
            text += advance(); // 消费转义 "
            while (offset_ < content_.size() && peek() != '"') {
                text += advance();
            }
            if (offset_ < content_.size() && peek() == '"') {
                text += advance();
            }
        }
    }

    return makeToken(TokenKind::StringLiteral, text, startLine, startCol);
}

// === 反引号原始多行串扫描 (C3 扩展, ai/028 V1) ===
//
// 产物**不是**新 token，而是归一后的普通 StringLiteral：行界写成 CRLF、内嵌的 " 重新双写、
// 双写的反引号折成一枚 —— 于是 token.text 与手写的 VB6 串逐字节同形，parser / semantics /
// cgen 以及全仓 rawText 消费点 (ai/028 三-3 那四处各自剥引号折 "") 一行都不用改。
Token Lexer::scanRawString() {
    uint32_t startLine = line_, startCol = column_;
    advance(); // 消费开头的那枚反引号
    // 未闭合时只吃掉开头这一枚就退回。不退回的话整份余文会被吞进一个假字面量，
    // 一条 1007 会连带变成一片跨阶段的怪错。
    uint32_t bodyOff = offset_;
    uint32_t bodyLine = line_, bodyCol = column_;

    std::string body;
    bool atOpen = true;   // 起始反引号后紧跟的那一个换行按文本块惯例裁掉 (ai/028 八-1)
    bool closed = false;

    while (offset_ < content_.size()) {
        char c = peek();
        if (c == '`') {
            if (peekNext() == '`') {   // 双写 → 一枚字面反引号 (与 VB6 的 "" 同一心智)
                advance();
                advance();
                body += '`';
                atOpen = false;
                continue;
            }
            advance();                 // 未成对的那枚 = 闭合
            closed = true;
            break;
        }
        if (c == '\n' || c == '\r') {
            // 行界归一成 CRLF。fromFile() 已把 CRLF/孤立 CR 抹成 LF (source_manager.cpp)，
            // 所以源码行尾风格不可能影响串值；这里同时兜住不归一的 fromString() 通路。
            if (c == '\r' && peekNext() == '\n') advance();
            advance();
            if (atOpen) { atOpen = false; continue; }
            body += "\r\n";
            continue;
        }
        atOpen = false;
        if (c == '"') {
            advance();
            body += "\"\"";            // 重新双写, 交给下游那四处折叠
            continue;
        }
        body += advance();
    }

    if (!closed) {
        offset_ = bodyOff;
        line_ = bodyLine;
        column_ = bodyCol;
        SourceLocation loc{std::string(buffer_->filePath()), startLine, startCol};
        diag_.error(DiagnosticID::LexUnterminatedRawString, loc,
                    "unterminated raw string (missing closing backtick)");
        return makeToken(TokenKind::Invalid, "`", startLine, startCol);
    }

    return makeToken(TokenKind::StringLiteral, "\"" + body + "\"", startLine, startCol);
}

// === 日期字面量扫描 ===

Token Lexer::scanDateLiteral() {
    uint32_t startLine = line_, startCol = column_;
    std::string text;
    text += advance(); // 消费 #

    while (offset_ < content_.size() && peek() != '#') {
        if (peek() == '\n') {
            SourceLocation loc{std::string(buffer_->filePath()), startLine, startCol};
            diag_.error(DiagnosticID::LexInvalidCharLiteral, loc, "日期字面量未闭合");
            break;
        }
        text += advance();
    }

    if (peek() == '#') {
        text += advance(); // 消费结尾 #
    }

    return makeToken(TokenKind::DateLiteral, text, startLine, startCol);
}

// === 注释扫描 ===

Token Lexer::scanComment() {
    uint32_t startLine = line_, startCol = column_;
    std::string text;
    text += advance(); // 消费 '

    // 注释到行尾
    while (offset_ < content_.size() && peek() != '\n') {
        text += advance();
    }

    // 检查行尾是否有行续接符 _
    // 在注释中的行续接在VB6中无效, 但我们仍记录
    return makeToken(TokenKind::Comment, text, startLine, startCol);
}

// === 行续接扫描 ===

Token Lexer::scanLineContinuation() {
    uint32_t startLine = line_, startCol = column_;
    std::string text;
    text += advance(); // 消费 _

    // 跳过空白
    while (offset_ < content_.size() && isWhitespace(peek()) && peek() != '\n') {
        text += advance();
    }

    // 跳过换行
    if (peek() == '\n') {
        text += advance();
    }

    // 跳过下一行开头的空白
    while (offset_ < content_.size() && isWhitespace(peek())) {
        advance(); // 不记录在text中
    }

    afterLineContinuation_ = true;
    return makeToken(TokenKind::LineContinuation, text, startLine, startCol);
}

// === 运算符扫描 (复杂情况) ===

Token Lexer::scanOperator() {
    uint32_t startLine = line_, startCol = column_;
    char c = peek();

    if (c == '&') {
        advance();
        // & 作为 Long类型后缀 : 在标识符后
        // & 作为字符串连接符 : 在表达式中间
        // 上下文决定, 这里统一返回Ampersand, 由语法分析器区分
        return makeToken(TokenKind::Ampersand, "&", startLine, startCol);
    }

    // 不应到达此处
    advance();
    return errorToken(std::string("意外运算符: '") + c + "'", startLine, startCol);
}

Token Lexer::scanNewLine() {
    uint32_t startLine = line_, startCol = column_;
    advance();
    return makeToken(TokenKind::NewLine, "\n", startLine, startCol);
}

} // namespace vb6c3
