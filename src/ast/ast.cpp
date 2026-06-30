#include "ast/ast.hpp"

namespace vb6c3 {

// ============================================================
// ASTNode 方法
// ============================================================

const char* ASTNode::kindName() const {
    switch (kind) {
    // 模块
    case ASTNodeKind::Module:          return "Module";

    // 声明
    case ASTNodeKind::SubDecl:         return "SubDecl";
    case ASTNodeKind::FunctionDecl:    return "FunctionDecl";
    case ASTNodeKind::PropertyDecl:    return "PropertyDecl";
    case ASTNodeKind::TypeDecl:        return "TypeDecl";
    case ASTNodeKind::TypeMember:      return "TypeMember";
    case ASTNodeKind::EnumDecl:        return "EnumDecl";
    case ASTNodeKind::EnumMember:      return "EnumMember";
    case ASTNodeKind::DeclareDecl:     return "DeclareDecl";
    case ASTNodeKind::EventDecl:       return "EventDecl";
    case ASTNodeKind::ConstDecl:       return "ConstDecl";
    case ASTNodeKind::VariableDecl:    return "VariableDecl";
    case ASTNodeKind::ParameterDecl:   return "ParameterDecl";

    // 语句
    case ASTNodeKind::Block:           return "Block";
    case ASTNodeKind::AssignmentStmt:  return "AssignmentStmt";
    case ASTNodeKind::SetStmt:         return "SetStmt";
    case ASTNodeKind::LetStmt:         return "LetStmt";
    case ASTNodeKind::IfStmt:          return "IfStmt";
    case ASTNodeKind::ElseIfClause:    return "ElseIfClause";
    case ASTNodeKind::ForStmt:         return "ForStmt";
    case ASTNodeKind::ForEachStmt:     return "ForEachStmt";
    case ASTNodeKind::DoLoopStmt:      return "DoLoopStmt";
    case ASTNodeKind::WhileWendStmt:   return "WhileWendStmt";
    case ASTNodeKind::SelectCaseStmt:  return "SelectCaseStmt";
    case ASTNodeKind::CaseClause:      return "CaseClause";
    case ASTNodeKind::WithStmt:        return "WithStmt";
    case ASTNodeKind::GoToStmt:        return "GoToStmt";
    case ASTNodeKind::GoSubStmt:       return "GoSubStmt";
    case ASTNodeKind::ReturnStmt:      return "ReturnStmt";
    case ASTNodeKind::OnErrorStmt:     return "OnErrorStmt";
    case ASTNodeKind::OnGoToStmt:      return "OnGoToStmt";
    case ASTNodeKind::OnGoSubStmt:     return "OnGoSubStmt";
    case ASTNodeKind::MidStmt:         return "MidStmt";  // P18-A

    case ASTNodeKind::EndStmt:         return "EndStmt";
    case ASTNodeKind::CallStmt:        return "CallStmt";
    case ASTNodeKind::ReDimStmt:       return "ReDimStmt";
    case ASTNodeKind::EraseStmt:       return "EraseStmt";
    case ASTNodeKind::LabelStmt:       return "LabelStmt";
    case ASTNodeKind::RaiseEventStmt:  return "RaiseEventStmt";
    case ASTNodeKind::OpenStmt:        return "OpenStmt";
    case ASTNodeKind::CloseStmt:       return "CloseStmt";
    case ASTNodeKind::GetStmt:         return "GetStmt";
    case ASTNodeKind::PutStmt:         return "PutStmt";
    case ASTNodeKind::InputStmt:       return "InputStmt";
    case ASTNodeKind::PrintStmt:       return "PrintStmt";
    case ASTNodeKind::WriteStmt:       return "WriteStmt";
    case ASTNodeKind::LineInputStmt:   return "LineInputStmt";
    case ASTNodeKind::WidthStmt:       return "WidthStmt";
    case ASTNodeKind::SeekStmt:        return "SeekStmt";
    case ASTNodeKind::LockStmt:        return "LockStmt";
    case ASTNodeKind::UnlockStmt:      return "UnlockStmt";
    case ASTNodeKind::NameStmt:        return "NameStmt";
    case ASTNodeKind::FileCopyStmt:    return "FileCopyStmt";
    case ASTNodeKind::KillStmt:        return "KillStmt";
    case ASTNodeKind::MkDirStmt:       return "MkDirStmt";
    case ASTNodeKind::RmDirStmt:       return "RmDirStmt";
    case ASTNodeKind::ChDirStmt:       return "ChDirStmt";
    case ASTNodeKind::ChDriveStmt:     return "ChDriveStmt";
    case ASTNodeKind::BeepStmt:        return "BeepStmt";
    case ASTNodeKind::DoEventsStmt:    return "DoEventsStmt";
    case ASTNodeKind::AttributeStmt:   return "AttributeStmt";
    case ASTNodeKind::OptionStmt:      return "OptionStmt";
    case ASTNodeKind::ImplementsStmt:  return "ImplementsStmt";
    case ASTNodeKind::DefTypeStmt:     return "DefTypeStmt";

    // 表达式
    case ASTNodeKind::BinaryExpr:        return "BinaryExpr";
    case ASTNodeKind::UnaryExpr:         return "UnaryExpr";
    case ASTNodeKind::LiteralExpr:       return "LiteralExpr";
    case ASTNodeKind::IdentifierExpr:    return "IdentifierExpr";
    case ASTNodeKind::MemberAccessExpr:  return "MemberAccessExpr";
    case ASTNodeKind::DictionaryAccessExpr: return "DictionaryAccessExpr";
    case ASTNodeKind::IndexOrCallExpr:   return "IndexOrCallExpr";
    case ASTNodeKind::NewExpr:           return "NewExpr";
    case ASTNodeKind::TypeOfExpr:        return "TypeOfExpr";
    case ASTNodeKind::AddressOfExpr:     return "AddressOfExpr";
    case ASTNodeKind::MeExpr:            return "MeExpr";
    case ASTNodeKind::WithMemberExpr:    return "WithMemberExpr";

    // 类型引用
    case ASTNodeKind::SimpleTypeRef:     return "SimpleTypeRef";
    case ASTNodeKind::ArrayTypeRef:      return "ArrayTypeRef";
    case ASTNodeKind::FixedStringTypeRef:return "FixedStringTypeRef";

    default: return "UnknownASTNode";
    }
}

// ============================================================
// 枚举转字符串工具
// ============================================================

const char* binaryOpToString(BinaryOp op) {
    switch (op) {
    case BinaryOp::Add:     return "+";
    case BinaryOp::Sub:     return "-";
    case BinaryOp::Mul:     return "*";
    case BinaryOp::Div:     return "/";
    case BinaryOp::IntDiv:  return "\\";
    case BinaryOp::Mod:     return "Mod";
    case BinaryOp::Pow:     return "^";
    case BinaryOp::Concat:  return "&";
    case BinaryOp::Eq:      return "=";
    case BinaryOp::Neq:     return "<>";
    case BinaryOp::Lt:      return "<";
    case BinaryOp::Gt:      return ">";
    case BinaryOp::Le:      return "<=";
    case BinaryOp::Ge:      return ">=";
    case BinaryOp::And:     return "And";
    case BinaryOp::Or:      return "Or";
    case BinaryOp::Xor:     return "Xor";
    case BinaryOp::Eqv:     return "Eqv";
    case BinaryOp::Imp:     return "Imp";
    case BinaryOp::Like:    return "Like";
    case BinaryOp::Is:      return "Is";
    default:                return "???";
    }
}

const char* unaryOpToString(UnaryOp op) {
    switch (op) {
    case UnaryOp::Negate: return "-";
    case UnaryOp::Not:    return "Not";
    default:              return "???";
    }
}

const char* literalKindToString(LiteralKind k) {
    switch (k) {
    case LiteralKind::Integer:  return "Integer";
    case LiteralKind::Long:     return "Long";
    case LiteralKind::Single:   return "Single";
    case LiteralKind::Double:   return "Double";
    case LiteralKind::Currency: return "Currency";
    case LiteralKind::Decimal:  return "Decimal";
    case LiteralKind::String:   return "String";
    case LiteralKind::Date:     return "Date";
    case LiteralKind::Boolean:  return "Boolean";
    case LiteralKind::Nothing:  return "Nothing";
    case LiteralKind::Empty:    return "Empty";
    case LiteralKind::Null:     return "Null";
    default:                    return "Unknown";
    }
}

const char* exitKindToString(ExitKind k) {
    switch (k) {
    case ExitKind::Do:       return "Do";
    case ExitKind::For:      return "For";
    case ExitKind::Sub:      return "Sub";
    case ExitKind::Function: return "Function";
    case ExitKind::Property: return "Property";
    default:                 return "Unknown";
    }
}

const char* procKindToString(ProcKind k) {
    switch (k) {
    case ProcKind::Sub:          return "Sub";
    case ProcKind::Function:     return "Function";
    case ProcKind::PropertyGet:  return "Property Get";
    case ProcKind::PropertyLet:  return "Property Let";
    case ProcKind::PropertySet:  return "Property Set";
    default:                     return "Unknown";
    }
}

} // namespace vb6c3
