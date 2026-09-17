#pragma once
// vb6rtl_variant.h - VARIANT（VB6 Variant 类型）构造 / 转换 / 清理
// 由 vb6rtl.h 伞头 include；生成代码不要直接 include 本文件
#include "vb6rtl_base.h"

#ifdef __cplusplus
extern "C" {
#endif

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
    vb6_vtDecimal = 14,
    vb6_vtByte = 17,
    vb6_vtArray = 0x2000,
} vb6_vartype;


typedef struct vb6_VARIANT {
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
        struct vb6_SafeArray1D* parray;  /* P20-37: array pointer for GetAllSettings etc */
        struct { uint16_t wReserved1; uint8_t scale; uint8_t sign; uint32_t Hi32; uint32_t Lo32; uint32_t Mid32; } decVal;
    };
} vb6_VARIANT;

#undef vb6_VARIANT  /* 取消Windows vb6_VARIANT, 使用VB6简化版 */

// Variant构造
static inline vb6_VARIANT vb6_VariantEmpty(void) {
    vb6_VARIANT v;
    memset(&v, 0, sizeof(v));
    v.vt = vb6_vtEmpty;
    return v;
}

static inline vb6_VARIANT vb6_VariantNull(void) {
    vb6_VARIANT v;
    memset(&v, 0, sizeof(v));
    v.vt = vb6_vtNull;
    return v;
}

static inline vb6_VARIANT vb6_VariantInt(int16_t val) {
    vb6_VARIANT v; memset(&v, 0, sizeof(v));
    v.vt = vb6_vtInteger; v.iVal = val; return v;
}

static inline vb6_VARIANT vb6_VariantLong(int32_t val) {
    vb6_VARIANT v; memset(&v, 0, sizeof(v));
    v.vt = vb6_vtLong; v.lVal = val; return v;
}

static inline vb6_VARIANT vb6_VariantDouble(double val) {
    vb6_VARIANT v; memset(&v, 0, sizeof(v));
    v.vt = vb6_vtDouble; v.dblVal = val; return v;
}

static inline vb6_VARIANT vb6_VariantString(BSTR val) {
    vb6_VARIANT v; memset(&v, 0, sizeof(v));
    v.vt = vb6_vtBSTR; v.bstrVal = val; return v;
}

static inline vb6_VARIANT vb6_VariantBool(int16_t val) {
    vb6_VARIANT v; memset(&v, 0, sizeof(v));
    v.vt = vb6_vtBoolean; v.boolVal = val; return v;
}

// Variant containing a SafeArray (VB6: a = Array(1,2,3))
static inline vb6_VARIANT vb6_VariantArray(void* _arr) {
    vb6_VARIANT v; memset(&v, 0, sizeof(v));
    v.vt = vb6_vtArray | vb6_vtVariant; v.parray = (struct vb6_SafeArray1D*)_arr; return v;
}

// Fix 024: Variant containing a Byte (VT_UI1)
static inline vb6_VARIANT vb6_VariantByte(uint8_t val) {
    vb6_VARIANT v; memset(&v, 0, sizeof(v));
    v.vt = vb6_vtByte; v.bVal = val; return v;
}

// Fix 024: Variant containing a COM object pointer (VT_DISPATCH)
static inline vb6_VARIANT vb6_VariantObject(void* val) {
    vb6_VARIANT v; memset(&v, 0, sizeof(v));
    v.vt = vb6_vtDispatch; v.pdispVal = val; return v;
}

// Fix 024: Variant identity passthrough — used by vb6_VariantFromValue
//           when the expression is ALREADY a vb6_VARIANT (no wrapping).
static inline vb6_VARIANT vb6_VariantIdentity(vb6_VARIANT v) { return v; }

