#pragma once
// ast_fwd.hpp - 第三节 前向声明 + 第四节 智能指针别名
// 由 src/ast/ast.hpp 拆出（2026-09-17），内容与原文件对应区间逐字节相同。

#include "ast/detail/ast_enums.hpp"

namespace vb6c3 {

// ============================================================
// 第三节 前向声明
// ============================================================

class ASTNode;
class Expr;
class Stmt;
class Decl;

// 具体表达式
class BinaryExpr;
class UnaryExpr;
class LiteralExpr;
class IdentifierExpr;
class MemberAccessExpr;
class DictionaryAccessExpr;
class IndexOrCallExpr;
class NewExpr;
class TypeOfExpr;
class AddressOfExpr;
class MeExpr;
class WithMemberExpr;

// 具体语句
class Block;
class AssignmentStmt;
class SetStmt;
class LetStmt;
class IfStmt;
class ElseIfClause;
class ForStmt;
class ForEachStmt;
class DoLoopStmt;
class WhileWendStmt;
class SelectCaseStmt;
class CaseClause;
class WithStmt;
class GoToStmt;
class GoSubStmt;
class ReturnStmt;
class OnErrorStmt;
class ResumeStmt;
class ErrorStmt;
class OnGoToStmt;
class OnGoSubStmt;
class MidStmt;
class ExitStmt;
class StopStmt;
class AsmStmt;   // ai/vb-asm-extension-spec: Asm 块
class EndStmt;
class CallStmt;
class ReDimStmt;
class EraseStmt;
class LabelStmt;
class RaiseEventStmt;
class OpenStmt;
class CloseStmt;
class GetStmt;
class PutStmt;
class InputStmt;
class PrintStmt;
class WriteStmt;
class LineInputStmt;
class WidthStmt;
class SeekStmt;
class LockStmt;
class UnlockStmt;
class ResetStmt;
class NameStmt;
class FileCopyStmt;
class KillStmt;
class MkDirStmt;
class RmDirStmt;
class ChDirStmt;
class ChDriveStmt;
class BeepStmt;
class DoEventsStmt;
class AttributeStmt;
class OptionStmt;
class ImplementsStmt;
class DefTypeStmt;
class LocalDeclStmt;

// 具体声明
class SubDecl;
class FunctionDecl;
class PropertyDecl;
class TypeDecl;
class TypeMember;
class EnumDecl;
class EnumMember;
class DeclareDecl;
class EventDecl;
class DelegateDecl;
class InterfaceDecl;
class ConstDecl;
class VariableDecl;
class ParameterDecl;

// 模块
class Module;

// 类型引用
class SimpleTypeRef;
class ArrayTypeRef;
class FixedStringTypeRef;

// ============================================================
// 第四节 智能指针别名
// ============================================================

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;
using DeclPtr = std::unique_ptr<Decl>;
using ExprList = std::vector<ExprPtr>;
using StmtList = std::vector<StmtPtr>;
using DeclList = std::vector<DeclPtr>;

} // namespace vb6c3
