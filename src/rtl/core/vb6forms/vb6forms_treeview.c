// vb6forms_treeview.c — ai/029 C29-8a: VB6 TreeView 控件的标量属性面 (原生 SysTreeView32)
//
// 复刻口径 (ai/内置控件/TreeView 控件.md + D6): **不加载 MSCOMCTL.OCX** (它是 32 位 inproc,
//   x64 进程里 CoCreateInstance 直接失败)。SysTreeView32 由 comctl32 注册, 创建那一刀早就在
//   (controlTypeToWin32Class 有格, 实测 vb6_CreateControl 拿到过真句柄), 本文件补的是**属性面**:
//   此前 cgen 的读写表里 TreeView 零格 ⇒ 属性一律落到通用兜底 vb6_ComGetObjectProp(裸 HWND, L"…"),
//   于是 `tv1.CheckBoxes = True` 静默丢、读回空串 (029 §九 C29-8 前置测量那四条读数)。
//
// 本批范围 = 不吃 Nodes 集合的那几格:
//   LineStyle      0 = tvwTreeLines(默认) / 1 = tvwRootLines  → TVS_HASLINES ± TVS_LINESATROOT
//   CheckBoxes     Boolean, VB6 默认 False                   → TVS_CHECKBOXES
//   HotTracking    Boolean, VB6 默认 False                   → TVS_TRACKSELECT
//   HideSelection  Boolean, VB6 默认 True                     → TVS_SHOWSELALWAYS 取反
//   Indentation    缇 (Win32 侧是像素)                        → TVM_SETINDENT / TVM_GETINDENT
//   留 C29-8b (等 C29-3 立完成员对象机制): Style 四态(带图标)、PathSeparator、LabelEdit、
//   Sorted、SelectedItem、Nodes/Node 一族、TVN_* 事件。
//
// 真值放哪: 前四条的**真值就是窗口样式位本身** —— getter 直接读 GWL_STYLE, 读数与观感同源,
//   答不出"表里写着开、窗口上却没框"这种假话。Indentation 例外: VB6 侧存缇、Win32 侧是像素,
//   且 TVM_SETINDENT 会把超界值静默钳掉, 所以缇值另存窗口属性 (照 vb6forms_progress.c 的
//   SetProp +1 偏移手法: GetPropW 对"未设置"与"存了 0"都返回 NULL, 不偏移就把 `= 0` 当成没写)。

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#endif

#include "vb6forms.h"
#include "vb6forms_internal.h"

#ifdef _WIN32

// commctrl.h 在低 _WIN32_IE 下不给这几个样式位, 且 cgen 那侧也按字面量发 —— 两边都钉死数值。
#ifndef TVS_HASBUTTONS
#define TVS_HASBUTTONS     0x0001L
#endif
#ifndef TVS_HASLINES
#define TVS_HASLINES       0x0002L
#endif
#ifndef TVS_LINESATROOT
#define TVS_LINESATROOT    0x0004L
#endif
#ifndef TVS_SHOWSELALWAYS
#define TVS_SHOWSELALWAYS  0x0020L
#endif
#ifndef TVS_CHECKBOXES
#define TVS_CHECKBOXES     0x0100L
#endif
#ifndef TVS_TRACKSELECT
#define TVS_TRACKSELECT    0x0200L
#endif
#ifndef TVM_SETINDENT
#define TVM_SETINDENT      (WM_USER + 104)
#endif
#ifndef TVM_GETINDENT
#define TVM_GETINDENT      (WM_USER + 105)
#endif

// TVM_SETINDENT 的合法域是 0..50 像素 (超界由控件钳掉), 缇换算后按同一上限钳,
// 免得"钳位"发生在控件里而 VB6 侧读数与显示不一致。
#define VB6_TV_INDENT_MAX_PX 50

static const wchar_t kTvIndentation[] = L"VB6_TV_Indentation";

static DWORD vb6_TvStyle(void* hwnd) {
    return (DWORD)(UINT_PTR)GetWindowLongPtrW((HWND)hwnd, GWL_STYLE);
}

