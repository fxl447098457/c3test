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
    } while (tok.kind == TokenKind::Comment);
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
    while (cur_.kind == TokenKind::NewLine || cur_.kind == TokenKind::Comment) {
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

// ============================================================
// 软关键字与标识符名
// ============================================================

bool Parser::isSoftKeyword(TokenKind kind) const {
    // VB6 软关键字: 可以在标识符位置使用的保留字
    // 参考: VB6 语言规范 + 常见实践 (Name/String/Integer 等均可作变量名)
    switch (kind) {
        // 内置类型名 (常作变量/参数名)
        case TokenKind::Boolean:   case TokenKind::Byte:     case TokenKind::Integer:
        case TokenKind::Long:      case TokenKind::LongLong: case TokenKind::LongPtr:
        case TokenKind::Single:    case TokenKind::Double:   case TokenKind::Currency:
        case TokenKind::Decimal:   case TokenKind::Date:     case TokenKind::Object:
        case TokenKind::String:    case TokenKind::Variant:  case TokenKind::Any:
        // 软语句关键字
        case TokenKind::Name:      case TokenKind::Step:     case TokenKind::Error:
        case TokenKind::Width:     case TokenKind::Access:
        // 文件 I/O 模式关键字
        case TokenKind::Input:     case TokenKind::Output:   case TokenKind::Append:
        case TokenKind::Binary:    case TokenKind::Random:
        case TokenKind::Read:      case TokenKind::Write:    case TokenKind::ReadWrite:
        case TokenKind::Shared:    case TokenKind::Lock:     case TokenKind::Unlock:
        case TokenKind::Put:       case TokenKind::Seek:     case TokenKind::Close:
        case TokenKind::Open:      case TokenKind::Print:    case TokenKind::Line:
        case TokenKind::FreeFile:  case TokenKind::EOF_keyword:
        case TokenKind::Get:
        // 文件系统关键字
        case TokenKind::ChDir:     case TokenKind::ChDrive:  case TokenKind::MkDir:
        case TokenKind::RmDir:     case TokenKind::CurDir:   case TokenKind::Dir:
        case TokenKind::FileCopy:  case TokenKind::Kill:     case TokenKind::SetAttr:
        case TokenKind::GetAttr:   case TokenKind::FileLen:  case TokenKind::FileDateTime:
        // 内置函数关键字
        case TokenKind::MsgBox:    case TokenKind::InputBox: case TokenKind::RGB:
        case TokenKind::QBColor:   case TokenKind::Format:   case TokenKind::Mid:
        case TokenKind::Load:      case TokenKind::Unload:   case TokenKind::LSet:
        case TokenKind::RSet:      case TokenKind::SavePicture: case TokenKind::LoadPicture:
        case TokenKind::CreateObject: case TokenKind::GetObject:
        // 杂项语句关键字
        case TokenKind::Beep:      case TokenKind::DoEvents: case TokenKind::SendKeys:
        case TokenKind::AppActivate: case TokenKind::Shell:  case TokenKind::Environ:
        case TokenKind::Command:   case TokenKind::Randomize: case TokenKind::Timer:
        // 日期/时间关键字
        case TokenKind::DateValue: case TokenKind::TimeValue: case TokenKind::DateAdd:
        case TokenKind::DateDiff:  case TokenKind::DatePart: case TokenKind::DateSerial:
        case TokenKind::TimeSerial:
        // 上下文关键字
        case TokenKind::Compare:   case TokenKind::Base:     case TokenKind::Text:
        case TokenKind::Binary2:   case TokenKind::Explicit: case TokenKind::Private2:
        case TokenKind::Attribute: case TokenKind::Begin:
        // 其他
        case TokenKind::Resume:    case TokenKind::Stop:
        case TokenKind::Let:       case TokenKind::Set:
            return true;
        default:
            return false;
    }
}

bool Parser::canBeName(TokenKind kind) const {
    return kind == TokenKind::Identifier || isSoftKeyword(kind);
}

Token Parser::expectName(const std::string& msg) {
    if (canBeName(cur_.kind)) {
        return advance();
    }
    diag_.error(DiagnosticID::ParseExpectedToken, currentLoc(),
        msg + " (got " + std::string(Token::kindToString(cur_.kind)) + ")");
    return Token{cur_.kind, cur_.text, cur_.line, cur_.column, cur_.length, {0}};
}

SourceLocation Parser::currentLoc() const {
    return SourceLocation{buffer_->filePath(), cur_.line, cur_.column};
}

// ============================================================
// 模块解析 (最高层)
// ============================================================

std::unique_ptr<Module> Parser::parseModule(bool isClassModule) {
    auto mod = std::make_unique<Module>(currentLoc(), buffer_->filePath());
    mod->isClassModule = isClassModule;
    parseModuleBody(*mod);
    return mod;
}

void Parser::parseModuleBody(Module& mod) {
    skipNewLines();

    // 类模块头部: 跳过 VERSION 1.0 CLASS ... BEGIN ... END 块
    // VB6 .cls 文件格式:
    //   VERSION 1.0 CLASS
    //   BEGIN
    //     MultiUse = -1  'True
    //     ...
    //   END
    if (mod.isClassModule) {
        // 检查是否有 VERSION 标记
        if (cur_.kind == TokenKind::Identifier &&
            toLower(cur_.text) == "version") {
            // 跳过 VERSION 1.0 CLASS 行
            skipToNextLine();
            skipNewLines();

            // 解析 BEGIN ... END 块, 提取 MultiUse 等 instancing 属性
            if (cur_.kind == TokenKind::Begin) {
                advance();  // consume BEGIN
                int depth = 1;
                while (cur_.kind != TokenKind::EndOfFile && depth > 0) {
                    if (cur_.kind == TokenKind::Begin) {
                        depth++;
                    } else if (cur_.kind == TokenKind::End) {
                        depth--;
                        if (depth == 0) {
                            advance();  // consume END
                            break;
                        }
                    } else if (depth == 1 && cur_.kind == TokenKind::Identifier) {
                        // 顶层属性: 检查是否为 MultiUse
                        std::string attrName = toLower(cur_.text);
                        if (attrName == "multiuse") {
                            advance();  // consume MultiUse
                            if (cur_.kind == TokenKind::Equals) {
                                advance();  // consume =
                                // -1 = True (MultiUse), 0 = False (Private)
                                if (cur_.kind == TokenKind::Minus) {
                                    advance();  // consume -
                                }
                                if (cur_.kind == TokenKind::IntegerLiteral ||
                                    cur_.kind == TokenKind::LongLiteral) {
                                    int val = std::atoi(cur_.text.c_str());
                                    if (val != 0) {
                                        mod.instancing = VBInstancing::MultiUse;
                                    }
                                    advance();
                                }
                            }
                            continue;  // 已消费属性, 不再 advance
                        }
                    }
                    advance();
                }
            }
            skipNewLines();
        }
    }

    while (cur_.kind != TokenKind::EndOfFile) {
        skipNewLines();
        if (cur_.kind == TokenKind::EndOfFile) break;

        // Option 语句 (必须在最前面)
        if (cur_.kind == TokenKind::Option) {
            mod.options.push_back(parseOption());
            expectEndOfStatement();
            continue;
        }

        // Implements 语句
        if (cur_.kind == TokenKind::Implements) {
            mod.implements.push_back(parseImplements());
            expectEndOfStatement();
            continue;
        }

        // DefType 语句
        if (checkAny({TokenKind::DefBool, TokenKind::DefByte, TokenKind::DefInt,
                      TokenKind::DefLng, TokenKind::DefCur, TokenKind::DefSng,
                      TokenKind::DefDbl, TokenKind::DefDate, TokenKind::DefStr,
                      TokenKind::DefObj, TokenKind::DefVar})) {
            mod.defTypes.push_back(parseDefType());
            expectEndOfStatement();
            continue;
        }

        // Attribute 语句
        if (cur_.kind == TokenKind::Attribute) {
            mod.attributes.push_back(parseAttribute());
            expectEndOfStatement();
            continue;
        }

        // 声明
        if (isDeclarationStart()) {
            auto decl = parseDeclaration();
            if (decl) {
                mod.declarations.push_back(std::move(decl));
            }
            continue;
        }

        // 不应该出现在模块级的语句 → 报错并跳过
        diag_.error(DiagnosticID::ParseUnexpectedToken, currentLoc(),
            "unexpected token at module level: " + cur_.text);
        advance();
        skipToNextLine();
    }
}

std::unique_ptr<OptionStmt> Parser::parseOption() {
    auto loc = currentLoc();
    advance(); // consume 'Option'

    if (match(TokenKind::Explicit)) {
        return std::make_unique<OptionStmt>(loc, OptionKind::Explicit);
    }
    if (match(TokenKind::Compare)) {
        if (match(TokenKind::Text)) {
            return std::make_unique<OptionStmt>(loc, OptionKind::CompareText);
        }
        if (match(TokenKind::Binary2)) {
            return std::make_unique<OptionStmt>(loc, OptionKind::CompareBinary);
        }
        // Option Compare 无后续 → 报错
        diag_.error(DiagnosticID::ParseExpectedToken, currentLoc(),
            "expected 'Text' or 'Binary' after 'Option Compare'");
        return std::make_unique<OptionStmt>(loc, OptionKind::CompareBinary);
    }
    if (match(TokenKind::Base)) {
        if (match(TokenKind::IntegerLiteral)) {
            // 简化: 0 或 1
            return std::make_unique<OptionStmt>(loc, OptionKind::BaseZero);
        }
        // 检查是否是 1
        if (cur_.kind == TokenKind::IntegerLiteral && cur_.intValue == 1) {
            advance();
            return std::make_unique<OptionStmt>(loc, OptionKind::BaseOne);
        }
        return std::make_unique<OptionStmt>(loc, OptionKind::BaseZero);
    }
    if (match(TokenKind::Private2)) {
        if (match(TokenKind::Class)) {
            // Option Private Module — VB6 特有
            return std::make_unique<OptionStmt>(loc, OptionKind::PrivateModule);
        }
        // 不完整
        diag_.error(DiagnosticID::ParseExpectedToken, currentLoc(),
            "expected 'Module' after 'Option Private'");
        return std::make_unique<OptionStmt>(loc, OptionKind::PrivateModule);
    }

    diag_.error(DiagnosticID::ParseExpectedToken, currentLoc(),
        "expected 'Explicit', 'Compare', 'Base', or 'Private' after 'Option'");
    return std::make_unique<OptionStmt>(loc, OptionKind::Explicit);
}

std::unique_ptr<ImplementsStmt> Parser::parseImplements() {
    auto loc = currentLoc();
    advance(); // consume 'Implements'
    auto nameTok = expectName("expected interface name");
    return std::make_unique<ImplementsStmt>(loc, nameTok.text);
}

std::unique_ptr<DefTypeStmt> Parser::parseDefType() {
    auto loc = currentLoc();
    DefTypeKind dk;
    switch (cur_.kind) {
        case TokenKind::DefBool: dk = DefTypeKind::Bool; break;
        case TokenKind::DefByte: dk = DefTypeKind::Byte; break;
        case TokenKind::DefInt:  dk = DefTypeKind::Int;  break;
        case TokenKind::DefLng:  dk = DefTypeKind::Lng;  break;
        case TokenKind::DefCur:  dk = DefTypeKind::Cur;  break;
        case TokenKind::DefSng:  dk = DefTypeKind::Sng;  break;
        case TokenKind::DefDbl:  dk = DefTypeKind::Dbl;  break;
        case TokenKind::DefDate: dk = DefTypeKind::Date; break;
        case TokenKind::DefStr:  dk = DefTypeKind::Str;  break;
        case TokenKind::DefObj:  dk = DefTypeKind::Obj;  break;
        case TokenKind::DefVar:  dk = DefTypeKind::Var;  break;
        default:
            diag_.error(DiagnosticID::ParseUnexpectedToken, currentLoc(),
                "expected DefType keyword");
            return nullptr;
    }
    advance(); // consume DefInt/DefStr/...

    // 解析字母范围: A-C, D, E-G
    std::vector<DefTypeStmt::LetterRange> ranges;
    do {
        auto letterTok = expectName("expected letter range");
        char from = letterTok.text.empty() ? 'A' : static_cast<char>(std::toupper(letterTok.text[0]));
        char to = from;
        if (match(TokenKind::Minus)) {
            auto endTok = expectName("expected letter");
            to = endTok.text.empty() ? 'Z' : static_cast<char>(std::toupper(endTok.text[0]));
        }
        ranges.push_back({from, to});
    } while (match(TokenKind::Comma));

    return std::make_unique<DefTypeStmt>(loc, dk, std::move(ranges));
}

std::unique_ptr<AttributeStmt> Parser::parseAttribute() {
    auto loc = currentLoc();
    advance(); // consume 'Attribute'
    // Attribute VB_Name = "ModuleName"
    auto nameTok = expectName("expected attribute name");
    std::string attrName = nameTok.text;

    // 可能有 . (如 Attribute VB_Name)
    while (match(TokenKind::Dot)) {
        auto part = expectName("expected attribute name part");
        attrName += "." + part.text;
    }

    expect(TokenKind::Equals, DiagnosticID::ParseExpectedToken,
           "expected '=' in attribute");

    auto value = parseExpression();
    return std::make_unique<AttributeStmt>(loc, attrName, std::move(value));
}

// ============================================================
// 运算符优先级查找
// ============================================================

BindingPower Parser::getBindingPower(TokenKind kind) const {
    auto it = bpTable_.find(static_cast<int>(kind));
    if (it != bpTable_.end()) {
        return it->second;
    }
    return {0, 0};  // 非中缀运算符
}

BinaryOp Parser::tokenToBinaryOp(TokenKind kind) const {
    switch (kind) {
        case TokenKind::Or:    return BinaryOp::Or;
        case TokenKind::Xor:   return BinaryOp::Xor;
        case TokenKind::And:   return BinaryOp::And;
        case TokenKind::Eqv:   return BinaryOp::Eqv;
        case TokenKind::Imp:   return BinaryOp::Imp;
        case TokenKind::Equals:      return BinaryOp::Eq;
        case TokenKind::NotEquals:   return BinaryOp::Neq;
        case TokenKind::LessThan:    return BinaryOp::Lt;
        case TokenKind::GreaterThan: return BinaryOp::Gt;
        case TokenKind::LessEqual:   return BinaryOp::Le;
        case TokenKind::GreaterEqual:return BinaryOp::Ge;
        case TokenKind::Ampersand:   return BinaryOp::Concat;
        case TokenKind::Plus:        return BinaryOp::Add;
        case TokenKind::Minus:       return BinaryOp::Sub;
        case TokenKind::Mod:         return BinaryOp::Mod;
        case TokenKind::BackSlash:   return BinaryOp::IntDiv;
        case TokenKind::Star:        return BinaryOp::Mul;
        case TokenKind::Slash:       return BinaryOp::Div;
        case TokenKind::Caret:       return BinaryOp::Pow;
        case TokenKind::Like:        return BinaryOp::Like;
        case TokenKind::Is:          return BinaryOp::Is;
        case TokenKind::IsKeyword:   return BinaryOp::Is;
        default:
            return BinaryOp::Add;  // 不应到达
    }
}

bool Parser::isInfixOperator(TokenKind kind) const {
    return bpTable_.find(static_cast<int>(kind)) != bpTable_.end();
}

bool Parser::isPrefixOperator(TokenKind kind) const {
    return kind == TokenKind::Minus || kind == TokenKind::Not;
}

// ============================================================
// 辅助
// ============================================================

bool Parser::identifierEquals(const std::string& a, const std::string& b) const {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++) {
        if (std::toupper(static_cast<unsigned char>(a[i])) !=
            std::toupper(static_cast<unsigned char>(b[i])))
            return false;
    }
    return true;
}

std::string Parser::toLower(const std::string& s) const {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return result;
}

bool Parser::isEndBlock() const {
    if (cur_.kind != TokenKind::End) return false;
    // next_ 是 End 后面的 token
    switch (next_.kind) {
        case TokenKind::If:
        case TokenKind::Sub:
        case TokenKind::Function:
        case TokenKind::Property:
        case TokenKind::Type:
        case TokenKind::Enum:
        case TokenKind::Select:
        case TokenKind::With:
        case TokenKind::Do:  // 不存在 End Do, 但容错
            return true;
        default:
            return false;
    }
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
