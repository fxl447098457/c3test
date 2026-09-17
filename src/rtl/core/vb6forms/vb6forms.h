#pragma once
// VB6 Win32窗体运行时 (P7)
// 提供Win32窗口注册、创建、消息循环、控件管理等基础功能
// 编译器生成的C代码调用此运行时API

#ifdef _WIN32
#include <windows.h>
#endif
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 窗体框架
// ============================================================

// 注册窗体窗口类 (内部调用RegisterClassEx)
// className: 窗口类名 (如 "VB6_Form1")
// wndProc: 窗口过程
// hInstance: 应用实例
// iconResId: 图标资源ID (0=默认)
// 返回: 0=成功, -1=失败
int vb6_RegisterFormClass(const char* className, void* wndProc, void* hInstance, int iconResId);

// 创建窗体窗口
// className: 已注册的窗口类名
// formName: 窗口标题
// x, y: 窗口位置 (CW_USEDEFAULT表示系统默认)
// width, height: 客户区大小 (缇, 1缇=1/15像素)
// hInstance: 应用实例
// userData: 传递给WM_CREATE的用户数据指针
// 返回: 窗口句柄 (HWND)
void* vb6_CreateFormWindow(const char* className, const char* formName,
    int x, int y, int width, int height, void* hInstance, void* userData);

// VB6缇(Twip)转像素
// VB6坐标单位: 1英寸=1440缇, 1像素=15缇 (96DPI标准)
int vb6_TwipToX(int twips);
int vb6_TwipToY(int twips);

// ============================================================
// 控件创建
// ============================================================

// 创建子控件 (通用)
// win32Class: Win32窗口类名 (BUTTON, EDIT, STATIC, etc.)
// controlName: 控件名 (用于SetWindowText和WM_COMMAND标识)
// style: 窗口样式 (WS_CHILD | WS_VISIBLE | ...)
// exStyle: 扩展样式
// x, y, width, height: 位置和大小 (缇)
// id: 控件ID (用于WM_COMMAND)
// hParent: 父窗口句柄
// hInstance: 应用实例
// 返回: 控件窗口句柄
void* vb6_CreateControl(const char* win32Class, const char* controlName,
    long style, long exStyle,
    int x, int y, int width, int height,
    int id, void* hParent, void* hInstance);

// 控件ID分配器 (每个窗体独立ID空间)
int vb6_NextControlId(void);

// 重置控件ID计数器 (新窗体开始时调用)
void vb6_ResetControlId(void);

// ============================================================
// Timer管理
// ============================================================

// 设置定时器 (VB6 Timer控件底层实现)
// interval: 间隔毫秒 (VB6 Interval属性)
// callback: 定时器回调函数 (Timer_Timer事件)
// 返回: 定时器ID (用于vb6_KillTimer)
int vb6_SetTimer(void* hwnd, int interval, void* callback);

// 销毁定时器
void vb6_KillTimer(int timerId);

// P24-Timer: WndProc dispatch for WM_TIMER (generated WndProc calls this)
void vb6_DispatchTimer(int timerId);

// ============================================================
// 消息循环
// ============================================================

// 标准VB6消息循环 (GetMessage + TranslateMessage + DispatchMessage)
// 同时处理WM_TIMER回调分发
// 返回: WM_QUIT的wParam值
int vb6_MessageLoop(void);

// DoEvents — 处理消息队列中的待处理消息
// 包括WM_TIMER回调分发
// 返回: 处理的消息数
int vb6_DoEvents(void);

// ============================================================
// 窗体事件桥接
// ============================================================

// 设置窗体用户数据 (将VB6窗体对象指针存入GWLP_USERDATA)
void vb6_SetFormUserData(void* hwnd, void* userData);

// 获取窗体用户数据
void* vb6_GetFormUserData(void* hwnd);

// ============================================================
// 窗体工具函数
// ============================================================

// 获取应用程序实例句柄
void* vb6_GetAppInstance(void);

// 设置应用程序实例句柄 (WinMain中调用)
void vb6_SetAppInstance(void* hInstance);

