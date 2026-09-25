#include "lexer/lexer.hpp"
#include <cctype>
#include <algorithm>

namespace vb6c3 {

Lexer::Lexer(std::shared_ptr<SourceBuffer> buffer, Diagnostics& diag)
    : buffer_(std::move(buffer))
    , diag_(diag)
    , content_(buffer_->content())
{
    initKeywords();
}

// === 字符操作 ===

char Lexer::peek() const {
    if (offset_ >= content_.size()) return '\0';
    return content_[offset_];
}

char Lexer::peekNext() const {
    if (offset_ + 1 >= content_.size()) return '\0';
    return content_[offset_ + 1];
}

char Lexer::peekNextNext() const {
    if (offset_ + 2 >= content_.size()) return '\0';
    return content_[offset_ + 2];
}

char Lexer::advance() {
    char c = content_[offset_++];
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

bool Lexer::match(char expected) {
    if (offset_ >= content_.size() || content_[offset_] != expected) return false;
    advance();
    return true;
}

bool Lexer::isAlpha(char c) const {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool Lexer::isDigit(char c) const {
    return c >= '0' && c <= '9';
}

bool Lexer::isHexDigit(char c) const {
    return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

bool Lexer::isOctDigit(char c) const {
    return c >= '0' && c <= '7';
}

bool Lexer::isBinDigit(char c) const {
    return c == '0' || c == '1';
}

bool Lexer::isAlphaNumeric(char c) const {
    return isAlpha(c) || isDigit(c);
}

bool Lexer::isWhitespace(char c) const {
    return c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v';
}

bool Lexer::isLineContinuation() const {
    // VB6行续接: 行尾 _ (前面有空格, 后面只能是空白+换行)
    if (offset_ >= content_.size() || content_[offset_] != '_') return false;
    // 检查_后面是否只有空白和换行
    size_t pos = offset_ + 1;
    while (pos < content_.size()) {
        char c = content_[pos];
        if (c == '\n') return true;
        if (c == '\r') {
            // \r\n or standalone \r
            return true;
        }
        if (c != ' ' && c != '\t') return false;
        pos++;
    }
    // _ 在文件末尾也算续行
    return pos >= content_.size();
}

// === Token工厂 ===

Token Lexer::makeToken(TokenKind kind, const std::string& text, uint32_t startLine, uint32_t startCol) {
    Token tok;
    tok.kind = kind;
    tok.text = text;
    tok.line = startLine;
    tok.column = startCol;
    tok.length = static_cast<uint32_t>(text.size());
    return tok;
}

Token Lexer::errorToken(const std::string& msg, uint32_t startLine, uint32_t startCol) {
    SourceLocation loc{std::string(buffer_->filePath()), startLine, startCol};
    diag_.error(DiagnosticID::LexUnrecognizedToken, loc, msg);
    return makeToken(TokenKind::Invalid, msg, startLine, startCol);
}

// === 前瞻接口 ===

Token Lexer::nextToken() {
    if (!lookahead_.empty()) {
        Token tok = std::move(lookahead_.front());
        lookahead_.pop_front();
        return tok;
    }
    return scanToken();
}

const Token& Lexer::peekToken() {
    if (lookahead_.empty()) {
        lookahead_.push_back(scanToken());
    }
    return lookahead_.front();
}

const Token& Lexer::peekToken2() {
    while (lookahead_.size() < 2) {
        lookahead_.push_back(scanToken());
    }
    return lookahead_[1];
}

// === 核心扫描 ===

Token Lexer::scanToken() {
    // 跳过空白 (不含换行)
    while (offset_ < content_.size() && isWhitespace(peek())) {
        advance();
    }

    // 文件末尾
    if (offset_ >= content_.size()) {
        return makeToken(TokenKind::EndOfFile, "", line_, column_);
    }

    uint32_t startLine = line_;
    uint32_t startCol = column_;
    char c = peek();

    // 换行
    if (c == '\n') {
        advance();
        return makeToken(TokenKind::NewLine, "\n", startLine, startCol);
    }

    // 注释: ' 或 REM
    if (c == '\'') {
        return scanComment();
    }

    // 条件编译: #If, #ElseIf, #Else, #End, #Const
    // 文件号: #1, #2 等
    // 日期字面量: #1/1/2026#
    if (c == '#') {
        advance(); // 消费 #

        // 检查 # 后是否跟纯数字 (文件号场景: As #1, Print #1, 等)
        // 文件号是 # 后跟1-3位数字, 且不是日期模式 (#mm/...#)
        if (isDigit(peek())) {
            // 向前看: 如果数字后面跟 / 或 - 则是日期, 否则是文件号
            size_t saveOff = offset_;
            uint32_t saveCol = column_;
            // 跳过数字
            while (offset_ < content_.size() && isDigit(peek())) {
                advance();
            }
            char afterDigits = peek();
            // 恢复位置
            offset_ = saveOff;
            column_ = saveCol;

            if (afterDigits == '/' || afterDigits == '-') {
                // 日期字面量: 回退 # 由scanDateLiteral处理
                offset_--;
                column_--;
                return scanDateLiteral();
            } else {
                // 文件号: 返回 Hash token, 数字由下次scanToken读取
                return makeToken(TokenKind::Hash, "#", startLine, startCol);
            }
        }

        // 非数字: 检查条件编译指令或返回Hash
        if (isAlpha(peek())) {
            size_t saveOff = offset_;
            uint32_t saveCol = column_;
            std::string text = "#";
            while (offset_ < content_.size() && (isAlpha(peek()) || isDigit(peek()) || peek() == '_')) {
                text += advance();
            }
            std::string lower = text;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            auto it = keywords_.find(lower);
            if (it != keywords_.end()) {
                return makeToken(it->second, text, startLine, startCol);
            }
            // 不是条件编译指令 (#fnum 等文件号引用)
            // 回退到 # 后, 只返回 Hash token, 后续标识符由下次 scanToken 读取
            offset_ = saveOff;
            column_ = saveCol;
            return makeToken(TokenKind::Hash, "#", startLine, startCol);
        }

        // 其他情况: 返回 Hash token (如 # 后跟空格)
        return makeToken(TokenKind::Hash, "#", startLine, startCol);
    }

    // 行续接符 _ (必须在 isAlpha 检查之前, 因为 isAlpha('_') 返回 true,
    // 否则 _ 会被当作标识符首字符, 导致行续接逻辑成为死代码)
    if (c == '_' && isLineContinuation()) {
        return scanLineContinuation();
    }

    // 标识符/关键字
    if (isAlpha(c) || c == '[') {
        if (c == '[') {
            // 方括号标识符 [name with spaces]
            advance(); // 消费 [
            std::string text = "[";
            while (offset_ < content_.size() && peek() != ']') {
                text += advance();
            }
            if (peek() == ']') {
                text += advance(); // 消费 ]
            }
            // 方括号标识符始终作为Identifier
            return makeToken(TokenKind::Identifier, text, startLine, startCol);
        }
        return scanIdentifierOrKeyword();
    }

    // 数字
    if (isDigit(c)) {
        return scanNumber();
    }

    // &H/&O/&B 十六进制/八进制/二进制, & 长整型后缀, & 字符串连接
    if (c == '&') {
        char next = peekNext();
        char lower_next = (next >= 'A' && next <= 'Z') ? (next + 32) : next;
        if (lower_next == 'h') return scanHexNumber();
        if (lower_next == 'o') return scanOctNumber();
        if (lower_next == 'b') return scanBinNumber();
        // 否则是运算符& (字符串连接) 或 Long类型后缀
        return scanOperator();
    }

    // 字符串
    if (c == '"') {
        return scanString();
    }

    // 反引号原始多行串 (C3 扩展, ai/028 V1)。VB6 里 ` 不是任何记号的开头，
    // 走 default 分支必然报"意外字符"，所以把这一格变成特性是纯加法。
    if (c == '`') {
        return scanRawString();
    }

    // 运算符和分隔符
    switch (c) {
        case '+': advance(); return makeToken(TokenKind::Plus, "+", startLine, startCol);
        case '-': advance(); return makeToken(TokenKind::Minus, "-", startLine, startCol);
        case '*': advance(); return makeToken(TokenKind::Star, "*", startLine, startCol);
        case '/': advance(); return makeToken(TokenKind::Slash, "/", startLine, startCol);
        case '\\': advance(); return makeToken(TokenKind::BackSlash, "\\", startLine, startCol);
        case '^': advance(); return makeToken(TokenKind::Caret, "^", startLine, startCol);
        case '(': advance(); return makeToken(TokenKind::LeftParen, "(", startLine, startCol);
        case ')': advance(); return makeToken(TokenKind::RightParen, ")", startLine, startCol);
        case '.': advance(); return makeToken(TokenKind::Dot, ".", startLine, startCol);
        case ',': advance(); return makeToken(TokenKind::Comma, ",", startLine, startCol);
        case ':':
            advance();
            if (match('=')) return makeToken(TokenKind::Assign, ":=", startLine, startCol);
            return makeToken(TokenKind::Colon, ":", startLine, startCol);
        case ';': advance(); return makeToken(TokenKind::Semicolon, ";", startLine, startCol);
        case '!': advance(); return makeToken(TokenKind::Exclamation, "!", startLine, startCol);
        case '%': advance(); return makeToken(TokenKind::Percent, "%", startLine, startCol);
        case '@': advance(); return makeToken(TokenKind::AtSign, "@", startLine, startCol);
        case '$': advance(); return makeToken(TokenKind::Dollar, "$", startLine, startCol);
        case '=': advance(); return makeToken(TokenKind::Equals, "=", startLine, startCol);
        case '<':
            advance();
            if (match('>')) return makeToken(TokenKind::NotEquals, "<>", startLine, startCol);
            if (match('=')) return makeToken(TokenKind::LessEqual, "<=", startLine, startCol);
            return makeToken(TokenKind::LessThan, "<", startLine, startCol);
        case '>':
            advance();
            if (match('=')) return makeToken(TokenKind::GreaterEqual, ">=", startLine, startCol);
            return makeToken(TokenKind::GreaterThan, ">", startLine, startCol);
        case '_':
            // 行续接符 (必须出现在行尾)
            if (isLineContinuation()) {
                return scanLineContinuation();
            }
            // 否则作为标识符的第一个字符
            return scanIdentifierOrKeyword();
        default:
            advance();
            return errorToken(std::string("意外字符: '") + c + "'", startLine, startCol);
    }
}

// === 标识符/关键字扫描 ===

Token Lexer::scanIdentifierOrKeyword() {
    uint32_t startLine = line_, startCol = column_;
    std::string text;

    while (offset_ < content_.size() && isAlphaNumeric(peek())) {
        text += advance();
    }

    // 可能带有类型后缀: name$, name%, name&, name!, name#, name@
    // 注意: 这些后缀是标识符的一部分, 不单独返回
    if (offset_ < content_.size()) {
        char c = peek();
        if (c == '$' || c == '%' || c == '&' || c == '!' || c == '#' || c == '@') {
            // 检查后缀后面是否是非字母数字 (确认这是类型后缀而非运算符)
            char next = peekNext();
            if (!isAlpha(next) && next != '_' && !isDigit(next)) {
                text += advance(); // 消费类型后缀
            }
        }
    }

    // 查找关键字
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    auto it = keywords_.find(lower);
    if (it != keywords_.end()) {
        // Rem 关键字: 作为注释处理, 消费到行尾 (与 ' 注释行为一致)
        if (it->second == TokenKind::REM_keyword) {
            while (offset_ < content_.size() && peek() != '\n') {
                text += advance();
            }
            return makeToken(TokenKind::Comment, text, startLine, startCol);
        }
        return makeToken(it->second, text, startLine, startCol);
    }

    return makeToken(TokenKind::Identifier, text, startLine, startCol);
}

} // namespace vb6c3