// 改样式位后必须让控件重算非客户区/重绘: 复选框位改的是每个节点的度量, 只 SetWindowLong
// 的话旧度量还在用 (实测要 SWP_FRAMECHANGED 才立刻画得出方框)。
static void vb6_TvSetBit(void* hwnd, DWORD mask, int on) {
    if (!hwnd) return;
    DWORD cur = vb6_TvStyle(hwnd);
    DWORD next = on ? (cur | mask) : (cur & ~mask);
    if (next == cur) return;
    SetWindowLongPtrW((HWND)hwnd, GWL_STYLE, (LONG_PTR)next);
    SetWindowPos((HWND)hwnd, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    InvalidateRect((HWND)hwnd, NULL, TRUE);
}

// VB6 的 True 是 -1, 不是 1 (与 vb6_GetControlVisible 同口径)。
static int32_t vb6_TvBitAsVbBool(void* hwnd, DWORD mask, int32_t defIfNoHwnd) {
    if (!hwnd) return defIfNoHwnd;
    return (vb6_TvStyle(hwnd) & mask) ? -1 : 0;
}

// ---------------- LineStyle ----------------
// 0 = tvwTreeLines: 画树线但不画根节点那一层; 1 = tvwRootLines: 根层也画线。
// 两种取值都带 TVS_HASLINES (区别只在 LINESATROOT), 所以 setter 得把 HASLINES 一并立住 ——
// 单独改 LINESATROOT 而 HASLINES 关着的话, 观感是"没有线", 与读数不符。
void vb6_TreeView_SetLineStyle(void* hwnd, int32_t val) {
    if (!hwnd) return;
    vb6_TvSetBit(hwnd, TVS_HASLINES, 1);
    vb6_TvSetBit(hwnd, TVS_LINESATROOT, val != 0);
}

int32_t vb6_TreeView_GetLineStyle(void* hwnd) {
    if (!hwnd) return 0;
    return (vb6_TvStyle(hwnd) & TVS_LINESATROOT) ? 1 : 0;
}

// ---------------- CheckBoxes ----------------
void vb6_TreeView_SetCheckBoxes(void* hwnd, int32_t val) {
    vb6_TvSetBit(hwnd, TVS_CHECKBOXES, val != 0);
}

int32_t vb6_TreeView_GetCheckBoxes(void* hwnd) {
    return vb6_TvBitAsVbBool(hwnd, TVS_CHECKBOXES, 0);
}

// ---------------- HotTracking ----------------
void vb6_TreeView_SetHotTracking(void* hwnd, int32_t val) {
    vb6_TvSetBit(hwnd, TVS_TRACKSELECT, val != 0);
}

int32_t vb6_TreeView_GetHotTracking(void* hwnd) {
    return vb6_TvBitAsVbBool(hwnd, TVS_TRACKSELECT, 0);
}

// ---------------- HideSelection ----------------
// VB6 语义与 Win32 那一位是**反**的: HideSelection=True (默认) = 失焦时不画选中项,
// 而 Win32 的 TVS_SHOWSELALWAYS = 失焦仍画选中。
void vb6_TreeView_SetHideSelection(void* hwnd, int32_t val) {
    vb6_TvSetBit(hwnd, TVS_SHOWSELALWAYS, val == 0);
}

int32_t vb6_TreeView_GetHideSelection(void* hwnd) {
    if (!hwnd) return -1;   // VB6 默认 True
    return (vb6_TvStyle(hwnd) & TVS_SHOWSELALWAYS) ? 0 : -1;
}

// ---------------- Indentation ----------------
void vb6_TreeView_SetIndentation(void* hwnd, int32_t twips) {
    if (!hwnd) return;
    if (twips < 0) twips = 0;
    SetPropW((HWND)hwnd, kTvIndentation, (HANDLE)(INT_PTR)(twips + 1));
    int px = vb6_TwipToX(twips);
    if (px > VB6_TV_INDENT_MAX_PX) px = VB6_TV_INDENT_MAX_PX;
    SendMessageW((HWND)hwnd, TVM_SETINDENT, 0, (LPARAM)px);
}

int32_t vb6_TreeView_GetIndentation(void* hwnd) {
    if (!hwnd) return 0;
    HANDLE stored = GetPropW((HWND)hwnd, kTvIndentation);
    if (stored) return (int32_t)(INT_PTR)stored - 1;
    // 没写过 ⇒ 读控件自己的当前值 (创建时 TVM_SETINDENT 尚未发过), 换算回缇。
    int px = (int)(INT_PTR)SendMessageW((HWND)hwnd, TVM_GETINDENT, 0, 0);
    int dpi = vb6_DpiX();
    return dpi > 0 ? (int32_t)MulDiv(px, 1440, dpi) : 0;
}

// ---------------- 设计期初值 ----------------
// cgen 在建窗后按 .frm 里**写过**的属性发这一条; -999 = 未写 (不动窗口)。
// 为什么不用 ProgressBar 那套 -1 当哨兵: VB6 的布尔属性序列化成 `CheckBoxes = -1`,
// -1 恰好就是 True, 拿它当"未指定"等于设计期永远勾不上复选框。
void vb6_TreeView_Init(void* hwnd, int32_t lineStyle, int32_t indentation,
                       int32_t checkboxes, int32_t hotTracking, int32_t hideSelection) {
    if (!hwnd) return;
    if (lineStyle     != -999) vb6_TreeView_SetLineStyle(hwnd, lineStyle);
    if (indentation   != -999) vb6_TreeView_SetIndentation(hwnd, indentation);
    if (checkboxes    != -999) vb6_TreeView_SetCheckBoxes(hwnd, checkboxes);
    if (hotTracking   != -999) vb6_TreeView_SetHotTracking(hwnd, hotTracking);
    if (hideSelection != -999) vb6_TreeView_SetHideSelection(hwnd, hideSelection);
}

#endif // _WIN32
