// vb6rtl.c - VB6运行时库最小实现
// 仅支持 hello.bas 等简单程序运行

#include "vb6rtl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <time.h>

// ============================================================
// BSTR 操作
// ============================================================

BSTR vb6_BSTR_Concat(BSTR a, BSTR b) {
    int32_t lenA = vb6_BSTR_Len(a);
    int32_t lenB = vb6_BSTR_Len(b);
    int32_t total = lenA + lenB;

    uint32_t* p = (uint32_t*)malloc(sizeof(uint32_t) + (total + 1) * sizeof(wchar_t));
    if (!p) return NULL;
    *p = (uint32_t)total;
    BSTR result = (BSTR)(p + 1);

    if (a) memcpy(result, a, lenA * sizeof(wchar_t));
    if (b) memcpy(result + lenA, b, lenB * sizeof(wchar_t));
    result[total] = L'\0';

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

BSTR vb6_Format(VARIANT expr, BSTR fmt) {
    // 简化实现
    (void)fmt;
    switch (expr.vt) {
        case vb6_vtLong:
            return vb6_Str(expr.lVal);
        case vb6_vtDouble:
        case vb6_vtDate: {
            wchar_t buf[64];
            swprintf(buf, 64, L"%g", expr.dblVal);
            return vb6_BSTR_FromStr(buf);
        }
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
double vb6_CDbl(VARIANT x) {
    switch (x.vt) {
        case vb6_vtInteger: return (double)x.iVal;
        case vb6_vtLong: return (double)x.lVal;
        case vb6_vtSingle: return (double)x.fltVal;
        case vb6_vtDouble: return x.dblVal;
        case vb6_vtBoolean: return x.boolVal ? -1.0 : 0.0;
        default: return 0.0;
    }
}

BSTR vb6_CStr(VARIANT x) {
    return vb6_Format(x, NULL);
}

// ============================================================
// 类型检查
// ============================================================

int32_t vb6_IsNumeric(VARIANT v) {
    switch (v.vt) {
        case vb6_vtInteger: case vb6_vtLong: case vb6_vtSingle:
        case vb6_vtDouble: case vb6_vtCurrency: case vb6_vtByte:
        case vb6_vtBoolean:
            return -1;  // VB6 True
        default:
            return 0;
    }
}

int32_t vb6_IsNull(VARIANT v) { return v.vt == vb6_vtNull ? -1 : 0; }
int32_t vb6_IsEmpty(VARIANT v) { return v.vt == vb6_vtEmpty ? -1 : 0; }
int32_t vb6_IsObject(VARIANT v) { return (v.vt == vb6_vtDispatch && v.pdispVal != NULL) ? -1 : 0; }

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

void* vb6_NewObject(const wchar_t* className) {
    (void)className;
    return NULL;
}

int32_t vb6_TypeOf(void* obj, const wchar_t* typeName) {
    (void)obj; (void)typeName;
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
}

void vb6_Exit(void) {
    // 清理运行时资源
}

void vb6_End(void) {
    // 终止程序
    exit(0);
}
