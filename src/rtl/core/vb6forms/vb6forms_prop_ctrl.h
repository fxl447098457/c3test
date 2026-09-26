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

// C29-3: ListImages 集合 / ListImage 成员对象 —— **真 IDispatch** (见 vb6forms_memberobj.c)。
// 这是 ai/029 D1 "成员对象走真 IDispatch" 的立样: `Set itm = ...Add(..)` / `itm.Key` /
// `For Each n In ListImages` 都靠它。后面 ListView(ListItems/ColumnHeaders) / TreeView(Nodes)
// / StatusBar(Panels) / Toolbar(Buttons) 按同一形状继续挂 kind。
// **原型必须在这里声明** (同下面 SSTab 那条注释): 生成代码只 include 这一族头,
// 漏了就是 C 隐式声明返回 int → x64 把指针截成 32 位 → 0xC0000005。
void*  vb6_ImageList_ListImages(void* slot);               // ImageList1.ListImages → 集合对象
void*  vb6_ImageList_ListImages_Add(void* slot, int32_t index, const wchar_t* key, void* pic);
                                                           // ListImages.Add(..) → ListImage 对象
void*  vb6_ImageList_PictureAt(void* slot, int32_t index);  // ListImage.Picture (1 基)

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

// ===================== OLE 拖放 (P20-44) =====================
//   目标侧: OLEDropMode<>0 时 Register; 处理器直接注册成回调 (签名与 C3 生成的
//   `void X_OLEDragDrop(void** Data, int32_t* Effect, int16_t* Button,
//    int16_t* Shift, float* X, float* Y)` 一致, 不需要 thunk)。
//   **原型必须在这里**: 生成代码只 include 这一族头, 漏了 = x64 隐式 int 截指针。
int32_t vb6_OLEDrop_Register(void* hwnd);
void    vb6_OLEDrop_Revoke(void* hwnd);
void    vb6_OLEDrop_RevokeAll(void);
void    vb6_OLEDrop_SetHandler(void* hwnd, int32_t kind, void* cb);
//   DataObject 取值 (GetText 返回 BSTR 副本)
//   造 DataObject (目标/源两侧都用): text 可空; files/fileCount 是 CF_HDROP 清单。
//   **必须有原型**: 生成码会这样调 —— 漏原型 = 隐式 int = x64 指针截断 (实测崩)。
void*   vb6_oleDD_MakeDataObject(const wchar_t* text, wchar_t** files, int32_t fileCount);
void*   vb6_oleDD_GetText(void* dataObj);
int32_t vb6_oleDD_GetFileCount(void* dataObj);
const wchar_t* vb6_oleDD_GetFile(void* dataObj, int32_t idx);
//   源侧: X.OLEDrag → 造 DataObject(控件文本) + 注册源事件 + DoDragDrop。
//   源事件 (SetHandler kind 2..5): OLEStartDrag/OLESetData/OLEGiveFeedback/OLECompleteDrag
int32_t vb6_OLEDrag_Start(void* hwnd, void* dataObj, int32_t allowedEffects);
void    vb6_oleDD_SetText(void* dataObj, void* bstrText);
void    vb6_oleDD_Clear(void* dataObj);
void*   vb6_oleDD_GetFileBstr(void* dataObj, int32_t idx);

// ===================== ListView (P20-45) =====================
//   **全部原型必须在这里**: 生成代码只 include 这一族头。漏一个 = C 隐式声明返回 int
//   = x64 把指针截成 32 位 (实测过, 崩在第一次解引用)。
//   下标口径: ColumnHeaders/ListItems **1 基**; SubItems(i) **1 基且 i=1 是第 2 列**;
//   ColumnHeaders.Add 的 index 是插入位 (<=0 追加)。True 一律 = -1。
void    vb6_ListView_Init(void* hwnd, int32_t view, int32_t gridLines, int32_t fullRowSelect,
                          int32_t multiSelect, int32_t checkBoxes, int32_t hideHeaders,
                          int32_t allowColReorder, int32_t labelEdit);
