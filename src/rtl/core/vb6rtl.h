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

#ifdef _WIN32
#include <windows.h>
#include <oleauto.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// BSTR - VB6 字符串类型 (OLE BSTR)
// ============================================================

// BSTR 本质是 wchar_t* (指向OLE BSTR长度前缀之后的字符数据)
// Windows: 使用OLE API (SysAllocString/SysFreeString/SysStringLen)
// 非Windows: 自定义malloc实现
typedef wchar_t* BSTR;

// 空BSTR
static inline BSTR vb6_BSTR_Empty(void) {
#ifdef _WIN32
    return SysAllocStringLen(L"", 0);
#else
    return NULL;
#endif
}

// 定长字符串初始化: 返回len个空格的BSTR (VB6 Dim a As String * N)
static inline BSTR vb6_BSTR_FixedSTR(int32_t len) {
    if (len <= 0) return vb6_BSTR_Empty();
#ifdef _WIN32
    BSTR bstr = SysAllocStringLen(NULL, len);
    if (bstr) { for (int32_t i = 0; i < len; i++) bstr[i] = L' '; }
    return bstr;
#else
    return NULL;
#endif
}

// 从宽字符串创建BSTR
// Windows: 使用SysAllocString (分配副本, 可安全释放)
// 非Windows: malloc分配自定义长度前缀
static inline BSTR vb6_BSTR_FromStr(const wchar_t* s) {
    if (!s) return NULL;
#ifdef _WIN32
    return SysAllocString(s);
#else
    size_t len = wcslen(s);
    uint32_t* p = (uint32_t*)malloc(sizeof(uint32_t) + (len + 1) * sizeof(wchar_t));
    if (!p) return NULL;
    *p = (uint32_t)len;
    BSTR bstr = (BSTR)(p + 1);
    memcpy(bstr, s, (len + 1) * sizeof(wchar_t));
    return bstr;
#endif
}

// 复制BSTR (创建独立副本, 调用方负责释放)
static inline BSTR vb6_BSTR_FromBSTR(BSTR bstr) {
    if (!bstr) return vb6_BSTR_Empty();
#ifdef _WIN32
    return SysAllocString(bstr);
#else
    return vb6_BSTR_FromStr(bstr);
#endif
}

// 释放BSTR
static inline void vb6_BSTR_Free(BSTR bstr) {
    if (bstr) {
#ifdef _WIN32
        SysFreeString(bstr);
#else
        uint32_t* p = ((uint32_t*)bstr) - 1;
        free(p);
#endif
    }
}

// M22: BSTR->ANSI conversion (for Declare A-version API ByVal String params)
// Converts BSTR(UTF-16) to GBK multibyte string. Caller must free with vb6_FreeANSI.
static inline char* vb6_BSTR_ToANSI(BSTR bstr) {
    if (!bstr) {
        char* r = (char*)malloc(1);
        if (r) r[0] = '\0';
        return r;
    }
#ifdef _WIN32
    int len = WideCharToMultiByte(CP_ACP, 0, bstr, -1, NULL, 0, NULL, NULL);
    if (len <= 0) { char* r = (char*)malloc(1); if (r) r[0] = '\0'; return r; }
    char* buf = (char*)malloc(len);
    if (!buf) return NULL;
    WideCharToMultiByte(CP_ACP, 0, bstr, -1, buf, len, NULL, NULL);
    return buf;
#else
    // Non-Windows: simplified to UTF-8
    size_t len = wcstombs(NULL, bstr, 0);
    if (len == (size_t)-1) { char* r = (char*)malloc(1); if (r) r[0] = '\0'; return r; }
    char* buf = (char*)malloc(len + 1);
    if (!buf) return NULL;
    wcstombs(buf, bstr, len + 1);
    return buf;
#endif
}

// M22: Free ANSI string buffer
static inline void vb6_FreeANSI(char* ansi) {
    free(ansi);
}

// BSTR赋值 (释放旧值, 复制新值, 防止悬垂指针和双重释放)
static inline void vb6_BSTR_Assign(BSTR* target, BSTR source) {
    if (target) {
        BSTR copy = source ? vb6_BSTR_FromBSTR(source) : vb6_BSTR_Empty();
        vb6_BSTR_Free(*target);
        *target = copy;
    }
}

// BSTR连接
BSTR vb6_BSTR_Concat(BSTR a, BSTR b);

