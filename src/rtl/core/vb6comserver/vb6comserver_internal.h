// vb6comserver_internal.h - vb6comserver 模块内部共享声明 (P6.6 ActiveX DLL)
// 仅供 RTL 内部各 .c 使用; 生成代码只 include vb6comserver.h
//
// 拆分说明: vb6comserver.c 按 COM 接口家族拆为多个编译单元, 各单元由 MSVC
// 独立编译成 .obj 再链接, 文件级 static 不跨文件可见 —— 因此把跨族共享的
// 符号集中声明在此, 定义仍留在 vb6comserver.c。

#ifndef VB6C3_VB6COMSERVER_INTERNAL_H
#define VB6C3_VB6COMSERVER_INTERNAL_H

#include "vb6comserver.h"

// --- 共享 IID 常量 (定义在 vb6comserver.c) ---
extern const IID IID_IUnknown_;
extern const IID IID_IDispatch_;
extern const IID IID_IClassFactory_;
extern const IID IID_IConnectionPointContainer_;
extern const IID IID_IConnectionPoint_;
extern const IID IID_IProvideClassInfo2_;

// --- 共享工具函数 (定义在 vb6comserver.c) ---
// ANSI CLSID 字符串 -> GUID (MSVC 不导出 vb6_CLSIDFromStrA, 手工转换)
HRESULT vb6_CLSIDFromStrA(const char* str, CLSID* clsid);

#endif // VB6C3_VB6COMSERVER_INTERNAL_H
