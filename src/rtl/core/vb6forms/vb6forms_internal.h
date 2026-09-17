// vb6forms_internal.h - vb6forms 模块内部共享声明 (P7 Win32 窗体运行时)
// 仅供 RTL 内部各 .c 使用; 生成代码只 include vb6forms.h
//
// 拆分说明: vb6forms.c 按控件/窗体功能家族拆为多个编译单元, 各单元由 MSVC
// 独立编译成 .obj 再链接, 文件级 static 不跨文件可见 —— 因此把跨族共享的
// 符号集中声明在此, 定义仍留在 vb6forms.c。

#ifndef VB6C3_VB6FORMS_INTERNAL_H
#define VB6C3_VB6FORMS_INTERNAL_H

#include "vb6forms.h"

// --- 跨族共享的内部状态 (定义在 vb6forms.c) ---
// 应用实例句柄 (vb6_SetAppInstance 设置, 多处属性设置与控件创建需要)
extern HINSTANCE g_hInstance;

#endif // VB6C3_VB6FORMS_INTERNAL_H
