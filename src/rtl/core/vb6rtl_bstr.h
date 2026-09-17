#pragma once
// vb6rtl_bstr.h - BSTR（VB6 字符串类型）与字符串内存管理
// 由 vb6rtl.h 伞头 include；生成代码不要直接 include 本文件
#include "vb6rtl_base.h"

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

#ifdef __cplusplus
}
#endif