int32_t vb6_ListView_GetView(void* hwnd);
int32_t vb6_ListView_GetGridLines(void* hwnd);
int32_t vb6_ListView_GetFullRowSelect(void* hwnd);
int32_t vb6_ListView_GetMultiSelect(void* hwnd);
int32_t vb6_ListView_GetCheckBoxes(void* hwnd);
int32_t vb6_ListView_GetHideColumnHeaders(void* hwnd);
int32_t vb6_ListView_GetAllowColumnReorder(void* hwnd);
int32_t vb6_ListView_GetLabelEdit(void* hwnd);
int32_t vb6_ListView_GetSorted(void* hwnd);
int32_t vb6_ListView_GetSortKey(void* hwnd);
int32_t vb6_ListView_GetSortOrder(void* hwnd);
void    vb6_ListView_SetView(void* hwnd, int32_t val);
void    vb6_ListView_SetGridLines(void* hwnd, int32_t val);
void    vb6_ListView_SetFullRowSelect(void* hwnd, int32_t val);
void    vb6_ListView_SetMultiSelect(void* hwnd, int32_t val);
void    vb6_ListView_SetCheckBoxes(void* hwnd, int32_t val);
void    vb6_ListView_SetHideColumnHeaders(void* hwnd, int32_t val);
void    vb6_ListView_SetAllowColumnReorder(void* hwnd, int32_t val);
void    vb6_ListView_SetLabelEdit(void* hwnd, int32_t val);
void    vb6_ListView_SetSorted(void* hwnd, int32_t val);
void    vb6_ListView_SetSortKey(void* hwnd, int32_t val);
void    vb6_ListView_SetSortOrder(void* hwnd, int32_t val);
//   列头
int32_t vb6_ListView_GetColumnCount(void* hwnd);
int32_t vb6_ListView_AddColumn(void* hwnd, int32_t idx, void* keyBstr, void* textBstr,
                               int32_t width, int32_t align);
void*   vb6_ListView_GetColumnText(void* hwnd, int32_t idx);
void    vb6_ListView_SetColumnText(void* hwnd, int32_t idx, void* bstr);
int32_t vb6_ListView_GetColumnWidth(void* hwnd, int32_t idx);
void    vb6_ListView_SetColumnWidth(void* hwnd, int32_t idx, int32_t w);
int32_t vb6_ListView_GetColumnAlign(void* hwnd, int32_t idx);
void    vb6_ListView_SetColumnAlign(void* hwnd, int32_t idx, int32_t val);
void    vb6_ListView_RemoveColumn(void* hwnd, int32_t idx);
// C29-7 事件面: WM_NOTIFY 的 LVN_* → 1 基下标 (0 = 与本控件无关), 再由下标取成员对象。
// 生成代码在 WM_NOTIFY 分支里用 (见 cgen_form_wndproc_dispatch.inc)。
int32_t vb6_ListView_OnNotify(void* hwnd, int32_t code, void* lParam);
// ===================== OLE 容器 (C29-OLE, 判据只本地跑不进 CI) =====================
// 窗口类 VB6_OLECONTAINER 自注册 (vb6_OleCon_RegisterClasses, cgen 在窗体创建前发射)。
// 嵌入对象依赖目标机器的 OLE 服务器 —— 判据用系统自带 Package (packager.dll, 双架构都有)。
int       vb6_RegisterOleConClass(void* hInstance);                // 进程一次, 窗体创建前调
// ===================== Data 控件 (C29-Data, 判据走 Text ISAM 零建库) =====================
// 不可见类 VB6_DATA 自注册。后端 = odbc32.dll (系统自带, 动态加载);
// provider 由用户 Connect 属性给完整 ODBC 连接串 (不替用户挑引擎)。
int       vb6_RegisterDataClass(void* hInstance);
void      vb6_Data_Init(void* hwnd, const wchar_t* databaseName, const wchar_t* recordSource,
                        const wchar_t* connect);
