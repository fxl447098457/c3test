#pragma once
// vb6rtl.h - VB6运行时库最小头文件
// 为C代码生成器提供VB6基本类型的C定义
// P3.6 阎段: 最小子集, 仅支撑 hello.bas 等简单程序

#include <stdint.h>
#include <stddef.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// BSTR - VB6 字符串类型 (OLE BSTR 简化实现)
// ============================================================

// BSTR 本质是 wchar_t* (指向长度前缀之后的字符数据)
// 简化实现: 直接用 wchar_t*, 不做引用计数
typedef wchar_t* BSTR;

// 空BSTR
static inline BSTR vb6_BSTR_Empty(void) {
    return NULL;
}

// 从宽字符串字面量创建BSTR (零拷贝, 不修改原字符串)
// 注意: 字面量的BSTR不能用SysFreeString释放
static inline BSTR vb6_BSTR_FromStr(const wchar_t* s) {
    if (!s) return NULL;
    size_t len = wcslen(s);
    // 分配4字节长度前缀 + 字符数据 + null终止符
    uint32_t* p = (uint32_t*)malloc(sizeof(uint32_t) + (len + 1) * sizeof(wchar_t));
    if (!p) return NULL;
    *p = (uint32_t)len;
    BSTR bstr = (BSTR)(p + 1);
    memcpy(bstr, s, (len + 1) * sizeof(wchar_t));
    return bstr;
}

// 释放BSTR
static inline void vb6_BSTR_Free(BSTR bstr) {
    if (bstr) {
        uint32_t* p = ((uint32_t*)bstr) - 1;
        free(p);
    }
}

// BSTR连接
BSTR vb6_BSTR_Concat(BSTR a, BSTR b);

// BSTR长度
static inline int32_t vb6_BSTR_Len(BSTR bstr) {
    if (!bstr) return 0;
    uint32_t* p = ((uint32_t*)bstr) - 1;
    return (int32_t)*p;
}

// ============================================================
// VARIANT - VB6 Variant类型 (简化实现)
// ============================================================

// Vb6VarType 与 Vb6Type 枚举对应
typedef enum vb6_vartype {
    vb6_vtEmpty = 0,
    vb6_vtNull = 1,
    vb6_vtInteger = 2,
    vb6_vtLong = 3,
    vb6_vtSingle = 4,
    vb6_vtDouble = 5,
    vb6_vtCurrency = 6,
    vb6_vtDate = 7,
    vb6_vtBSTR = 8,
    vb6_vtDispatch = 9,
    vb6_vtError = 10,
    vb6_vtBoolean = 11,
    vb6_vtVariant = 12,
    vb6_vtByte = 17,
} vb6_vartype;

typedef struct VARIANT {
    vb6_vartype vt;
    union {
        int16_t iVal;
        int32_t lVal;
        float fltVal;
        double dblVal;
        BSTR bstrVal;
        void* pdispVal;
        int16_t boolVal;
        uint8_t bVal;
        int64_t cyVal;
    };
} VARIANT;

// Variant构造
static inline VARIANT vb6_VariantEmpty(void) {
    VARIANT v;
    memset(&v, 0, sizeof(v));
    v.vt = vb6_vtEmpty;
    return v;
}

static inline VARIANT vb6_VariantNull(void) {
    VARIANT v;
    memset(&v, 0, sizeof(v));
    v.vt = vb6_vtNull;
    return v;
}

static inline VARIANT vb6_VariantInt(int16_t val) {
    VARIANT v; memset(&v, 0, sizeof(v));
    v.vt = vb6_vtInteger; v.iVal = val; return v;
}

static inline VARIANT vb6_VariantLong(int32_t val) {
    VARIANT v; memset(&v, 0, sizeof(v));
    v.vt = vb6_vtLong; v.lVal = val; return v;
}

static inline VARIANT vb6_VariantDouble(double val) {
    VARIANT v; memset(&v, 0, sizeof(v));
    v.vt = vb6_vtDouble; v.dblVal = val; return v;
}

