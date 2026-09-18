#pragma once
// vb6rtl_class_com.h - 类实例支持与 COM 互操作（后期绑定 / vtable 前期绑定）
// 由 vb6rtl.h 伞头 include；生成代码不要直接 include 本文件
#include "vb6rtl_base.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 类支持 (类实例分配/释放)
// ============================================================

void* vb6_Alloc(size_t size);
void vb6_Free(void* ptr);

// As New Collection 自动实例化: cgen 生成 vb6_cls_<Name>_New(), 内建 Collection
// 无项目类名, 由 RTL 提供别名桥接到 vb6_Collection_New() (定义在 vb6forms_uc.c)。
void* vb6_cls_Collection_New(void);

// P21-14: SavePicture — save picture to file (GDI+ BMP save)
void vb6_SavePicture(void* hBitmap, BSTR filename);

// P21-15: Load statement — preload form without showing
void vb6_LoadForm(void* hwnd);

// P21-16: NPer — number of periods (financial)
double vb6_NPer(double rate, double pmt, double pv, double fv, int32_t type_);

// P21-17: FileAttr — return file mode/position
int32_t vb6_FileAttr(int32_t filenumber, int32_t attribute);

// P21-27: Erl — error line number
int32_t vb6_Erl(void);

// P21-28: Tab — print column positioning
BSTR vb6_Tab(int32_t column);

// P21-29: Spc — print space insertion
BSTR vb6_Spc(int32_t count);

// P22-11: For Each COM collection (IEnumVARIANT) - cgen uses these via void*
void* vb6_ForEach_Init(void* disp);
int32_t vb6_ForEach_Next(void* enumPtr, void* outVar);  // outVar is VARIANT*
void vb6_ForEach_Release(void* enumPtr);

// P21-19: IRR — internal rate of return
double vb6_IRR(void* valuesArray, double guess);

// P21-20: MIRR — modified internal rate of return
double vb6_MIRR(void* valuesArray, double financeRate, double reinvestRate);

// P20-37: Registry functions (VB6: SaveSetting/GetSetting/DeleteSetting/GetAllSettings)
// VB6 registry path: HKEY_CURRENT_USER\Software\VB and VBA Program Settings
void vb6_SaveSetting(BSTR appName, BSTR section, BSTR key, BSTR setting);
BSTR vb6_GetSetting(BSTR appName, BSTR section, BSTR key, BSTR default_);
void vb6_DeleteSetting(BSTR appName, BSTR section, BSTR key);
// GetAllSettings returns a SafeArray of (key, value) pairs - returns 2D variant array
vb6_VARIANT vb6_GetAllSettings(BSTR appName, BSTR section);

// P21-18: LoadPicture enhancement - OleLoadPicturePath for ICO/CUR/WMF/EMF/GIF/JPG/PNG
void* vb6_LoadPictureEx(BSTR pathname);

// ============================================================
// COM 互操作 (P6)
// ============================================================

// CreateObject(progId) — 通过ProgID创建COM对象，返回IDispatch*
// VB6: Set obj = CreateObject("Scripting.FileSystemObject")
void* vb6_CreateObject(const wchar_t* progId);

// GetObject(pathName, progId) — 获取已运行的COM对象或从文件加载
// VB6: Set obj = GetObject(, "Excel.Application")
// pathName可为NULL，progId不可为NULL
void* vb6_GetObject(const wchar_t* pathName, const wchar_t* progId);

// IsNothing(obj) — 检查对象引用是否为Nothing(空)
// VB6: If obj Is Nothing Then ...
int32_t vb6_IsNothing(void* obj);

// ReleaseObject(&ptr) — 释放COM对象引用(IUnknown::Release)并置NULL
// VB6: Set obj = Nothing
void vb6_ReleaseObject(void** objPtr);

// COM对象方法/属性调用 — 后期绑定 (P6.2)
// 通过IDispatch::Invoke调用方法/属性
// args参数为Windows vb6_VARIANT数组指针(由vb6com.c定义), cgen通过void*传递
void* vb6_ComCall(void* disp, const wchar_t* methodName,
                  void* args, int32_t argc);
// P24-10: COM默认成员调用 (按DISPID直接调用, 跳过名称查找)
void* vb6_ComCallByDispid(void* disp, int32_t dispid,
                         void* args, int32_t argc);
void* vb6_ComGetProp(void* disp, const wchar_t* propName);
void vb6_ComSetProp(void* disp, const wchar_t* propName, void* value);
void vb6_ComSetPropArg(void* disp, const wchar_t* propName, void** args, int32_t argc, void* value);
void vb6_ComSetRef(void* disp, const wchar_t* propName, void* objRef);

