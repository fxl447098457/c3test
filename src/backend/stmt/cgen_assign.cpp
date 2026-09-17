#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>

namespace vb6c3 {

// --- cgen_assign.cpp: 赋值语句生成 (Assignment / Set / Let / Mid$) ---
// 2026-09-17 拆分：原 1367 行单文件（visit(AssignmentStmt&) 函数体 1352 行）拆为
//   本文件（伞文件）                      —— 前置说明 + visit(AssignmentStmt&) 函数骨架
//   detail/cgen_assign_stmt_special.inc   —— 语句特殊形态：Date$/Time$ / Mid$ / LSet-RSet / 控件属性 / 跨模块变量（原 12~405 行）
//   detail/cgen_assign_prop_write.inc     —— 属性写入分派：类 Property Let / 默认属性 / With 块 / 链式 COM（原 406~828 行）
//   detail/cgen_assign_com_prop.inc       —— 参数化属性与 COM 写：P25/P25b / 类内 Property Let / COM SetProp / 返回值解封（原 829~1055 行）
//   detail/cgen_assign_value_sem.inc      —— 值语义：返回值变量 / BSTR / Variant 赋值（原 1056~1362 行）
// 四个 .inc 是「函数体片段」，在 visit() 函数体内被 #include（C++ 允许），故不用 .cpp/.hpp 后缀 ——
// 它们不是独立编译单元，单独 include 会编译不过。片段局部变量原样不动，逐行未改 → 零行为改动。

void CCodeGen::visit(AssignmentStmt& node) {
#include "backend/detail/cgen_assign_stmt_special.inc"
#include "backend/detail/cgen_assign_prop_write.inc"
#include "backend/detail/cgen_assign_com_prop.inc"
#include "backend/detail/cgen_assign_value_sem.inc"
}


} // namespace vb6c3
