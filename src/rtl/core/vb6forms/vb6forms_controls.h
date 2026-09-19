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

// ============================================================
// Fix 112: in-project UserControl (.ctl) instance host + form/control host object model
// ============================================================

// UserControl host descriptor — generated code emits one per .ctl module and calls vb6_UC_Register
typedef struct vb6_UserControlDesc {
    const char* typeName;             // VB6 control type name, e.g. "ucChartBar"
    int32_t     scaleMode;            // .ctl design-time ScaleMode (1=Twip 3=Pixel)
    void*     (*create)(void);        // vb6_cls_X_New()
    void      (*init)(void* me);      // UserControl_Initialize
    void      (*paint)(void* me);     // UserControl_Paint
    void      (*resize)(void* me);    // UserControl_Resize (NULL if absent)
    void      (*show)(void* me);      // UserControl_Show (NULL if absent)
    void      (*terminate)(void* me); // UserControl_Terminate / vb6_cls_X_Destroy
    // ---- czUI fix: 鼠标事件转发 (全部可为 NULL) ----
    // VB6 运行时会把宿主窗口收到的鼠标消息转成 UserControl_MouseDown/Up/Move;
    // X/Y 已换算为控件当前 ScaleMode 单位。button: 1=左 2=右; shift: VB6 Shift。
    void      (*mouseDown)(void* me, int32_t button, int32_t shift, float x, float y);
    void      (*mouseUp)(void* me, int32_t button, int32_t shift, float x, float y);
    void      (*mouseMove)(void* me, int32_t button, int32_t shift, float x, float y);
    void      (*dblClick)(void* me);
} vb6_UserControlDesc;

// .ctl module self-registration (type name case-insensitive, duplicate ignored)
void vb6_UC_Register(const vb6_UserControlDesc* desc);

// Create a host child window for a UserControl instance on a form
// left/top/width/height in twips; ctrlName is the control instance name (Controls / Name)
// Returns host child HWND (NULL = type not registered)
void* vb6_UC_HostCreate(const char* typeName, int32_t left, int32_t top,
                        int32_t width, int32_t height, void* hParent, void* hInstance,
                        const char* ctrlName, int32_t index);

// host HWND -> control instance (vb6_cls_X*); NULL if not a host window
void* vb6_UC_InstanceOf(void* hwnd);
// control instance -> host HWND; NULL if unknown
void* vb6_UC_HwndOf(void* instance);
int32_t vb6_UC_IsHostHwnd(void* hwnd);

// Switch in an instance's host state (vb6_UserControl_*) before calling its public members.
void vb6_UC_Enter(void* hwnd);
void vb6_UC_RefreshCurrent(void);

// ---- host object model (form/control HWND, Controls collection, Font object) ----
// vb6_com_* / vb6_CallByName / vb6_TypeName consult these before touching lpVtbl.
void vb6_HostObj_Register(void* hwnd, const char* name, const char* vbTypeName,
                          int32_t isForm, int32_t index);
int32_t vb6_Host_IsHostObject(void* obj);
const wchar_t* vb6_Host_TypeNameOf(void* obj);
int32_t vb6_Host_GetProp(void* obj, const wchar_t* name, void* outVariant);
int32_t vb6_Host_SetProp(void* obj, const wchar_t* name, const void* inVariant);
int32_t vb6_Host_Call(void* obj, const wchar_t* name, int32_t argc,
                      void** argv, void* outVariant);
void* vb6_UC_NewFont(void);
// Fix 119: 在 vb6_UC_HostCreate 之前设定该实例的字体 (.frm BeginProperty Font 块)
void  vb6_UC_SetPendingFont(void* f);
// ---- czUI fix: 设计器子控件实例化 (.ctl 设计面上的 TextBox 等属于每个实例) ----
// 在 ucHostInit (宿主实例初始化) 期间调用, 以当前 UC 宿主窗口为父创建真实子窗口。
// left/top/width/height 单位为缇。返回子窗口句柄 (NULL = 失败)。
void* vb6_UC_CreateDesignEdit(int32_t left, int32_t top, int32_t width, int32_t height);
// 设计器 Timer: cb(ctx) 在每次 WM_TIMER 触发 (受 vb6_SetTimerEnabled/Interval 属性控制)
void* vb6_UC_CreateDesignTimer(void (*cb)(void*), void* ctx);
// ---- czUI fix: 轻量 PropertyBag (IDispatch) ----
// 供生成的 UserControl_ReadProperties 在运行期以 VB6 语义读取设计期持久化属性,
// 使 .ctl 内部"读取后同步"逻辑 (如 toggle 的 m_AnimPos 与 Checked 同步) 得以执行。
void* vb6_UC_PropBagCreate(void);
void  vb6_UC_PropBagFree(void* bag);
void  vb6_UC_BagPutStr(void* bag, const wchar_t* name, const wchar_t* value);
void  vb6_UC_BagPutInt(void* bag, const wchar_t* name, int32_t value);
void  vb6_UC_BagPutDbl(void* bag, const wchar_t* name, double value);
void  vb6_UC_BagPutBool(void* bag, const wchar_t* name, int32_t value);
// Fix 125: 字体对象身份判定 + 字段定位 (供 COM 属性读写层直接操作字体字段)
int32_t vb6_UC_IsFont(const void* p);
void*   vb6_UC_FontField(void* p, const wchar_t* name, int32_t* kind);
// Fix 128: 原地把 src 字体的字段拷进 dst 字体 (用于 Property Set 型字体属性,
// 避免调用其 Set 实现体里的 Refresh 破坏图表状态)
void    vb6_UC_FontAssign(void* dst, void* src);
int32_t vb6_UC_ControlsIsCollection(void* p);
// Fix 112c: built-in Collection (New Collection does not go through COM)
void* vb6_Collection_New(void);
int32_t vb6_Collection_IsCollection(void* p);
// 必须在此声明: 调用方是另一个编译单元 vb6com_foreach.c, 缺声明会被隐式当作返回
// int, 返回的枚举句柄指针在 x64 下被截断 → For Each 解引用即 0xC0000005.
void* vb6_Collection_EnumInit(void* coll);
int32_t vb6_Collection_EnumNext(void* enumPtr, void* outVariant);
int32_t vb6_UC_ControlsCount(void* coll);
void* vb6_UC_ControlsItem(void* coll, int32_t index);
void* vb6_UC_ControlsEnumInit(void* coll);
int32_t vb6_UC_ControlsEnumNext(void* enumPtr, void* outVariant);
// Windows VARIANT <-> vb6_VARIANT conversion + cleanup (used inside vb6com_* hooks)
void vb6_Host_ToWinVariant(const void* inV, void* outV);
void vb6_Host_FromWinVariant(const void* inV, void* outV);
void vb6_Host_ClearVariant(void* v);

#ifdef __cplusplus
}
#endif
