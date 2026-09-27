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

// Fix 161b-decl-out: Declare A 版 ByVal String 当**可写缓冲**时的 ANSI 转换。
// 与 vb6_BSTR_ToANSI 的区别: 后者按 NUL 结尾算长度 (-1), 而 VB6 的
// `buf = String$(256, 0)` 内容全是 NUL ⇒ 按 -1 只转出 1 字节, 远小于缓冲容量,
// 被调用方一写就越界 (t1 实测: 只回读到 NUL ⇒ 空串)。这里按 **BSTR 的字符数**
// (SysStringLen, 含内嵌 NUL) 全量转换, 并额外多留 1 字节给终止符, 缓冲容量足够。
static inline char* vb6_BSTR_ToANSI_cap(BSTR bstr) {
    if (!bstr) { char* r = (char*)malloc(1); if (r) r[0] = '\0'; return r; }
#ifdef _WIN32
    UINT clen = SysStringLen(bstr);
    if (clen == 0) { char* r = (char*)malloc(1); if (r) r[0] = '\0'; return r; }
    int len = WideCharToMultiByte(CP_ACP, 0, bstr, (int)clen, NULL, 0, NULL, NULL);
    if (len <= 0) { char* r = (char*)malloc(1); if (r) r[0] = '\0'; return r; }
    char* buf = (char*)malloc((size_t)len + 1);
    if (!buf) return NULL;
    WideCharToMultiByte(CP_ACP, 0, bstr, (int)clen, buf, len, NULL, NULL);
    buf[len] = '\0';
    return buf;
#else
    size_t clen = wcslen(bstr);
    if (clen == 0) { char* r = (char*)malloc(1); if (r) r[0] = '\0'; return r; }
    size_t len = wcstombs(NULL, bstr, clen);
    if (len == (size_t)-1) { char* r = (char*)malloc(1); if (r) r[0] = '\0'; return r; }
    char* buf = (char*)malloc(len + 1);
    if (!buf) return NULL;
    wcstombs(buf, bstr, clen);
    buf[len] = '\0';
    return buf;
#endif
}

// Fix 161b-decl-out: Declare A 版 ByVal String **出参回写**。
// VB6 里 `Declare Function GetUserName Lib ... (ByVal lpBuffer As String, ...)`
// 的 ByVal String 实参是"可写缓冲"——被调用方填字节后, VB6 按 NUL 截断并把内容
// 回写成新的 String。C3 侧实参是 vb6_BSTR_ToANSI() 的临时副本, 故调用后须把
// 副本按 NUL 截断转回 BSTR (cap 是缓冲容量上限, <=0 表示按 NUL 定长)。
static inline BSTR vb6_BSTR_FromANSIBuf(const char* buf, int32_t cap) {
    if (!buf) return vb6_BSTR_Empty();
#ifdef _WIN32
    // 只取 NUL 前的有效字节; cap 只是上限, 实际以 NUL 为准 (API 写成有终止)
    int n = 0;
    if (cap > 0) {
        while (n < cap && buf[n] != '\0') n++;
    } else {
        while (buf[n] != '\0') n++;
    }
    if (n <= 0) return vb6_BSTR_Empty();
    int wlen = MultiByteToWideChar(CP_ACP, 0, buf, n, NULL, 0);
    if (wlen <= 0) return vb6_BSTR_Empty();
    BSTR r = SysAllocStringLen(NULL, (UINT)wlen);
    if (!r) return vb6_BSTR_Empty();
    MultiByteToWideChar(CP_ACP, 0, buf, n, r, wlen);
    r[wlen] = L'\0';
    return r;
#else
    int n = 0;
    if (cap > 0) { while (n < cap && buf[n] != '\0') n++; } else { while (buf[n] != '\0') n++; }
    if (n <= 0) return vb6_BSTR_Empty();
    size_t wlen = mbstowcs(NULL, buf, (size_t)n);
    if (wlen == (size_t)-1) return vb6_BSTR_Empty();
    wchar_t* tmp = (wchar_t*)malloc((wlen + 1) * sizeof(wchar_t));
    if (!tmp) return vb6_BSTR_Empty();
    mbstowcs(tmp, buf, (size_t)n);
    tmp[wlen] = L'\0';
    BSTR r = vb6_BSTR_FromStr(tmp);
    free(tmp);
    return r;
#endif
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