// BSTR连接并释放第一个参数 (用于链式Concat中间节点, 防止临时BSTR泄漏)
// a & b & c → vb6_BSTR_ConcatFree(vb6_BSTR_Concat(a, b), c) → 内层结果被ConcatFree释放
BSTR vb6_BSTR_ConcatFree(BSTR a, BSTR b);

// BSTR转移所有权赋值 (free旧值+直接持有source, 不做deep copy)
// 用于Concat等返回新BSTR的赋值场景, source是返回的新BSTR不存在共享引用
static inline void vb6_BSTR_AssignMove(BSTR* target, BSTR source) {
    if (target) {
        vb6_BSTR_Free(*target);
        *target = source;
    }
}

// BSTR长度 (返回字符数, 非字节数)
static inline int32_t vb6_BSTR_Len(BSTR bstr) {
    if (!bstr) return 0;
#ifdef _WIN32
    return (int32_t)SysStringLen(bstr);
#else
    uint32_t* p = ((uint32_t*)bstr) - 1;
    return (int32_t)*p;
#endif
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

// ============================================================
// 内置函数 (最小子集)
// ============================================================

// 字符串函数
int32_t vb6_Len(BSTR s);
int32_t vb6_LenB_BSTR(BSTR s);  // Fix 048: LenB for BSTR — byte length of string
// Fix 048: LenB — _Generic macro: BSTR → byte length, UDT → sizeof
// VB6 LenB() works for both strings (byte length) and UDTs (structure size)
#define vb6_LenB(x) _Generic((x), \
    BSTR: vb6_LenB_BSTR(x), \
    const BSTR: vb6_LenB_BSTR(x), \
    default: ((int32_t)sizeof(x)) \
)
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
// Fix 090d: VB6 类型转换函数的字符串解析 (支持 &H/&O 前缀, 见 vb6rtl.c)
double vb6_NumVal(BSTR s);
BSTR vb6_Str(int32_t n);
BSTR vb6_Format(vb6_VARIANT expr, BSTR fmt);

// 消息框
int32_t vb6_MsgBox(BSTR prompt, int32_t buttons, BSTR title);
// P7.7: single-arg convenience (MsgBox "text" -> vb6_MsgBox1(text))
static inline int32_t vb6_MsgBox1(BSTR prompt) {
    return vb6_MsgBox(prompt, 0, NULL);
}

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
BSTR vb6_CStr(vb6_VARIANT x);
// M22: typed CStr overloads
BSTR vb6_CStrLong(int32_t x);
BSTR vb6_CStrDbl(double x);
BSTR vb6_CStrBool(int16_t x);
BSTR vb6_CStrByte(uint8_t x);
BSTR vb6_CStrDate(double x);
// P8.4: Variant版转换函数
// Fix 090d: 前置声明 (定义在下方"类型转换"区, 供 inline V 变体引用)
int16_t vb6_CBool(double v);
uint8_t vb6_CByte(double v);
float vb6_CSng(double v);
static inline int16_t vb6_CIntV(vb6_VARIANT v) { return vb6_CInt(vb6_VariantToDouble(v)); }
static inline int32_t vb6_CLngV(vb6_VARIANT v) { return vb6_CLng(vb6_VariantToDouble(v)); }
static inline double vb6_CDblV(vb6_VARIANT v) { return vb6_VariantToDouble(v); }
static inline uint8_t vb6_CByteV(vb6_VARIANT v) { return vb6_CByte(vb6_VariantToDouble(v)); }
static inline float vb6_CSngV(vb6_VARIANT v) { return (float)vb6_VariantToDouble(v); }
static inline int16_t vb6_CBoolV(vb6_VARIANT v) { return (v.vt == vb6_vtBoolean) ? v.boolVal : vb6_CBool(vb6_VariantToDouble(v)); }

// 类型检查
int32_t vb6_IsNumeric(vb6_VARIANT v);
int32_t vb6_IsNull(vb6_VARIANT v);
int32_t vb6_IsEmpty(vb6_VARIANT v);
int32_t vb6_IsObject(vb6_VARIANT v);
int32_t vb6_IsArray(vb6_VARIANT v);
int32_t vb6_IsDate(vb6_VARIANT v);
int32_t vb6_IsError(vb6_VARIANT v);
// P21-09: CVErr — create VT_ERROR Variant
vb6_VARIANT vb6_CVErr(int32_t errorNumber);

