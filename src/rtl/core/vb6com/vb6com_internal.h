// vb6com_internal.h - vb6com 模块内部共享声明 (P6 COM 互操作)
// 仅供 RTL 内部各 .c 使用; 生成代码只 include vb6com.h
//
// 拆分说明: vb6com.c 按 COM 调用层次拆为多个编译单元, 各单元由 MSVC 独立
// 编译成 .obj 再链接, 文件级 static 不跨文件可见 —— 因此把跨族共享的符号
// 集中声明在此, 定义仍留在 vb6com.c。

#ifndef VB6C3_VB6COM_INTERNAL_H
#define VB6C3_VB6COM_INTERNAL_H

#include "vb6com.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// P24-08: COM错误传播 — 将HRESULT/EXCEPINFO转换为VB6运行时错误
// VB6行为: COM方法失败时自动触发Err.Raise, 可被On Error捕获
// (定义在 vb6rtl.c; vb6com 系列不 include vb6rtl.h, 以避免 VARIANT 冲突)
extern void vb6_RaiseError(int32_t errNum, void* description);
extern int32_t vb6_err_resume_next;

// --- 跨族共享的内部辅助 (定义在 vb6com.c) ---
// COM错误->VB6错误转换
// hr: Invoke返回的HRESULT / excep: EXCEPINFO结构 / context: 调用上下文
void vb6_ComCheckError(HRESULT hr, EXCEPINFO* excep, const wchar_t* context);

// IUnknown 的 C 风格 vtable 辅助 (避免 C++ IUnknown 方法调用问题)
HRESULT vb6_UnknownQI(void* obj, REFIID riid, void** ppv);
ULONG vb6_UnknownRelease(void* obj);

#endif // VB6C3_VB6COM_INTERNAL_H
