#pragma once
// vb6forms_prop.h - 控件属性读写 (P7.5)：Text/Caption、Value、Visible、Enabled、位置大小、字体、前景背景色
// 由 vb6forms.h 伞头按固定顺序 include，不要单独使用
// 内容 = 拆分前 vb6forms.h 第 142~203 行，逐行未改

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 控件属性读写 (P7.5)
// ============================================================

// Text/Caption属性 (TextBox/Label/CommandButton/Form)
// 返回BSTR (调用者负责vb6_SysFreeString释放, 或直接传给函数)
void* vb6_GetControlText(void* hwnd);
void vb6_SetControlText(void* hwnd, void* bstr);

// Value属性 (CheckBox/OptionButton: 0=Unchecked, 1=Checked, 2=Grayed)
int vb6_GetCheckValue(void* hwnd);
void vb6_SetCheckValue(void* hwnd, int value);

// Visible属性
int vb6_GetControlVisible(void* hwnd);
void vb6_SetControlVisible(void* hwnd, int visible);

// Enabled属性
int vb6_GetControlEnabled(void* hwnd);
void vb6_SetControlEnabled(void* hwnd, int enabled);

// P11.8: Position/Size attributes (all visible controls, in pixels)
// VB6 uses twips internally, but Win32 uses pixels; RTL handles conversion
int vb6_GetControlLeft(void* hwnd);
void vb6_SetControlLeft(void* hwnd, int left);
int vb6_GetControlTop(void* hwnd);
void vb6_SetControlTop(void* hwnd, int top);
int vb6_GetControlWidth(void* hwnd);
void vb6_SetControlWidth(void* hwnd, int width);
int vb6_GetControlHeight(void* hwnd);
void vb6_SetControlHeight(void* hwnd, int height);

// P11.8: hWnd attribute (read-only, returns the Win32 HWND as pointer)
void* vb6_GetControlHwnd(void* hwnd);

// P13.1: Font properties (all visible controls with text)
// FontName: returns BSTR (caller responsible for SysFreeString)
void* vb6_GetControlFontName(void* hwnd);
void vb6_SetControlFontName(void* hwnd, void* bstrName);
// FontSize: returns VB6 Single (points) as float
float vb6_GetControlFontSize(void* hwnd);
void vb6_SetControlFontSize(void* hwnd, float sizePt);
// FontBold: VB6 True=-1, False=0
int vb6_GetControlFontBold(void* hwnd);
void vb6_SetControlFontBold(void* hwnd, int bold);
// FontItalic
int vb6_GetControlFontItalic(void* hwnd);
void vb6_SetControlFontItalic(void* hwnd, int italic);
// FontUnderline
int vb6_GetControlFontUnderline(void* hwnd);
void vb6_SetControlFontUnderline(void* hwnd, int underline);
// FontStrikethrough
int vb6_GetControlFontStrikethrough(void* hwnd);
void vb6_SetControlFontStrikethrough(void* hwnd, int strike);

// P13.2: ForeColor/BackColor (all visible controls)
// Returns OLE color (VB6 Long), 0x00BBGGRR format
int vb6_GetControlForeColor(void* hwnd);
void vb6_SetControlForeColor(void* hwnd, int color);
int vb6_GetControlBackColor(void* hwnd);
void vb6_SetControlBackColor(void* hwnd, int color);


#ifdef __cplusplus
}
#endif
