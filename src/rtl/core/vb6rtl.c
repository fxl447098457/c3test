// vb6rtl.c - VB6运行时库最小实现
// 仅支持 hello.bas 等简单程序运行

#include "vb6rtl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#endif

// ============================================================
// COM互操作前向声明 (实现在vb6com.c中，避免vb6_VARIANT类型冲突)
// ============================================================
extern void* vb6_CreateObject(const wchar_t* progId);
extern void* vb6_GetObject(const wchar_t* pathName, const wchar_t* progId);
extern int32_t vb6_IsNothing(void* obj);
extern void vb6_ReleaseObject(void** objPtr);
extern void* vb6_ComCall(void* disp, const wchar_t* methodName, void* args, int32_t argc);
extern void* vb6_ComGetProp(void* disp, const wchar_t* propName);
extern void vb6_ComSetProp(void* disp, const wchar_t* propName, void* value);
extern void vb6_ComSetRef(void* disp, const wchar_t* propName, void* objRef);
extern void vb6_ComInit(void);
extern void vb6_ComExit(void);

// ============================================================
// BSTR 操作
// ============================================================

BSTR vb6_BSTR_Concat(BSTR a, BSTR b) {
    int32_t lenA = vb6_BSTR_Len(a);
    int32_t lenB = vb6_BSTR_Len(b);
    int32_t total = lenA + lenB;

    BSTR result = SysAllocStringLen(NULL, total);
    if (!result) return NULL;

    if (a) memcpy(result, a, lenA * sizeof(wchar_t));
    if (b) memcpy(result + lenA, b, lenB * sizeof(wchar_t));
    result[total] = L'\0';

    return result;
}

BSTR vb6_BSTR_ConcatFree(BSTR a, BSTR b) {
    BSTR result = vb6_BSTR_Concat(a, b);
    vb6_BSTR_Free(a);  /* 释放中间临时BSTR */
    return result;
}

// ============================================================
// 内置函数实现
// ============================================================

int32_t vb6_Len(BSTR s) {
    return vb6_BSTR_Len(s);
}

BSTR vb6_Left(BSTR s, int32_t n) {
    if (!s || n <= 0) return vb6_BSTR_Empty();
    int32_t len = vb6_BSTR_Len(s);
    if (n > len) n = len;
    wchar_t* buf = (wchar_t*)malloc((n + 1) * sizeof(wchar_t));
    memcpy(buf, s, n * sizeof(wchar_t));
    buf[n] = L'\0';
    BSTR result = vb6_BSTR_FromStr(buf);
    free(buf);
    return result;
}

BSTR vb6_Right(BSTR s, int32_t n) {
    if (!s || n <= 0) return vb6_BSTR_Empty();
    int32_t len = vb6_BSTR_Len(s);
    if (n > len) n = len;
    return vb6_BSTR_FromStr(s + len - n);
}

BSTR vb6_Mid(BSTR s, int32_t start, int32_t len) {
    if (!s || start < 1) return vb6_BSTR_Empty();
    int32_t slen = vb6_BSTR_Len(s);
    int32_t offset = start - 1;  // VB6是1-based
    if (offset >= slen) return vb6_BSTR_Empty();
    if (offset + len > slen) len = slen - offset;
    wchar_t* buf = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
    memcpy(buf, s + offset, len * sizeof(wchar_t));
    buf[len] = L'\0';
    BSTR result = vb6_BSTR_FromStr(buf);
    free(buf);
    return result;
}

int32_t vb6_InStr(int32_t start, BSTR haystack, BSTR needle) {
    if (!haystack || !needle) return 0;
    int32_t hlen = vb6_BSTR_Len(haystack);
    int32_t nlen = vb6_BSTR_Len(needle);
    if (nlen == 0) return start;
    if (start < 1) start = 1;
    for (int32_t i = start - 1; i <= hlen - nlen; i++) {
        if (memcmp(haystack + i, needle, nlen * sizeof(wchar_t)) == 0) {
            return i + 1;  // 1-based
        }
    }
    return 0;
}

BSTR vb6_UCase(BSTR s) {
    if (!s) return vb6_BSTR_Empty();
    int32_t len = vb6_BSTR_Len(s);
    wchar_t* buf = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
    for (int32_t i = 0; i < len; i++) {
        buf[i] = towupper(s[i]);
    }
    buf[len] = L'\0';
    BSTR result = vb6_BSTR_FromStr(buf);
    free(buf);
    return result;
}

BSTR vb6_LCase(BSTR s) {
    if (!s) return vb6_BSTR_Empty();
    int32_t len = vb6_BSTR_Len(s);
    wchar_t* buf = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
    for (int32_t i = 0; i < len; i++) {
        buf[i] = towlower(s[i]);
    }
    buf[len] = L'\0';
    BSTR result = vb6_BSTR_FromStr(buf);
    free(buf);
    return result;
}

BSTR vb6_Trim(BSTR s) {
    if (!s) return vb6_BSTR_Empty();
    int32_t len = vb6_BSTR_Len(s);
    int32_t start = 0, end = len;
    while (start < len && iswspace(s[start])) start++;
    while (end > start && iswspace(s[end - 1])) end--;
    int32_t trimmed = end - start;
    wchar_t* buf = (wchar_t*)malloc((trimmed + 1) * sizeof(wchar_t));
    memcpy(buf, s + start, trimmed * sizeof(wchar_t));
    buf[trimmed] = L'\0';
    BSTR result = vb6_BSTR_FromStr(buf);
    free(buf);
    return result;
}

BSTR vb6_LTrim(BSTR s) {
    if (!s) return vb6_BSTR_Empty();
    int32_t len = vb6_BSTR_Len(s);
    int32_t start = 0;
    while (start < len && iswspace(s[start])) start++;
    return vb6_BSTR_FromStr(s + start);
}

BSTR vb6_RTrim(BSTR s) {
    if (!s) return vb6_BSTR_Empty();
    int32_t len = vb6_BSTR_Len(s);
    while (len > 0 && iswspace(s[len - 1])) len--;
    wchar_t* buf = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
    memcpy(buf, s, len * sizeof(wchar_t));
    buf[len] = L'\0';
    BSTR result = vb6_BSTR_FromStr(buf);
    free(buf);
    return result;
}

BSTR vb6_Chr(int32_t code) {
    wchar_t buf[2] = { (wchar_t)code, L'\0' };
    return vb6_BSTR_FromStr(buf);
}

int32_t vb6_Asc(BSTR s) {
    if (!s || vb6_BSTR_Len(s) == 0) return 0;
    return (int32_t)s[0];
}

double vb6_Val(BSTR s) {
    if (!s) return 0.0;
    // 简化: 转为窄字符串用strtod
    char narrow[256];
    size_t len = vb6_BSTR_Len(s);
    if (len > 255) len = 255;
    for (size_t i = 0; i < len; i++) narrow[i] = (char)s[i];
    narrow[len] = '\0';
    return strtod(narrow, NULL);
}

BSTR vb6_Str(int32_t n) {
    wchar_t buf[32];
    swprintf(buf, 32, L"%d", n);
    return vb6_BSTR_FromStr(buf);
}

BSTR vb6_Format(vb6_VARIANT expr, BSTR fmt) {
    // P8.4: 改进版 - 支持更多Variant类型转换
    (void)fmt;
    switch (expr.vt) {
        case vb6_vtEmpty:
            return vb6_BSTR_FromStr(L"");
        case vb6_vtNull:
            return vb6_BSTR_FromStr(L"");
        case vb6_vtInteger:
            return vb6_Str((int32_t)expr.iVal);
        case vb6_vtLong:
            return vb6_Str(expr.lVal);
        case vb6_vtDouble:
        case vb6_vtDate: {
            wchar_t buf[64];
            swprintf(buf, 64, L"%g", expr.dblVal);
            return vb6_BSTR_FromStr(buf);
        }
        case vb6_vtBSTR:
            // P8.4: String类型直接返回副本
            return expr.bstrVal ? vb6_BSTR_FromBSTR(expr.bstrVal) : vb6_BSTR_Empty();
        case vb6_vtBoolean:
            return vb6_BSTR_FromStr(expr.boolVal ? L"True" : L"False");
        case vb6_vtByte:
            return vb6_Str((int32_t)expr.bVal);
        default:
            return vb6_BSTR_FromStr(L"");
    }
}

// ============================================================
// MsgBox
// ============================================================

int32_t vb6_MsgBox(BSTR prompt, int32_t buttons, BSTR title) {
    (void)buttons;
    // 简化: 控制台输出
    wprintf(L"MsgBox: %s\n", prompt ? prompt : L"");
    (void)title;
    return 1;  // vbOK
}

// ============================================================
// 数值函数
// ============================================================

