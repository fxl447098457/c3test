// vb6rtl_format.c - VB6 运行时库: 格式化家族：vb6_Format 用户自定义格式实现
// 2026-09-17 从 src/rtl/core/vb6rtl/vb6rtl.c 按家族拆出（纯搬移，逐行未改）:
//   原第 284~893 行
// 2026-09-17 二次拆分：vb6_Format 函数体 606 行按既有 Step 分节切为 6 个片段，
// 本文件（伞文件）只保留 25 行前置说明 + 函数签名与片段 #include + 收尾两行。
// 片段与原 vb6rtl.c 行区间对照（本文件行号 = 原 vb6rtl.c 行号 - 258）：
//   vb6rtl_format_extract.inc       原 285~339    数值提取与无格式串默认转换（Step 1~2）
//   vb6rtl_format_parse.inc         原 340~455    命名格式识别与用户格式串解析（Step 3~4）
//   vb6rtl_format_string.inc        原 456~551    格式类型判定与字符串格式应用（Step 5~6）
//   vb6rtl_format_numeric_pre.inc   原 552~656    数值格式：校验、千分位缩放、占位符计数
//   vb6rtl_format_numeric_body.inc  原 657~790    数值格式：数字格式化与 0/# 补位
//   vb6rtl_format_numeric_tail.inc  原 791~890    数值格式：字面量拼装与回退（Step 7 尾 + Step 8）
// 六个 .inc 是「函数体片段」，在 vb6_Format() 函数体内被 #include（C 允许），故不用 .c/.h 后缀 ——
// 它们不是独立编译单元，单独编译会不过。片段逐行未改，局部变量与 static 表原样不动 → 零行为改动。

#include "vb6rtl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <wchar.h>
#include <wctype.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <oleauto.h>
#include <olectl.h>
#include <windows.h>
#endif

// P24-08: MessageBoxW (user32) + GetConsoleWindow (kernel32)
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")

BSTR vb6_Format(vb6_VARIANT expr, BSTR fmt) {
#include "vb6rtl_format_extract.inc"
#include "vb6rtl_format_parse.inc"
#include "vb6rtl_format_string.inc"
#include "vb6rtl_format_numeric_pre.inc"
#include "vb6rtl_format_numeric_body.inc"
#include "vb6rtl_format_numeric_tail.inc"
}


