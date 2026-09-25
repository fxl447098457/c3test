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

// P13.11b: ProgressBar properties (VB6 ProgressBar / msctls_progress32)
// Min/Max/Value: VB6 默认 0 / 100 / 0
// Orientation: 0 = ccOrientationHorizontal, 1 = ccOrientationVertical
// Scrolling:   0 = ccScrollingSmooth, 1 = ccScrollingStandard (默认)
// 见 vb6forms_progress.c —— 纯显示控件, 无事件无方法。
int32_t vb6_GetProgressBarMin(void* hwnd);
void vb6_SetProgressBarMin(void* hwnd, int32_t val);
int32_t vb6_GetProgressBarMax(void* hwnd);
void vb6_SetProgressBarMax(void* hwnd, int32_t val);
int32_t vb6_GetProgressBarValue(void* hwnd);
void vb6_SetProgressBarValue(void* hwnd, int32_t val);
int32_t vb6_GetProgressBarOrientation(void* hwnd);
void vb6_SetProgressBarOrientation(void* hwnd, int32_t val);
int32_t vb6_GetProgressBarScrolling(void* hwnd);
void vb6_SetProgressBarScrolling(void* hwnd, int32_t val);
// 设计期灌值 (未写进 .frm 的属性传 -1); 控件创建后由生成代码调用一次
void vb6_InitProgressBar(void* hwnd, int32_t min, int32_t max, int32_t value,
                         int32_t orientation, int32_t scrolling);
void vb6_ProgressBarPostCreate(void* hwnd);

// P13.11c: ImageList properties (VB6 ImageList / comctl32 ImageList_* API)
// 无窗口, 句柄存在 vb6_com_<Name> 那个 void* 槽里 (见 vb6forms_imagelist.c)。
// slot 是生成代码里 `static void* vb6_com_<Name>` 的**地址** (ImageList 无窗口, 槽里
// 存的是复刻实例指针而不是 HWND), Create 负责 malloc 并写回 *slot。
void   vb6_ImageList_Create(void** slot, int32_t width, int32_t height);
void   vb6_ImageList_Destroy(void* slot);
int32_t vb6_GetImageListImageWidth(void* slot);
void   vb6_SetImageListImageWidth(void* slot, int32_t val);
int32_t vb6_GetImageListImageHeight(void* slot);
void   vb6_SetImageListImageHeight(void* slot, int32_t val);
// ListImages.Add(index, key, picture) —— picture 是 **LoadPicture() 返回活着的 IPicture
// 对象** (VB6 StdPicture), 不是裸字节也不是用完即弃的句柄。本函数会 AddRef 一份长期持有,
// 对应 Remove/Clear/销毁时 Release; 返回 VB6 语义的 1 基 Index。
int32_t vb6_ImageList_AddPicture(void* slot, int32_t index, const wchar_t* key,
                                 void* picture);
// .frx 设计期图片: cgen 已把字节烤成 hex 数组
int32_t vb6_ImageList_AddDesignTimeImage(void* slot, const wchar_t* key,
                                         const void* data, int32_t size);
// Remove(index 或 key); Clear()
void   vb6_ImageList_RemoveImage(void* slot, const wchar_t* keyOrIndex, int32_t index);
void   vb6_ImageList_RemoveAtIndex(void* slot, int32_t index);
void   vb6_ImageList_ClearImages(void* slot);
int32_t vb6_GetImageListCount(void* slot);        // ListImages.Count
int32_t vb6_ImageListIndexAt(void* slot, int32_t index);  // ListImage.Index (传入/返回都是 1 基)
// ListImages("KeyString") 按 Key 取项 —— 生成代码里 Item 的实参是宽字符串而不是下标
void*  vb6_GetImageListKeyByKey(void* slot, const wchar_t* key);
int32_t vb6_ImageListIndexByKey(void* slot, const wchar_t* key);
void*  vb6_GetImageListKeyAt(void* slot, int32_t index);   // ListImage.Key (BSTR)
void*  vb6_GetImageListHandle(void* slot);        // 真 HIMAGELIST, 供其它控件挂接

// P13.11d: StatusBar properties (VB6 StatusBar / msctls_status32, 见 vb6forms_statusbar.c)
// 复刻口径: **不加载 mscomctl.ocx**, 用 comctl32 的 msctls_status32 自己算面板文本
// (SDK 10.0.19041.0 的 commctrl.h 里没有 SBT_CAPS/SBT_TIME/SBT_DATE, 这四个"系统面板"
//  得在 RTL 里现算 + 自己装时钟)。
// ===================== SSTab (P20-42) =====================
//   TabDlg.SSTab 复刻: comctl32 的 SysTabControl32, 不加载 TABCTL32.OCX。
//   Tab / TabCaption(i) / TabVisible(i) 的下标一律 **0 基** (与 VB6 集合的 1 基不同)。
//   TabOrientation 0=上(默认) 1=下 2=左 3=右; TabStyle 0=选项卡对话框式 1=属性页式。
//   **原型必须在这里声明**: 生成代码只 include 这一族头, 漏了就是 C 隐式声明返回 int,
//   在 x64 下把 BSTR 指针截成 32 位 → 0xC0000005 (x86 反而"看起来正常", 极易漏诊)。
void    vb6_SSTab_Init(void* hwnd, int32_t tabs, int32_t curTab, int32_t orientation,
                       int32_t tabStyle, int32_t tabsPerRow, int32_t wordWrap);