double vb6_Abs(double x) { return fabs(x); }
int32_t vb6_Sgn(double x) { return (x > 0) ? 1 : (x < 0) ? -1 : 0; }
double vb6_Sqr(double x) { return sqrt(x); }
double vb6_Round(double x, int32_t decimals) {
    double factor = pow(10.0, (double)decimals);
    return round(x * factor) / factor;
}
float vb6_Rnd(int32_t seed) {
    (void)seed;
    return (float)rand() / (float)RAND_MAX;
}

// ============================================================
// 转换函数
// ============================================================

int16_t vb6_CInt(double x) { return (int16_t)round(x); }
int32_t vb6_CLng(double x) { return (int32_t)round(x); }
double vb6_CDbl(double x) {
    return x;
}

BSTR vb6_CStr(vb6_VARIANT x) {
    return vb6_Format(x, NULL);
}

// ============================================================
// 类型检查
// ============================================================

int32_t vb6_IsNumeric(vb6_VARIANT v) {
    switch (v.vt) {
        case vb6_vtInteger: case vb6_vtLong: case vb6_vtSingle:
        case vb6_vtDouble: case vb6_vtCurrency: case vb6_vtByte:
        case vb6_vtBoolean:
            return -1;  // VB6 True
        default:
            return 0;
    }
}

int32_t vb6_IsNull(vb6_VARIANT v) { return v.vt == vb6_vtNull ? -1 : 0; }
int32_t vb6_IsEmpty(vb6_VARIANT v) { return v.vt == vb6_vtEmpty ? -1 : 0; }
int32_t vb6_IsObject(vb6_VARIANT v) { return (v.vt == vb6_vtDispatch && v.pdispVal != NULL) ? -1 : 0; }
int32_t vb6_IsArray(vb6_VARIANT v) { (void)v; return 0; }  // 简化: 暂不支持
int32_t vb6_IsDate(vb6_VARIANT v) { return v.vt == vb6_vtDate ? -1 : 0; }
int32_t vb6_IsError(vb6_VARIANT v) { return v.vt == vb6_vtError ? -1 : 0; }

// P8.4: VarType - 返回Variant的VT类型码
int32_t vb6_VarType(vb6_VARIANT v) { return (int32_t)v.vt; }

// P8.4: TypeName - 返回Variant类型的VB6类型名
BSTR vb6_TypeName(vb6_VARIANT v) {
    const wchar_t* name = L"Empty";
    switch (v.vt) {
        case vb6_vtEmpty:    name = L"Empty"; break;
        case vb6_vtNull:     name = L"Null"; break;
        case vb6_vtInteger:  name = L"Integer"; break;
        case vb6_vtLong:     name = L"Long"; break;
        case vb6_vtSingle:   name = L"Single"; break;
        case vb6_vtDouble:   name = L"Double"; break;
        case vb6_vtCurrency: name = L"Currency"; break;
        case vb6_vtDate:     name = L"Date"; break;
        case vb6_vtBSTR:     name = L"String"; break;
        case vb6_vtDispatch: name = L"Object"; break;
        case vb6_vtError:    name = L"Error"; break;
        case vb6_vtBoolean:  name = L"Boolean"; break;
        case vb6_vtByte:     name = L"Byte"; break;
        default:             name = L"Variant"; break;
    }
    return vb6_BSTR_FromStr(name);
}

// ============================================================
// Debug对象
// ============================================================

void vb6_Debug_Print(BSTR s) {
    if (s) {
        wprintf(L"%ls\n", s);
    } else {
        wprintf(L"\n");
    }
    fflush(stdout);
}

void vb6_Debug_PrintInt(int32_t n) {
    wprintf(L"%d\n", n);
    fflush(stdout);
}

void vb6_Debug_PrintDouble(double d) {
    wprintf(L"%g\n", d);
    fflush(stdout);
}

// ============================================================
// 对象操作 (占位)
// ============================================================

// ============================================================
// 对象操作 (P6: COM互操作)
// ============================================================

void* vb6_NewObject(const wchar_t* className) {
    // 对于未知类名，尝试通过COM创建 (Dim x As New ClassName，className不在已知类中)
    // VB6中如果className不是项目内的类模块，则尝试COM创建
    return vb6_CreateObject(className);
}

int32_t vb6_TypeOf(void* obj, const wchar_t* typeName) {
    if (!obj) return 0;  // Nothing不匹配任何类型
    // TypeOf的完整实现需要IDispatch/ITypeInfo，在vb6com.c中
    // 简化版: 始终返回False (后续P6.2完善)
    (void)typeName;
    return 0;
}

void* vb6_DictAccess(void* obj, const wchar_t* key) {
    (void)obj; (void)key;
    return NULL;
}

// ============================================================
// Debug.Print 变参版 (cgen生成用)
// ============================================================

void vb6_DebugPrintStr(BSTR s) {
    if (s) {
        wprintf(L"%ls", s);
    }
    wprintf(L"\n");
    fflush(stdout);
}

// Debug.Print 分项输出
void vb6_DebugWriteBSTR(BSTR s) {
    if (s) wprintf(L"%ls", s);
    fflush(stdout);
}

void vb6_DebugWriteLong(int32_t n) {
    wprintf(L"%d", n);
    fflush(stdout);
}

void vb6_DebugWriteDouble(double d) {
    wprintf(L"%g", d);
    fflush(stdout);
}

void vb6_DebugWriteNewline(void) {
    wprintf(L"\n");
    fflush(stdout);
}

void vb6_DebugOutputFmt(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    for (const char* p = fmt; *p; p++) {
        switch (*p) {
            case 's': {
                BSTR s = va_arg(args, BSTR);
                if (s) wprintf(L"%ls", s);
                break;
            }
            case 'd': {
                int32_t n = va_arg(args, int32_t);
                wprintf(L"%d", n);
                break;
            }
            case 'f': {
                double d = va_arg(args, double);
                wprintf(L"%g", d);
                break;
            }
            default:
                break;
        }
    }
    wprintf(L"\n");
    fflush(stdout);
    va_end(args);
}

// ============================================================
// 整除 / 幂运算
// ============================================================

int32_t vb6_IntDiv(int32_t a, int32_t b) {
    if (b == 0) return 0;  // TODO: raise error
    // VB6 \ 运算符: 截断到整数 (C的整数除法对正负数的行为与VB6一致)
    return a / b;
}

double vb6_Pow(double base, double exp) {
    return pow(base, exp);
}

// ============================================================
// 运行时初始化/退出
// ============================================================

void vb6_Init(void) {
    // 初始化随机种子
    srand((unsigned int)time(NULL));
    // 初始化COM库 (实现在vb6com.c中)
    vb6_ComInit();
}

void vb6_Exit(void) {
    // 清理COM库
    vb6_ComExit();
}

void vb6_End(void) {
    vb6_Exit();
    exit(0);
}

void vb6_Beep(void) {
#ifdef _WIN32
    // Beep() requires windows.h, use MessageBeep as fallback
    MessageBeep(0);
#else
    putchar('\a');
    fflush(stdout);
#endif
}

// ============================================================
// 类支持: 实例分配/释放
// ============================================================

void* vb6_Alloc(size_t size) {
    void* p = calloc(1, size);  // calloc 自动清零 = VB6默认值初始化
    return p;
}

void vb6_Free(void* ptr) {
    free(ptr);
}

int32_t vb6_VariantToLong(vb6_VARIANT v) {
    switch (v.vt) {
        case vb6_vtBoolean: return v.boolVal ? -1 : 0;
        case vb6_vtByte:    return (int32_t)v.bVal;
        case vb6_vtInteger: return (int32_t)v.iVal;
        case vb6_vtLong:    return v.lVal;
        case vb6_vtSingle:  return (int32_t)round(v.fltVal);
        case vb6_vtDouble:  return (int32_t)round(v.dblVal);
        case vb6_vtCurrency:return (int32_t)(v.cyVal / 10000);
        case vb6_vtBSTR:    return (int32_t)vb6_Val(v.bstrVal);
        default:            return 0;
    }
}

double vb6_VariantToDouble(vb6_VARIANT v) {
    switch (v.vt) {
        case vb6_vtBoolean: return v.boolVal ? -1.0 : 0.0;
        case vb6_vtByte:    return (double)v.bVal;
        case vb6_vtInteger: return (double)v.iVal;
        case vb6_vtLong:    return (double)v.lVal;
        case vb6_vtSingle:  return (double)v.fltVal;
        case vb6_vtDouble:  return v.dblVal;
        case vb6_vtCurrency:return (double)v.cyVal / 10000.0;
        case vb6_vtBSTR:    return vb6_Val(v.bstrVal);
        default:            return 0.0;
    }
}

BSTR vb6_VariantToString(vb6_VARIANT v) {
    return vb6_CStr(v);
}