// 显示窗体 (vbModeless=0, vbModal=1)
// 模态时: 禁用父窗口, 进入本地消息循环直到窗体关闭
void vb6_ShowForm(void* hwnd, int modal);

// 卸载窗体
void vb6_UnloadForm(void* hwnd);

// M22-Issue6: 窗体表面Print (VB6的"Print expr"语句)
// hwnd: 窗体HWND, text: BSTR要输出的文本
// 使用TextOutW在窗体HDC上绘制, 维护CurrentX/CurrentY位置
void vb6_Form_Print(void* hwnd, void* bstrText);

// ============================================================
// Form_Unload回调
// ============================================================

// 设置Form_Unload回调 (WM_CLOSE时查询是否允许关闭)
// callback: 返回0=允许关闭, 返回非0=取消关闭
void vb6_SetFormUnloadCallback(void* callback);

// 查询Form_Unload (由WndProc的WM_CLOSE调用)
// 返回: 0=允许关闭, 非0=取消关闭
int vb6_QueryFormUnload(void);

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
// P13.13: Picture (PictureBox/Image)
// vb6_LoadPictureFromFile: load picture from file (BMP/ICO/CUR/WMF/EMF)
// Returns HBITMAP/HICON (as void*), or NULL on failure
void* vb6_LoadPictureFromFile(const char* filePath);
// vb6_LoadPictureFromResource: load picture from resource
// Returns handle from resource, or NULL on failure
void* vb6_LoadPictureFromResource(void* hInstance, int resourceId, const char* type);
// P24: Load picture from memory buffer (JPEG/BMP/ICO/PNG/GIF via OleLoadPicture + IStream)
// Returns HBITMAP/HICON handle, or NULL on failure

void* vb6_LoadPictureFromMemory(const void* data, int size);
// Load picture from memory as COM IDispatch* (IPictureDisp)
// For passing pictures to COM controls (e.g. ImageList.ListImages.Add)
// Caller must Release the returned IDispatch* (via vb6_ReleaseObject)
void* vb6_LoadPictureAsCom(const void* data, int size);

void* vb6_LoadIconFromMemory(const void* data, int size);

void vb6_GraphicalBtn_SetImage(void* hwnd, void* hBitmap);
// P24: Convert UTF-8 string to wide string (caller must free())
wchar_t* vb6_Utf8ToWide(const char* utf8);
// Set/get Picture property on PictureBox/Image controls
void* vb6_GetControlPicture(void* hwnd);
void vb6_SetControlPicture(void* hwnd, void* hPicture);
// Set Picture from COM IPictureDisp object (extracts HBITMAP via IPicture::get_Handle)
void vb6_SetControlPictureFromCom(void* hwnd, void* pPictureDisp);
// AutoSize for PictureBox: resize to fit picture
int vb6_GetPictureAutoSize(void* hwnd);
void vb6_SetPictureAutoSize(void* hwnd, int autoSize);

// P17.2: Image.Stretch property

int vb6_GetImageStretch(void* hwnd);

void vb6_SetImageStretch(void* hwnd, int stretch);

// P17.2: Image subclass for WM_PAINT (StretchBlt rendering)

void vb6_InstallImageSubclass(void* hwnd);

