// vb6rtl.c - VB6运行时库最小实现
// 仅支持 hello.bas 等简单程序运行
// 2026-09-17 按家族拆分为 13 个编译单元（纯搬移，逐行未改；清单见 ai/022-源码拆分进度表.md）。
// 本文件保留：头部 include + 运行时初始化/退出 + 内存/类支持 + Variant 转换与比较 + 错误处理。
// 拆出文件与原行区间：
//   vb6rtl_string.c        40~64, 65~283, 1606~1728, 1729~1789, 4667~4698
//   vb6rtl_format.c        284~893
//   vb6rtl_conv.c          894~902, 903~918, 919~953, 1272~1327, 1328~1439, 2680~2707, 3149~3182
//   vb6rtl_misc.c          954~1271, 2512~2567
//   vb6rtl_system.c        1790~1950, 1951~1993, 1994~2065, 2066~2133, 2134~2208, 2209~2236
//   vb6rtl_compat.c        2237~2511, 2568~2679
//   vb6rtl_date.c          2708~2827, 2828~3044, 3045~3148
//   vb6rtl_array.c         3183~3373, 3374~3617
//   vb6rtl_file.c          3618~3918, 4115~4181, 4595~4666
//   vb6rtl_paramarray.c    4182~4430
//   vb6rtl_financial.c     4431~4594
//   vb6rtl_com.c           4699~4793, 4955~5327
//   vb6rtl_registry.c      4794~4954