// P8.4: Variant清理 - 释放内含BSTR等资源
void vb6_VariantClear(vb6_VARIANT* v) {
    if (!v) return;
    // 释放BSTR
    if (v->vt == vb6_vtBSTR && v->bstrVal) {
        vb6_BSTR_Free(v->bstrVal);
        v->bstrVal = NULL;
    }
    // 释放IDispatch指针
    if (v->vt == vb6_vtDispatch && v->pdispVal) {
        vb6_ReleaseObject(&v->pdispVal);
        v->pdispVal = NULL;
    }
    v->vt = vb6_vtEmpty;
}

// P8.4: Variant深拷贝 - 复制BSTR等需要独立所有权的资源
void vb6_VariantCopy(vb6_VARIANT* dst, const vb6_VARIANT* src) {
    if (!dst || !src) return;
    *dst = *src;  // 浅拷贝
    // BSTR需要深拷贝
    if (src->vt == vb6_vtBSTR && src->bstrVal) {
        dst->bstrVal = vb6_BSTR_FromBSTR(src->bstrVal);
    }
    // Dispatch需要AddRef
    if (src->vt == vb6_vtDispatch && src->pdispVal) {
        // COM AddRef would go here; simplified: just copy pointer
        dst->pdispVal = src->pdispVal;
    }
}

// ============================================================
// 字符串函数 (补充)
// ============================================================

BSTR vb6_Replace(BSTR expr, BSTR find, BSTR rep, int32_t start, int32_t count, int32_t compare) {
    (void)compare;  // 简化: 仅支持二进制比较
    if (!expr || !find) return expr ? vb6_BSTR_FromStr(expr) : vb6_BSTR_Empty();
    int32_t exprLen = vb6_BSTR_Len(expr);
    int32_t findLen = vb6_BSTR_Len(find);
    int32_t repLen = rep ? vb6_BSTR_Len(rep) : 0;
    if (findLen == 0 || exprLen == 0) return vb6_BSTR_FromStr(expr);

    if (start < 1) start = 1;
    int32_t maxCount = (count == -1) ? INT32_MAX : count;

    // 计算结果长度
    int32_t matches = 0;
    int32_t pos = start - 1;
    while (matches < maxCount) {
        wchar_t* found = wcsstr(expr + pos, find);
        if (!found) break;
        matches++;
        pos = (int32_t)(found - expr) + findLen;
    }
    if (matches == 0) return vb6_BSTR_FromStr(expr);

    int32_t resultLen = exprLen + matches * (repLen - findLen);
    uint32_t* p = (uint32_t*)malloc(sizeof(uint32_t) + (resultLen + 1) * sizeof(wchar_t));
    if (!p) return NULL;
    *p = (uint32_t)resultLen;
    BSTR result = (BSTR)(p + 1);

    // 执行替换
    pos = start - 1;
    int32_t outPos = 0;
    int32_t done = 0;
    while (done < matches) {
        wchar_t* found = wcsstr(expr + pos, find);
        if (!found) break;
        int32_t beforeLen = (int32_t)(found - expr) - pos;
        if (beforeLen > 0) {
            memcpy(result + outPos, expr + pos, beforeLen * sizeof(wchar_t));
            outPos += beforeLen;
        }
        if (repLen > 0) {
            memcpy(result + outPos, rep, repLen * sizeof(wchar_t));
            outPos += repLen;
        }
        pos = (int32_t)(found - expr) + findLen;
        done++;
    }
    // 剩余部分
    int32_t remain = exprLen - pos;
    if (remain > 0) memcpy(result + outPos, expr + pos, remain * sizeof(wchar_t));
    result[resultLen] = L'\0';
    return result;
}

BSTR vb6_Space(int32_t n) {
    if (n <= 0) return vb6_BSTR_Empty();
    wchar_t* buf = (wchar_t*)malloc((n + 1) * sizeof(wchar_t));
    for (int32_t i = 0; i < n; i++) buf[i] = L' ';
    buf[n] = L'\0';
    BSTR result = vb6_BSTR_FromStr(buf);
    free(buf);
    return result;
}

BSTR vb6_String(int32_t n, int32_t charCode) {
    if (n <= 0) return vb6_BSTR_Empty();
    wchar_t* buf = (wchar_t*)malloc((n + 1) * sizeof(wchar_t));
    for (int32_t i = 0; i < n; i++) buf[i] = (wchar_t)charCode;
    buf[n] = L'\0';
    BSTR result = vb6_BSTR_FromStr(buf);
    free(buf);
    return result;
}

int32_t vb6_StrComp(BSTR s1, BSTR s2, int32_t compare) {
    (void)compare;  // 简化: 仅二进制比较
    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;
    int32_t len1 = vb6_BSTR_Len(s1);
    int32_t len2 = vb6_BSTR_Len(s2);
    int32_t minLen = (len1 < len2) ? len1 : len2;
    int cmp = memcmp(s1, s2, minLen * sizeof(wchar_t));
    if (cmp != 0) return (cmp < 0) ? -1 : 1;
    if (len1 < len2) return -1;
    if (len1 > len2) return 1;
    return 0;
}

BSTR vb6_StrReverse(BSTR s) {
    if (!s) return vb6_BSTR_Empty();
    int32_t len = vb6_BSTR_Len(s);
    wchar_t* buf = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
    for (int32_t i = 0; i < len; i++) buf[i] = s[len - 1 - i];
    buf[len] = L'\0';
    BSTR result = vb6_BSTR_FromStr(buf);
    free(buf);
    return result;
}

int32_t vb6_InStrRev(BSTR haystack, BSTR needle, int32_t start, int32_t compare) {
    (void)compare;
    if (!haystack || !needle) return 0;
    int32_t hLen = vb6_BSTR_Len(haystack);
    int32_t nLen = vb6_BSTR_Len(needle);
    if (nLen == 0) return hLen;
    if (nLen > hLen) return 0;
    if (start <= 0 || start > hLen) start = hLen;
    for (int32_t i = start - nLen; i >= 0; i--) {
        if (memcmp(haystack + i, needle, nLen * sizeof(wchar_t)) == 0) {
            return i + 1;  // 1-based
        }
    }
    return 0;
}

BSTR vb6_LCase_str(BSTR s) { return vb6_LCase(s); }  // 别名
BSTR vb6_UCase_str(BSTR s) { return vb6_UCase(s); }

// ============================================================
// P14.2.1: Like运算符 - 通配符模式匹配
// 支持: ? (单字符), * (零或多个字符), # (单个数字),
//       [charlist] (字符列表), [!charlist] (排除字符列表)
// ============================================================

static int likeMatch(const wchar_t* src, const wchar_t* pat) {
    while (*pat) {
        if (*pat == L'*') {
            while (*pat == L'*') pat++;
            if (*pat == L'\0') return 1;
            while (*src) {
                if (likeMatch(src, pat)) return 1;
                src++;
            }
            return likeMatch(src, pat);
        }
        else if (*pat == L'?') {
            if (*src == L'\0') return 0;
            src++; pat++;
        }
        else if (*pat == L'#') {
            if (*src == L'\0') return 0;
            if (*src < L'0' || *src > L'9') return 0;
            src++; pat++;
        }
        else if (*pat == L'[') {
            pat++;
            int negate = 0;
            if (*pat == L'!') { negate = 1; pat++; }
            int match = 0;
            if (*src == L'\0') return 0;
            while (*pat && *pat != L']') {
                if (pat[1] == L'-' && pat[2] && pat[2] != L']') {
                    wchar_t lo = *pat, hi = pat[2];
                    if (*src >= lo && *src <= hi) match = 1;
                    pat += 3;
                } else {
                    if (*src == *pat) match = 1;
                    pat++;
                }
            }
            if (*pat == L']') pat++;
            if (negate) match = !match;
            if (!match) return 0;
            src++;
        }
        else {
            if (*src != *pat) return 0;
            src++; pat++;
        }
    }
    return (*src == L'\0');
}

int16_t vb6_Like(BSTR source, BSTR pattern) {
    const wchar_t* s = source ? source : L"";
    const wchar_t* p = pattern ? pattern : L"";
    return likeMatch(s, p) ? -1 : 0;
}

// ============================================================

// ============================================================
// P14.2.2: 系统函数 (Dir/CurDir/Shell/Environ/Command)
// ============================================================

// Dir() - 静态状态，支持多次调用遍历
static HANDLE vb6_dir_handle = INVALID_HANDLE_VALUE;
static WIN32_FIND_DATAW vb6_dir_data;
static int vb6_dir_first = 0;