// P18-F: 控件子类化基础设施 (GotFocus/LostFocus/MouseEnter/MouseLeave/控件级事件)
// 通用控件子类化安装 (复用VB6_OrigProc属性模式)
// subclassProc: 子类化窗口过程 (由cgen生成)
void vb6_InstallControlSubclass(void* hwnd, void* subclassProc);
// 获取原始窗口过程 (子类化Proc内调用CallWindowProc用)
void* vb6_GetOriginalWndProc(void* hwnd);
// 移除控件子类化 (WM_DESTROY时调用)
void vb6_RemoveControlSubclass(void* hwnd);
// 启动鼠标跟踪 (TrackMouseEvent封装, 用于MouseEnter/MouseLeave)
void vb6_StartMouseTracking(void* hwnd);
// Form属性 (P20-40)
int32_t vb6_GetKeyPreview(void* hwnd);
void vb6_SetKeyPreview(void* hwnd, int32_t val);
int32_t vb6_GetWindowState(void* hwnd);
void vb6_SetWindowState(void* hwnd, int32_t val);
int32_t vb6_GetScaleWidth(void* hwnd);
int32_t vb6_GetScaleHeight(void* hwnd);
int32_t vb6_GetControlBox(void* hwnd);
void vb6_SetControlBox(void* hwnd, int32_t val);
int32_t vb6_GetMaxButton(void* hwnd);
void vb6_SetMaxButton(void* hwnd, int32_t val);
int32_t vb6_GetMinButton(void* hwnd);
void vb6_SetMinButton(void* hwnd, int32_t val);
// Label属性 (P20-39)
int32_t vb6_GetLabelAutoSize(void* hwnd);
void vb6_SetLabelAutoSize(void* hwnd, int32_t val);
int32_t vb6_GetLabelWordWrap(void* hwnd);
void vb6_SetLabelWordWrap(void* hwnd, int32_t val);
int32_t vb6_GetLabelBackStyle(void* hwnd);
void vb6_SetLabelBackStyle(void* hwnd, int32_t val);
// Menu属性 (P20-36)
void* vb6_GetMenuCaption(void* menuHandle, int menuId);
void vb6_SetMenuCaption(void* menuHandle, int menuId, void* bstrCaption);
int32_t vb6_GetMenuChecked(void* menuHandle, int menuId);
void vb6_SetMenuChecked(void* menuHandle, int menuId, int32_t val);
int32_t vb6_GetMenuEnabled(void* menuHandle, int menuId);
void vb6_SetMenuEnabled(void* menuHandle, int menuId, int32_t val);
int32_t vb6_GetMenuVisible(void* menuHandle, int menuId);
void vb6_SetMenuVisible(void* menuHandle, int menuId, int32_t val);
// 按钮Style属性 (P20-38)
int32_t vb6_GetButtonStyle(void* hwnd);
void vb6_SetButtonStyle(void* hwnd, int32_t val);
// Shape属性 (P20-34)
int32_t vb6_GetShapeType(void* hwnd);
void vb6_SetShapeType(void* hwnd, int32_t val);
int32_t vb6_GetShapeBorderWidth(void* hwnd);
void vb6_SetShapeBorderWidth(void* hwnd, int32_t val);
int32_t vb6_GetShapeBorderStyle(void* hwnd);
void vb6_SetShapeBorderStyle(void* hwnd, int32_t val);
int32_t vb6_GetShapeFillStyle(void* hwnd);
void vb6_SetShapeFillStyle(void* hwnd, int32_t val);
int32_t vb6_GetShapeBorderColor(void* hwnd);
void vb6_SetShapeBorderColor(void* hwnd, int32_t val);
int32_t vb6_GetShapeFillColor(void* hwnd);
void vb6_SetShapeFillColor(void* hwnd, int32_t val);
// Shape/Line窗口类注册 (P20-35)
void vb6_RegisterShapeLineClasses(void* hInstance);
// Line属性 (P20-35)
int32_t vb6_GetLineX1(void* hwnd);
void vb6_SetLineX1(void* hwnd, int32_t val);
int32_t vb6_GetLineY1(void* hwnd);
void vb6_SetLineY1(void* hwnd, int32_t val);
int32_t vb6_GetLineX2(void* hwnd);
void vb6_SetLineX2(void* hwnd, int32_t val);
int32_t vb6_GetLineY2(void* hwnd);
void vb6_SetLineY2(void* hwnd, int32_t val);
int32_t vb6_GetLineBorderWidth(void* hwnd);
void vb6_SetLineBorderWidth(void* hwnd, int32_t val);
int32_t vb6_GetLineBorderStyle(void* hwnd);
void vb6_SetLineBorderStyle(void* hwnd, int32_t val);
int32_t vb6_GetLineColor(void* hwnd);
void vb6_SetLineColor(void* hwnd, int32_t val);
// PictureBox图形属性 (P20-42)
int32_t vb6_GetAutoRedraw(void* hwnd);
void vb6_SetAutoRedraw(void* hwnd, int32_t val);
int32_t vb6_GetScaleMode(void* hwnd);
void vb6_SetScaleMode(void* hwnd, int32_t val);
float vb6_GetCurrentX(void* hwnd);
void vb6_SetCurrentX(void* hwnd, float val);
float vb6_GetCurrentY(void* hwnd);
void vb6_SetCurrentY(void* hwnd, float val);
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