void      vb6_Data_SetDatabaseName(void* hwnd, const wchar_t* v);
void      vb6_Data_SetRecordSource(void* hwnd, const wchar_t* v);
void      vb6_Data_SetConnect(void* hwnd, const wchar_t* v);
wchar_t*  vb6_Data_GetDatabaseName(void* hwnd);
wchar_t*  vb6_Data_GetRecordSource(void* hwnd);
wchar_t*  vb6_Data_GetConnect(void* hwnd);
int       vb6_Data_Refresh(void* hwnd);
int32_t   vb6_Data_BOF(void* hwnd);
int32_t   vb6_Data_EOF(void* hwnd);
int32_t   vb6_Data_RecordCount(void* hwnd);
int32_t   vb6_Data_FieldCount(void* hwnd);
int32_t   vb6_Data_CurrentRow(void* hwnd);
void      vb6_Data_MoveFirst(void* hwnd);
void      vb6_Data_MoveLast(void* hwnd);
void      vb6_Data_MoveNext(void* hwnd);
void      vb6_Data_MovePrevious(void* hwnd);
wchar_t*  vb6_Data_FieldName(void* hwnd, int32_t idx);
void      vb6_Data_FieldValue(void* hwnd, const wchar_t* nameOrIndex, wchar_t* out, int32_t outCap);
void*     vb6_Data_Self(void* hwnd);                               /* Recordset 链透传 */
wchar_t*  vb6_Data_FieldValueStr(void* hwnd, const wchar_t* nameOrIndex);  /* 静态缓冲 */
void      vb6_Data_Bind(void* hwnd, void* ctlHwnd, const wchar_t* fieldName);
void      vb6_Data_SetRepositionHandler(void* hwnd, void* fn);
void*     vb6_Data_RecordsetObj(void* hwnd);                       /* Recordset 真 IDispatch */
void      vb6_Data_FieldValueByIdx(void* hwnd, int32_t idx, wchar_t* out, int32_t outCap);
void      vb6_OleCon_Init(void* hwnd, const wchar_t* cls, int oletTypeAllowed,
                          int sizeMode, int displayAsIcon, int autoActivate);
int       vb6_OleCon_CreateEmbed(void* hwnd, const wchar_t* sourceDoc);  /* NULL=按 Class 新建 */
int       vb6_OleCon_CreateLink(void* hwnd, const wchar_t* sourceDoc, const wchar_t* sourceItem);
int       vb6_OleCon_ReadFromFile(void* hwnd, const wchar_t* path);
int       vb6_OleCon_SaveToFile(void* hwnd, const wchar_t* path);
int       vb6_OleCon_DoVerb(void* hwnd, int verb);
int       vb6_OleCon_Close(void* hwnd);
void*     vb6_OleCon_GetObject(void* hwnd);                        // Object 属性 → IDispatch*
int       vb6_OleCon_GetOleType(void* hwnd);                       // 0=嵌入 1=链接 2=无
void      vb6_OleCon_Copy(void* hwnd);
int       vb6_OleCon_Paste(void* hwnd);
int       vb6_OleCon_InsertObjDlg(void* hwnd);
int       vb6_OleCon_GetOLETypeAllowed(void* hwnd);
void      vb6_OleCon_SetOLETypeAllowed(void* hwnd, int v);
int       vb6_OleCon_GetSizeMode(void* hwnd);
void      vb6_OleCon_SetSizeMode(void* hwnd, int v);
int       vb6_OleCon_GetDisplayAsIcon(void* hwnd);
void      vb6_OleCon_SetDisplayAsIcon(void* hwnd, int v);
int       vb6_OleCon_GetAutoActivate(void* hwnd);
void      vb6_OleCon_SetAutoActivate(void* hwnd, int v);
int       vb6_OleCon_GetAutoVerbMenu(void* hwnd);
void      vb6_OleCon_SetAutoVerbMenu(void* hwnd, int v);
int       vb6_OleCon_GetBorderStyle(void* hwnd);
void      vb6_OleCon_SetBorderStyle(void* hwnd, int v);
wchar_t*  vb6_OleCon_GetClass(void* hwnd);
wchar_t*  vb6_OleCon_GetSourceDoc(void* hwnd);
wchar_t*  vb6_OleCon_GetSourceItem(void* hwnd);
void*   vb6_ListView_ListItemAt(void* hwnd, int32_t index);
void*   vb6_ListView_ColumnHeaderAt(void* hwnd, int32_t index);
void*   vb6_ListView_GetColumnKey(void* hwnd, int32_t idx);
int32_t vb6_ListView_GetColumnIndexByKey(void* hwnd, void* keyBstr);
void    vb6_ListView_ClearColumns(void* hwnd);
//   行 + SubItems
int32_t vb6_ListView_GetItemCount(void* hwnd);
int32_t vb6_ListView_AddItem(void* hwnd, int32_t idx, void* keyBstr, void* textBstr,
                             int32_t icon, int32_t smallIcon);
