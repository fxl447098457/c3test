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