BSTR vb6_Dir(BSTR pathname, int32_t attributes) {
    if (pathname && vb6_BSTR_Len(pathname) > 0) {
        // 新搜索: 关闭之前的句柄
        if (vb6_dir_handle != INVALID_HANDLE_VALUE) {
            FindClose(vb6_dir_handle);
            vb6_dir_handle = INVALID_HANDLE_VALUE;
        }
        vb6_dir_handle = FindFirstFileW(pathname, &vb6_dir_data);
        if (vb6_dir_handle == INVALID_HANDLE_VALUE) {
            return vb6_BSTR_Empty();  // 未找到
        }
        vb6_dir_first = 1;
        // 跳过 . 和 ..
        while (wcscmp(vb6_dir_data.cFileName, L".") == 0 ||
               wcscmp(vb6_dir_data.cFileName, L"..") == 0) {
            if (!FindNextFileW(vb6_dir_handle, &vb6_dir_data)) {
                FindClose(vb6_dir_handle);
                vb6_dir_handle = INVALID_HANDLE_VALUE;
                return vb6_BSTR_Empty();
            }
        }
        return vb6_BSTR_FromStr(vb6_dir_data.cFileName);
    } else {
        // 继续搜索
        if (vb6_dir_handle == INVALID_HANDLE_VALUE) {
            return vb6_BSTR_Empty();
        }
        while (FindNextFileW(vb6_dir_handle, &vb6_dir_data)) {
            if (wcscmp(vb6_dir_data.cFileName, L".") == 0 ||
                wcscmp(vb6_dir_data.cFileName, L"..") == 0) {
                continue;
            }
            return vb6_BSTR_FromStr(vb6_dir_data.cFileName);
        }
        FindClose(vb6_dir_handle);
        vb6_dir_handle = INVALID_HANDLE_VALUE;
        return vb6_BSTR_Empty();
    }
}

BSTR vb6_CurDir(BSTR drive) {
    wchar_t buf[MAX_PATH];
    if (drive && vb6_BSTR_Len(drive) > 0) {
        wchar_t driveLetter[4];
        driveLetter[0] = drive[0];
        driveLetter[1] = L':';
        driveLetter[2] = L'\0';
        if (GetDriveTypeW(driveLetter) == DRIVE_NO_ROOT_DIR) {
            return vb6_BSTR_Empty();
        }
        // 切换到指定驱动器获取当前目录
        wchar_t oldDir[MAX_PATH];
        GetCurrentDirectoryW(MAX_PATH, oldDir);
        SetCurrentDirectoryW(driveLetter);
        GetCurrentDirectoryW(MAX_PATH, buf);
        SetCurrentDirectoryW(oldDir);
    } else {
        GetCurrentDirectoryW(MAX_PATH, buf);
    }
    return vb6_BSTR_FromStr(buf);
}

