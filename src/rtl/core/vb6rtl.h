#pragma once
// vb6rtl.h - VB6 运行时库公共头（伞头）
// 为 C 代码生成器提供 VB6 基本类型与运行时函数的 C 声明。
// 2026-09-17 按家族拆为 7 个子头，本文件只负责按依赖顺序 include；
// 生成代码（cgen_base/cgen_util）与 RTL 源（vb6rtl.c）都只 include 本文件。
//
// 依赖顺序（勿调整，与拆分前的单文件顺序一致）：
//   base -> bstr -> variant -> builtin -> array -> class_com -> runtime

#include "vb6rtl_base.h"
#include "vb6rtl_bstr.h"
#include "vb6rtl_variant.h"
#include "vb6rtl_builtin.h"
#include "vb6rtl_array.h"
#include "vb6rtl_class_com.h"
#include "vb6rtl_runtime.h"
