#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>

namespace vb6c3 {

// --- cgen_util.cpp: ActiveX DLL 入口代码生成 (generateDllEntry) ---

// 2026-09-17 拆分：原 699 行单文件（函数体 685 行）拆为
//   本文件（伞文件）                        —— 前置说明 + generateDllEntry 函数骨架
//   detail/cgen_util_dllentry_prelude.inc   —— 局部结构体 CoClassInfo + 哈希/格式化辅助 lambda（原 12~72 行）
//   detail/cgen_util_dllentry_collect.inc   —— coclass 收集、空类桩、前向声明与 IDispatch 桥接（原 73~397 行）
//   detail/cgen_util_dllentry_tables.inc    —— 方法描述表、IID/事件常量表与 coclass 描述表（原 398~623 行）
//   detail/cgen_util_dllentry_exports.inc   —— DLL 导出函数（原 624~696 行）
// 四个 .inc 是「函数体片段」，在 generateDllEntry() 函数体内被 #include（C++ 允许），故不用 .cpp/.hpp 后缀 ——
// 它们不是独立编译单元，单独 include 会编译不过。片段局部 lambda 与 CoClassInfo 原样不动，逐行未改 → 零行为改动。

std::string CCodeGen::generateDllEntry(const std::string& progId, const std::vector<SymbolTable*>& allSymTabs) {
#include "backend/detail/cgen_util_dllentry_prelude.inc"
#include "backend/detail/cgen_util_dllentry_collect.inc"
#include "backend/detail/cgen_util_dllentry_tables.inc"
#include "backend/detail/cgen_util_dllentry_exports.inc"
}
} // namespace vb6c3