BSTR vb6_TypeName(vb6_VARIANT v);
int32_t vb6_VarType(vb6_VARIANT v);

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
BSTR vb6_UCase_str(BSTR s);
int16_t vb6_Like(BSTR source, BSTR pattern);  // Like运算符

// ============================================================
// P14.2.2: 系统函数
// ============================================================
BSTR vb6_Dir(BSTR pathname, int32_t attributes);
BSTR vb6_CurDir(BSTR drive);
int32_t vb6_Shell(BSTR pathname, int32_t windowstyle);
BSTR vb6_Environ(BSTR envstring);
BSTR vb6_Command(void);










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
void vb6_DateSet(BSTR dateStr);
void vb6_TimeSet(BSTR timeStr);
int32_t vb6_Year(double date);
int32_t vb6_Month(double date);
int32_t vb6_Day(double date);
int32_t vb6_Hour(double time);
int32_t vb6_Minute(double time);
int32_t vb6_Second(double time);

// P14.2.4: DateAdd/DateDiff/DatePart/DateSerial
double vb6_DateSerial(int32_t year, int32_t month, int32_t day);
// P14.2.4: DateAdd/DateDiff/DatePart
double vb6_DateAdd(BSTR interval, double number, double date);
int64_t vb6_DateDiff(BSTR interval, double date1, double date2, int32_t firstDayOfWeek, int32_t firstWeekOfYear);
int32_t vb6_DatePart(BSTR interval, double date, int32_t firstDayOfWeek, int32_t firstWeekOfYear);

// P21-B: Weekday/DateValue/TimeSerial/TimeValue
int32_t vb6_Weekday(double date, int32_t firstDayOfWeek);
double vb6_DateValue(BSTR dateStr);
double vb6_TimeSerial(int32_t hour, int32_t minute, int32_t second);
double vb6_TimeValue(BSTR timeStr);

// P14.3.4: App全局对象属性
BSTR vb6_App_Path(void);     // App.Path - EXE所在目录
BSTR vb6_App_EXEName(void);  // App.EXEName - EXE文件名(不含扩展名)
int32_t vb6_App_hInstance(void); // App.hInstance - 模块实例句柄

// P18-C: Clipboard 对象
void   vb6_Clipboard_SetText(BSTR text);
BSTR   vb6_Clipboard_GetText(void);
void   vb6_Clipboard_Clear(void);
void   vb6_Clipboard_SetData(void* pPicture);
int32_t vb6_Clipboard_GetFormat(int32_t format);  // 1=vbCFText, 2=vbCFBitmap, etc.

// P18-C: Screen 对象
int32_t vb6_Screen_Width(void);      // Screen.Width (twips)
int32_t vb6_Screen_Height(void);     // Screen.Height (twips)
int32_t vb6_Screen_MouseX(void);     // Mouse position X (twips)
int32_t vb6_Screen_MouseY(void);     // Mouse position Y (twips)
void*  vb6_Screen_ActiveControl(void);  // Active control HWND
void*  vb6_Screen_ActiveForm(void);     // Active form HWND
int32_t vb6_Screen_TwipsPerPixelX(void);
int32_t vb6_Screen_TwipsPerPixelY(void);

// P18-C: Printer 对象
void   vb6_Printer_Print(BSTR text);
void   vb6_Printer_EndDoc(void);
void   vb6_Printer_NewPage(void);
int32_t vb6_Printer_Width(void);
int32_t vb6_Printer_Height(void);
int32_t vb6_Printer_CurrentX(void);
int32_t vb6_Printer_CurrentY(void);
void   vb6_Printer_SetCurrentX(int32_t x);
void   vb6_Printer_SetCurrentY(int32_t y);

// P18-C: Forms 集合
int32_t vb6_Forms_Count(void);
void*  vb6_Forms_Item(int32_t index);  // 0-based
void   vb6_Forms_Register(void* hwnd);   // 窗体创建时注册
void   vb6_Forms_Unregister(void* hwnd); // 窗体销毁时注销

// P14.2.4: IIf / InputBox
BSTR vb6_IIfBSTR(int32_t cond, BSTR truepart, BSTR falsepart);
int32_t vb6_IIfLong(int32_t cond, int32_t truepart, int32_t falsepart);
double vb6_IIfDouble(int32_t cond, double truepart, double falsepart);
vb6_VARIANT vb6_IIfVariant(int32_t cond, vb6_VARIANT truepart, vb6_VARIANT falsepart);
BSTR vb6_InputBox(BSTR prompt, BSTR title, BSTR defaultstr, int32_t xpos, int32_t ypos, BSTR helpfile, int32_t context);