void*   vb6_ListView_GetItemText(void* hwnd, int32_t idx);
void    vb6_ListView_SetItemText(void* hwnd, int32_t idx, void* bstr);
void*   vb6_ListView_GetItemKey(void* hwnd, int32_t idx);
void    vb6_ListView_SetItemKey(void* hwnd, int32_t idx, void* bstr);
int32_t vb6_ListView_GetItemIndexByKey(void* hwnd, void* keyBstr);
void*   vb6_ListView_GetItemSub(void* hwnd, int32_t idx, int32_t sub);
void    vb6_ListView_SetItemSub(void* hwnd, int32_t idx, int32_t sub, void* bstr);
int32_t vb6_ListView_GetItemSubCount(void* hwnd, int32_t idx);
int32_t vb6_ListView_GetItemSelected(void* hwnd, int32_t idx);
void    vb6_ListView_SetItemSelected(void* hwnd, int32_t idx, int32_t val);
int32_t vb6_ListView_GetItemChecked(void* hwnd, int32_t idx);
void    vb6_ListView_SetItemChecked(void* hwnd, int32_t idx, int32_t val);
int32_t vb6_ListView_GetSelectedIndex(void* hwnd);
void    vb6_ListView_RemoveItem(void* hwnd, int32_t idx);
void    vb6_ListView_ClearItems(void* hwnd);
void    vb6_ListView_SetImageList(void* hwnd, void* himl, int32_t which);

// ---- C29-7: ListView 成员对象入口 (真 IDispatch, 见 vb6forms_memberobj.c) ----
//   ListView1.ListItems / .ColumnHeaders 返回**集合对象**; 它们的 Add 返回**成员对象**
//   (ListItem / ColumnHeader), 于是 `Set itm = .ListItems.Add(..)` 之后
//   `itm.Text` / `itm.SubItems(i)` / `itm.Selected` 全部走晚绑定。
//   ⚠ owner 这里传的是 **HWND** —— 与 ImageList 那族 (vb6_com_X 实例指针) 不同。
void*   vb6_ListView_ListItems(void* hwnd);
void*   vb6_ListView_ListItems_Add(void* hwnd, int32_t index, const wchar_t* key,
                                   const wchar_t* text, int32_t icon, int32_t smallIcon);
void*   vb6_ListView_ColumnHeaders(void* hwnd);
void*   vb6_ListView_ColumnHeaders_Add(void* hwnd, int32_t index, const wchar_t* key,
                                       const wchar_t* text, int32_t width, int32_t align);

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
// C29-4 事件面: NM_CLICK/NM_DBLCLK → 1 基面板号 (在 NMHDR.idFrom), 再取 Panel 对象回调。
int32_t vb6_StatusBar_OnNotify(void* hwnd, int32_t code, void* lParam);
void*   vb6_StatusBar_PanelAt(void* hwnd, int32_t index);
void    vb6_StatusBar_SimClick(void* hwnd, int32_t panelIdx, int32_t dblClick);  /* 判据专用 */
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


// ============================================================
// C29-9 / D6: CommonDialog（原生 comdlg32，不走 MSComDlg.OCX）
// ============================================================
// 定义在 vb6forms_ctrl.c。**每个入口都要有声明** —— C29-1b 的教训：有定义没声明时
// 生成代码按"返回 int"的隐式原型编译，字符串句柄被截成 32 位。
// 本头不引 oleauto.h，故 BSTR 一律写成 wchar_t*（OLECHAR = wchar_t，同一类型）。

// 属性宿主是一枚自注册的不可见子窗口 VB6_COMMONDIALOG（029 决策 D6）
void vb6_RegisterCommDialogClass(void* hInstance);
// C29-T: Timer 的身份类（不可见、0x0），CreateControls 开头注册。
void vb6_RegisterTimerClass(void* hInstance);