// Fix 024: Polymorphic Variant constructor via C11 _Generic.
//   用法: vb6_VARIANT v = vb6_VariantFromValue(any_C_expr);
//   按实参表达式的C类型(编译期推断)选择合适的Variant构造函数:
//     _Bool/bool                        -> VariantBool
//     整型(char/short/int/long/long long,
//          signed/unsigned, wchar_t)    -> VariantLong 或 VariantByte(uchar) 或 VariantInt(short)
//     浮点(float/double/long double)    -> VariantDouble
//     wchar_t*  (BSTR别名)              -> VariantString
//     struct vb6_SafeArray1D*           -> VariantArray
//     vb6_VARIANT (已是变体)            -> VariantIdentity (no-op)
//     其他指针 (void*/class*/type*)     -> VariantObject
//   用途: 当 C3 的 inferExprType 把标量/LenB(...)误判为 Variant 时,
//         temp 变量声明 "vb6_VARIANT _vcmp_N = scalar;" 会触发 C2440.
//         改用 vb6_VariantFromValue(scalar) 让编译器按实类型自动包装, 消除 C2440.
#ifdef _MSC_VER
// Fix 092u: MSVC 中 char 默认等价于 signed char, _Generic 关联列表同时出现
// char:/signed char: 被视为重复关联 (error C7700, "char 与之前的 char 不兼容").
// MSVC 下仅保留 char: (已覆盖 signed char); 其他编译器保留标准的三态区分.
#define vb6_VariantFromValue(x) _Generic((x), \
    _Bool:                vb6_VariantBool, \
    char:                 vb6_VariantLong, \
    unsigned char:        vb6_VariantByte, \
    short:                vb6_VariantInt, \
    int:                  vb6_VariantLong, \
    unsigned int:         vb6_VariantLong, \
    long:                 vb6_VariantLong, \
    unsigned long:        vb6_VariantLong, \
    long long:            vb6_VariantLong, \
    unsigned long long:   vb6_VariantLong, \
    wchar_t:              vb6_VariantLong, \
    float:                vb6_VariantDouble, \
    double:               vb6_VariantDouble, \
    wchar_t*:             vb6_VariantString, \
    struct vb6_SafeArray1D*: vb6_VariantArray, \
    vb6_VARIANT:          vb6_VariantIdentity, \
    default:              vb6_VariantObject \
)((x))
#else
#define vb6_VariantFromValue(x) _Generic((x), \
    _Bool:                vb6_VariantBool, \
    char:                 vb6_VariantLong, \
    signed char:          vb6_VariantLong, \
    unsigned char:        vb6_VariantByte, \
    short:                vb6_VariantInt, \
    int:                  vb6_VariantLong, \
    unsigned int:         vb6_VariantLong, \
    long:                 vb6_VariantLong, \
    unsigned long:        vb6_VariantLong, \
    long long:            vb6_VariantLong, \
    unsigned long long:   vb6_VariantLong, \
    wchar_t:              vb6_VariantLong, \
    float:                vb6_VariantDouble, \
    double:               vb6_VariantDouble, \
    wchar_t*:             vb6_VariantString, \
    struct vb6_SafeArray1D*: vb6_VariantArray, \
    vb6_VARIANT:          vb6_VariantIdentity, \
    default:              vb6_VariantObject \
)((x))
#endif

// Index into a Variant that holds an array -- returns element as vb6_VARIANT
vb6_VARIANT vb6_VariantArrayGet(vb6_VARIANT* v, int32_t index);
// Set element in a Variant that holds an array
void vb6_VariantArraySet(vb6_VARIANT* v, int32_t index, vb6_VARIANT val);
// Fix 084f: Variant 数组嵌套索引按值版本 — vGateway(lIdx)(0) 中内层
// vb6_VariantArrayGet(&vGateway, lIdx) 返回 vb6_VARIANT 值(非左值无法取地址),
// 此函数按值接收后再按索引取元素, 替代非法的 (vb6_VARIANT){...} 复合字面量.
vb6_VARIANT vb6_VariantArrayGetVal(vb6_VARIANT v, int32_t index);

// Variant转基本类型
int32_t vb6_VariantToLong(vb6_VARIANT v);
// Fix 093a: Variant → Boolean (CBool 语义). 调用点 (ToolsJsonVba 等) 依赖符号名
// vb6_VariantToBool; 此前缺失 → LNK2019.
int16_t vb6_VariantToBool(vb6_VARIANT v);
double vb6_VariantToDouble(vb6_VARIANT v);
BSTR vb6_VariantToString(vb6_VARIANT v);
// Fix 029: Variant → SafeArray extraction (variant holding array).
// 用于调用点反向强制: callee 期望 vb6_SafeArray1D* 但实参是 vb6_VARIANT.
struct vb6_SafeArray1D* vb6_VariantToSafeArray1D(vb6_VARIANT v);
// Fix 029: Variant → void* (按值传入, 避免调用点包装时的左值问题).
// 与 vb6_VariantToObject(vb6_VARIANT*) 互补; 后者要求实参是左值 (取地址),
// 但 IndexOrCallExpr 调用点包装的实参可能是函数返回的右值, 无法取址.
void* vb6_VariantToObjectVal(vb6_VARIANT v);

// P8.4: Variant清理(释放内含BSTR等资源)
void vb6_VariantClear(vb6_VARIANT* v);
// P8.4: Variant深拷贝(复制BSTR)
void vb6_VariantCopy(vb6_VARIANT* dst, const vb6_VARIANT* src);

#ifdef __cplusplus
}
#endif