// 类型转换 (补充)
int16_t vb6_CBool(double v);
uint8_t vb6_CByte(double v);
float vb6_CSng(double v);
double vb6_CDate(vb6_VARIANT v);
BSTR vb6_Hex(int32_t n);
BSTR vb6_Oct(int32_t n);

// P18-A: 兼容性填平 — 新增RTL函数
int64_t vb6_CCur(double v);            // CCur: value * 10000
vb6_VARIANT vb6_CDec(vb6_VARIANT v);     // P20-07: CDec返回真实DECIMAL (vt=14)
long vb6_RGB(int32_t r, int32_t g, int32_t b);  // RGB: OLE color
long vb6_QBColor(int32_t n);        // QBColor: 16-color lookup
double vb6_FileDateTime(BSTR pathname); // FileDateTime: 文件修改时间→VB6 date serial
int32_t vb6_FileLen(BSTR pathname);    // FileLen: 文件大小(字节)
void   vb6_SendKeys(BSTR keys, int32_t wait);    // SendKeys: 发送按键
void   vb6_AppActivate(BSTR title, int32_t wait); // AppActivate: 激活窗口(标题或数字PID)
void   vb6_AppActivateByPid(int32_t pid, int32_t wait); // AppActivate: 按进程ID激活窗口
void   vb6_MidSet(BSTR* target, int32_t start, int32_t len, BSTR replacement); // Mid$ statement赋值

// P18-D: 兼容性填平 — 字符串/指针/格式化函数
int32_t vb6_AscW(BSTR s);           // AscW: Unicode code point
BSTR   vb6_ChrW(int32_t code);      // ChrW: Unicode character
int32_t vb6_AscB(BSTR s);           // AscB: first byte value
BSTR   vb6_ChrB(int32_t code);      // ChrB: single-byte string
double  vb6_Timer(void);            // Timer: seconds since midnight (fractional)
BSTR   vb6_StrConv(BSTR text, int32_t conversion, int32_t localeID); // StrConv
struct vb6_SafeArray1D; // forward declaration
struct vb6_SafeArray1D* vb6_Filter(struct vb6_SafeArray1D* source, BSTR match, int32_t include, int32_t compare);
void*    vb6_StrPtr(BSTR s);          // StrPtr: address of string data
uintptr_t vb6_ObjPtr(void* obj);       // ObjPtr: address of object
BSTR   vb6_LSet(BSTR str, int32_t length);  // LSet: left-justify
BSTR   vb6_RSet(BSTR str, int32_t length);  // RSet: right-justify
BSTR   vb6_WeekdayName(int32_t weekday, int32_t abbreviate, int32_t firstDayOfWeek);
BSTR   vb6_MonthName(int32_t month, int32_t abbreviate);
BSTR   vb6_FormatCurrency(double value, int32_t numDigits, int32_t incLeading, int32_t useParens, int32_t groupDigits);
BSTR   vb6_FormatNumber(double value, int32_t numDigits, int32_t incLeading, int32_t useParens, int32_t groupDigits);
BSTR   vb6_FormatPercent(double value, int32_t numDigits, int32_t incLeading, int32_t useParens, int32_t groupDigits);

// P18-E: 兼容性填平 — 金融函数+文件锁定+Partition
double vb6_SLN(double cost, double salvage, double life);
double vb6_SYD(double cost, double salvage, double life, double period);
double vb6_DDB(double cost, double salvage, double life, double period, double factor);
double vb6_FV(double rate, double nper, double pmt, double pv, int32_t type);
double vb6_PV(double rate, double nper, double pmt, double fv, int32_t type);
double vb6_Pmt(double rate, double nper, double pv, double fv, int32_t type);
double vb6_IPmt(double rate, double per, double nper, double pv, double fv, int32_t type);
double vb6_PPmt(double rate, double per, double nper, double pv, double fv, int32_t type);
double vb6_RATE(double nper, double pmt, double pv, double fv, int32_t type, double guess);
double vb6_NPV(double rate, struct vb6_SafeArray1D* values);
void   vb6_Lock(int32_t filenum, int64_t start, int64_t end);
void   vb6_Unlock(int32_t filenum, int64_t start, int64_t end);
void   vb6_Reset(void);
BSTR   vb6_Partition(int64_t number, int64_t start, int64_t stop, int64_t interval);

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
    vb6_sa_variant=8,   // vb6_VARIANT (需要逐元素清理)
    vb6_sa_ptr   = 9,   // void* (对象引用)
    vb6_sa_currency = 10, // int64_t (VB6 Currency: value*10000)
    vb6_sa_udt    = 11,   // UDT (elemSize set externally)
} vb6_safearray_elemtype;

