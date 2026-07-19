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

// === 关键字映射 ===

void Lexer::initKeywords() {
    // VB6关键字 (全小写映射, 因为VB6不区分大小写)
    keywords_ = {
        {"if", TokenKind::If}, {"then", TokenKind::Then},
        {"elseif", TokenKind::ElseIf}, {"else", TokenKind::Else},
        {"end", TokenKind::End},

        {"for", TokenKind::For}, {"to", TokenKind::To},
        {"step", TokenKind::Step}, {"next", TokenKind::Next},
        {"each", TokenKind::Each}, {"in", TokenKind::In},

        {"do", TokenKind::Do}, {"loop", TokenKind::Loop},
        {"while", TokenKind::While}, {"until", TokenKind::Until},
        {"wend", TokenKind::Wend},

        {"select", TokenKind::Select}, {"case", TokenKind::Case},
        {"is", TokenKind::IsKeyword},

        {"with", TokenKind::With},

        {"dim", TokenKind::Dim}, {"redim", TokenKind::ReDim},
        {"preserve", TokenKind::Preserve},
        {"const", TokenKind::Const},
        {"public", TokenKind::Public}, {"private", TokenKind::Private},
        {"static", TokenKind::Static}, {"friend", TokenKind::Friend},
        {"global", TokenKind::Global},

        {"as", TokenKind::As}, {"new", TokenKind::New},
        {"withevents", TokenKind::WithEvents},

        {"sub", TokenKind::Sub}, {"function", TokenKind::Function},
        {"property", TokenKind::Property},
        {"get", TokenKind::Get}, {"let", TokenKind::Let}, {"set", TokenKind::Set},
        {"call", TokenKind::Call},
        {"declare", TokenKind::Declare}, {"lib", TokenKind::Lib},
        {"alias", TokenKind::Alias}, {"cdecl", TokenKind::CDecl},
        {"byval", TokenKind::ByVal}, {"byref", TokenKind::ByRef},
        {"optional", TokenKind::Optional},
        {"paramarray", TokenKind::ParamArray},
        {"default", TokenKind::Default},

        {"type", TokenKind::Type}, {"enum", TokenKind::Enum},
        {"event", TokenKind::Event}, {"raiseevent", TokenKind::RaiseEvent},
        {"implements", TokenKind::Implements},
        {"class", TokenKind::Class},

        {"boolean", TokenKind::Boolean}, {"byte", TokenKind::Byte},
        {"integer", TokenKind::Integer}, {"long", TokenKind::Long},
        {"longlong", TokenKind::LongLong}, {"longptr", TokenKind::LongPtr},
        {"single", TokenKind::Single}, {"double", TokenKind::Double},
        {"currency", TokenKind::Currency}, {"decimal", TokenKind::Decimal},
        {"date", TokenKind::Date}, {"object", TokenKind::Object},
        {"string", TokenKind::String}, {"variant", TokenKind::Variant},
        {"any", TokenKind::Any},

        {"defbool", TokenKind::DefBool}, {"defbyte", TokenKind::DefByte},
        {"defint", TokenKind::DefInt}, {"deflng", TokenKind::DefLng},
        {"defcur", TokenKind::DefCur}, {"defsng", TokenKind::DefSng},
        {"defdbl", TokenKind::DefDbl}, {"defdate", TokenKind::DefDate},
        {"defstr", TokenKind::DefStr}, {"defobj", TokenKind::DefObj},
        {"defvar", TokenKind::DefVar},

        {"goto", TokenKind::GoTo}, {"gosub", TokenKind::GoSub},
        {"return", TokenKind::Return}, {"on", TokenKind::On},
        {"resume", TokenKind::Resume}, {"error", TokenKind::Error},
        {"stop", TokenKind::Stop},
        {"exit", TokenKind::Exit}, {"continue", TokenKind::Continue},

        {"open", TokenKind::Open}, {"close", TokenKind::Close},
        {"input", TokenKind::Input}, {"output", TokenKind::Output},
        {"append", TokenKind::Append}, {"binary", TokenKind::Binary},
        {"random", TokenKind::Random}, {"access", TokenKind::Access},
        {"read", TokenKind::Read}, {"write", TokenKind::Write},
        {"readwrite", TokenKind::ReadWrite}, {"shared", TokenKind::Shared},
        {"lock", TokenKind::Lock}, {"unlock", TokenKind::Unlock}, {"reset", TokenKind::Reset},
        {"get", TokenKind::Get}, {"put", TokenKind::Put},
        {"seek", TokenKind::Seek}, {"line", TokenKind::Line},
        {"width", TokenKind::Width}, {"print", TokenKind::Print},
        {"name", TokenKind::Name}, {"freefile", TokenKind::FreeFile},
        {"eof", TokenKind::EOF_keyword},

        {"chdir", TokenKind::ChDir}, {"chdrive", TokenKind::ChDrive},
        {"mkdir", TokenKind::MkDir}, {"rmdir", TokenKind::RmDir},
        {"curdir", TokenKind::CurDir}, {"dir", TokenKind::Dir},
        {"filecopy", TokenKind::FileCopy}, {"kill", TokenKind::Kill},
        {"setattr", TokenKind::SetAttr}, {"getattr", TokenKind::GetAttr},
        {"filelen", TokenKind::FileLen}, {"filedatetime", TokenKind::FileDateTime},

        {"beep", TokenKind::Beep}, {"doevents", TokenKind::DoEvents},
        {"sendkeys", TokenKind::SendKeys}, {"appactivate", TokenKind::AppActivate},
        {"shell", TokenKind::Shell}, {"environ", TokenKind::Environ},
        {"command", TokenKind::Command},
        {"randomize", TokenKind::Randomize}, {"timer", TokenKind::Timer},

        {"option", TokenKind::Option}, {"explicit", TokenKind::Explicit},
        {"compare", TokenKind::Compare}, {"base", TokenKind::Base},
        {"text", TokenKind::Text},

        {"attribute", TokenKind::Attribute},
        {"begin", TokenKind::Begin},
        {"rem", TokenKind::REM_keyword},

        {"mid", TokenKind::Mid}, {"lset", TokenKind::LSet},
        {"rset", TokenKind::RSet},
        {"msgbox", TokenKind::MsgBox}, {"inputbox", TokenKind::InputBox},
        {"rgb", TokenKind::RGB}, {"qbcolor", TokenKind::QBColor},
        {"load", TokenKind::Load}, {"unload", TokenKind::Unload},
        {"savepicture", TokenKind::SavePicture}, {"loadpicture", TokenKind::LoadPicture},
        {"createobject", TokenKind::CreateObject},
        {"getobject", TokenKind::GetObject},
        {"format", TokenKind::Format},

        {"and", TokenKind::And}, {"or", TokenKind::Or},
        {"xor", TokenKind::Xor}, {"not", TokenKind::Not},
        {"eqv", TokenKind::Eqv}, {"imp", TokenKind::Imp},
        {"like", TokenKind::Like}, {"mod", TokenKind::Mod},
        {"addressof", TokenKind::AddressOf},
        {"typeof", TokenKind::TypeOf},

        {"true", TokenKind::TrueKeyword}, {"false", TokenKind::FalseKeyword},
        {"nothing", TokenKind::NothingKeyword}, {"empty", TokenKind::EmptyKeyword},
        {"null", TokenKind::NullKeyword}, {"me", TokenKind::MeKeyword},

        // 条件编译 (以#开头, 但关键字映射同样)
        {"#if", TokenKind::HashIf},
        {"#elseif", TokenKind::HashElseIf},
        {"#else", TokenKind::HashElse},
        {"#end", TokenKind::HashEnd},
        {"#const", TokenKind::HashConst},
    };
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
