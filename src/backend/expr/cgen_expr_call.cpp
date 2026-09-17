#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>
#include <cstdio>

namespace vb6c3 {

// --- cgen_expr_call.cpp: IndexOrCallExpr 求值（数组索引 / 函数调用统一路径） ---
// 2026-09-17 拆分：原 3627 行单文件（单函数 visit(IndexOrCallExpr&) 占函数体 3611 行）拆为
//   本文件（伞文件）                             —— 前置说明 + visit(IndexOrCallExpr&) 函数骨架
//   detail/cgen_expr_call_prelude.inc           —— 头部局部变量 + ParamArray 识别 + Variant 数组索引识别（原 13~299 行）
//   detail/cgen_expr_call_builtin_pre.inc       —— 内置函数前置拦截：IIf / VarPtr / TypeName(Me) / Choose / Array / IsMissing / Len(udt)（原 300~620 行）
//   detail/cgen_expr_call_callee_ident.inc      —— Fix 037 IdentifierExpr callee：Pattern F / F2 / G（原 621~805 行）
//   detail/cgen_expr_call_callee_member.inc     —— Fix 037 MemberAccessExpr callee：Pattern A / B / C / D（原 806~1009 行）
//   detail/cgen_expr_call_callee_withm.inc      —— Fix 060 WithMemberExpr callee + callee 发射 + WebBrowser/ListBox 方法拦截（原 1010~1167 行）
//   detail/cgen_expr_call_com_bind.inc          —— COM 后期/前期绑定检测（P6.2/P6.3/P6.4）+ callee 拆分判定（原 1168~1553 行）
//   detail/cgen_expr_call_callee_params.inc     —— callee 形参解析：calleeParams / builtin / Declare / ANSI（原 1554~1770 行）
//   detail/cgen_expr_call_arg_emit.inc          —— 位置实参发射（一）：UDT 打包 / Declare ANSI / ByRef 取地址（原 1771~2136 行）
//   detail/cgen_expr_call_arg_variant.inc       —— 位置实参发射（二）：ByVal Variant 包装 / Variant 提取（原 2137~2453 行）
//   detail/cgen_expr_call_named_args.inc        —— named 实参（含 Optional 空位填充）+ argList 组装 + ParamArray 展开（原 2454~2860 行）
//   detail/cgen_expr_call_builtin_fixup.inc     —— 内置函数调用点修正：UBound/LBound、字符串/日期/财务函数、实参个数（原 2861~3250 行）
//   detail/cgen_expr_call_pad_conv.inc          —— Optional 参数补齐 + 返回/实参类型修正链与收口（原 3251~3623 行）
// 12 个 .inc 是「函数体片段」，在 visit(IndexOrCallExpr&) 的函数体内被 #include（C++ 允许），故不用 .cpp/.hpp 后缀 ——
// 它们不是独立编译单元，单独 include 会编译不过。片段内局部变量与 Pattern 拦截分支原样不动，逐行未改 → 零行为改动。
// 其中 arg_emit / arg_variant 两段位于 1771 行那个遍历位置实参的 for 循环内部（相对花括号深度 1），
// 单看片段自身不闭合，但头 + 全部片段 + 尾拼回后与原文件逐行一致、语义等价。

void CCodeGen::visit(IndexOrCallExpr& node) {
#include "backend/detail/cgen_expr_call_prelude.inc"
#include "backend/detail/cgen_expr_call_builtin_pre.inc"
#include "backend/detail/cgen_expr_call_callee_ident.inc"
#include "backend/detail/cgen_expr_call_callee_member.inc"
#include "backend/detail/cgen_expr_call_callee_withm.inc"
#include "backend/detail/cgen_expr_call_com_bind.inc"
#include "backend/detail/cgen_expr_call_callee_params.inc"
#include "backend/detail/cgen_expr_call_arg_emit.inc"
#include "backend/detail/cgen_expr_call_arg_variant.inc"
#include "backend/detail/cgen_expr_call_named_args.inc"
#include "backend/detail/cgen_expr_call_builtin_fixup.inc"
#include "backend/detail/cgen_expr_call_pad_conv.inc"
}

} // namespace vb6c3
