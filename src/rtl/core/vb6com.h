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
void* vb6_ComGetProp(void* disp, const wchar_t* propName);
void vb6_ComSetProp(void* disp, const wchar_t* propName, void* value);
void vb6_ComSetRef(void* disp, const wchar_t* propName, void* objRef);

// COM VARIANT封装/解封 (P6.2 cgen使用)
// vb6rtl.h通过void*声明这些函数，避免VARIANT类型冲突

// 将BSTR封装为VARIANT (返回堆分配的VARIANT*, 需vb6_ComVarClear释放)
void* vb6_ComPackBSTR(const wchar_t* bstr);

// 将int32_t封装为VARIANT
void* vb6_ComPackInt(int32_t val);

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

// COM初始化/退出 (由vb6_Init/vb6_Exit调用)
void vb6_ComInit(void);
void vb6_ComExit(void);

#ifdef __cplusplus
}
#endif