// ============================================================
// MDI窗体 (P7.7)
// ============================================================
// VB6 MDIForm (父窗体) + MDIChild=True的子窗体
// Win32实现: MDI客户窗口(MDICLIENT) + CreateMDIWindow

// 注册MDI父窗体窗口类
int vb6_RegisterMDIFormClass(const char* className, void* wndProc, void* hInstance, int iconResId);

// 创建MDI父窗体 (包含MDICLIENT子窗口)
void* vb6_CreateMDIFormWindow(const char* className, const char* formName,
    int x, int y, int width, int height, void* hInstance);

// 创建MDI子窗体
void* vb6_CreateMDIChildWindow(const char* className, const char* formName,
    int x, int y, int width, int height, void* hMDIClient, void* hInstance);

// 获取MDI客户窗口句柄 (从MDI父窗体获取)
void* vb6_GetMDIClient(void* hMDIForm);

// MDI消息循环 (处理TranslateMDISysAccel)
int vb6_MDIMessageLoop(void* hAccelTable);

// MDI窗口排列
void vb6_MDITile(void* hMDIClient, int style);     // style: 0=horizontal, 1=vertical
void vb6_MDICascade(void* hMDIClient);
void vb6_MDIArrangeIcons(void* hMDIClient);

// MDI子窗体管理
void* vb6_MDIGetActive(void* hMDIClient);           // 获取活动子窗体
void vb6_MDIActivate(void* hMDIClient, void* hChild); // 激活子窗体


// ============================================================
// WebView2宿主 (P7.9)
// ============================================================
// VB6 WebBrowser控件 → Windows WebView2 (Edge Chromium) 映射
// 替代原有SHDocVw.WebBrowser (IE内核)

// 创建WebView2控件
// hParent: 父窗口句柄
// x, y, width, height: 位置和大小 (像素)
// controlName: 控件名 (用于标识)
// 返回: 控件窗口句柄 (HWND), WebView2异步初始化完成后可Navigate
void* vb6_CreateWebView(void* hParent, int x, int y, int width, int height, const char* controlName);

// 导航到URL
// hwnd: CreateWebView返回的句柄
// url: 要导航的URL
// 返回: 0=成功, -1=WebView2未初始化, -2=参数无效
int vb6_WebViewNavigate(void* hwnd, const char* url);

// 获取当前URL (返回BSTR, 调用者负责释放)
void* vb6_WebViewGetUrl(void* hwnd);

// 获取WebView2就绪状态
// 返回: 1=就绪(Navigate可用), 0=正在初始化, -1=初始化失败
int vb6_WebViewIsReady(void* hwnd);

// 设置WebView2大小 (响应式调整)
void vb6_WebViewResize(void* hwnd, int width, int height);

// 执行JavaScript
// script: JavaScript代码
// 返回: 0=成功, -1=未就绪, -2=参数无效
int vb6_WebViewExecuteScript(void* hwnd, const char* script);

// WebView2事件回调类型
// DocumentComplete: 导航完成
typedef void (*vb6_WebViewEventCallback)(void* hwnd, const char* url);

// 设置DocumentComplete事件回调
void vb6_WebViewSetDocumentCompleteCallback(void* hwnd, vb6_WebViewEventCallback callback);

// P20-50: WebBrowser导航方法占位
int vb6_WebViewGoBack(void* hwnd);
int vb6_WebViewGoForward(void* hwnd);
int vb6_WebViewRefresh(void* hwnd);

// ============================================================
// 动态加载控件 (Controls.Add)
// ============================================================

// 设置Form窗口的IDispatch指针 (Form创建后调用)
// 用于 Me.Controls.Add 等 COM 属性访问
void vb6_Form_SetDispatch(void* hwnd, void* pDispatch);

// Me.Controls.Add(progId, name) → 返回新控件的IDispatch*
// 等价于 VB6: Set ctrl = Me.Controls.Add("WMPlayer.OCX", "WMP1")
void* vb6_Form_ControlsAdd(void* hwnd, const wchar_t* progId, const wchar_t* ctrlName);

