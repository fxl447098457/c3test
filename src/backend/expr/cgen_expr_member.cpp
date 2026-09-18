#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <cstdlib>
#include <functional>
#include <cstdio>

namespace vb6c3 {

// --- cgen_expr_member.cpp: MemberAccessExpr 求值（类成员 / COM / UDT 字段访问） ---
// 2026-09-17 拆分：原 1496 行单文件（CCodeGen::visit(MemberAccessExpr&) 函数体 1481 行）拆为
//   本文件（伞文件）                          —— include + namespace + 函数签名
//   detail/cgen_expr_member_precheck.inc      —— AddressOf 模块限定函数 / Err·内建对象 / 控件数组 / 内置对象方法（原 13~131 行）
//   detail/cgen_expr_member_form_builtin.inc  —— Form 的 Me.member（Fix 023e）与 IdentifierExpr 首段：App / 窗体默认实例 / Clipboard·Screen·Printer·Forms / 控件属性（原 132~362 行）
//   detail/cgen_expr_member_obj_dispatch.inc  —— COM 前期绑定·接口·后期绑定·Variant·promoted 分派；UDT 字段与函数名引用（Fix 031/085/084z-4/086）（原 363~552 行）
//   detail/cgen_expr_member_class_module.inc  —— 类实例成员分派（优先级2）与模块名.方法（优先级3）、模块前缀解析（原 553~774 行）
//   detail/cgen_expr_member_m22_module.inc    —— M22 跨模块变量访问与函数调用结果上的方法调用（Fix 083d/084y-4）（原 775~943 行）
//   detail/cgen_expr_member_generic_access.inc—— 通用成员访问 + 链式 COM 检测 / Variant 数组元素 / UDT 字段链（Fix 090af/085）（原 944~1063 行）
//   detail/cgen_expr_member_voidptr_com.inc   —— void* 字段的 COM 化（Fix 023）与类方法链式调用（Fix 015）（原 1064~1254 行）
//   detail/cgen_expr_member_class_fallback.inc—— 类实例成员 fallback（Fix 010r-10/011r-1/014）与函数尾（原 1255~1493 行）
// 八个 .inc 是「函数体片段」，在 visit(MemberAccessExpr&) 函数体内被 #include（C++ 允许），故不用 .cpp/.hpp 后缀 ——
// 它们不是独立编译单元，单独 include 会编译不过。片段内容逐行未改（含原缩进层级）→ 零行为改动。

void CCodeGen::visit(MemberAccessExpr& node) {
#include "backend/detail/expr/cgen_expr_member_precheck.inc"
#include "backend/detail/expr/cgen_expr_member_form_builtin.inc"
#include "backend/detail/expr/cgen_expr_member_obj_dispatch.inc"
#include "backend/detail/expr/cgen_expr_member_class_module.inc"
#include "backend/detail/expr/cgen_expr_member_m22_module.inc"
#include "backend/detail/expr/cgen_expr_member_generic_access.inc"
#include "backend/detail/expr/cgen_expr_member_voidptr_com.inc"
#include "backend/detail/expr/cgen_expr_member_class_fallback.inc"
}

} // namespace vb6c3