#include "vb6rtl.h"
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
// P18-C: Option Compare
int g_vb6_optionCompareText = 0;  // 0=Binary(default), 1=Text
int vb6_StrCmp(const wchar_t* a, const wchar_t* b) {
    if (g_vb6_optionCompareText) return _wcsicmp(a, b);
    return wcscmp(a, b);
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

// Variant → LongPtr (指针/句柄语义). 数值提取语义与 vb6_VariantToLong 一致,
// 但返回 intptr_t, 避免 x64 下把 64 位句柄/指针截断为 32 位.
intptr_t vb6_VariantToLongPtr(vb6_VARIANT v) {
    switch (v.vt) {
        case vb6_vtBoolean: return v.boolVal ? -1 : 0;
        case vb6_vtByte:    return (intptr_t)v.bVal;
        case vb6_vtInteger: return (intptr_t)v.iVal;
        case vb6_vtLong:    return (intptr_t)v.lVal;
        case vb6_vtSingle:  return (intptr_t)round(v.fltVal);
        case vb6_vtDouble:  return (intptr_t)round(v.dblVal);
        case vb6_vtCurrency:return (intptr_t)(v.cyVal / 10000);
        case vb6_vtBSTR:    return (intptr_t)vb6_Val(v.bstrVal);
        case VT_I8:         return (intptr_t)v.llVal;
        case VT_UI8: {      /* 位模式按无符号解释 */
            uint64_t uv;
            memcpy(&uv, &v.llVal, sizeof(uv));
            return (intptr_t)uv;
        }
        default:            return 0;
    }
}

// Fix 093a: Variant → Boolean (VB6 CBool 语义, True = -1). BSTR 先按
// "True"/"False" 文本判断, 其余按数值 != 0 (VB6 CBool 对数字非零即 True).
int16_t vb6_VariantToBool(vb6_VARIANT v) {
    switch (v.vt) {
        case vb6_vtBoolean:  return v.boolVal ? -1 : 0;
        case vb6_vtByte:     return v.bVal ? -1 : 0;
        case vb6_vtInteger:  return v.iVal ? -1 : 0;
        case vb6_vtLong:     return v.lVal ? -1 : 0;
        case vb6_vtSingle:   return v.fltVal != 0.0f ? -1 : 0;
        case vb6_vtDouble:   return v.dblVal != 0.0 ? -1 : 0;
        case vb6_vtCurrency: return v.cyVal ? -1 : 0;
        case vb6_vtDispatch: return v.pdispVal ? -1 : 0;
        case vb6_vtBSTR:
            if (!v.bstrVal) return 0;
            if (_wcsicmp(v.bstrVal, L"true") == 0) return -1;
            if (_wcsicmp(v.bstrVal, L"false") == 0) return 0;
            return vb6_Val(v.bstrVal) != 0.0 ? -1 : 0;
        default:             return 0;  // Empty / Null / Error / 数组
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

// Fix 029: 从 Variant 中提取 SafeArray1D* (当 Variant 持有数组时).
// 用于调用点反向强制: callee 期望 vb6_SafeArray1D* 但实参是 vb6_VARIANT.
struct vb6_SafeArray1D* vb6_VariantToSafeArray1D(vb6_VARIANT v) {
    if ((v.vt & vb6_vtArray) && v.parray) {
        return v.parray;
    }
    /* Variant 不持有数组时返回 NULL (与 VB6 行为一致; 调用方需 NULL 检查) */
    return NULL;
}

// Fix 029: vb6_VariantToObject 的右值兼容版本.
// vb6_VariantToObject 接受 vb6_VARIANT* (要求实参左值), 而调用点包装的实参
// 经常是函数返回值 (vb6_VariantArrayGet(...) 等) 无法取址. 这里提供按值版本.
void* vb6_VariantToObjectVal(vb6_VARIANT v) {
    if (v.vt == vb6_vtDispatch) return v.pdispVal;
    return NULL;
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

BSTR vb6_ErrSource(void) { return vb6_err.source; }

void vb6_ErrRaise(int32_t errNum, BSTR source, BSTR description) {
    vb6_err.number = errNum;
    vb6_err.source = source;
    vb6_err.description = description;
    if (vb6_err_resume_next) return;
    if (vb6_err_jmp_active && vb6_error_jmp_set && vb6_error_jmp_ptr) {
        vb6_err_in_handler = 1;
        longjmp(*vb6_error_jmp_ptr, errNum);
    }
    // 未处理错误: 显示消息并退出
    fwprintf(stderr, L"Unhandled error %d", (int)errNum);
    if (description) fwprintf(stderr, L": %s", description);
    fwprintf(stderr, L"\n");
    ExitProcess(errNum);
}

void vb6_ErrRaiseNumber(int32_t errNum) {
    vb6_ErrRaise(errNum, NULL, NULL);
}
// ============================================================
// P24-Bug2: Variant比较函数
// 简化VB6语义: 两端都是字符串→字符串比较, 否则→Double数值比较
// ============================================================

static double vb6_VarToDouble_internal(vb6_VARIANT* v) {
    if (!v) return 0.0;
    switch ((vb6_vartype)v->vt) {
        case (vb6_vartype)VT_I2: return (double)v->iVal;
        case (vb6_vartype)VT_I4: return (double)v->lVal;
        case (vb6_vartype)VT_R4: return (double)v->fltVal;
        case (vb6_vartype)VT_R8: return v->dblVal;
        case (vb6_vartype)VT_BOOL: return v->boolVal ? -1.0 : 0.0;
        case (vb6_vartype)VT_BSTR: {
            if (!v->bstrVal) return 0.0;
            return wcstod(v->bstrVal, NULL);
        }
        default: return 0.0;
    }
}

static int32_t vb6_VarIsString(vb6_VARIANT* v) {
    return v && v->vt == (vb6_vartype)VT_BSTR;
}

int32_t vb6_VarCmpEq(vb6_VARIANT* a, vb6_VARIANT* b) {
    if (vb6_VarIsString(a) && vb6_VarIsString(b)) {
        // Fix 092v: 字符串 Variant 比较统一返回 VB6 Boolean (-1/0), 与数值分支一致.
        int eq = (a->bstrVal && b->bstrVal) ? (wcscmp(a->bstrVal, b->bstrVal) == 0) : (a->bstrVal == b->bstrVal);
        return eq ? -1 : 0;
    }
    double da = vb6_VarToDouble_internal(a);
    double db = vb6_VarToDouble_internal(b);
    return (da == db) ? -1 : 0;
}
int32_t vb6_VarCmpNe(vb6_VARIANT* a, vb6_VARIANT* b) {
    if (vb6_VarIsString(a) && vb6_VarIsString(b)) {
        int ne = (a->bstrVal && b->bstrVal) ? (wcscmp(a->bstrVal, b->bstrVal) != 0) : (a->bstrVal != b->bstrVal);
        return ne ? -1 : 0;
    }
    double da = vb6_VarToDouble_internal(a);
    double db = vb6_VarToDouble_internal(b);
    return (da != db) ? -1 : 0;
}
int32_t vb6_VarCmpLt(vb6_VARIANT* a, vb6_VARIANT* b) {
    if (vb6_VarIsString(a) && vb6_VarIsString(b)) {
        int lt = (a->bstrVal && b->bstrVal) ? (wcscmp(a->bstrVal, b->bstrVal) < 0) : 0;
        return lt ? -1 : 0;
    }
    return (vb6_VarToDouble_internal(a) < vb6_VarToDouble_internal(b)) ? -1 : 0;
}
int32_t vb6_VarCmpGt(vb6_VARIANT* a, vb6_VARIANT* b) {
    if (vb6_VarIsString(a) && vb6_VarIsString(b)) {
        int gt = (a->bstrVal && b->bstrVal) ? (wcscmp(a->bstrVal, b->bstrVal) > 0) : 0;
        return gt ? -1 : 0;
    }
    return (vb6_VarToDouble_internal(a) > vb6_VarToDouble_internal(b)) ? -1 : 0;
}
int32_t vb6_VarCmpLe(vb6_VARIANT* a, vb6_VARIANT* b) {
    if (vb6_VarIsString(a) && vb6_VarIsString(b)) {
        int le = (a->bstrVal && b->bstrVal) ? (wcscmp(a->bstrVal, b->bstrVal) <= 0) : 0;
        return le ? -1 : 0;
    }
    return (vb6_VarToDouble_internal(a) <= vb6_VarToDouble_internal(b)) ? -1 : 0;
}
int32_t vb6_VarCmpGe(vb6_VARIANT* a, vb6_VARIANT* b) {
    if (vb6_VarIsString(a) && vb6_VarIsString(b)) {
        int ge = (a->bstrVal && b->bstrVal) ? (wcscmp(a->bstrVal, b->bstrVal) >= 0) : 0;
        return ge ? -1 : 0;
    }
    return (vb6_VarToDouble_internal(a) >= vb6_VarToDouble_internal(b)) ? -1 : 0;
}

// Variant vs Long (常见场景: If v > 0 Then)
int32_t vb6_VarCmpLongEq(vb6_VARIANT* a, int32_t b) { vb6_VARIANT vb; memset(&vb, 0, sizeof(vb)); vb.vt = (vb6_vartype)VT_I4; vb.lVal = b; return vb6_VarCmpEq(a, &vb); }
int32_t vb6_VarCmpLongNe(vb6_VARIANT* a, int32_t b) { vb6_VARIANT vb; memset(&vb, 0, sizeof(vb)); vb.vt = (vb6_vartype)VT_I4; vb.lVal = b; return vb6_VarCmpNe(a, &vb); }
int32_t vb6_VarCmpLongLt(vb6_VARIANT* a, int32_t b) { vb6_VARIANT vb; memset(&vb, 0, sizeof(vb)); vb.vt = (vb6_vartype)VT_I4; vb.lVal = b; return vb6_VarCmpLt(a, &vb); }
int32_t vb6_VarCmpLongGt(vb6_VARIANT* a, int32_t b) { vb6_VARIANT vb; memset(&vb, 0, sizeof(vb)); vb.vt = (vb6_vartype)VT_I4; vb.lVal = b; return vb6_VarCmpGt(a, &vb); }
int32_t vb6_VarCmpLongLe(vb6_VARIANT* a, int32_t b) { vb6_VARIANT vb; memset(&vb, 0, sizeof(vb)); vb.vt = (vb6_vartype)VT_I4; vb.lVal = b; return vb6_VarCmpLe(a, &vb); }
int32_t vb6_VarCmpLongGe(vb6_VARIANT* a, int32_t b) { vb6_VARIANT vb; memset(&vb, 0, sizeof(vb)); vb.vt = (vb6_vartype)VT_I4; vb.lVal = b; return vb6_VarCmpGe(a, &vb); }


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
    // 未设置错误处理: GUI程序弹MessageBox, CLI程序输出stderr
    if (GetConsoleWindow()) {
        // CLI程序: 输出到stderr
        fwprintf(stderr, L"Unhandled VB6 Error #%d: %ls\n", errNum,
                 description ? description : L"(no description)");
    } else {
        // GUI程序: 弹出VB6风格错误对话框
        wchar_t msg[512];
        swprintf(msg, 512, L"Run-time error '%d':\n%ls",
                 errNum, description ? description : L"(no description)");
        MessageBoxW(NULL, msg, L"VB6 Runtime Error", MB_ICONERROR | MB_OK);
    }
    exit(errNum);
}

