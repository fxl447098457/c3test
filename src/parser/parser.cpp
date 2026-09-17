#include "parser/parser.hpp"
#include <algorithm>
#include <cctype>

namespace vb6c3 {

// ============================================================
// 构造函数 + 初始化
// ============================================================

Parser::Parser(std::shared_ptr<SourceBuffer> buffer, Diagnostics& diag,
               const PreprocessOptions& ppOpts)
    : preproc_(buffer, diag, ppOpts)
    , diag_(diag)
    , buffer_(buffer)
{
    initBindingPowers();
    // 预读前两个 token (跳过 Comment)
    cur_ = fetchNextToken();
    next_ = fetchNextToken();
}

void Parser::initBindingPowers() {
    // VB6 优先级表 (14 级, 按绑定力递增)
    // 左结合: l_bp < r_bp
    // 右结合: l_bp > r_bp (仅 ^ 幂运算)
    //
    // 级别 | 运算符              | l_bp | r_bp | 结合性
    // -----|--------------------|------|------|-------
    //  1   | Or, Xor            |  2   |  3   | 左
    //  2   | And                |  4   |  5   | 左
    //  3   | Not (一元前缀)     |  -   |  5   | 前缀
    //  4   | Eqv, Imp           |  6   |  7   | 左
    //  5   | =, <>, <, >, <=, >=|  8   |  9   | 左
    //  6   | & (Concat)         | 10   | 11   | 左
    //  7   | +, -               | 12   | 13   | 左
    //  8   | Mod                | 14   | 15   | 左
    //  9   | \ (IntDiv)         | 16   | 17   | 左
    // 10   | *, /               | 18   | 19   | 左
    // 11   | ^ (Pow)            | 21   | 20   | 右 (l_bp > r_bp!)
    // 12   | - (一元负)         |  -   | 19   | 前缀
    // 13   | Like, Is           | 22   | 23   | 左
    // 注意: 绑定力的数值不重要, 只需保持相对顺序

    // 中缀运算符 → BindingPower
    struct OpBp { TokenKind kind; int l; int r; };
    OpBp ops[] = {
        // 级1: Or, Xor
        { TokenKind::Or,    2,  3 },
        { TokenKind::Xor,   2,  3 },
        // 级2: And
        { TokenKind::And,   4,  5 },
        // 级4: Eqv, Imp
        { TokenKind::Eqv,   6,  7 },
        { TokenKind::Imp,   6,  7 },
        // 级5: 比较运算符
        { TokenKind::Equals,      8,  9 },
        { TokenKind::NotEquals,   8,  9 },
        { TokenKind::LessThan,    8,  9 },
        { TokenKind::GreaterThan, 8,  9 },
        { TokenKind::LessEqual,   8,  9 },
        { TokenKind::GreaterEqual,8,  9 },
        // 级6: 字符串连接
        { TokenKind::Ampersand, 10, 11 },
        // 级7: 加减
        { TokenKind::Plus,   12, 13 },
        { TokenKind::Minus,  12, 13 },
        // 级8: Mod
        { TokenKind::Mod,    14, 15 },
        // 级9: 整除
        { TokenKind::BackSlash, 16, 17 },
        // 级10: 乘除
        { TokenKind::Star,   18, 19 },
        { TokenKind::Slash,  18, 19 },
        // 级11: 幂 (右结合: l_bp=21 > r_bp=20)
        { TokenKind::Caret,  21, 20 },
        // 级13: Like, Is (Is在词法器中输出为IsKeyword，两者都需要注册)
        { TokenKind::Like,     22, 23 },
        { TokenKind::Is,       22, 23 },
        { TokenKind::IsKeyword,22, 23 },
    };

    for (auto& op : ops) {
        bpTable_[static_cast<int>(op.kind)] = {op.l, op.r};
    }
}

// ============================================================
// Token 消费接口
// ============================================================

const Token& Parser::peek() const {
    return cur_;
}

const Token& Parser::peek2() const {
    return next_;
}

Token Parser::advance() {
    if (++advanceCount_ > MAX_ADVANCES) {
        // 安全限制: 防止无限循环消耗内存
        SourceLocation loc = currentLoc();
        diag_.error(DiagnosticID::ParseUnexpectedToken, loc,
            "解析器advance调用超过上限(" + std::to_string(MAX_ADVANCES) + "), 可能存在无限循环");
        cur_.kind = TokenKind::EndOfFile;
        return cur_;
    }
    Token tok = std::move(cur_);
    cur_ = std::move(next_);
    next_ = fetchNextToken();
    prevTok_ = tok;  // Fix 043c: 保存已消费的 token 用于空格检测
    return tok;
}

Token Parser::fetchNextToken() {
    Token tok;
    int safetyCounter = 0;
    do {
        tok = preproc_.nextToken();
        if (++safetyCounter > 1000000) {
            // 安全限制: 防止无限循环
            tok.kind = TokenKind::EndOfFile;
            break;
        }
    } while (tok.kind == TokenKind::Comment || tok.kind == TokenKind::LineContinuation);
    return tok;
}

Token Parser::expect(TokenKind kind, DiagnosticID diagId, const std::string& msg) {
    if (cur_.kind == kind) {
        return advance();
    }
    diag_.error(diagId, currentLoc(),
        msg + " (got " + std::string(Token::kindToString(cur_.kind)) + ")");
    // 不消费, 让调用者决定后续动作
    return Token{cur_.kind, cur_.text, cur_.line, cur_.column, cur_.length, {0}};
}

Token Parser::expect(TokenKind kind, const std::string& msg) {
    return expect(kind, DiagnosticID::ParseExpectedToken, msg);
}

bool Parser::match(TokenKind kind) {
    if (cur_.kind == kind) {
        advance();
        return true;
    }
    return false;
}

bool Parser::expectOrSkip(TokenKind kind, DiagnosticID diagId, const std::string& msg) {
    if (cur_.kind == kind) {
        advance();
        return true;
    }
    diag_.error(diagId, currentLoc(),
        msg + " (got " + std::string(Token::kindToString(cur_.kind)) + ")");
    return false;
}

bool Parser::check(TokenKind kind) const {
    return cur_.kind == kind;
}

bool Parser::checkAny(std::initializer_list<TokenKind> kinds) const {
    for (auto k : kinds) {
        if (cur_.kind == k) return true;
    }
    return false;
}

bool Parser::isStatementStart() const {
    switch (cur_.kind) {
        // 赋值/调用: 标识符、.member(with)、!dict
        case TokenKind::Identifier:
        case TokenKind::Dot:
        case TokenKind::Exclamation:
        // 块语句关键字
        case TokenKind::If:
        case TokenKind::For:
        case TokenKind::Do:
        case TokenKind::While:
        case TokenKind::Select:
        case TokenKind::With:
        // 跳转
        case TokenKind::GoTo:
        case TokenKind::GoSub:
        case TokenKind::Return:
        case TokenKind::On:
        case TokenKind::Exit:
        case TokenKind::Stop:
        case TokenKind::End:
        // 赋值关键字
        case TokenKind::Set:
        case TokenKind::Let:
        case TokenKind::Call:
        // 声明 (Dim 也可出现在过程体内)
        case TokenKind::Dim:
        case TokenKind::ReDim:
        case TokenKind::Const:
        case TokenKind::Static:
        case TokenKind::Public:
        case TokenKind::Private:
        // 声明关键字在过程体内也可出现
        case TokenKind::Sub:
        case TokenKind::Function:
        case TokenKind::Property:
        case TokenKind::Type:
        case TokenKind::Enum:
        case TokenKind::Declare:
        case TokenKind::Event:
        // 文件I/O
        case TokenKind::Open:
        case TokenKind::Close:
        case TokenKind::Get:
        case TokenKind::Put:
        case TokenKind::Input:
        case TokenKind::Output:
        case TokenKind::Print:
        case TokenKind::Write:
        case TokenKind::Seek:
        case TokenKind::Lock:
        case TokenKind::Unlock:
        case TokenKind::Name:
        case TokenKind::Line:
        case TokenKind::Width:
        case TokenKind::FileCopy:
        case TokenKind::Kill:
        case TokenKind::MkDir:
        case TokenKind::RmDir:
        case TokenKind::ChDir:
        case TokenKind::ChDrive:
        // 杂项
        case TokenKind::Beep:
        case TokenKind::DoEvents:
        case TokenKind::Attribute:
        case TokenKind::Implements:
        case TokenKind::RaiseEvent:
        // Option (过程体内极少, 但解析器应容错)
        case TokenKind::Option:
        // DefType
        case TokenKind::DefBool:
        case TokenKind::DefByte:
        case TokenKind::DefInt:
        case TokenKind::DefLng:
        case TokenKind::DefCur:
        case TokenKind::DefSng:
        case TokenKind::DefDbl:
        case TokenKind::DefDate:
        case TokenKind::DefStr:
        case TokenKind::DefObj:
        case TokenKind::DefVar:
        // Me
        case TokenKind::MeKeyword:
            return true;
        default:
            // 软关键字也可作为语句开头 (变量名赋值/调用)
            return isSoftKeyword(cur_.kind);
    }
}

bool Parser::isDeclarationStart() const {
    switch (cur_.kind) {
        case TokenKind::Dim:
        case TokenKind::Const:
        case TokenKind::Public:
        case TokenKind::Private:
        case TokenKind::Static:
        case TokenKind::Sub:
        case TokenKind::Function:
        case TokenKind::Property:
        case TokenKind::Type:
        case TokenKind::Enum:
        case TokenKind::Declare:
        case TokenKind::Event:
        case TokenKind::Friend:
        case TokenKind::Global:
            return true;
        default:
            return false;
    }
}

// ============================================================
// 新行处理
// ============================================================

void Parser::skipNewLines() {
    while (cur_.kind == TokenKind::NewLine || cur_.kind == TokenKind::Comment || cur_.kind == TokenKind::LineContinuation) {
        advance();
    }
}

bool Parser::expectEndOfStatement() {
    // 接受 NewLine 或冒号
    if (cur_.kind == TokenKind::NewLine) {
        skipNewLines();
        return true;
    }
    if (match(TokenKind::Colon)) {
        skipNewLines();
        return true;
    }
    // 如果下一个是 EndOfFile, 也可以
    if (cur_.kind == TokenKind::EndOfFile) {
        return true;
    }
    // 报错并跳过当前token, 防止无限循环
    diag_.error(DiagnosticID::ParseExpectedEndOfStatement, currentLoc(),
        "expected end of statement (newline or :)");
    advance();  // 必须前进, 否则调用方while循环可能死循环
    return false;
}

SourceLocation Parser::currentLoc() const {
    return SourceLocation{buffer_->filePath(), cur_.line, cur_.column};
}

// ============================================================
// 错误恢复
// ============================================================

void Parser::synchronize() {
    // 跳过 token 直到遇到语句开始或块终止
    while (!isStatementStart() &&
           cur_.kind != TokenKind::EndOfFile &&
           cur_.kind != TokenKind::End &&
           cur_.kind != TokenKind::Else &&
           cur_.kind != TokenKind::ElseIf &&
           cur_.kind != TokenKind::Loop &&
           cur_.kind != TokenKind::Wend &&
           cur_.kind != TokenKind::Next &&
           cur_.kind != TokenKind::Case) {
        advance();
    }
}

void Parser::skipToNextLine() {
    while (cur_.kind != TokenKind::NewLine &&
           cur_.kind != TokenKind::EndOfFile) {
        advance();
    }
    skipNewLines();
}

} // namespace vb6c3
