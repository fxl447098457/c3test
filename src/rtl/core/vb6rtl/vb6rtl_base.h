#pragma once
// vb6rtl_base.h - VB6 运行时公共基础头：标准库 / 平台 include
// 由 vb6rtl.h 伞头按固定顺序 include，不要单独使用

#include <stdint.h>
#include <stddef.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>

#ifdef _WIN32
#include <windows.h>
#include <oleauto.h>
#endif
