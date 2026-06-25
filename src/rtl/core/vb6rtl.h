#pragma once
// vb6rtl.h - VB6运行时库最小头文件
// 为C代码生成器提供VB6基本类型的C定义
// P3.6 阎段: 最小子集, 仅支撑 hello.bas 等简单程序

#include <stdint.h>
#include <stddef.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

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
double vb6_CDbl(VARIANT x);
BSTR vb6_CStr(VARIANT x);

// 类型检查
int32_t vb6_IsNumeric(VARIANT v);
int32_t vb6_IsNull(VARIANT v);
int32_t vb6_IsEmpty(VARIANT v);
int32_t vb6_IsObject(VARIANT v);

// 内置对象
void vb6_Debug_Print(BSTR s);
void vb6_Debug_PrintInt(int32_t n);
void vb6_Debug_PrintDouble(double d);

// Debug.Print 变参版 (cgen保留接口, 暂未使用)
// 内部使用stdarg, fmt字符: s=BSTR, d=int32_t, f=double
void vb6_DebugOutputFmt(const char* fmt, ...);

// 简化版: 单BSTR输出+换行
void vb6_DebugPrintStr(BSTR s);

// 整除
int32_t vb6_IntDiv(int32_t a, int32_t b);

// 幂运算
double vb6_Pow(double base, double exp);

// 对象操作
void* vb6_NewObject(const wchar_t* className);
int32_t vb6_TypeOf(void* obj, const wchar_t* typeName);
void* vb6_DictAccess(void* obj, const wchar_t* key);

// 字典访问
BSTR vb6_BSTR_Concat(BSTR a, BSTR b);

// ============================================================
// 运行时初始化/退出
// ============================================================

void vb6_Init(void);
void vb6_Exit(void);
void vb6_End(void);

#ifdef __cplusplus
}
#endif