int32_t vb6_Shell(BSTR pathname, int32_t windowstyle) {
    if (!pathname) return 0;
    // 使用WinExec简化实现 (返回值>31表示成功)
    // VB6 Shell返回进程ID，WinExec返回实例句柄
    UINT ret = WinExec(NULL, 0);  // avoid unused warning
    (void)ret;
    STARTUPINFOW si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = (WORD)((windowstyle > 0) ? windowstyle : SW_SHOWNORMAL);
    if (CreateProcessW(NULL, pathname, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        DWORD pid = pi.dwProcessId;
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return (int32_t)pid;
    }
    return 0;
}

BSTR vb6_Environ(BSTR envstring) {
    if (!envstring) return vb6_BSTR_Empty();
    wchar_t buf[32768];  // Windows最大环境变量长度
    DWORD len = GetEnvironmentVariableW(envstring, buf, 32768);
    if (len == 0) return vb6_BSTR_Empty();
    return vb6_BSTR_FromStr(buf);
}

BSTR vb6_Command(void) {
    LPWSTR cmdLine = GetCommandLineW();
    if (!cmdLine) return vb6_BSTR_Empty();
    // 跳过可执行文件名 (可能在引号内)
    while (*cmdLine == L' ') cmdLine++;
    if (*cmdLine == L'"') {
        cmdLine++;
        while (*cmdLine && *cmdLine != L'"') cmdLine++;
        if (*cmdLine == L'"') cmdLine++;
    } else {
        while (*cmdLine && *cmdLine != L' ') cmdLine++;
    }
    while (*cmdLine == L' ') cmdLine++;
    if (*cmdLine == L'\0') return vb6_BSTR_Empty();
    return vb6_BSTR_FromStr(cmdLine);
}

// ============================================================
// P14.2.3: Split/Join 字符串数组函数
// ============================================================

vb6_SafeArray1D* vb6_Split(BSTR expr, BSTR delimiter, int32_t limit, int32_t compare) {
    (void)compare;  // simplified: binary compare only
    if (!expr) expr = vb6_BSTR_Empty();
    // Default delimiter is space " " when NULL is passed
    BSTR defaultDelim = NULL;
    if (!delimiter) {
        defaultDelim = vb6_BSTR_FromStr(L" ");
        delimiter = defaultDelim;
    }
    if (limit == 0) limit = -1;
    
    int32_t exprLen = vb6_BSTR_Len(expr);
    int32_t delimLen = vb6_BSTR_Len(delimiter);
    
    // Empty string -> single empty element
    if (exprLen == 0) {
        vb6_SafeArray1D* arr = vb6_SafeArrayCreate1D(vb6_sa_bstr, 0, 0);
        if (arr) VB6_SA_AT(BSTR, arr, 0) = vb6_BSTR_Empty();
        return arr;
    }
    
    // Count substrings
    int32_t count = 1;
    if (delimLen > 0) {
        for (int32_t i = 0; i <= exprLen - delimLen; ) {
            if (memcmp(expr + i, delimiter, delimLen * sizeof(wchar_t)) == 0) {
                count++;
                i += delimLen;
                if (limit > 0 && count >= limit) break;
            } else {
                i++;
            }
        }
    } else {
        // Empty delimiter: split each character
        count = exprLen;
    }
    if (limit > 0 && count > limit) count = limit;
    
    // Create array
    vb6_SafeArray1D* arr = vb6_SafeArrayCreate1D(vb6_sa_bstr, 0, count - 1);
    if (!arr) return NULL;
    
    // Populate elements
    int32_t idx = 0, start = 0;
    if (delimLen > 0) {
        for (int32_t i = 0; i <= exprLen - delimLen && idx < count - 1; ) {
            if (memcmp(expr + i, delimiter, delimLen * sizeof(wchar_t)) == 0) {
                int32_t len = i - start;
                VB6_SA_AT(BSTR, arr, idx) = SysAllocStringLen(expr + start, len);
                idx++;
                start = i + delimLen;
                i += delimLen;
            } else {
                i++;
            }
        }
        // Last element
        VB6_SA_AT(BSTR, arr, idx) = SysAllocStringLen(expr + start, exprLen - start);
    } else {
        // Empty delimiter: each character as element
        for (int32_t i = 0; i < count; i++) {
            VB6_SA_AT(BSTR, arr, i) = SysAllocStringLen(expr + i, 1);
        }
    }
    
    if (defaultDelim) vb6_BSTR_Free(defaultDelim);
    return arr;
}

BSTR vb6_Join(vb6_SafeArray1D* arr, BSTR delimiter) {
    BSTR defaultDelim = NULL;
    if (!delimiter) { defaultDelim = vb6_BSTR_FromStr(L" "); delimiter = defaultDelim; }
    if (!arr || arr->count <= 0) { if (defaultDelim) vb6_BSTR_Free(defaultDelim); return vb6_BSTR_Empty(); }
    
    int32_t delimLen = vb6_BSTR_Len(delimiter);
    
    // Calculate total length
    int32_t totalLen = 0;
    for (int32_t i = 0; i < arr->count; i++) {
        BSTR elem = VB6_SA_AT(BSTR, arr, i + arr->lBound);
        totalLen += elem ? vb6_BSTR_Len(elem) : 0;
        if (i < arr->count - 1) totalLen += delimLen;
    }
    
    // Build result
    wchar_t* buf = (wchar_t*)malloc((totalLen + 1) * sizeof(wchar_t));
    if (!buf) { if (defaultDelim) vb6_BSTR_Free(defaultDelim); return vb6_BSTR_Empty(); }
    int32_t pos = 0;
    for (int32_t i = 0; i < arr->count; i++) {
        BSTR elem = VB6_SA_AT(BSTR, arr, i + arr->lBound);
        if (elem) {
            int32_t elemLen = vb6_BSTR_Len(elem);
            memcpy(buf + pos, elem, elemLen * sizeof(wchar_t));
            pos += elemLen;
        }
        if (i < arr->count - 1 && delimLen > 0) {
            memcpy(buf + pos, delimiter, delimLen * sizeof(wchar_t));
            pos += delimLen;
        }
    }
    buf[totalLen] = L'\0';
    BSTR result = vb6_BSTR_FromStr(buf);
    free(buf);
    if (defaultDelim) vb6_BSTR_Free(defaultDelim);
    return result;
}

// ============================================================

double vb6_Sin(double x) { return sin(x); }
double vb6_Cos(double x) { return cos(x); }
double vb6_Tan(double x) { return tan(x); }
double vb6_Atn(double x) { return atan(x); }
double vb6_Log(double x) { return log(x); }
double vb6_Exp(double x) { return exp(x); }
double vb6_Fix(double x) { return (x >= 0) ? floor(x) : ceil(x); }
double vb6_Int(double x) { return floor(x); }

void vb6_Randomize(double seed) {
    if (seed == 0.0) {
        srand((unsigned int)time(NULL));
    } else {
        srand((unsigned int)seed);
    }
}

float vb6_Rnd_Full(int32_t seed) {
    if (seed < 0) {
        srand((unsigned int)seed);
    }
    // seed > 0 或省略: 返回下一个随机数
    // seed == 0: 返回上一个随机数 (简化: 仍返回新值)
    return (float)rand() / (float)RAND_MAX;
}

// ============================================================
// 日期时间函数
// ============================================================

static int32_t vb6_date_to_serial(int32_t year, int32_t month, int32_t day) {
    // Excel/Lotus日期序列号: 1900-01-01 = 1 (含Lotus bug: 1900-02-29 = 60)
    if (month <= 2) { year--; month += 12; }
    int32_t a = year / 100;
    int32_t b = 2 - a + a / 4;
    return (int32_t)(365.25 * (year + 4716)) + (int32_t)(30.6001 * (month + 1)) + day + b - 1524;
    // 调整到VB6的基准(1899-12-30 = 0)
}

// Excel序列号 → 年月日 (Julian Date Number逆运算)
// 基于 vb6_date_to_serial 的逆运算, 含Lotus 1900-02-29 bug兼容
static void vb6_serial_to_date(int32_t serial, int32_t* year, int32_t* month, int32_t* day) {
    // VB6/OLE Automation日期序列号 → 年月日
    // OLE日期: 1899-12-30=0, 1900-01-01=2, 1900-02-28=60, 1900-03-01=61
    // 注意: OLE日期系统不含Lotus 1900-02-29 bug(那是Excel的)
    // serial = JD - 2415019, 因此 JD = serial + 2415019
    int32_t jd = serial + 2415019;
    int32_t a = jd + 32044;
    int32_t b = (4 * a + 3) / 146097;
    int32_t c = a - (146097 * b) / 4;
    int32_t d = (4 * c + 3) / 1461;
    int32_t e = c - (1461 * d) / 4;
    int32_t m = (5 * e + 2) / 153;
    *day = e - (153 * m + 2) / 5 + 1;
    *month = m + 3 - 12 * (m / 10);
    *year = 100 * b + d - 4800 + m / 10;
}

static double vb6_now_serial(void) {
    time_t t = time(NULL);
    struct tm* lt = localtime(&t);
    // OLE日期: serial = JD(date) - JD(1899-12-30), 无Lotus bug
    int32_t jd_date = vb6_date_to_serial(1900 + lt->tm_year, 1 + lt->tm_mon, lt->tm_mday);
    int32_t jd_base = vb6_date_to_serial(1899, 12, 30);
    int32_t datePart = jd_date - jd_base;
    double timePart = (lt->tm_hour * 3600.0 + lt->tm_min * 60.0 + lt->tm_sec) / 86400.0;
    return (double)datePart + timePart;
}

double vb6_Now(void) { return vb6_now_serial(); }
double vb6_Date(void) { return (double)(int32_t)vb6_now_serial(); }
double vb6_Time(void) { double n = vb6_now_serial(); return n - (double)(int32_t)n; }

int32_t vb6_Year(double date) {
    int32_t y, m, d;
    vb6_serial_to_date((int32_t)date, &y, &m, &d);
    return y;
}

int32_t vb6_Month(double date) {
    int32_t y, m, d;
    vb6_serial_to_date((int32_t)date, &y, &m, &d);
    return m;
}

int32_t vb6_Day(double date) {
    int32_t y, m, d;
    vb6_serial_to_date((int32_t)date, &y, &m, &d);
    return d;
}

int32_t vb6_Hour(double time) {
    double t = time - (double)(int32_t)time;
    if (t < 0) t += 1.0;
    return (int32_t)(t * 24.0) % 24;
}

int32_t vb6_Minute(double time) {
    double t = time - (double)(int32_t)time;
    if (t < 0) t += 1.0;
    return (int32_t)(t * 1440.0) % 60;
}

int32_t vb6_Second(double time) {
    double t = time - (double)(int32_t)time;
    if (t < 0) t += 1.0;
    return (int32_t)(t * 86400.0) % 60;
}

// ============================================================
// 类型转换 (补充)
// ============================================================

int16_t vb6_CBool(double v) {
    return (v != 0.0) ? -1 : 0;  // VB6 True = -1
}

uint8_t vb6_CByte(double v) {
    return (uint8_t)(int32_t)v;
}

float vb6_CSng(double v) {
    return (float)v;
}

double vb6_CDate(vb6_VARIANT v) {
    // 简化: 仅支持从字符串解析日期, 或从数值转换
    if (v.vt == vb6_vtDouble || v.vt == vb6_vtSingle || v.vt == vb6_vtLong || v.vt == vb6_vtInteger)
        return vb6_VariantToDouble(v);
    return 0.0;
}

BSTR vb6_Hex(int32_t n) {
    wchar_t buf[16];
    swprintf(buf, 16, L"%X", n);
    return vb6_BSTR_FromStr(buf);
}

BSTR vb6_Oct(int32_t n) {
    wchar_t buf[16];
    swprintf(buf, 16, L"%o", n);
    return vb6_BSTR_FromStr(buf);
}

// ============================================================
// SAFEARRAY - VB6 数组实现
// ============================================================

// 安全数组元素大小表
static int32_t vb6_sa_elem_size(vb6_safearray_elemtype t) {
    switch (t) {
        case vb6_sa_bool:    return (int32_t)sizeof(int16_t);
        case vb6_sa_byte:    return (int32_t)sizeof(uint8_t);
        case vb6_sa_int:     return (int32_t)sizeof(int16_t);
        case vb6_sa_long:    return (int32_t)sizeof(int32_t);
        case vb6_sa_single:  return (int32_t)sizeof(float);
        case vb6_sa_double:  return (int32_t)sizeof(double);
        case vb6_sa_bstr:    return (int32_t)sizeof(BSTR);
        case vb6_sa_variant: return (int32_t)sizeof(vb6_VARIANT);
        case vb6_sa_ptr:     return (int32_t)sizeof(void*);
        default:             return 4;
    }
}

vb6_SafeArray1D* vb6_SafeArrayCreate1D(vb6_safearray_elemtype elemType,
    int32_t lBound, int32_t uBound) {
    vb6_SafeArray1D* arr = (vb6_SafeArray1D*)calloc(1, sizeof(vb6_SafeArray1D));
    if (!arr) return NULL;
    arr->elemType = elemType;
    arr->elemSize = vb6_sa_elem_size(elemType);
    arr->lBound = lBound;
    arr->uBound = uBound;
    arr->count = uBound - lBound + 1;
    arr->isDynamic = 0;
    if (arr->count > 0) {
        arr->data = calloc((size_t)arr->count, (size_t)arr->elemSize);
    }
    return arr;
}

vb6_SafeArray1D* vb6_SafeArrayReDim1D(vb6_safearray_elemtype elemType,
    int32_t lBound, int32_t uBound) {
    vb6_SafeArray1D* arr = vb6_SafeArrayCreate1D(elemType, lBound, uBound);
    if (arr) arr->isDynamic = 1;
    return arr;
}

vb6_SafeArray1D* vb6_SafeArrayReDimPreserve1D(vb6_SafeArray1D* arr,
    int32_t newLBound, int32_t newUBound) {
    if (!arr) return vb6_SafeArrayReDim1D(vb6_sa_empty, newLBound, newUBound);

    int32_t newCount = newUBound - newLBound + 1;
    if (newCount <= 0) {
        vb6_SafeArrayDestroy1D(arr);
        return NULL;
    }

    // 分配新数据区 (零初始化)
    void* newData = calloc((size_t)newCount, (size_t)arr->elemSize);
    if (!newData) return arr;  // 分配失败, 返回原数组

    // 复制旧数据到新区域 (取交集)
    int32_t copyStart = (arr->lBound > newLBound) ? arr->lBound : newLBound;
    int32_t copyEnd   = (arr->uBound < newUBound) ? arr->uBound : newUBound;
    if (copyStart <= copyEnd) {
        int32_t srcOff = copyStart - arr->lBound;
        int32_t dstOff = copyStart - newLBound;
        int32_t copyLen = (copyEnd - copyStart + 1) * arr->elemSize;
        memcpy((char*)newData + dstOff * arr->elemSize,
               (char*)arr->data + srcOff * arr->elemSize,
               (size_t)copyLen);
    }

    // BSTR元素: 旧区域中被丢弃的元素需要释放
    if (arr->elemType == vb6_sa_bstr) {
        for (int32_t i = arr->lBound; i <= arr->uBound; i++) {
            // 在新范围之外的BSTR需要释放
            if (i < newLBound || i > newUBound) {
                BSTR* slot = &VB6_SA_AT(BSTR, arr, i);
                if (*slot) vb6_BSTR_Free(*slot);
            }
        }
    }

    free(arr->data);
    arr->data = newData;
    arr->lBound = newLBound;
    arr->uBound = newUBound;
    arr->count = newCount;
    return arr;
}

void vb6_SafeArrayDestroy1D(vb6_SafeArray1D* arr) {
    if (!arr) return;
    // BSTR元素: 逐个释放
    if (arr->elemType == vb6_sa_bstr && arr->data) {
        for (int32_t i = 0; i < arr->count; i++) {
            BSTR* slot = (BSTR*)((char*)arr->data + i * arr->elemSize);
            if (*slot) vb6_BSTR_Free(*slot);
        }
    }
    // vb6_VARIANT元素: 逐个清理BSTR
    if (arr->elemType == vb6_sa_variant && arr->data) {
        for (int32_t i = 0; i < arr->count; i++) {
            vb6_VARIANT* slot = (vb6_VARIANT*)((char*)arr->data + i * arr->elemSize);
            if (slot->vt == vb6_vtBSTR && slot->bstrVal) {
                vb6_BSTR_Free(slot->bstrVal);
            }
        }
    }
    if (arr->data) free(arr->data);
    free(arr);
}

void* vb6_SafeArrayGetPtr(vb6_SafeArray1D* arr, int32_t index) {
    if (!arr || index < arr->lBound || index > arr->uBound) return NULL;
    return (char*)arr->data + (index - arr->lBound) * arr->elemSize;
}

void vb6_SafeArrayPutElem(vb6_SafeArray1D* arr, int32_t index, void* value) {
    if (!arr || index < arr->lBound || index > arr->uBound) return;
    void* dest = (char*)arr->data + (index - arr->lBound) * arr->elemSize;
    // BSTR: 先释放旧值, 再赋新值
    if (arr->elemType == vb6_sa_bstr) {
        BSTR* slot = (BSTR*)dest;
        if (*slot) vb6_BSTR_Free(*slot);
        BSTR newVal = *(BSTR*)value;
        *slot = newVal;
    } else {
        memcpy(dest, value, (size_t)arr->elemSize);
    }
}

int32_t vb6_UBound(vb6_SafeArray1D* safeArray, int32_t dimension) {
    (void)dimension;  // 一维数组忽略维度参数
    if (!safeArray) return 0;
    return safeArray->uBound;
}

int32_t vb6_LBound(vb6_SafeArray1D* safeArray, int32_t dimension) {
    (void)dimension;
    if (!safeArray) return 0;
    return safeArray->lBound;
}

// ============================================================
// SAFEARRAY ND - VB6 多维数组实现
// ============================================================

vb6_SafeArrayND* vb6_SafeArrayCreateND(vb6_safearray_elemtype elemType,
    int32_t dimCount, vb6_SafeArrayBound bounds[]) {
    if (dimCount <= 0 || dimCount > 16) return NULL;

    vb6_SafeArrayND* arr = (vb6_SafeArrayND*)calloc(1, sizeof(vb6_SafeArrayND));
    if (!arr) return NULL;

    arr->dimCount = dimCount;
    arr->elemType = elemType;
    arr->elemSize = vb6_sa_elem_size(elemType);

    int32_t total = 1;
    for (int32_t d = 0; d < dimCount; d++) {
        arr->bounds[d] = bounds[d];
        if (bounds[d].cElements <= 0) {
            arr->totalElements = 0;
            arr->data = NULL;
            return arr;
        }
        total *= bounds[d].cElements;
    }
    arr->totalElements = total;

    if (total > 0) {
        arr->data = calloc((size_t)total, (size_t)arr->elemSize);
        if (!arr->data) {
            free(arr);
            return NULL;
        }
    }

    return arr;
}

void vb6_SafeArrayDestroyND(vb6_SafeArrayND* arr) {
    if (!arr) return;

    if (arr->elemType == vb6_sa_bstr && arr->data) {
        for (int32_t i = 0; i < arr->totalElements; i++) {
            BSTR* slot = (BSTR*)((char*)arr->data + i * arr->elemSize);
            if (*slot) vb6_BSTR_Free(*slot);
        }
    }
    if (arr->elemType == vb6_sa_variant && arr->data) {
        for (int32_t i = 0; i < arr->totalElements; i++) {
            vb6_VARIANT* slot = (vb6_VARIANT*)((char*)arr->data + i * arr->elemSize);
            if (slot->vt == vb6_vtBSTR && slot->bstrVal) {
                vb6_BSTR_Free(slot->bstrVal);
            }
        }
    }

    if (arr->data) free(arr->data);
    free(arr);
}

int32_t vb6_SafeArrayND_Offset(vb6_SafeArrayND* arr, int32_t dimCount, int32_t indices[]) {
    int32_t offset = indices[0] - arr->bounds[0].lBound;
    int32_t stride = arr->bounds[0].cElements;
    for (int32_t d = 1; d < dimCount; d++) {
        offset += (indices[d] - arr->bounds[d].lBound) * stride;
        stride *= arr->bounds[d].cElements;
    }
    return offset;
}

void* vb6_SafeArrayND_GetPtr(vb6_SafeArrayND* arr, ...) {
    if (!arr) return NULL;
    int32_t indices[16];
    va_list ap;
    va_start(ap, arr);
    for (int32_t d = 0; d < arr->dimCount; d++) {
        indices[d] = va_arg(ap, int32_t);
    }
    va_end(ap);

    int32_t offset = vb6_SafeArrayND_Offset(arr, arr->dimCount, indices);
    if (offset < 0 || offset >= arr->totalElements) return NULL;
    return (char*)arr->data + offset * arr->elemSize;
}

vb6_SafeArrayND* vb6_SafeArrayReDimND(vb6_safearray_elemtype elemType,
    int32_t dimCount, vb6_SafeArrayBound bounds[]) {
    return vb6_SafeArrayCreateND(elemType, dimCount, bounds);
}

vb6_SafeArrayND* vb6_SafeArrayReDimPreserveND(vb6_SafeArrayND* arr,
    int32_t dimCount, vb6_SafeArrayBound newBounds[]) {
    if (!arr) return vb6_SafeArrayCreateND(vb6_sa_empty, dimCount, newBounds);

    int32_t newTotal = 1;
    for (int32_t d = 0; d < dimCount; d++) {
        if (newBounds[d].cElements <= 0) {
            vb6_SafeArrayDestroyND(arr);
            return NULL;
        }
        newTotal *= newBounds[d].cElements;
    }

    void* newData = calloc((size_t)newTotal, (size_t)arr->elemSize);
    if (!newData) return arr;

    if (arr->data && arr->totalElements > 0) {
        int32_t minDims = (dimCount < arr->dimCount) ? dimCount : arr->dimCount;

        int32_t copyCounts[16];
        int32_t oldCounts[16];
        int32_t newCounts[16];
        int32_t oldStrides[16];
        int32_t newStrides[16];

        for (int32_t d = 0; d < dimCount; d++)
            newCounts[d] = newBounds[d].cElements;
        for (int32_t d = 0; d < minDims; d++)
            oldCounts[d] = arr->bounds[d].cElements;
        for (int32_t d = minDims; d < 16; d++)
            oldCounts[d] = 0;

        for (int32_t d = 0; d < dimCount; d++)
            copyCounts[d] = (oldCounts[d] < newCounts[d]) ? oldCounts[d] : newCounts[d];

        oldStrides[minDims - 1] = 1;
        for (int32_t d = minDims - 2; d >= 0; d--)
            oldStrides[d] = oldStrides[d + 1] * arr->bounds[d + 1].cElements;

        newStrides[dimCount - 1] = 1;
        for (int32_t d = dimCount - 2; d >= 0; d--)
            newStrides[d] = newStrides[d + 1] * newBounds[d + 1].cElements;

        int32_t iterMax = 1;
        for (int32_t d = 0; d < minDims; d++)
            iterMax *= copyCounts[d];

        for (int32_t linear = 0; linear < iterMax; linear++) {
            int32_t tmp = linear;
            int32_t oldOff = 0, newOff = 0;
            int32_t bounds_check = 1;
            for (int32_t d = minDims - 1; d >= 0; d--) {
                int32_t idx = tmp % copyCounts[d];
                tmp /= copyCounts[d];
                if (idx >= oldCounts[d] || idx >= newCounts[d]) {
                    bounds_check = 0;
                    break;
                }
                oldOff += idx * oldStrides[d];
                newOff += idx * newStrides[d];
            }
            if (bounds_check) {
                memcpy((char*)newData + newOff * arr->elemSize,
                       (char*)arr->data + oldOff * arr->elemSize,
                       (size_t)arr->elemSize);
            }
        }
    }

    if (arr->data) free(arr->data);
    arr->data = newData;
    arr->dimCount = dimCount;
    arr->totalElements = newTotal;
    for (int32_t d = 0; d < dimCount; d++)
        arr->bounds[d] = newBounds[d];
    for (int32_t d = dimCount; d < 16; d++) {
        arr->bounds[d].lBound = 0;
        arr->bounds[d].cElements = 0;
    }

    return arr;
}

int32_t vb6_UBoundND(vb6_SafeArrayND* arr, int32_t dimension) {
    if (!arr || dimension < 1 || dimension > arr->dimCount) return 0;
    return arr->bounds[dimension - 1].lBound + arr->bounds[dimension - 1].cElements - 1;
}

int32_t vb6_LBoundND(vb6_SafeArrayND* arr, int32_t dimension) {
    if (!arr || dimension < 1 || dimension > arr->dimCount) return 0;
    return arr->bounds[dimension - 1].lBound;
}

// ============================================================
// 文件 I/O (MVP)
// ============================================================

// VB6文件I/O使用通道号(1-511), 我们用文件指针表实现
#define VB6_MAX_FILES 32
static FILE* vb6_file_table[VB6_MAX_FILES] = {0};
static int32_t vb6_file_mode[VB6_MAX_FILES] = {0};  // 1=Input, 2=Output, 4=Random, 8=Append, 16=Binary
static int32_t vb6_file_reclen[VB6_MAX_FILES] = {0}; // P8.2: 记录长度 (Random模式)

int32_t vb6_FreeFile(void) {
    for (int32_t i = 1; i < VB6_MAX_FILES; i++) {
        if (!vb6_file_table[i]) return i;
    }
    return -1;  // 无可用通道
}

int32_t vb6_Open(BSTR pathname, int32_t mode, int32_t access, int32_t filenumber, int32_t reclength) {
    (void)access;  // 简化: 忽略access参数
    if (filenumber < 1 || filenumber >= VB6_MAX_FILES) return 0;
    if (vb6_file_table[filenumber]) return 0;  // 已打开

    // pathname: BSTR → 窄字符串
    int32_t len = vb6_BSTR_Len(pathname);
    char* narrow = (char*)malloc(len + 1);
    for (int32_t i = 0; i < len; i++) narrow[i] = (char)pathname[i];
    narrow[len] = '\0';

    const char* modeStr = "";
    switch (mode) {
        case 1: modeStr = "r"; break;   // Input
        case 2: modeStr = "w"; break;   // Output
        case 4: modeStr = "r+b"; break; // Random
        case 8: modeStr = "a"; break;   // Append
        case 16: modeStr = "r+b"; break; // Binary (读写)
        default: free(narrow); return 0;
    }

    // Random/Binary模式需要文件存在才能r+b, 否则先创建
    FILE* f = NULL;
    if (mode == 4 || mode == 16) {
        // Random/Binary模式需要读写, 尝试打开已有文件, 不存在则创建
        f = fopen(narrow, "r+b");
        if (!f) f = fopen(narrow, "w+b");
    } else {
        f = fopen(narrow, modeStr);
    }
    free(narrow);

    if (!f) {
        vb6_RaiseError(53, vb6_BSTR_FromStr(L"File not found"));
        return 0;
    }
    vb6_file_table[filenumber] = f;
    vb6_file_mode[filenumber] = mode;
    vb6_file_reclen[filenumber] = (reclength > 0) ? reclength : 128;  // P8.2: 默认128
    return -1;  // True
}

int32_t vb6_Close(int32_t filenumber) {
    if (filenumber < 1 || filenumber >= VB6_MAX_FILES) return 0;
    if (vb6_file_table[filenumber]) {
        fclose(vb6_file_table[filenumber]);
        vb6_file_table[filenumber] = NULL;
        vb6_file_mode[filenumber] = 0;
        vb6_file_reclen[filenumber] = 0;
    }
    return -1;
}

int32_t vb6_EOF(int32_t filenumber) {
    if (filenumber < 1 || filenumber >= VB6_MAX_FILES || !vb6_file_table[filenumber]) return -1;
    FILE* f = vb6_file_table[filenumber];
    int c = fgetc(f);
    if (c == EOF) return -1;  // True
    ungetc(c, f);
    return 0;  // False
}

int32_t vb6_LOF(int32_t filenumber) {
    if (filenumber < 1 || filenumber >= VB6_MAX_FILES || !vb6_file_table[filenumber]) return 0;
    FILE* f = vb6_file_table[filenumber];
    long cur = ftell(f);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, cur, SEEK_SET);
    return (int32_t)size;
}

