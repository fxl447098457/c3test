#include "ast/ast_printer.hpp"
#include "ast/ast.hpp"
#include "ast/ast_visitor.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace vb6c3 {

// ============================================================
// AST 打印器实现
// 内部使用 ASTVisitor 递归打印
// ============================================================
// 2026-09-17 拆分：原 581 行单文件（PrintVisitor 类体 549 行）拆为
//   本文件（伞文件）                                    —— include + namespace + ASTPrinter 构造 + PrintVisitor 类骨架 + 桥接函数
//   detail/ast_printer_visitor_entry.inc               —— 构造 + print(Module&) 入口（原 22~34 行）
//   detail/ast_printer_visitor_decl.inc                —— 声明类节点 visit（原 35~127 行）
//   detail/ast_printer_visitor_stmt.inc                —— 语句类节点 visit（原 128~349 行）
//   detail/ast_printer_visitor_expr.inc                —— 表达式与类型引用节点 visit（原 350~442 行）
//   detail/ast_printer_visitor_support.inc             —— private 成员与三个派发 helper（原 443~570 行）
// 五个 .inc 是「类体片段」，在 class PrintVisitor 的类体内被 #include（C++ 允许），故不用 .cpp/.hpp 后缀 ——
// 它们不是独立编译单元，单独 include 会编译不过。片段内容逐行未改（含原缩进 4 空格层级）→ 零行为改动。

ASTPrinter::ASTPrinter(std::ostream& os) : os_(os) {}

namespace {

// 内部打印 Visitor
class PrintVisitor : public ASTVisitor {
public:
#include "ast/detail/ast_printer_visitor_entry.inc"
#include "ast/detail/ast_printer_visitor_decl.inc"
#include "ast/detail/ast_printer_visitor_stmt.inc"
#include "ast/detail/ast_printer_visitor_expr.inc"
#include "ast/detail/ast_printer_visitor_support.inc"
};

} // anonymous namespace

// ASTPrinter::print 桥接到 PrintVisitor
void ASTPrinter::print(Module& module) {
    PrintVisitor visitor(os_);
    visitor.print(module);
}

} // namespace vb6c3