// 一维数组描述符 (VB6绝大多数用例是一维)
typedef struct vb6_SafeArray1D {
    int32_t signature;                  // Fix 082g: 魔数标识1D数组 = 0x5A1D ('SA1D')
    vb6_safearray_elemtype elemType;  // 元素类型
    int32_t elemSize;                  // 单个元素字节数
    int32_t lBound;                    // 下界 (VB6默认0, 可指定1)
    int32_t uBound;                    // 上界
    int32_t count;                     // 元素个数 = uBound - lBound + 1
    void*   data;                      // 数据指针 (calloc分配, 零初始化)
    int32_t isDynamic;                 // 是否动态数组 (ReDim创建)
} vb6_SafeArray1D;

// P14.2.3: 字符串数组函数
vb6_SafeArray1D* vb6_Split(BSTR expr, BSTR delimiter, int32_t limit, int32_t compare);
BSTR vb6_Join(vb6_SafeArray1D* arr, BSTR delimiter);

// 创建一维静态数组
vb6_SafeArray1D* vb6_SafeArrayCreate1D(vb6_safearray_elemtype elemType,
    int32_t lBound, int32_t uBound);

// 创建一维动态数组 (ReDim)
vb6_SafeArray1D* vb6_SafeArrayReDim1D(vb6_safearray_elemtype elemType,
    int32_t lBound, int32_t uBound);