int32_t vb6_SSTab_GetTabs(void* hwnd);
void    vb6_SSTab_SetTabs(void* hwnd, int32_t n);
int32_t vb6_SSTab_GetTab(void* hwnd);
void    vb6_SSTab_SetTab(void* hwnd, int32_t idx);
void*   vb6_SSTab_GetTabCaption(void* hwnd, int32_t idx);          // BSTR
void    vb6_SSTab_SetTabCaption(void* hwnd, int32_t idx, void* bstr);
int32_t vb6_SSTab_GetTabVisible(void* hwnd, int32_t idx);          // VB6 True = -1
void    vb6_SSTab_SetTabVisible(void* hwnd, int32_t idx, int32_t v);
int32_t vb6_SSTab_GetTabOrientation(void* hwnd);
void    vb6_SSTab_SetTabOrientation(void* hwnd, int32_t v);
int32_t vb6_SSTab_GetTabStyle(void* hwnd);
void    vb6_SSTab_SetTabStyle(void* hwnd, int32_t v);
int32_t vb6_SSTab_GetTabsPerRow(void* hwnd);
void    vb6_SSTab_SetTabsPerRow(void* hwnd, int32_t v);
int32_t vb6_SSTab_GetWordWrap(void* hwnd);
void    vb6_SSTab_SetWordWrap(void* hwnd, int32_t v);
// 容器: 登记"某个子控件属于第 page 页", 切页时 RTL 只动可见性, 不动 Left
void    vb6_SSTab_RegisterChild(void* hwnd, void* childHwnd, int32_t page);
// 事件: 窗体 WndProc 收到 TCN_SELCHANGE 后调用, 返回**切换前**的页号
int32_t vb6_SSTab_OnSelChange(void* hwnd);

//   StatusBar: Align 0=None 1=Top 2=Bottom(默认) 3=Left 4=Right
//              Style 0=sbrNormal(多面板, 默认) 1=sbrSimple(单格, 读 SimpleText)
//   Panels.Add(index, key, text) 返回 **VB6 语义的 1 基 Index**; 插到中间时后面整体后移。
//   Panels 的下标参数一律是 1 基 (生成代码传的就是 VB6 的 Index, RTL 第一步减一)。
//   Panel.AutoSize: 0=sbrFixed 1=sbrSpring 2=sbrContents
//   Panel.Style:   0=sbrText 1=sbrCaps 2=sbrNum 5=sbrTime 6=sbrDate
void    vb6_StatusBar_Init(void* hwnd, int32_t style, int32_t align);
void    vb6_StatusBar_Destroy(void* hwnd);
int32_t vb6_StatusBar_GetStyle(void* hwnd);
void    vb6_StatusBar_SetStyle(void* hwnd, int32_t val);
void*   vb6_StatusBar_GetSimpleText(void* hwnd);          // BSTR, NULL = 空
void    vb6_StatusBar_SetSimpleText(void* hwnd, const wchar_t* text);
int32_t vb6_StatusBar_GetAlign(void* hwnd);
void    vb6_StatusBar_SetAlign(void* hwnd, int32_t val);
// --- Panels 集合 ---
int32_t vb6_StatusBar_GetPanelsCount(void* hwnd);
int32_t vb6_StatusBar_AddPanel(void* hwnd, int32_t index, const wchar_t* key,
                               const wchar_t* text);
void    vb6_StatusBar_RemovePanel(void* hwnd, const wchar_t* keyOrIndex, int32_t index);
void    vb6_StatusBar_ClearPanels(void* hwnd);
// --- 面板取值 (index 一律 1 基) ---
void*   vb6_StatusBar_GetPanelText(void* hwnd, int32_t index);
void*   vb6_StatusBar_GetPanelTextByKey(void* hwnd, const wchar_t* key);
void*   vb6_StatusBar_GetPanelKey(void* hwnd, int32_t index);
void*   vb6_StatusBar_GetPanelKeyByKey(void* hwnd, const wchar_t* key);
void    vb6_StatusBar_SetPanelKey(void* hwnd, int32_t index, const wchar_t* key);
void    vb6_StatusBar_SetPanelText(void* hwnd, int32_t index, const wchar_t* text);
int32_t vb6_StatusBar_GetPanelIndexByKey(void* hwnd, const wchar_t* key);  // 0 = 未找到
int32_t vb6_StatusBar_GetPanelWidth(void* hwnd, int32_t index);
void    vb6_StatusBar_SetPanelWidth(void* hwnd, int32_t index, int32_t val);
int32_t vb6_StatusBar_GetPanelMinWidth(void* hwnd, int32_t index);
void    vb6_StatusBar_SetPanelMinWidth(void* hwnd, int32_t index, int32_t val);
int32_t vb6_StatusBar_GetPanelAutoSize(void* hwnd, int32_t index);
void    vb6_StatusBar_SetPanelAutoSize(void* hwnd, int32_t index, int32_t val);
int32_t vb6_StatusBar_GetPanelStyle(void* hwnd, int32_t index);
void    vb6_StatusBar_SetPanelStyle(void* hwnd, int32_t index, int32_t val);
void*   vb6_StatusBar_GetPanelToolTip(void* hwnd, int32_t index);
void    vb6_StatusBar_SetPanelToolTip(void* hwnd, int32_t index, const wchar_t* text);

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
