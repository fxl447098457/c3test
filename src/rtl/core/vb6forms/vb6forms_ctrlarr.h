#pragma once
// vb6forms_ctrlarr.h - VB6 控件数组 (P7.6)：稀疏 HWND 数组 + Load/Unload
// 由 vb6forms.h 伞头按固定顺序 include，不要单独使用
// 内容 = 拆分前 vb6forms.h 第 450~488 行，逐行未改

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 控件数组 (P7.6)
// ============================================================
// VB6控件数组: 同名控件带不同Index, 共享事件处理器(Index参数)
// 运行时使用稀疏数组存储HWND, 最大256个元素

#define VB6_CTRLARR_MAX 256

// 控件数组描述符 (编译器生成 static vb6_CtrlArr 变量)
typedef struct vb6_CtrlArr {
    void* hwnds[VB6_CTRLARR_MAX];  // HWND数组, 按Index索引
    int   count;                     // 已有元素数
    int   lowerBound;                // 最小Index (通常0)
    int   upperBound;                // 最大Index
} vb6_CtrlArr;

// 初始化控件数组
void vb6_CtrlArr_Init(vb6_CtrlArr* arr);

// 设置数组元素的HWND
void vb6_CtrlArr_SetAt(vb6_CtrlArr* arr, int index, void* hwnd);

// 获取数组元素的HWND (返回NULL表示索引越界或元素未创建)
void* vb6_CtrlArr_GetAt(const vb6_CtrlArr* arr, int index);

// 获取数组大小
int vb6_CtrlArr_GetCount(const vb6_CtrlArr* arr);

// 获取数组下界
int vb6_CtrlArr_LBound(const vb6_CtrlArr* arr);

// 获取数组上界
int vb6_CtrlArr_UBound(const vb6_CtrlArr* arr);

// 动态加载控件数组元素 (VB6 Load语句)
void* vb6_CtrlArr_Load(vb6_CtrlArr* arr, int index, void* hParent, void* hInstance);

// 动态卸载控件数组元素 (VB6 Unload语句)
void vb6_CtrlArr_Unload(vb6_CtrlArr* arr, int index);


#ifdef __cplusplus
}
#endif
