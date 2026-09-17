// semantic_analyzer_internal.h - semantic_analyzer 模块内部共享声明
// 仅供本模块各 .cpp 使用; 外部只 include semantics/semantic_analyzer.hpp
//
// 拆分说明: semantic_analyzer.cpp 按 Visitor 家族拆为多个编译单元, 文件级
// static 不跨编译单元可见 —— 因此把跨族使用的分派辅助集中声明在此, 定义仍
// 留在 semantic_analyzer.cpp。

#ifndef VB6C3_SEMANTIC_ANALYZER_INTERNAL_H
#define VB6C3_SEMANTIC_ANALYZER_INTERNAL_H

#include "semantics/semantic_analyzer.hpp"

namespace vb6c3 {

// --- 手动分派辅助 (AST节点无accept, 按kind分派到visitor) ---
// 拆分后不再 static: 声明/语句/表达式各族都需要调用
void dispatchDecl(Decl& decl, SemanticAnalyzer& analyzer);
void dispatchStmt(Stmt& stmt, SemanticAnalyzer& analyzer);
void dispatchExpr(Expr& expr, SemanticAnalyzer& analyzer);

} // namespace vb6c3

#endif // VB6C3_SEMANTIC_ANALYZER_INTERNAL_H
