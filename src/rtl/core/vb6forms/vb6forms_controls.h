#pragma once
// vb6forms_controls.h - 动态加载控件 (Controls.Add)
// 由 vb6forms.h 伞头按固定顺序 include，不要单独使用
// 内容 = 拆分前 vb6forms.h 第 568~579 行，逐行未改

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 动态加载控件 (Controls.Add)
// ============================================================

// 设置Form窗口的IDispatch指针 (Form创建后调用)
// 用于 Me.Controls.Add 等 COM 属性访问
void vb6_Form_SetDispatch(void* hwnd, void* pDispatch);

// Me.Controls.Add(progId, name) → 返回新控件的IDispatch*
// 等价于 VB6: Set ctrl = Me.Controls.Add("WMPlayer.OCX", "WMP1")
void* vb6_Form_ControlsAdd(void* hwnd, const wchar_t* progId, const wchar_t* ctrlName);


#ifdef __cplusplus
}
#endif