static inline VARIANT vb6_VariantString(BSTR val) {
    VARIANT v; memset(&v, 0, sizeof(v));
    v.vt = vb6_vtBSTR; v.bstrVal = val; return v;
}

static inline VARIANT vb6_VariantBool(int16_t val) {
    VARIANT v; memset(&v, 0, sizeof(v));
    v.vt = vb6_vtBoolean; v.boolVal = val; return v;
}

// Variant转基本类型
int32_t vb6_VariantToLong(VARIANT v);
double vb6_VariantToDouble(VARIANT v);
BSTR vb6_VariantToString(VARIANT v);

// ============================================================
// 内置函数 (最小子集)
// ============================================================

// 字符串函数
int32_t vb6_Len(BSTR s);
BSTR vb6_Left(BSTR s, int32_t n);
BSTR vb6_Right(BSTR s, int32_t n);
BSTR vb6_Mid(BSTR s, int32_t start, int32_t len);
int32_t vb6_InStr(int32_t start, BSTR haystack, BSTR needle);
BSTR vb6_UCase(BSTR s);
BSTR vb6_LCase(BSTR s);
BSTR vb6_Trim(BSTR s);
BSTR vb6_LTrim(BSTR s);
BSTR vb6_RTrim(BSTR s);
BSTR vb6_Chr(int32_t code);
int32_t vb6_Asc(BSTR s);
double vb6_Val(BSTR s);
BSTR vb6_Str(int32_t n);
BSTR vb6_Format(VARIANT expr, BSTR fmt);

// 消息框
int32_t vb6_MsgBox(BSTR prompt, int32_t buttons, BSTR title);

// 数值函数
double vb6_Abs(double x);
int32_t vb6_Sgn(double x);
double vb6_Sqr(double x);
double vb6_Round(double x, int32_t decimals);
float vb6_Rnd(int32_t seed);

// 转换函数
int16_t vb6_CInt(double x);
int32_t vb6_CLng(double x);
double vb6_CDbl(double x);
BSTR vb6_CStr(VARIANT x);

// 类型检查
int32_t vb6_IsNumeric(VARIANT v);
int32_t vb6_IsNull(VARIANT v);
int32_t vb6_IsEmpty(VARIANT v);
int32_t vb6_IsObject(VARIANT v);
int32_t vb6_IsArray(VARIANT v);
int32_t vb6_IsDate(VARIANT v);
int32_t vb6_IsError(VARIANT v);
BSTR vb6_TypeName(VARIANT v);
int32_t vb6_VarType(VARIANT v);

// 内置对象
void vb6_Debug_Print(BSTR s);
void vb6_Debug_PrintInt(int32_t n);
void vb6_Debug_PrintDouble(double d);

// Debug.Print 变参版 (cgen保留接口, 暂未使用)
// 内部使用stdarg, fmt字符: s=BSTR, d=int32_t, f=double
void vb6_DebugOutputFmt(const char* fmt, ...);

// 简化版: 单BSTR输出+换行
void vb6_DebugPrintStr(BSTR s);

// Debug.Print 分项输出: 逐参数输出不换行, 最后统一换行
void vb6_DebugWriteBSTR(BSTR s);       // 输出BSTR片段(不换行)
void vb6_DebugWriteLong(int32_t n);    // 输出整数片段(不换行)
void vb6_DebugWriteDouble(double d);   // 输出浮点片段(不换行)
void vb6_DebugWriteNewline(void);      // 输出换行

// 整除
int32_t vb6_IntDiv(int32_t a, int32_t b);

// 幂运算
double vb6_Pow(double base, double exp);

// 对象操作
void* vb6_NewObject(const wchar_t* className);
int32_t vb6_TypeOf(void* obj, const wchar_t* typeName);
void* vb6_DictAccess(void* obj, const wchar_t* key);