int32_t vb6_Loc(int32_t filenumber) {
    if (filenumber < 1 || filenumber >= VB6_MAX_FILES || !vb6_file_table[filenumber]) return 0;
    // 简化: 返回当前字节位置 / 128 (VB6 Random模式)
    return (int32_t)(ftell(vb6_file_table[filenumber]) / 128) + 1;
}

void vb6_Print(int32_t filenumber, BSTR s) {
    if (filenumber < 1 || filenumber >= VB6_MAX_FILES || !vb6_file_table[filenumber]) return;
    FILE* f = vb6_file_table[filenumber];
    if (s) {
        int32_t len = vb6_BSTR_Len(s);
        for (int32_t i = 0; i < len; i++) fputc((char)s[i], f);
    }
    fputc('\n', f);
    fflush(f);
}

void vb6_Write(int32_t filenumber, BSTR s) {
    if (filenumber < 1 || filenumber >= VB6_MAX_FILES || !vb6_file_table[filenumber]) return;
    FILE* f = vb6_file_table[filenumber];
    fputc('"', f);
    if (s) {
        int32_t len = vb6_BSTR_Len(s);
        for (int32_t i = 0; i < len; i++) fputc((char)s[i], f);
    }
    fputc('"', f);
    fputc(',', f);  // VB6 Write用逗号分隔
    fflush(f);
}

