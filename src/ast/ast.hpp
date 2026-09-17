#pragma once
// vb6c - Visual Basic 6.0 Compiler
// AST节点体系 - 覆盖VB6全部语法结构
// P1.1 定义
//
// 2026-09-17 按家族拆为 8 个子头（放在 detail/），本文件只负责按依赖顺序 include；
// 对外仍是唯一入口（6 处引用写 #include "ast/ast.hpp"，无需改动）。
// 依赖顺序（勿调整，与拆分前的单文件顺序一致）：
//   enums -> fwd -> base -> expr -> stmt -> stmt_io -> decl -> util

#include "ast/detail/ast_enums.hpp"
#include "ast/detail/ast_fwd.hpp"
#include "ast/detail/ast_base.hpp"
#include "ast/detail/ast_expr.hpp"
#include "ast/detail/ast_stmt.hpp"
#include "ast/detail/ast_stmt_io.hpp"
#include "ast/detail/ast_decl.hpp"
#include "ast/detail/ast_util.hpp"
