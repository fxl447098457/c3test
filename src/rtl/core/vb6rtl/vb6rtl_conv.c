// vb6rtl_conv.c - VB6 运行时库: 转换家族：MsgBox/数值函数/类型转换/类型检查/随机数/Debug 对象
// 2026-09-17 从 src/rtl/core/vb6rtl/vb6rtl.c 按家族拆出（纯搬移，逐行未改）:
//   原第 894~902 行
//   原第 903~918 行
//   原第 919~953 行
//   原第 1272~1327 行
//   原第 1328~1439 行
//   原第 2680~2707 行
//   原第 3149~3182 行

#include "vb6rtl.h"
#include "vb6forms.h"   /* Fix 112: 宿主对象模型 (窗体/控件/集合/字体) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <wchar.h>
#include <wctype.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <oleauto.h>
#include <olectl.h>
#include <windows.h>
#endif

// P24-08: MessageBoxW (user32) + GetConsoleWindow (kernel32)
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")

// ============================================================
// COM互操作前向声明 (实现在vb6com.c中，避免vb6_VARIANT类型冲突)
// ============================================================
extern void* vb6_CreateObject(const wchar_t* progId);
extern void* vb6_NewBuiltinObject(const wchar_t* className);
extern void* vb6_GetObject(const wchar_t* pathName, const wchar_t* progId);
extern int32_t vb6_IsNothing(void* obj);
extern void vb6_ReleaseObject(void** objPtr);
extern void* vb6_ComCall(void* disp, const wchar_t* methodName, void* args, int32_t argc);
extern void* vb6_ComCallByDispid(void* disp, int32_t dispid, void* args, int32_t argc);
extern void* vb6_ComGetProp(void* disp, const wchar_t* propName);
extern void vb6_ComSetProp(void* disp, const wchar_t* propName, void* value);
extern void vb6_ComSetRef(void* disp, const wchar_t* propName, void* objRef);
extern void vb6_ComInit(void);
extern void vb6_ComExit(void);

// ============================================================
// MsgBox
// ============================================================

int32_t vb6_MsgBox(BSTR prompt, int32_t buttons, BSTR title) {
    /* Win32 MessageBox */
    return (int32_t)MessageBoxW(NULL, prompt ? prompt : L"", title ? title : L"", (UINT)buttons);
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

// M22: typed CStr overloads (C has no overloading, use suffix)
BSTR vb6_CStrLong(int32_t x) {
    vb6_VARIANT v; memset(&v, 0, sizeof(v)); v.vt = (vb6_vartype)VT_I4; v.lVal = x;
    return vb6_Format(v, NULL);
}
BSTR vb6_CStrDbl(double x) {
    vb6_VARIANT v; memset(&v, 0, sizeof(v)); v.vt = (vb6_vartype)VT_R8; v.dblVal = x;
    return vb6_Format(v, NULL);
}
BSTR vb6_CStrBool(int16_t x) {
    vb6_VARIANT v; memset(&v, 0, sizeof(v)); v.vt = (vb6_vartype)VT_BOOL; v.boolVal = x;
    return vb6_Format(v, NULL);
}
BSTR vb6_CStrByte(uint8_t x) {
    vb6_VARIANT v; memset(&v, 0, sizeof(v)); v.vt = (vb6_vartype)VT_UI1; v.bVal = x;
    return vb6_Format(v, NULL);
}
BSTR vb6_CStrDate(double x) {
    vb6_VARIANT v; memset(&v, 0, sizeof(v)); v.vt = (vb6_vartype)VT_DATE; v.dblVal = x;
    return vb6_Format(v, NULL);
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
int32_t vb6_IsArray(vb6_VARIANT v) { return (v.vt & 0x2000) ? -1 : 0; }  // VT_ARRAY=0x2000
int32_t vb6_IsDate(vb6_VARIANT v) { return v.vt == vb6_vtDate ? -1 : 0; }
int32_t vb6_IsError(vb6_VARIANT v) { return v.vt == vb6_vtError ? -1 : 0; }
// P21-09: CVErr — create VT_ERROR Variant
vb6_VARIANT vb6_CVErr(int32_t errorNumber) {
    vb6_VARIANT v;
    memset(&v, 0, sizeof(v));
    v.vt = vb6_vtError;
    v.lVal = errorNumber;
    return v;
}


// P8.4: VarType - 返回Variant的VT类型码
int32_t vb6_VarType(vb6_VARIANT v) { return (int32_t)v.vt; }

// P8.4: TypeName - 返回Variant类型的VB6类型名
BSTR vb6_TypeName(vb6_VARIANT v) {
    const wchar_t* name = L"Empty";
    /* Fix 112: 宿主对象 (窗体/控件 HWND, Controls 集合, Font) 返回 VB6 类型名 */
    if (v.vt == vb6_vtDispatch && v.pdispVal) {
        const wchar_t* hostName = vb6_Host_TypeNameOf(v.pdispVal);
        if (hostName) return vb6_BSTR_FromStr(hostName);
    }
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
    // Fix 112c: Collection 是 VB6 内建类, 不能走 CLSIDFromProgID (必失败 → 429 弹窗)
    if (className && _wcsicmp(className, L"Collection") == 0) {
        return vb6_Collection_New();
    }
    // Fix 112: StdFont → RTL 内建字体对象 (With m_TitleFont: .Size/.Bold 直接写结构体)
    if (className && _wcsicmp(className, L"StdFont") == 0) {
        return vb6_UC_NewFont();
    }
    // 对于未知类名，尝试通过COM创建 (Dim x As New ClassName，className不在已知类中)
    // VB6中如果className不是项目内的类模块，则尝试COM创建
    // Fix 103: VB6 内建对象 (Collection 等) 实现于运行时内部, 不注册 ProgID —— 直接查
    // 注册表必然失败并抛 429 (实测 New Collection: ProgID: Collection, 0x800401F3)。
    // 先探测 C3 运行时自带的内建实现, 未命中才回退到 COM 注册表。
    void* builtin = vb6_NewBuiltinObject(className);
    if (builtin) return builtin;
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