BSTR vb6_LineInput(int32_t filenumber) {
    if (filenumber < 1 || filenumber >= VB6_MAX_FILES || !vb6_file_table[filenumber])
        return vb6_BSTR_Empty();
    char buf[4096];
    if (!fgets(buf, sizeof(buf), vb6_file_table[filenumber]))
        return vb6_BSTR_Empty();
    // 去除换行
    int32_t len = (int32_t)strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) len--;
    // 转宽字符串
    wchar_t wbuf[4096];
    for (int32_t i = 0; i < len; i++) wbuf[i] = (wchar_t)(unsigned char)buf[i];
    wbuf[len] = L'\0';
    return vb6_BSTR_FromStr(wbuf);
}

int32_t vb6_Input(int32_t filenumber, BSTR* outVar) {
    // 简化: 读取一行
    BSTR result = vb6_LineInput(filenumber);
    if (outVar) *outVar = result;
    return (result != NULL) ? -1 : 0;
}

int32_t vb6_Kill(BSTR pathname) {
    int32_t len = vb6_BSTR_Len(pathname);
    char narrow[512];
    for (int32_t i = 0; i < len && i < 511; i++) narrow[i] = (char)pathname[i];
    narrow[len < 512 ? len : 511] = '\0';
    return (remove(narrow) == 0) ? -1 : 0;
}

// Helper: BSTR → narrow string
static void vb6_bstr_to_narrow(BSTR bstr, char* buf, int32_t bufSize) {
    int32_t len = vb6_BSTR_Len(bstr);
    if (len >= bufSize) len = bufSize - 1;
    for (int32_t i = 0; i < len; i++) buf[i] = (char)bstr[i];
    buf[len] = '\0';
}

int32_t vb6_MkDir(BSTR pathname) {
    char narrow[512];
    vb6_bstr_to_narrow(pathname, narrow, sizeof(narrow));
#ifdef _WIN32
    return (_mkdir(narrow) == 0) ? -1 : 0;
#else
    return (mkdir(narrow, 0755) == 0) ? -1 : 0;
#endif
}

int32_t vb6_RmDir(BSTR pathname) {
    char narrow[512];
    vb6_bstr_to_narrow(pathname, narrow, sizeof(narrow));
#ifdef _WIN32
    return (_rmdir(narrow) == 0) ? -1 : 0;
#else
    return (rmdir(narrow) == 0) ? -1 : 0;
#endif
}

