#pragma once
// vb6forms_prop_ctrl.h - 控件专有属性 (P13.3~P13.15)：ListBox/ComboBox、TextBox、Alignment/Align、Tab*、ToolTip、Tag、MousePointer、BorderStyle、ScrollBar、Timer、Sel*、CommandButton Default/Cancel
// 由 vb6forms.h 伞头按固定顺序 include，不要单独使用
// 内容 = 拆分前 vb6forms.h 第 204~328 行，逐行未改

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


// ============================================================
// 
// P13.3: ListBox/ComboBox properties
// ListCount: number of items
int vb6_GetListCount(void* hwnd);
// ListIndex: current selection index (-1=none)
int vb6_GetListIndex(void* hwnd);
void vb6_SetListIndex(void* hwnd, int index);
// List(idx): get item text by index (returns BSTR)
void* vb6_GetListItem(void* hwnd, int index);
// AddItem: add string to list
void vb6_AddItem(void* hwnd, void* bstrItem);
// RemoveItem: remove item by index
void vb6_RemoveItem(void* hwnd, int index);
// Clear: remove all items
void vb6_ClearList(void* hwnd);
// P14.4.3: ListBox/ComboBox extended properties
// SetListItem: set item text by index (List(idx) = value)
void vb6_SetListItem(void* hwnd, int index, void* bstrItem);
// Selected(idx): get/set selection state (for MultiSelect ListBox)
int vb6_GetSelected(void* hwnd, int index);
void vb6_SetSelected(void* hwnd, int index, int selected);
// ItemData(idx): get/set per-item Long data
int32_t vb6_GetItemData(void* hwnd, int index);
void vb6_SetItemData(void* hwnd, int index, int32_t data);
// NewIndex: index of most recently added item
int vb6_GetNewIndex(void* hwnd);

// P13.4: TextBox-specific properties
// MultiLine: read/write, VB6 True=-1
int vb6_GetMultiLine(void* hwnd);
void vb6_SetMultiLine(void* hwnd, int multiline);
// ScrollBars: 0=None, 1=Horizontal, 2=Vertical, 3=Both
int vb6_GetScrollBars(void* hwnd);
void vb6_SetScrollBars(void* hwnd, int scrollbars);
// MaxLength: maximum text length (0=unlimited)
int vb6_GetMaxLength(void* hwnd);
void vb6_SetMaxLength(void* hwnd, int maxlength);
// PasswordChar: returns BSTR (single char)
void* vb6_GetPasswordChar(void* hwnd);
void vb6_SetPasswordChar(void* hwnd, void* bstrChar);
// Locked: VB6 True=-1
int vb6_GetLocked(void* hwnd);
void vb6_SetLocked(void* hwnd, int locked);

// P13.5: Alignment (TextBox/Label/CheckBox/OptionButton/Frame)
// 0=Left, 1=Right, 2=Center
int vb6_GetAlignment(void* hwnd);
void vb6_SetAlignment(void* hwnd, int align);

// P13.5b: Align (PictureBox/Frame 停靠到父窗体客户区边缘)
// 0=vbAlignNone, 1=vbAlignTop, 2=vbAlignBottom, 3=vbAlignLeft, 4=vbAlignRight
int vb6_GetControlAlign(void* hwnd);
void vb6_SetControlAlign(void* hwnd, int align);

// P13.6: TabIndex/TabStop
int vb6_GetTabIndex(void* hwnd);
void vb6_SetTabIndex(void* hwnd, int index);
int vb6_GetTabStop(void* hwnd);
void vb6_SetTabStop(void* hwnd, int tabstop);

// P20-12: CausesValidation (default True=-1)
int vb6_GetCausesValidation(void* hwnd);
void vb6_SetCausesValidation(void* hwnd, int causes);

// P13.8: ToolTipText (returns BSTR)
void* vb6_GetToolTipText(void* hwnd);
void vb6_SetToolTipText(void* hwnd, void* bstrText);

// P13.9: Tag (returns BSTR, stored as window property)
void* vb6_GetControlTag(void* hwnd);
void vb6_SetControlTag(void* hwnd, void* bstrTag);

// P13.7: MousePointer/MouseIcon (all visible controls)
// MousePointer: 0=Default, 1=Arrow, 2=Cross, 3=I-Beam, 4=Icon, 5=Size, 6=Size NE-SW,
//   7=Size N-S, 8=Size NW-SE, 9=Size W-E, 10=Up Arrow, 11=Hourglass, 12=No Drop,
//   13=App Starting, 14=Help, 15=Size All, 99=Custom
int vb6_GetMousePointer(void* hwnd);
void vb6_SetMousePointer(void* hwnd, int pointer);
// MouseIcon: custom cursor handle (stored as window property)
void* vb6_GetMouseIcon(void* hwnd);
void vb6_SetMouseIcon(void* hwnd, void* hCursor);

// P13.10: BorderStyle (Form/TextBox/ComboBox/ListBox)
// Form: 0=None, 1=Fixed Single, 2=Sizable, 3=Fixed Dialog, 4=Fixed ToolWindow, 5=Sizable ToolWindow
// TextBox: 0=None, 1=Fixed Single
int vb6_GetBorderStyle(void* hwnd);
void vb6_SetBorderStyle(void* hwnd, int style);

// P13.11: ScrollBar properties (HScrollBar/VScrollBar)
int vb6_GetScrollMin(void* hwnd);
void vb6_SetScrollMin(void* hwnd, int min);
int vb6_GetScrollMax(void* hwnd);
void vb6_SetScrollMax(void* hwnd, int max);
int vb6_GetScrollValue(void* hwnd);
void vb6_SetScrollValue(void* hwnd, int value);
int vb6_GetLargeChange(void* hwnd);
void vb6_SetLargeChange(void* hwnd, int change);
int vb6_GetSmallChange(void* hwnd);
void vb6_SetSmallChange(void* hwnd, int change);

// P13.12: Timer properties
// Interval: milliseconds (0=disabled)
int vb6_GetTimerInterval(void* hwnd);
void vb6_SetTimerInterval(void* hwnd, int interval);
// Timer Enabled (separate from generic Enabled for Timer specifics)
int vb6_GetTimerEnabled(void* hwnd);
void vb6_SetTimerEnabled(void* hwnd, int enabled);

// P13.14: TextBox selection properties
int vb6_GetSelStart(void* hwnd);
void vb6_SetSelStart(void* hwnd, int start);
int vb6_GetSelLength(void* hwnd);
void vb6_SetSelLength(void* hwnd, int length);
void* vb6_GetSelText(void* hwnd);  // returns BSTR
void vb6_SetSelText(void* hwnd, void* bstrText);

// P13.15: CommandButton Default/Cancel
// Default: True if button responds to Enter key
int vb6_GetDefaultButton(void* hwnd);
void vb6_SetDefaultButton(void* hwnd, int isDefault);
// Cancel: True if button responds to Esc key
int vb6_GetCancelButton(void* hwnd);
void vb6_SetCancelButton(void* hwnd, int isCancel);

#ifdef __cplusplus
}
#endif