// UDT版ReDim: elemSize由调用方提供 (用于UDT动态数组)
vb6_SafeArray1D* vb6_SafeArrayReDim1D_Udt(int32_t elemSize,
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

// P21-B: Array() function support
vb6_SafeArray1D* vb6_ArrayCreate(int32_t count);
void vb6_ArraySetLong(vb6_SafeArray1D* arr, int32_t index, int32_t val);
void vb6_ArraySetDouble(vb6_SafeArray1D* arr, int32_t index, double val);
void vb6_ArraySetBSTR(vb6_SafeArray1D* arr, int32_t index, BSTR val);
void vb6_ArraySetVariant(vb6_SafeArray1D* arr, int32_t index, vb6_VARIANT val);
// ============================================================
// SAFEARRAY ND - VB6 多维数组
// ============================================================

typedef struct vb6_SafeArrayBound {
    int32_t lBound;
    int32_t cElements;
} vb6_SafeArrayBound;

typedef struct vb6_SafeArrayND {
    int32_t dimCount;
    vb6_safearray_elemtype elemType;
    int32_t elemSize;
    int32_t totalElements;
    void* data;
    vb6_SafeArrayBound bounds[16];
} vb6_SafeArrayND;

vb6_SafeArrayND* vb6_SafeArrayCreateND(vb6_safearray_elemtype elemType,
    int32_t dimCount, vb6_SafeArrayBound bounds[]);
void vb6_SafeArrayDestroyND(vb6_SafeArrayND* arr);
vb6_SafeArrayND* vb6_SafeArrayReDimND(vb6_safearray_elemtype elemType,
    int32_t dimCount, vb6_SafeArrayBound bounds[]);
vb6_SafeArrayND* vb6_SafeArrayReDimND_Udt(int32_t elemSize,
    int32_t dimCount, vb6_SafeArrayBound bounds[]);
vb6_SafeArrayND* vb6_SafeArrayReDimPreserveND(vb6_SafeArrayND* arr,
    int32_t dimCount, vb6_SafeArrayBound newBounds[]);

int32_t vb6_SafeArrayND_Offset(vb6_SafeArrayND* arr, int32_t dimCount, int32_t indices[]);
void* vb6_SafeArrayND_GetPtr(vb6_SafeArrayND* arr, ...);

int32_t vb6_UBoundND(vb6_SafeArrayND* arr, int32_t dimension);
int32_t vb6_LBoundND(vb6_SafeArrayND* arr, int32_t dimension);

#define VB6_SA_ND_AT1(elemType, arr, i) \
    (*((elemType*)((arr)->data) + \
       ((i) - (arr)->bounds[0].lBound)))

#define VB6_SA_ND_AT2(elemType, arr, i, j) \
    (*((elemType*)((arr)->data) + \
       (((i) - (arr)->bounds[0].lBound) + \
        ((j) - (arr)->bounds[1].lBound) * (arr)->bounds[0].cElements)))

#define VB6_SA_ND_AT3(elemType, arr, i, j, k) \
    (*((elemType*)((arr)->data) + \
       (((i) - (arr)->bounds[0].lBound) + \
        ((j) - (arr)->bounds[1].lBound) * (arr)->bounds[0].cElements + \
        ((k) - (arr)->bounds[2].lBound) * (arr)->bounds[0].cElements * (arr)->bounds[1].cElements)))

// 文件 I/O
int32_t vb6_FreeFile(void);
int32_t vb6_Open(BSTR pathname, int32_t mode, int32_t access, int32_t filenumber, int32_t reclength);
int32_t vb6_Close(int32_t filenumber);
int32_t vb6_CloseAll();  // Close all open files
int32_t vb6_EOF(int32_t filenumber);
int32_t vb6_LOF(int32_t filenumber);
int32_t vb6_Loc(int32_t filenumber);// P21-13: Seek function (return current file position)
int32_t vb6_SeekFunc(int32_t filenumber);
// P21-13: Seek statement (set file position)
void vb6_SeekStmt(int32_t filenumber, int32_t position);

// P22-08: Width# — set file output line width
void vb6_Width(int32_t filenumber, int32_t width);

void vb6_Print(int32_t filenumber, BSTR s);
void vb6_Write(int32_t filenumber, BSTR s);
BSTR vb6_LineInput(int32_t filenumber);
int32_t vb6_Input(int32_t filenumber, BSTR* outVar);
BSTR vb6_InputString(int32_t filenumber, int32_t count);  // P15.4: Input function
int32_t vb6_Kill(BSTR pathname);
int32_t vb6_MkDir(BSTR pathname);
int32_t vb6_RmDir(BSTR pathname);
int32_t vb6_ChDir(BSTR pathname);
int32_t vb6_ChDrive(BSTR drive);
int32_t vb6_Name(BSTR oldPath, BSTR newPath);
int32_t vb6_FileCopy(BSTR source, BSTR destination);

// P8.2: 随机/二进制文件访问 (Get/Put)
int32_t vb6_Get(int32_t filenumber, int32_t recnumber, void* varPtr, int32_t varSize);
int32_t vb6_Put(int32_t filenumber, int32_t recnumber, void* varPtr, int32_t varSize);

// 错误处理
// Variant比较 (VB6语义: 数值vs数值, 字符串vs字符串, 混合转Double)
int32_t vb6_VarCmpEq(vb6_VARIANT* a, vb6_VARIANT* b);
int32_t vb6_VarCmpNe(vb6_VARIANT* a, vb6_VARIANT* b);
int32_t vb6_VarCmpLt(vb6_VARIANT* a, vb6_VARIANT* b);
int32_t vb6_VarCmpGt(vb6_VARIANT* a, vb6_VARIANT* b);
int32_t vb6_VarCmpLe(vb6_VARIANT* a, vb6_VARIANT* b);
int32_t vb6_VarCmpGe(vb6_VARIANT* a, vb6_VARIANT* b);
int32_t vb6_VarCmpLongEq(vb6_VARIANT* a, int32_t b);
int32_t vb6_VarCmpLongLt(vb6_VARIANT* a, int32_t b);
int32_t vb6_VarCmpLongGt(vb6_VARIANT* a, int32_t b);
int32_t vb6_VarCmpLongLe(vb6_VARIANT* a, int32_t b);
int32_t vb6_VarCmpLongGe(vb6_VARIANT* a, int32_t b);
int32_t vb6_VarCmpLongNe(vb6_VARIANT* a, int32_t b);

int32_t vb6_ErrNumber(void);
BSTR vb6_ErrDescription(void);
void vb6_ErrClear(void);
void vb6_RaiseError(int32_t errNum, BSTR description);
BSTR vb6_ErrSource(void);
void vb6_ErrRaise(int32_t errNum, BSTR source, BSTR description);
void vb6_ErrRaiseNumber(int32_t errNum);

// 全局错误处理状态 (由cgen生成的代码直接使用)
extern int32_t vb6_err_resume_next;  // On Error Resume Next 标志
extern int32_t vb6_err_jmp_active;    // On Error GoTo label 标志
extern void* vb6_err_handler_label;   // 错误跳转标签 (MVP, 暂不用)
extern jmp_buf* vb6_error_jmp_ptr;    // 指向当前函数的局部jmp_buf (P12.3)
extern int32_t vb6_error_jmp_set;     // setjmp 是否已设置

// P14.1.2: Resume恢复点跟踪
extern int32_t vb6_err_resume_point;    // 出错语句resume点索引
extern int32_t vb6_err_resume_next_point;   // 出错下一句resume点索引
extern int32_t vb6_err_dispatch;        // dispatch switch变量
extern int32_t vb6_err_in_handler;      // 当前在On Error GoTo处理器中

// P12.3: On Error嵌套 — 保存/恢复调用者的错误处理状态
#define VB6_ERR_STACK_SIZE 8
void vb6_SaveErrState(void);      // 保存当前错误状态到栈 (函数入口调用)
void vb6_RestoreErrState(void);   // 从栈恢复错误状态 (函数出口调用)

// 字典访问
BSTR vb6_BSTR_Concat(BSTR a, BSTR b);

// ============================================================
// 类支持 (类实例分配/释放)
// ============================================================

void* vb6_Alloc(size_t size);
void vb6_Free(void* ptr);

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

// ============================================================
// 运行时初始化/退出
// ============================================================

// ============================================================
// ParamArray runtime support (P14.1.5)
// ============================================================

SAFEARRAY* vb6_PA_Create(int32_t count);
void vb6_PA_Destroy(SAFEARRAY* psa);
void vb6_PA_SetVariant(SAFEARRAY* psa, int32_t index, VARIANT* pv);
void vb6_PA_SetLong(SAFEARRAY* psa, int32_t index, int32_t val);
void vb6_PA_SetLongPtr(SAFEARRAY* psa, int32_t index, intptr_t val);  /* Fix 082: x64-safe VarPtr parameter */
void vb6_PA_SetDouble(SAFEARRAY* psa, int32_t index, double val);
void vb6_PA_SetBSTR(SAFEARRAY* psa, int32_t index, BSTR val);
VARIANT vb6_PA_GetVariant(SAFEARRAY* psa, int32_t index);
int32_t vb6_PA_GetLong(SAFEARRAY* psa, int32_t index);
double vb6_PA_GetDouble(SAFEARRAY* psa, int32_t index);
BSTR vb6_PA_GetBSTR(SAFEARRAY* psa, int32_t index);
int32_t vb6_IsMissing(SAFEARRAY* psa);
int32_t vb6_PA_UBound(SAFEARRAY* psa);
int32_t vb6_PA_LBound(SAFEARRAY* psa);

void vb6_Init(void);
void vb6_Exit(void);
void vb6_End(void);
void vb6_Beep(void);

// P18-C: Option Compare (Text/Binary)
extern int g_vb6_optionCompareText;  // 0=Binary(default), 1=Text
int vb6_StrCmp(const wchar_t* a, const wchar_t* b);  // respects Option Compare

// Fix 048: Missing runtime functions that were generating C4013 warnings

// vb6_SA_Destroy — generic SafeArray destroyer (used by COM class destructor codegen)
// Destroys 1D SafeArray; for NULL it's a no-op
static inline void vb6_SA_Destroy(void* arr) {
    if (arr) vb6_SafeArrayDestroy1D((vb6_SafeArray1D*)arr);
}

// vb6_DebugAssert — Debug.Assert (no-op in compiled mode)
static inline void vb6_DebugAssert(int32_t cond) {
    (void)cond;  /* no-op: Debug.Assert only active in IDE */
}

// vb6_DebugPrint — Debug.Print (no-op in compiled mode, use DebugPrintStr for actual output)
static inline void vb6_DebugPrint(BSTR s) {
    vb6_DebugPrintStr(s);
}

// vb6_LoadResData — LoadResData (returns empty Variant, resource loading not supported)
vb6_VARIANT vb6_LoadResData(int32_t resourceId, int32_t resourceType);


#ifdef __cplusplus
}
#endif