int32_t vb6_ChDir(BSTR pathname) {
    char narrow[512];
    vb6_bstr_to_narrow(pathname, narrow, sizeof(narrow));
#ifdef _WIN32
    return (_chdir(narrow) == 0) ? -1 : 0;
#else
    return (chdir(narrow) == 0) ? -1 : 0;
#endif
}

int32_t vb6_ChDrive(BSTR drive) {
    (void)drive;  // 简化: 不实现驱动器切换
    return -1;
}

int32_t vb6_Name(BSTR oldPath, BSTR newPath) {
    char oldNarrow[512], newNarrow[512];
    vb6_bstr_to_narrow(oldPath, oldNarrow, sizeof(oldNarrow));
    vb6_bstr_to_narrow(newPath, newNarrow, sizeof(newNarrow));
    return (rename(oldNarrow, newNarrow) == 0) ? -1 : 0;
}

int32_t vb6_FileCopy(BSTR source, BSTR destination) {
    char srcNarrow[512], dstNarrow[512];
    vb6_bstr_to_narrow(source, srcNarrow, sizeof(srcNarrow));
    vb6_bstr_to_narrow(destination, dstNarrow, sizeof(dstNarrow));
    FILE* sf = fopen(srcNarrow, "rb");
    if (!sf) return 0;
    FILE* df = fopen(dstNarrow, "wb");
    if (!df) { fclose(sf); return 0; }
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), sf)) > 0) {
        fwrite(buf, 1, n, df);
    }
    fclose(sf);
    fclose(df);
    return -1;
}

// ============================================================
// 错误处理 (MVP: 全局标志 + setjmp/longjmp)
// ============================================================

// 全局Err对象
typedef struct vb6_ErrObject {
    int32_t number;
    BSTR description;
    BSTR source;
} vb6_ErrObject;

static vb6_ErrObject vb6_err = {0, NULL, NULL};

// 全局错误处理状态 (由cgen生成的代码直接使用)
int32_t vb6_err_resume_next = 0;
int32_t vb6_err_jmp_active = 0;
void* vb6_err_handler_label = NULL;

// 错误跳转缓冲区 (支持On Error GoTo label)
#include <setjmp.h>
jmp_buf* vb6_error_jmp_ptr = NULL;
int32_t vb6_error_jmp_set = 0;

// P14.1.2: Resume恢复点跟踪
int32_t vb6_err_resume_point = 0;
int32_t vb6_err_resume_next_point = 0;
int32_t vb6_err_dispatch = 0;
int32_t vb6_err_in_handler = 0;

// P12.3: On Error嵌套栈 — 保存/恢复错误处理状态
typedef struct vb6_ErrFrame {
    jmp_buf* jmp_ptr;
    int32_t jmp_set;
    int32_t jmp_active;
    int32_t resume_next;
    int32_t resume_point;          // P14.1.2
    int32_t resume_next_point;     // P14.1.2
    int32_t in_handler;            // P14.1.2
} vb6_ErrFrame;

static vb6_ErrFrame vb6_err_stack[VB6_ERR_STACK_SIZE];
static int32_t vb6_err_stack_top = 0;

void vb6_SaveErrState(void) {
    if (vb6_err_stack_top < VB6_ERR_STACK_SIZE) {
        vb6_err_stack[vb6_err_stack_top].jmp_ptr = vb6_error_jmp_ptr;
        vb6_err_stack[vb6_err_stack_top].jmp_set = vb6_error_jmp_set;
        vb6_err_stack[vb6_err_stack_top].jmp_active = vb6_err_jmp_active;
        vb6_err_stack[vb6_err_stack_top].resume_next = vb6_err_resume_next;
        vb6_err_stack[vb6_err_stack_top].resume_point = vb6_err_resume_point;
        vb6_err_stack[vb6_err_stack_top].resume_next_point = vb6_err_resume_next_point;
        vb6_err_stack[vb6_err_stack_top].in_handler = vb6_err_in_handler;
        vb6_err_stack_top++;
    }
}

void vb6_RestoreErrState(void) {
    if (vb6_err_stack_top > 0) {
        vb6_err_stack_top--;
        vb6_error_jmp_ptr = vb6_err_stack[vb6_err_stack_top].jmp_ptr;
        vb6_error_jmp_set = vb6_err_stack[vb6_err_stack_top].jmp_set;
        vb6_err_jmp_active = vb6_err_stack[vb6_err_stack_top].jmp_active;
        vb6_err_resume_next = vb6_err_stack[vb6_err_stack_top].resume_next;
        vb6_err_resume_point = vb6_err_stack[vb6_err_stack_top].resume_point;
        vb6_err_resume_next_point = vb6_err_stack[vb6_err_stack_top].resume_next_point;
        vb6_err_in_handler = vb6_err_stack[vb6_err_stack_top].in_handler;
    }
}

int32_t vb6_ErrNumber(void) { return vb6_err.number; }
BSTR vb6_ErrDescription(void) { return vb6_err.description; }
void vb6_ErrClear(void) { vb6_err.number = 0; vb6_err.description = NULL; vb6_err.source = NULL; }

void vb6_RaiseError(int32_t errNum, BSTR description) {
    vb6_err.number = errNum;
    vb6_err.description = description;
    if (vb6_err_resume_next) {
        // On Error Resume Next: 忽略错误, 继续执行
        return;
    }
    if (vb6_err_jmp_active && vb6_error_jmp_set && vb6_error_jmp_ptr) {
        // On Error GoTo label: longjmp 跳到 setjmp 点
        vb6_err_in_handler = 1;  // P14.1.2: 标记进入错误处理器
        longjmp(*vb6_error_jmp_ptr, errNum);
    }
    // 未设置错误处理: 终止程序
    fwprintf(stderr, L"Unhandled VB6 Error #%d: %ls\n", errNum,
             description ? description : L"(no description)");
    exit(errNum);
}

// ============================================================
// P8.2: 随机/二进制文件访问 (Get/Put)
// ============================================================

int32_t vb6_Get(int32_t filenumber, int32_t recnumber, void* varPtr, int32_t varSize) {
    if (filenumber < 1 || filenumber >= VB6_MAX_FILES || !vb6_file_table[filenumber]) return 0;
    FILE* f = vb6_file_table[filenumber];
    int32_t mode = vb6_file_mode[filenumber];

    if (mode == 4) {
        // Random模式: recnumber是1-based记录号, 按reclength定位
        int32_t reclen = vb6_file_reclen[filenumber];
        if (reclen <= 0) reclen = 128;
        long pos = (long)(recnumber - 1) * reclen;
        fseek(f, pos, SEEK_SET);
        // 读取min(varSize, reclen)字节
        int32_t readLen = (varSize < reclen) ? varSize : reclen;
        size_t n = fread(varPtr, 1, readLen, f);
        // 不足部分填零
        if ((int32_t)n < varSize) {
            memset((char*)varPtr + n, 0, varSize - n);
        }
    } else if (mode == 16) {
        // Binary模式: recnumber是1-based字节位置
        if (recnumber > 0) {
            fseek(f, (long)(recnumber - 1), SEEK_SET);
        }
        fread(varPtr, 1, varSize, f);
    } else {
        return 0;  // 不支持的模式
    }
    return -1;  // True
}

int32_t vb6_Put(int32_t filenumber, int32_t recnumber, void* varPtr, int32_t varSize) {
    if (filenumber < 1 || filenumber >= VB6_MAX_FILES || !vb6_file_table[filenumber]) return 0;
    FILE* f = vb6_file_table[filenumber];
    int32_t mode = vb6_file_mode[filenumber];

    if (mode == 4) {
        // Random模式: recnumber是1-based记录号, 按reclength定位
        int32_t reclen = vb6_file_reclen[filenumber];
        if (reclen <= 0) reclen = 128;
        long pos = (long)(recnumber - 1) * reclen;
        fseek(f, pos, SEEK_SET);
        // 写入min(varSize, reclen)字节, 不足部分填零
        int32_t writeLen = (varSize < reclen) ? varSize : reclen;
        fwrite(varPtr, 1, writeLen, f);
        if (writeLen < reclen) {
            // 记录剩余部分填零
            char zero = 0;
            for (int32_t i = writeLen; i < reclen; i++) fwrite(&zero, 1, 1, f);
        }
    } else if (mode == 16) {
        // Binary模式: recnumber是1-based字节位置
        if (recnumber > 0) {
            fseek(f, (long)(recnumber - 1), SEEK_SET);
        }
        fwrite(varPtr, 1, varSize, f);
    } else {
        return 0;  // 不支持的模式
    }
    fflush(f);
    return -1;  // True
}