// 字符串函数 (补充)
BSTR vb6_Replace(BSTR expr, BSTR find, BSTR rep, int32_t start, int32_t count, int32_t compare);
BSTR vb6_Space(int32_t n);
BSTR vb6_String(int32_t n, int32_t charCode);
int32_t vb6_StrComp(BSTR s1, BSTR s2, int32_t compare);
BSTR vb6_StrReverse(BSTR s);
int32_t vb6_InStrRev(BSTR haystack, BSTR needle, int32_t start, int32_t compare);
BSTR vb6_LCase_str(BSTR s);  // LCase$别名
BSTR vb6_UCase_str(BSTR s);  // UCase$别名

// 数学函数 (补充)
double vb6_Sin(double x);
double vb6_Cos(double x);
double vb6_Tan(double x);
double vb6_Atn(double x);
double vb6_Log(double x);
double vb6_Exp(double x);
double vb6_Fix(double x);
double vb6_Int(double x);
void vb6_Randomize(double seed);
float vb6_Rnd_Full(int32_t seed);

// 日期时间函数
double vb6_Now(void);
double vb6_Date(void);
double vb6_Time(void);
int32_t vb6_Year(double date);
int32_t vb6_Month(double date);
int32_t vb6_Day(double date);
int32_t vb6_Hour(double time);
int32_t vb6_Minute(double time);
int32_t vb6_Second(double time);

// 类型转换 (补充)
int16_t vb6_CBool(double v);
uint8_t vb6_CByte(double v);
float vb6_CSng(double v);
double vb6_CDate(VARIANT v);
BSTR vb6_Hex(int32_t n);
BSTR vb6_Oct(int32_t n);

// ============================================================
// SAFEARRAY - VB6 动态/静态数组
// ============================================================

// 元素类型标识 (用于ReDim/Erase时正确清理)
typedef enum vb6_safearray_elemtype {
    vb6_sa_empty = 0,
    vb6_sa_bool  = 1,   // int16_t
    vb6_sa_byte  = 2,   // uint8_t
    vb6_sa_int   = 3,   // int16_t
    vb6_sa_long  = 4,   // int32_t
    vb6_sa_single= 5,   // float
    vb6_sa_double= 6,   // double
    vb6_sa_bstr  = 7,   // BSTR (需要逐元素释放)
    vb6_sa_variant=8,   // VARIANT (需要逐元素清理)
    vb6_sa_ptr   = 9,   // void* (对象引用)
} vb6_safearray_elemtype;

// 一维数组描述符 (VB6绝大多数用例是一维)
typedef struct vb6_SafeArray1D {
    vb6_safearray_elemtype elemType;  // 元素类型
    int32_t elemSize;                  // 单个元素字节数
    int32_t lBound;                    // 下界 (VB6默认0, 可指定1)
    int32_t uBound;                    // 上界
    int32_t count;                     // 元素个数 = uBound - lBound + 1
    void*   data;                      // 数据指针 (calloc分配, 零初始化)
    int32_t isDynamic;                 // 是否动态数组 (ReDim创建)
} vb6_SafeArray1D;

// 创建一维静态数组
vb6_SafeArray1D* vb6_SafeArrayCreate1D(vb6_safearray_elemtype elemType,
    int32_t lBound, int32_t uBound);

// 创建一维动态数组 (ReDim)
vb6_SafeArray1D* vb6_SafeArrayReDim1D(vb6_safearray_elemtype elemType,
    int32_t lBound, int32_t uBound);

// ReDim Preserve: 保留原有数据, 调整大小
vb6_SafeArray1D* vb6_SafeArrayReDimPreserve1D(vb6_SafeArray1D* arr,
    int32_t newLBound, int32_t newUBound);

// 销毁数组 (释放内存)
void vb6_SafeArrayDestroy1D(vb6_SafeArray1D* arr);

// 获取/设置元素 (void*通用版)
void* vb6_SafeArrayGetPtr(vb6_SafeArray1D* arr, int32_t index);
void  vb6_SafeArrayPutElem(vb6_SafeArray1D* arr, int32_t index, void* value);

