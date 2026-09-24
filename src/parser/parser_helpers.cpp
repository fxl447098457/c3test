#include "parser/parser.hpp"
#include <algorithm>
#include <cctype>

namespace vb6c3 {

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
        case TokenKind::Shared:    case TokenKind::Lock:     case TokenKind::Unlock: case TokenKind::Reset:
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
        case TokenKind::Attribute: case TokenKind::Begin: case TokenKind::Default:
        // tB 扩展接口关键字: 登记为软关键字, 存量代码里同名标识符 (变量/成员名) 不受影响
        case TokenKind::Interface: case TokenKind::Extends: case TokenKind::Inherits:
        case TokenKind::Protected:
        case TokenKind::Overridable: case TokenKind::Overrides:
        case TokenKind::NotOverridable:  // tB 扩展 (ai/022 B08b): 虚方法修饰符同为软关键字
        case TokenKind::Via:             // tB 扩展 (ai/022 B10): `Implements I Via m_h` 的 Via
        case TokenKind::CoClass:         // tB 扩展 (ai/026, B11/C01): CoClass 块名可作普通标识符
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

// VB6 的行号就是标签名: `100: ...` 声明, `GoTo 100` / `GoSub 100` / `Resume 100` /
// `On Error GoTo 100` / `On x GoTo 100, 200` 引用。标签名统一存为文本, 后端
// `vb6_label_ + cIdent(name)` 与语义层的字符串比对都无需为数字另开分支。
std::string Parser::expectLabelTarget(const std::string& msg) {
    if (cur_.kind == TokenKind::IntegerLiteral) {
        return advance().text;
    }
    return expectName(msg).text;
}

// Fix 028: 见 parser.hpp 注释。剥离 VB6 标识符末尾的类型后缀, 返回剥离后的名字和类型名。
Parser::TypeSuffixStrip Parser::stripTypeSuffix(const std::string& text) const {
    TypeSuffixStrip result;
    result.name = text;
    if (text.size() < 2) return result;  // 单字符标识符不含后缀
    const char last = text.back();
    switch (last) {
        case '$': result.typeName = "String";   break;
        case '%': result.typeName = "Integer";  break;
        case '&': result.typeName = "Long";     break;
        case '!': result.typeName = "Single";   break;
        case '#': result.typeName = "Double";   break;
        case '@': result.typeName = "Currency"; break;
        default:
            return result;  // 无类型后缀, name 保留原文
    }
    result.name = text.substr(0, text.size() - 1);
    return result;
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

} // namespace vb6c3
