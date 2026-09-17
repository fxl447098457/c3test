#pragma once
// ast_util.hpp - 第十一节 工具函数
// 由 src/ast/ast.hpp 拆出（2026-09-17），内容与原文件对应区间逐字节相同。

#include "ast/detail/ast_decl.hpp"

namespace vb6c3 {

// ============================================================
// 第十一节 工具函数
// ============================================================

// 判断节点是否为表达式
inline bool isExpr(ASTNodeKind k) {
    return k >= ASTNodeKind::BinaryExpr && k <= ASTNodeKind::WithMemberExpr;
}

// 判断节点是否为语句
inline bool isStmt(ASTNodeKind k) {
    return k >= ASTNodeKind::Block && k <= ASTNodeKind::LocalDeclStmt;
}

// 判断节点是否为声明
inline bool isDecl(ASTNodeKind k) {
    return k >= ASTNodeKind::SubDecl && k <= ASTNodeKind::MultiDecl;
}

// BinaryOp 转字符串
const char* binaryOpToString(BinaryOp op);

// UnaryOp 转字符串
const char* unaryOpToString(UnaryOp op);

// LiteralKind 转字符串
const char* literalKindToString(LiteralKind k);

// ExitKind 转字符串
const char* exitKindToString(ExitKind k);

// ProcKind 转字符串 (复用types.hpp中的ProcKind)
const char* procKindToString(ProcKind k);

} // namespace vb6c3