wchar_t* vb6_CdGetFilter(void* hwnd);      void vb6_CdSetFilter(void* hwnd, wchar_t* v);
wchar_t* vb6_CdGetFileName(void* hwnd);    void vb6_CdSetFileName(void* hwnd, wchar_t* v);
wchar_t* vb6_CdGetFileTitle(void* hwnd);   void vb6_CdSetFileTitle(void* hwnd, wchar_t* v);
wchar_t* vb6_CdGetDialogTitle(void* hwnd); void vb6_CdSetDialogTitle(void* hwnd, wchar_t* v);
wchar_t* vb6_CdGetInitDir(void* hwnd);     void vb6_CdSetInitDir(void* hwnd, wchar_t* v);
wchar_t* vb6_CdGetDefaultExt(void* hwnd);  void vb6_CdSetDefaultExt(void* hwnd, wchar_t* v);
wchar_t* vb6_CdGetFontName(void* hwnd);    void vb6_CdSetFontName(void* hwnd, wchar_t* v);
int vb6_CdGetFlags(void* hwnd);            void vb6_CdSetFlags(void* hwnd, int v);
int vb6_CdGetCancelError(void* hwnd);      void vb6_CdSetCancelError(void* hwnd, int v);
int vb6_CdGetColor(void* hwnd);            void vb6_CdSetColor(void* hwnd, int v);
int vb6_CdGetMin(void* hwnd);              void vb6_CdSetMin(void* hwnd, int v);
int vb6_CdGetMax(void* hwnd);              void vb6_CdSetMax(void* hwnd, int v);
int vb6_CdGetCopies(void* hwnd);           void vb6_CdSetCopies(void* hwnd, int v);
int vb6_CdGetFontSize(void* hwnd);         void vb6_CdSetFontSize(void* hwnd, int v);

// 六个 Show*：1 = 用户确认并已写回读数；0 = 取消（CancelError=True 时顺带报 32755）
int vb6_CdShowOpen(void* hwnd);
int vb6_CdShowSave(void* hwnd);
int vb6_CdShowColor(void* hwnd);
int vb6_CdShowFont(void* hwnd);
int vb6_CdShowPrinter(void* hwnd);
int vb6_CdShowAbout(void* hwnd);

// ===================== TreeView (ai/029 C29-8a) =====================
//   VB6 TreeView 的标量属性面，原生 SysTreeView32（不加载 MSCOMCTL.OCX）。
//   四条样式位属性的**真值就是 GWL_STYLE 那几位**（getter 直接读窗口），不是另存一份表；
//   Indentation 例外（VB6 侧缇 / Win32 侧像素，缇值存窗口属性）。True = -1。
//   Style / LabelEdit / Sorted / PathSeparator / Nodes 一族不在这一格里（C29-8b）。
void    vb6_TreeView_Init(void* hwnd, int32_t lineStyle, int32_t indentation,
                          int32_t checkboxes, int32_t hotTracking, int32_t hideSelection);
int32_t vb6_TreeView_GetLineStyle(void* hwnd);
void    vb6_TreeView_SetLineStyle(void* hwnd, int32_t val);
int32_t vb6_TreeView_GetCheckBoxes(void* hwnd);
void    vb6_TreeView_SetCheckBoxes(void* hwnd, int32_t val);
int32_t vb6_TreeView_GetHotTracking(void* hwnd);
void    vb6_TreeView_SetHotTracking(void* hwnd, int32_t val);
int32_t vb6_TreeView_GetHideSelection(void* hwnd);
void    vb6_TreeView_SetHideSelection(void* hwnd, int32_t val);
int32_t vb6_TreeView_GetIndentation(void* hwnd);
void    vb6_TreeView_SetIndentation(void* hwnd, int32_t twips);

// ===================== Toolbar (ai/029 C29-5a) =====================
//   VB6 Toolbar 的窗口 + 标量属性 + 设计期按钮，原生 ToolbarWindow32（不加载 OCX）。
//   ShowTips / TextStyle / AllowCustomize 的真值就是 GWL_STYLE 那几位；Align 存窗口属性
//   （停靠引擎还没接）。按钮表在本文件自己的固定槽表里，TB_ADDBUTTONSW 按它整表重发。
//   Buttons 的逐项 VB 侧读写与 ButtonClick 留给 5b（等成员对象机制）。True = -1。
void    vb6_Toolbar_Init(void* hwnd, int32_t showTips, int32_t textStyle,
                         int32_t allowCustomize, int32_t align);
