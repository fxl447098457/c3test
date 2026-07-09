#pragma once
// vb6com.h - VB6 COM互操作运行时 (P6)
// 独立于vb6rtl.h, 避免VARIANT定义冲突
// 此文件使用Windows原生VARIANT/IDispatch等类型

#include <windows.h>
#include <oleauto.h>
#include <wchar.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// CreateObject(progId) - 通过ProgID创建COM对象，返回IDispatch*
void* vb6_CreateObject(const wchar_t* progId);

// GetObject(pathName, progId) - 获取运行中的COM对象
void* vb6_GetObject(const wchar_t* pathName, const wchar_t* progId);

// IsNothing(obj) - 检查对象引用是否为Nothing
int32_t vb6_IsNothing(void* obj);

// ReleaseObject(&ptr) - 释放COM对象引用并置NULL
void vb6_ReleaseObject(void** objPtr);

// COM后期绑定 (P6.2)
// 返回VARIANT* (Windows VARIANT), 调用方需vb6_ComVarClear释放
void* vb6_ComCall(void* disp, const wchar_t* methodName,
                  void* args, int32_t argc);
void* vb6_ComCallByDispid(void* disp, int32_t dispid,
                         void* args, int32_t argc);
void* vb6_ComGetProp(void* disp, const wchar_t* propName);
void vb6_ComSetProp(void* disp, const wchar_t* propName, void* value);
void vb6_ComSetRef(void* disp, const wchar_t* propName, void* objRef);

// COM VARIANT封装/解封 (P6.2 cgen使用)
// vb6rtl.h通过void*声明这些函数，避免VARIANT类型冲突

// 将BSTR封装为VARIANT (返回堆分配的VARIANT*, 需vb6_ComVarClear释放)
void* vb6_ComPackBSTR(const wchar_t* bstr);

// 将int32_t封装为VARIANT
void* vb6_ComPackInt(int32_t val);


// 将VB6 Boolean (int32_t: -1=True, 0=False) 封装为VARIANT VT_BOOL
void* vb6_ComPackBool(int32_t val);
// 将double封装为VARIANT
void* vb6_ComPackDouble(double val);

// 将void*(IDispatch*)封装为VARIANT (用于对象参数)
void* vb6_ComPackObject(void* obj);

// 从VARIANT*解封BSTR (返回BSTR, 需vb6_BSTR_Free释放)
wchar_t* vb6_ComUnpackBSTR(void* variant);

// 从VARIANT*解封int32_t
int32_t vb6_ComUnpackInt(void* variant);

// 从VARIANT*解封double
double vb6_ComUnpackDouble(void* variant);

// 从VARIANT*解封对象(void*/IDispatch*)
void* vb6_ComUnpackObject(void* variant);

// 释放ComCall/ComGetProp返回的VARIANT* (值类型安全: 释清BSTR/数字等)
void vb6_ComVarClear(void* variant);

// 仅释放VARIANT结构体, 不清除内容 (对象引用已转移给调用方)
void vb6_ComVarFree(void* variant);

// 一体化COM辅助函数 (内部处理临时VARIANT清理, cgen直接使用)

// COM方法调用→对象 (内部UnpackObject+VarFree)
void* vb6_ComCallObject(void* disp, const wchar_t* methodName,
                        void* args, int32_t argc);
// COM方法调用→BSTR (内部UnpackBSTR+VarClear)
wchar_t* vb6_ComCallBSTR(void* disp, const wchar_t* methodName,
                         void* args, int32_t argc);
// COM方法调用→int32_t (内部UnpackInt+VarClear)
int32_t vb6_ComCallInt(void* disp, const wchar_t* methodName,
                       void* args, int32_t argc);
// COM方法调用→double (内部UnpackDouble+VarClear)
double vb6_ComCallDouble(void* disp, const wchar_t* methodName,
                         void* args, int32_t argc);

