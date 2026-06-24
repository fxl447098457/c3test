#pragma once
// VB6词法分析器 - 手写LL(2)扫描器
// 参考: FreeBASIC lex.bas (3-token前瞻缓冲)

#include "lexer/token.hpp"
#include "common/diagnostics.hpp"
#include "common/source_manager.hpp"
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <deque>

namespace vb6c3 {

class Lexer {
public:
    explicit Lexer(std::shared_ptr<SourceBuffer> buffer, Diagnostics& diag);

    // 读取下一个token, 推进位置
    Token nextToken();

    // 前瞻1个token (不推进位置)
    const Token& peekToken();

    // 前瞻2个token (不推进位置)
    const Token& peekToken2();

    // 当前位置
    uint32_t offset() const { return offset_; }
    uint32_t line() const { return line_; }
    uint32_t column() const { return column_; }

    // 是否到达文件末尾
    bool eof() const { return offset_ >= content_.size(); }

private:
    // 初始化关键字映射表
    void initKeywords();

    // 底层扫描函数
    Token scanToken();
    Token scanIdentifierOrKeyword();
    Token scanNumber();
    Token scanHexNumber();      // &H...
    Token scanOctNumber();      // &O...
    Token scanBinNumber();      // &B...
    Token scanString();
    Token scanDateLiteral();    // #...#
    Token scanOperator();
    Token scanComment();
    Token scanNewLine();
    Token scanLineContinuation();

    // 字符操作
    char peek() const;
    char peekNext() const;       // 前瞻1个字符
    char peekNextNext() const;   // 前瞻2个字符
    char advance();              // 前进1个字符
    bool match(char expected);   // 如果当前字符匹配则前进

    // 辅助判断
    bool isAlpha(char c) const;
    bool isDigit(char c) const;
    bool isHexDigit(char c) const;
    bool isOctDigit(char c) const;
    bool isBinDigit(char c) const;
    bool isAlphaNumeric(char c) const;
    bool isWhitespace(char c) const;
    bool isLineContinuation() const;

    // 创建token的辅助
    Token makeToken(TokenKind kind, const std::string& text, uint32_t startLine, uint32_t startCol);
    Token errorToken(const std::string& msg, uint32_t startLine, uint32_t startCol);

    // 状态
    std::shared_ptr<SourceBuffer> buffer_;
    Diagnostics& diag_;
    std::string_view content_;

    uint32_t offset_ = 0;
    uint32_t line_ = 1;
    uint32_t column_ = 1;

    // 前瞻缓冲
    static constexpr int LOOKAHEAD = 2;
    std::deque<Token> lookahead_;

    // 关键字映射 (小写)
    std::unordered_map<std::string, TokenKind> keywords_;

    // 行续接后的行号映射
    bool afterLineContinuation_ = false;
};

} // namespace vb6c3
