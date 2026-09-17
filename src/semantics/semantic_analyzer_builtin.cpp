#include "semantics/semantic_analyzer.hpp"
#include <algorithm>
#include <cctype>
#include <tuple>
#include <initializer_list>
#include "semantics/semantic_analyzer_internal.h"

namespace vb6c3 {

// --- semantic_analyzer_builtin.cpp: 内置函数/常量注册 (registerBuiltins) ---
//
// 2026-09-17 拆分：原 795 行单文件（其中 registerBuiltins 函数体 780 行）拆为
//   本文件（伞文件）                —— 前置说明 + registerBuiltins 函数骨架
//   builtin/builtin_consts.inc      —— 基础常量（MsgBox/颜色/字符串/布尔/杂项 + 键码，原 14~297 行）
//   builtin/builtin_consts_ext.inc  —— P21-25 / P22 / P23-04 扩展常量（原 298~503 行）
//   builtin/builtin_funcs.inc       —— 内置对象 + 内置函数注册（原 504~793 行）
// 三个 .inc 是「函数体片段」，在 registerBuiltins() 函数体内被 #include（C++ 允许），
// 故不用 .cpp/.hpp 后缀 —— 它们不是独立编译单元，单独 include 会编译不过。
// 片段内容逐行未改（含局部 lambda 与局部常量），注册顺序不变 → 零行为改动。

void SemanticAnalyzer::registerBuiltins() {
#include "semantics/builtin/builtin_consts.inc"
#include "semantics/builtin/builtin_consts_ext.inc"
#include "semantics/builtin/builtin_funcs.inc"
}
} // namespace vb6c3