// COM属性Get→BSTR (内部UnpackBSTR+VarClear)
wchar_t* vb6_ComGetStringProp(void* disp, const wchar_t* propName);
// COM属性Get→int32_t (内部UnpackInt+VarClear)
int32_t vb6_ComGetIntProp(void* disp, const wchar_t* propName);
// COM属性Get→double (内部UnpackDouble+VarClear)
double vb6_ComGetDoubleProp(void* disp, const wchar_t* propName);
// COM属性Get→对象 (内部UnpackObject+VarFree)
void* vb6_ComGetObjectProp(void* disp, const wchar_t* propName);

// 参数化属性Get (带参数, 如Dictionary.Item(key))
// 内部使用DISPATCH_METHOD|DISPATCH_PROPERTYGET组合标志
void* vb6_ComGetPropArg(void* disp, const wchar_t* propName,
                        void* args, int32_t argc);
wchar_t* vb6_ComGetPropertyString(void* disp, const wchar_t* propName,
                                   void* args, int32_t argc);
int32_t vb6_ComGetPropertyInt(void* disp, const wchar_t* propName,
                               void* args, int32_t argc);
double vb6_ComGetPropertyDouble(void* disp, const wchar_t* propName,
                                 void* args, int32_t argc);
void* vb6_ComGetPropertyObject(void* disp, const wchar_t* propName,
                                void* args, int32_t argc);
void* vb6_ComGetPropertyVariant(void* disp, const wchar_t* propName,
                                 void* args, int32_t argc);

// ============================================================
// P6.3: COM前期绑定运行时支持 (vtable直接调用)
// ============================================================

// QueryInterface获取指定接口指针 (用于前期绑定初始化)
// iidStr: IID字符串 (如 "{2A0A3E20-...}")
// 返回: 请求的接口指针, 失败返回NULL
void* vb6_ComQI(void* obj, const char* iidStr);

// 创建前期绑定COM对象: CreateObject + QI
// progId: ProgID, iidStr: 目标接口IID
// 返回: 接口指针 (前期绑定类型), 失败返回NULL
void* vb6_ComCreateTyped(const wchar_t* progId, const char* iidStr);

// 释放前期绑定COM对象 (与ReleaseObject类似, 但针对接口指针)
void vb6_ComReleaseTyped(void** objPtr);

// vtable直接调用辅助函数 (P6.3 cgen使用)
// obj: 接口指针, vtIndex: vtable偏移量
// 参数通过va_list传递, 返回值从VARIANT解封

// vtable调用→void (无返回值/丢弃)
void vb6_ComVtableCallVoid(void* obj, int32_t vtIndex, ...);

// vtable调用→BSTR
wchar_t* vb6_ComVtableGetBSTR(void* obj, int32_t vtIndex, ...);

// vtable调用→int32_t
int32_t vb6_ComVtableGetInt(void* obj, int32_t vtIndex, ...);

// vtable调用→double
double vb6_ComVtableGetDouble(void* obj, int32_t vtIndex, ...);

// vtable调用→对象(void*)
void* vb6_ComVtableGetObject(void* obj, int32_t vtIndex, ...);

// vtable调用→通用void* (VARIANT结果)
void* vb6_ComVtableGetVoid(void* obj, int32_t vtIndex, ...);

// COM初始化/退出 (由vb6_Init/vb6_Exit调用)
// P13.21: IConnectionPointContainer / Advise support (WithEvents)
// Connect a COM object's event source to a sink (IDispatch-based callback)
int vb6_ComAdvise(void* obj, const char* riidStr, void* sink, int* adviseCookie);
int vb6_ComUnadvise(void* obj, const char* riidStr, int adviseCookie);

// P13.22: Create a generic IDispatch event sink
// Maps DISPID (event IDs) to callback functions
void* vb6_CreateEventSink(const int* dispids, void** callbacks, int count);
void vb6_FreeEventSink(void* sink);
void vb6_ComInit(void);
void vb6_ComExit(void);


// P22-11: For Each COM collection (IEnumVARIANT)
void* vb6_ForEach_Init(void* disp);       // Call _NewEnum, return IEnumVARIANT*
int32_t vb6_ForEach_Next(void* enumPtr, VARIANT* outVar);  // Next element, returns 1=ok 0=done
void vb6_ForEach_Release(void* enumPtr);  // Release IEnumVARIANT

#ifdef __cplusplus
}
#endif