int32_t vb6_Toolbar_GetShowTips(void* hwnd);
void    vb6_Toolbar_SetShowTips(void* hwnd, int32_t val);
int32_t vb6_Toolbar_GetTextStyle(void* hwnd);
void    vb6_Toolbar_SetTextStyle(void* hwnd, int32_t val);
int32_t vb6_Toolbar_GetAllowCustomize(void* hwnd);
void    vb6_Toolbar_SetAllowCustomize(void* hwnd, int32_t val);
int32_t vb6_Toolbar_GetAlign(void* hwnd);
void    vb6_Toolbar_SetAlign(void* hwnd, int32_t val);
int     vb6_Toolbar_AddButton(void* hwnd, int32_t index, const wchar_t* key, const wchar_t* caption,
                              int32_t style, int32_t image, const wchar_t* tooltip, int32_t width);
int32_t vb6_Toolbar_GetButtonCount(void* hwnd);   // TB_BUTTONCOUNT（原生侧真数）
// ===================== TreeView 的 Nodes / Node (ai/029 C29-8b) =====================
//   结构住在原生树里 (父子/兄弟一律 TVM_GETNEXTITEM 现问)，这张表只存原生给不出的东西：
//   Key / Text / Tag / 两个图索引。集合序 = 插入序：Nodes(k)、Node.Index、For Each 都按它。
//   字符串 getter 返回**表内自有指针** (唯一消费者 memSetStr 当场拷成 BSTR)，不是 SysAllocString。
//   越界：整数族给 0，字符串族给空串 —— 与 VB6 "取不到就是 Nothing / """ 同读数。
//   Bold / Sorted / RelativeX / Node.Style 这些扁平层还没做的成员，一律不登记。
int32_t         vb6_TreeView_NodeCount(void* hwnd);
int32_t         vb6_TreeView_AddNode(void* hwnd, int32_t relative, int32_t relationship,
                                     const wchar_t* key, const wchar_t* text,
                                     int32_t image, int32_t selImage);
int32_t         vb6_TreeView_NodeIndexByKey(void* hwnd, const wchar_t* key);
const wchar_t*  vb6_TreeView_GetNodeText(void* hwnd, int32_t idx);
const wchar_t*  vb6_TreeView_GetNodeKey(void* hwnd, int32_t idx);
const wchar_t*  vb6_TreeView_GetNodeTag(void* hwnd, int32_t idx);
void            vb6_TreeView_SetNodeText(void* hwnd, int32_t idx, const wchar_t* v);
void            vb6_TreeView_SetNodeKey(void* hwnd, int32_t idx, const wchar_t* v);
void            vb6_TreeView_SetNodeTag(void* hwnd, int32_t idx, const wchar_t* v);
int32_t         vb6_TreeView_GetNodeChecked(void* hwnd, int32_t idx);
void            vb6_TreeView_SetNodeChecked(void* hwnd, int32_t idx, int32_t v);
int32_t         vb6_TreeView_GetNodeExpanded(void* hwnd, int32_t idx);
void            vb6_TreeView_SetNodeExpanded(void* hwnd, int32_t idx, int32_t v);
int32_t         vb6_TreeView_GetNodeParent(void* hwnd, int32_t idx);
int32_t         vb6_TreeView_GetNodeChild(void* hwnd, int32_t idx);
int32_t         vb6_TreeView_GetNodeChildren(void* hwnd, int32_t idx);
int32_t         vb6_TreeView_GetNodeNext(void* hwnd, int32_t idx);
int32_t         vb6_TreeView_GetNodePrev(void* hwnd, int32_t idx);
int32_t         vb6_TreeView_GetNodeRoot(void* hwnd, int32_t idx);
void            vb6_TreeView_NodeEnsureVisible(void* hwnd, int32_t idx);
int32_t         vb6_TreeView_RemoveNode(void* hwnd, int32_t idx);
void            vb6_TreeView_ClearNodes(void* hwnd);
void*           vb6_TreeView_Nodes(void* hwnd);        // 集合对象 (真 IDispatch)
void*           vb6_TreeView_NodeAt(void* hwnd, int32_t idx);   // 事件参数用

#ifdef __cplusplus
}
#endif
