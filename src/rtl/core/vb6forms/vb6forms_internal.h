// vb6forms_internal.h - vb6forms 模块内部共享声明 (P7 Win32 窗体运行时)
// 仅供 RTL 内部各 .c 使用; 生成代码只 include vb6forms.h
//
// 拆分说明: vb6forms.c 按控件/窗体功能家族拆为多个编译单元, 各单元由 MSVC
// 独立编译成 .obj 再链接, 文件级 static 不跨文件可见 —— 因此把跨族共享的
// 符号集中声明在此, 定义仍留在 vb6forms.c。

#ifndef VB6C3_VB6FORMS_INTERNAL_H
#define VB6C3_VB6FORMS_INTERNAL_H

#include "vb6forms.h"

#include <stdlib.h>   /* malloc (Fix 190 转码助手) */

// --- 跨族共享的内部状态 (定义在 vb6forms.c) ---
// 应用实例句柄 (vb6_SetAppInstance 设置, 多处属性设置与控件创建需要)
extern HINSTANCE g_hInstance;

// ============================================================
// Fix 190: 源码字符串 = UTF-8, 窗口层 = UTF-16
//
// C3 内部统一 UTF-8 (.frm/.bas 读入即转码), 生成的 C 也带 /utf-8 编译, 所以
// 生成代码传进来的**每一个字面量**(窗体 Caption、控件 Caption/Text、.frx
// 文本、控件名 …)都是 UTF-8 字节, 而不是系统 ACP。
//
// 旧实现把这些字节直接交给 CreateWindowExA/SetWindowTextA, 系统按 ACP 解释:
// CP936 机器上 "对比数据" 变成 "瀵规瘮鏁版嵁"; 韩/德/俄文更糟。正确做法是
// 窗口层全程走 W 系列: UTF-8 → UTF-16 只在边界转一次, 之后与语言无关。
//
// 放在头里做成 static inline, 供 vb6forms 各拆分单元共享 (文件级 static 不跨
// 编译单元可见)。
// ============================================================
static inline wchar_t* vb6_u8ToWideDup(const char* s) {
    if (!s) return NULL;
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (n <= 0) {
        /* 非法 UTF-8: 退回 ACP, 保证调用方拿到可用指针 */
        n = MultiByteToWideChar(CP_ACP, 0, s, -1, NULL, 0);
        if (n <= 0) return NULL;
        wchar_t* wa = (wchar_t*)malloc((size_t)n * sizeof(wchar_t));
        if (wa) MultiByteToWideChar(CP_ACP, 0, s, -1, wa, n);
        return wa;
    }
    wchar_t* w = (wchar_t*)malloc((size_t)n * sizeof(wchar_t));
    if (w) MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}

/* 栈缓冲版: 供类名/标题等短字符串使用 (超长即截断, 不分配) */
static inline void vb6_u8ToWideBuf(const char* s, wchar_t* out, int cap) {
    if (!out || cap <= 0) return;
    out[0] = 0;
    if (!s) return;
    if (MultiByteToWideChar(CP_UTF8, 0, s, -1, out, cap) <= 0)
        MultiByteToWideChar(CP_ACP, 0, s, -1, out, cap);
    out[cap - 1] = 0;
}

/* 宽 → UTF-8 (栈缓冲): 窗口层读回的 Unicode 文本转回 RTL 内部的 UTF-8 键 */
static inline void vb6_wideToU8Buf(const wchar_t* w, char* out, int cap) {
    if (!out || cap <= 0) return;
    out[0] = 0;
    if (!w) return;
    if (WideCharToMultiByte(CP_UTF8, 0, w, -1, out, cap, NULL, NULL) <= 0)
        WideCharToMultiByte(CP_ACP, 0, w, -1, out, cap, NULL, NULL);
    out[cap - 1] = 0;
}

#endif // VB6C3_VB6FORMS_INTERNAL_H