// COM vb6_VARIANT封装/解封 — cgen生成的C代码使用 (P6.2)
// 实际实现在vb6com.c, 此处用void*避免vb6_VARIANT类型冲突
void* vb6_ComPackBSTR(const wchar_t* bstr);
void* vb6_ComPackInt(int32_t val);
void* vb6_ComPackBool(int32_t val);
void* vb6_ComPackDouble(double val);
void* vb6_ComPackObject(void* obj);
// Fix 104: 省略实参占位 (VT_ERROR + DISP_E_PARAMNOTFOUND) — cgen 对有省略实参的
// COM 调用按形参位置插入. 必须在此声明: 缺声明会被 C 编译器按隐式 int 返回处理,
// x64 下返回值截断成 32 位 → 立即访问违例.
void* vb6_ComPackMissing(void);
wchar_t* vb6_ComUnpackBSTR(void* variant);
int32_t vb6_ComUnpackInt(void* variant);
double vb6_ComUnpackDouble(void* variant);
void* vb6_ComUnpackObject(void* variant);
void vb6_ComVarClear(void* variant);
void vb6_ComVarFree(void* variant);

// 一体化COM辅助函数 (内部处理临时vb6_VARIANT清理)
void* vb6_ComCallObject(void* disp, const wchar_t* methodName,
                        void* args, int32_t argc);
wchar_t* vb6_ComCallBSTR(void* disp, const wchar_t* methodName,
                         void* args, int32_t argc);
int32_t vb6_ComCallInt(void* disp, const wchar_t* methodName,
                       void* args, int32_t argc);
double vb6_ComCallDouble(void* disp, const wchar_t* methodName,
                         void* args, int32_t argc);
wchar_t* vb6_ComGetStringProp(void* disp, const wchar_t* propName);
int32_t vb6_ComGetIntProp(void* disp, const wchar_t* propName);
double vb6_ComGetDoubleProp(void* disp, const wchar_t* propName);
void* vb6_ComGetObjectProp(void* disp, const wchar_t* propName);
// COM属性Get→intptr_t (LongPtr: 句柄/指针). 兼容 32/64 位整数变体, 避免 x64 截断.
intptr_t vb6_ComGetLongPtrProp(void* disp, const wchar_t* propName);
// COM调用结果→vb6_VARIANT (后期绑定, 如dic.Item(key))
vb6_VARIANT vb6_VariantFromComResult(void* variant_ptr);
vb6_VARIANT vb6_VariantFromStackVARIANT(VARIANT* pv);  /* P24-03: 栈上VARIANT转换(不释放) */
void* vb6_VariantToObject(vb6_VARIANT* v);  /* P24-04: Extract IDispatch from Variant for COM late-binding */
void* vb6_ComPackVariant(vb6_VARIANT v);     /* P24-04: Pack vb6_VARIANT (by value) into Windows VARIANT */
// Fix 030: 通用 COM 参数打包宏 — 路由任意 C 类型实参通过 _Generic 选择合适的
// Variant 构造函数 (scalar->Long/Int/Double/Bool, BSTR->String, SafeArray1D*->Array,
// void*/class*->Object, vb6_VARIANT->Identity), 再交给 vb6_ComPackVariant 包装为
// Windows VARIANT. 用于 comPackExpr 无法准确判定 (inferExprType 回退 Variant) 的场景.
#define vb6_ComPackValue(x) vb6_ComPackVariant(vb6_VariantFromValue((x)))


// P14.3.5: CallByName - 按名称动态调用方法/属性
// calltype: 1=VbLet, 2=VbMethod, 3=VbGet
vb6_VARIANT vb6_CallByName(void* obj, const wchar_t* procName, int32_t callType,
                           void* args, int32_t argc);

// P6.3: COM前期绑定 (vtable直接调用)
void* vb6_ComQI(void* obj, const char* iidStr);
void* vb6_ComCreateTyped(const wchar_t* progId, const char* iidStr);
void vb6_ComReleaseTyped(void** objPtr);
void vb6_ComVtableCallVoid(void* obj, int32_t vtIndex, ...);
wchar_t* vb6_ComVtableGetBSTR(void* obj, int32_t vtIndex, ...);
int32_t vb6_ComVtableGetInt(void* obj, int32_t vtIndex, ...);
double vb6_ComVtableGetDouble(void* obj, int32_t vtIndex, ...);
void* vb6_ComVtableGetObject(void* obj, int32_t vtIndex, ...);
void* vb6_ComVtableGetVoid(void* obj, int32_t vtIndex, ...);

#ifdef __cplusplus
}
#endif