// ============================================================
// Fix 112: 工程内 UserControl (.ctl) 实例宿主 + 窗体/控件宿主对象模型
// ============================================================

// UserControl 宿主描述 — 生成代码为每个 .ctl 模块发射一份并调用 vb6_UC_Register
typedef struct vb6_UserControlDesc {
    const char* typeName;             // VB6 控件类型名, 如 "ucChartBar"
    int32_t     scaleMode;            // .ctl 设计期 ScaleMode (1=Twip 3=Pixel)
    void*     (*create)(void);        // vb6_cls_X_New()
    void      (*init)(void* me);      // UserControl_Initialize
    void      (*paint)(void* me);     // UserControl_Paint
    void      (*resize)(void* me);    // UserControl_Resize (无则 NULL)
    void      (*show)(void* me);      // UserControl_Show (无则 NULL)
    void      (*terminate)(void* me); // UserControl_Terminate / vb6_cls_X_Destroy
} vb6_UserControlDesc;

// .ctl 模块自注册 (类型名大小写不敏感, 重复注册忽略)
void vb6_UC_Register(const vb6_UserControlDesc* desc);

// 在窗体上创建一个 UserControl 实例的宿主子窗口
// left/top/width/height 为缇; ctrlName 为控件实例名 (供 Controls 枚举 / Name)
// 返回宿主子窗口 HWND (NULL=未注册该类型)
void* vb6_UC_HostCreate(const char* typeName, int32_t left, int32_t top,
                        int32_t width, int32_t height, void* hParent, void* hInstance,
                        const char* ctrlName, int32_t index);

// 宿主 HWND → 控件实例 (vb6_cls_X*); 非宿主窗口返回 NULL
void* vb6_UC_InstanceOf(void* hwnd);
// 控件实例 → 宿主 HWND; 未知返回 NULL
void* vb6_UC_HwndOf(void* instance);
int32_t vb6_UC_IsHostHwnd(void* hwnd);

// 换入某实例的宿主状态 (vb6_UserControl_*) — 窗体代码直接调用控件公开方法前使用。
// 有意不弹栈: "当前实例" = 最近进入者, 使 UserControl.Refresh/PropertyChange 生效。
void vb6_UC_Enter(void* hwnd);
void vb6_UC_RefreshCurrent(void);

// ---- 宿主对象模型 (窗体/控件 HWND, Controls 集合, Font 对象) ----
// vb6_com_* / vb6_CallByName / vb6_TypeName 在触碰 lpVtbl 之前先询问这里。
void vb6_HostObj_Register(void* hwnd, const char* name, const char* vbTypeName,
                          int32_t isForm, int32_t index);
int32_t vb6_Host_IsHostObject(void* obj);
const wchar_t* vb6_Host_TypeNameOf(void* obj);
int32_t vb6_Host_GetProp(void* obj, const wchar_t* name, void* outVariant);
int32_t vb6_Host_SetProp(void* obj, const wchar_t* name, const void* inVariant);
int32_t vb6_Host_Call(void* obj, const wchar_t* name, int32_t argc,
                      void** argv, void* outVariant);
void* vb6_UC_NewFont(void);
int32_t vb6_UC_ControlsIsCollection(void* p);
// Fix 112c: RTL 内建 Collection (New Collection 不走 COM)
void* vb6_Collection_New(void);
int32_t vb6_Collection_IsCollection(void* p);
int32_t vb6_UC_ControlsCount(void* coll);
void* vb6_UC_ControlsItem(void* coll, int32_t index);
void* vb6_UC_ControlsEnumInit(void* coll);
int32_t vb6_UC_ControlsEnumNext(void* enumPtr, void* outVariant);
// Windows VARIANT ↔ vb6_VARIANT 转换 + 清理 (vb6com_* 挂接点内部使用)
void vb6_Host_ToWinVariant(const void* inV, void* outV);
void vb6_Host_FromWinVariant(const void* inV, void* outV);
void vb6_Host_ClearVariant(void* v);

#ifdef __cplusplus
}
#endif