// 便捷宏: 按类型访问元素
#define VB6_SA_AT(type, arr, idx) (((type*)((arr)->data))[(idx) - (arr)->lBound])

// UBound/LBound (替换旧stub)
int32_t vb6_UBound(vb6_SafeArray1D* safeArray, int32_t dimension);
int32_t vb6_LBound(vb6_SafeArray1D* safeArray, int32_t dimension);

// 文件 I/O
int32_t vb6_FreeFile(void);
int32_t vb6_Open(BSTR pathname, int32_t mode, int32_t access, int32_t filenumber);
int32_t vb6_Close(int32_t filenumber);
int32_t vb6_EOF(int32_t filenumber);
int32_t vb6_LOF(int32_t filenumber);
int32_t vb6_Loc(int32_t filenumber);
void vb6_Print(int32_t filenumber, BSTR s);
void vb6_Write(int32_t filenumber, BSTR s);
BSTR vb6_LineInput(int32_t filenumber);
int32_t vb6_Input(int32_t filenumber, BSTR* outVar);
int32_t vb6_Kill(BSTR pathname);
int32_t vb6_MkDir(BSTR pathname);
int32_t vb6_RmDir(BSTR pathname);
int32_t vb6_ChDir(BSTR pathname);
int32_t vb6_ChDrive(BSTR drive);
int32_t vb6_Name(BSTR oldPath, BSTR newPath);
int32_t vb6_FileCopy(BSTR source, BSTR destination);

// 错误处理
int32_t vb6_ErrNumber(void);
BSTR vb6_ErrDescription(void);
void vb6_ErrClear(void);
void vb6_RaiseError(int32_t errNum, BSTR description);

// 全局错误处理状态 (由cgen生成的代码直接使用)
extern int32_t vb6_err_resume_next;  // On Error Resume Next 标志
extern int32_t vb6_err_jmp_active;    // On Error GoTo label 标志
extern void* vb6_err_handler_label;   // 错误跳转标签 (MVP, 暂不用)
extern jmp_buf vb6_error_jmp_buf;     // On Error GoTo label 的 setjmp 缓冲区
extern int32_t vb6_error_jmp_set;     // setjmp 是否已设置

// 字典访问
BSTR vb6_BSTR_Concat(BSTR a, BSTR b);

// ============================================================
// 类支持 (类实例分配/释放)
// ============================================================

void* vb6_Alloc(size_t size);
void vb6_Free(void* ptr);

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
// args参数为Windows VARIANT数组指针(由vb6com.c定义), cgen通过void*传递
void* vb6_ComCall(void* disp, const wchar_t* methodName,
                  void* args, int32_t argc);
void* vb6_ComGetProp(void* disp, const wchar_t* propName);
void vb6_ComSetProp(void* disp, const wchar_t* propName, void* value);
void vb6_ComSetRef(void* disp, const wchar_t* propName, void* objRef);

// COM VARIANT封装/解封 — cgen生成的C代码使用 (P6.2)
// 实际实现在vb6com.c, 此处用void*避免VARIANT类型冲突
void* vb6_ComPackBSTR(const wchar_t* bstr);
void* vb6_ComPackInt(int32_t val);
void* vb6_ComPackDouble(double val);
void* vb6_ComPackObject(void* obj);
wchar_t* vb6_ComUnpackBSTR(void* variant);
int32_t vb6_ComUnpackInt(void* variant);
double vb6_ComUnpackDouble(void* variant);
void* vb6_ComUnpackObject(void* variant);
void vb6_ComVarClear(void* variant);
void vb6_ComVarFree(void* variant);

// 一体化COM辅助函数 (内部处理临时VARIANT清理)
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

// ============================================================
// 运行时初始化/退出
// ============================================================

void vb6_Init(void);
void vb6_Exit(void);
void vb6_End(void);

#ifdef __cplusplus
}
#endif
