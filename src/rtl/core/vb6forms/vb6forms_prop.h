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
// Fix 150: 返回类型必须是 wchar_t* 而非 void* —— 代码生成的
// vb6_ComPackValue 走 C11 _Generic, void* 会落入 default 分支被当成
// 对象指针打包成 VT_DISPATCH, OLEAUT32 解引用假接口指针即崩
// (NewTab 主题下拉选择 0xC0000005 实测)。wchar_t* 才会命中
// VariantString 分支打包为 VT_BSTR。
wchar_t* vb6_GetControlText(void* hwnd);
void vb6_SetControlText(void* hwnd, void* bstr);

// Value属性 (CheckBox/OptionButton: 0=Unchecked, 1=Checked, 2=Grayed)
int vb6_GetCheckValue(void* hwnd);
void vb6_SetCheckValue(void* hwnd, int value);
// 账 #128-b: OptionButton 单开一对 —— VB6 那边它的 Value 是 Boolean(读回 -1/0)，
// 而 CheckBox 的是三态 Integer；写侧必须把非 0 折回 BST_CHECKED(1)，因为 BM_SETCHECK 不吃 -1。
int vb6_GetOptionValue(void* hwnd);
void vb6_SetOptionValue(void* hwnd, int value);

// Visible属性
int vb6_GetControlVisible(void* hwnd);
void vb6_SetControlVisible(void* hwnd, int visible);

// Enabled属性
int vb6_GetControlEnabled(void* hwnd);
void vb6_SetControlEnabled(void* hwnd, int enabled);

// C29-SL-l（账 #143）: VB6 的 `控件.SetFocus`。之前这一形从没登记过，两形（带括号与不带括号）
// 都落进 vb6_ComCall(裸 HWND, L"SetFocus", NULL, 0) —— 原生控件槽里是句柄不是 IDispatch，
// 于是编得过、链接得过、跑起来一声不响。RTL 就一句 SetFocus(hwnd)（焦点属于线程输入队列，
// 与窗口可见/激活无关，所以无头跑里也真能拿到 —— C29-SL-g 的 SimStdEvent kind=4 走同一条路，
// 实测会发 WM_SETFOCUS）。拿不到焦点时原生就是回 NULL 什么都不做，本项目不伪造、不重试。
void vb6_SetControlFocus(void* hwnd);

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
// C29-SL-p 判据证人（不是 VB6 属性）：窗口真在用的字体像素高度。
// 字号那条判据只问自存的数就是自洽假绿，得同时问窗口一次（#148 那条教训）。
int vb6_ControlFontPixelHeight(void* hwnd);
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

// Fix 187: WM_CTLCOLORSTATIC 统一应用 — 子控件 Set 过 VB6_BackColor 时
// 应用 Fore/Back 色并返回背景刷; 未设色返回 0 (调用方走 DefWindowProcW)。
// 主窗体 WndProc (生成代码) 与 SSTab 容器子类共用。
LRESULT vb6_ApplyCtlColorStatic(HDC hdc, HWND child);


#ifdef __cplusplus
}
#endif
