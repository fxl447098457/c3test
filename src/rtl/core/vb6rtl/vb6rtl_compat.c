// vb6rtl_compat.c - VB6 运行时库: 兼容填平家族：P18-D 字符串/指针/格式化函数 + Split/Join 数组-字符串互转
// 2026-09-17 从 src/rtl/core/vb6rtl/vb6rtl.c 按家族拆出（纯搬移，逐行未改）:
//   原第 2237~2511 行
//   原第 2568~2679 行

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
// P18-D: 兼容性填平 — 字符串/指针/格式化函数
// ============================================================

int32_t vb6_AscW(BSTR s) {
    if (!s || vb6_BSTR_Len(s) == 0) return 0;
    return (int32_t)s[0];  // Unicode code point (same as Asc for BMP)
}

BSTR vb6_ChrW(int32_t code) {
    wchar_t buf[2] = { (wchar_t)code, L'\0' };
    return vb6_BSTR_FromStr(buf);
}

int32_t vb6_AscB(BSTR s) {
    if (!s || vb6_BSTR_Len(s) == 0) return 0;
    return (int32_t)(s[0] & 0xFF);  // Low byte of first character
}

BSTR vb6_ChrB(int32_t code) {
    wchar_t buf[2] = { (wchar_t)(code & 0xFF), L'\0' };
    return vb6_BSTR_FromStr(buf);
}

double vb6_Timer(void) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    return (double)st.wHour * 3600.0 + (double)st.wMinute * 60.0 +
           (double)st.wSecond + (double)st.wMilliseconds / 1000.0;
}

BSTR vb6_StrConv(BSTR text, int32_t conversion, int32_t localeID) {
    if (!text) return vb6_BSTR_Empty();
    int32_t len = vb6_BSTR_Len(text);
    if (len == 0) return vb6_BSTR_Empty();

    /* vbUpperCase=1, vbLowerCase=2, vbProperCase=3 - simple wchar transforms */
    if (conversion == 1 || conversion == 2 || conversion == 3) {
        wchar_t* buf = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
        if (!buf) return vb6_BSTR_Empty();
        memcpy(buf, text, len * sizeof(wchar_t));
        buf[len] = L'\0';
        if (conversion == 1) {
            for (int i = 0; i < len; i++) buf[i] = towupper(buf[i]);
        } else if (conversion == 2) {
            for (int i = 0; i < len; i++) buf[i] = towlower(buf[i]);
        } else { /* vbProperCase */
            int capNext = 1;
            for (int i = 0; i < len; i++) {
                if (iswspace(buf[i])) { capNext = 1; }
                else if (capNext) { buf[i] = towupper(buf[i]); capNext = 0; }
                else { buf[i] = towlower(buf[i]); }
            }
        }
        BSTR result = vb6_BSTR_FromStr(buf);
        free(buf);
        return result;
    }

    /* vbWide=4, vbNarrow=8, vbKatakana=16, vbHiragana=32
       Use LCMapStringEx for CJK locale-aware conversions.
       These can be combined (e.g. vbWide+vbKatakana = 4+16 = 20).
       LCMapStringEx flag mapping:
         vbWide     -> LCMAP_FULLWIDTH     (0x00800000)
         vbNarrow   -> LCMAP_HALFWIDTH     (0x00400000)
         vbKatakana -> LCMAP_KATAKANA      (0x00200000)
         vbHiragana -> LCMAP_HIRAGANA      (0x00100000)
    */
    if (conversion & 0x3C) { /* bits 2-5: wide/narrow/katakana/hiragana */
        DWORD mapFlags = 0;
        if (conversion & 4)  mapFlags |= 0x00800000; /* LCMAP_FULLWIDTH */
        if (conversion & 8)  mapFlags |= 0x00400000; /* LCMAP_HALFWIDTH */
        if (conversion & 16) mapFlags |= 0x00200000; /* LCMAP_KATAKANA */
        if (conversion & 32) mapFlags |= 0x00100000; /* LCMAP_HIRAGANA */
        if (mapFlags == 0) goto strconv_unicode;

        /* Determine locale name from localeID */
        wchar_t localeName[85];
        if (localeID == 0) localeID = 0x0411; /* default to Japanese */
        if (!LCIDToLocaleName((DWORD)localeID, localeName, 84, 0)) {
            wcscpy(localeName, L"ja-JP"); /* fallback */
        }

        /* First call: get required buffer size */
        int outLen = LCMapStringEx(localeName, mapFlags, text, len, NULL, 0, NULL, NULL, 0);
        if (outLen <= 0) {
            /* LCMapStringEx failed - return original string unchanged */
            return SysAllocStringLen(text, len);
        }

        wchar_t* outBuf = (wchar_t*)malloc((outLen + 1) * sizeof(wchar_t));
        if (!outBuf) return vb6_BSTR_Empty();

        /* Second call: perform the mapping */
        outLen = LCMapStringEx(localeName, mapFlags, text, len, outBuf, outLen, NULL, NULL, 0);
        if (outLen <= 0) {
            free(outBuf);
            return SysAllocStringLen(text, len);
        }
        outBuf[outLen] = L'\0';
        BSTR result = vb6_BSTR_FromStr(outBuf);
        free(outBuf);
        return result;
    }

strconv_unicode:
    /* vbUnicode=64, vbFromUnicode=128: no-op (internal strings are already Unicode) */
    (void)localeID;
    return SysAllocStringLen(text, len);
}


struct vb6_SafeArray1D* vb6_Filter(struct vb6_SafeArray1D* source, BSTR match, int32_t include, int32_t compare) {
    (void)compare;
    if (!source) return NULL;
    int32_t count = source->count;
    /* First pass: count matching elements */
    int32_t matchCount = 0;
    for (int32_t i = 0; i < count; i++) {
        BSTR elem = VB6_SA_AT(BSTR, source, i);
        int32_t found = 0;
        if (elem && match) {
            int32_t elemLen = vb6_BSTR_Len(elem);
            int32_t matchLen = vb6_BSTR_Len(match);
            if (matchLen == 0) { found = 1; }
            else for (int32_t j = 0; j <= elemLen - matchLen; j++) {
                if (memcmp(elem + j, match, matchLen * sizeof(wchar_t)) == 0) { found = 1; break; }
            }
        }
        if (found == include) matchCount++;
    }
    /* Create result array */
    struct vb6_SafeArray1D* result = vb6_SafeArrayCreate1D(vb6_sa_bstr, 0, matchCount > 0 ? matchCount - 1 : 0);
    if (!result) return NULL;
    /* Second pass: copy matching elements */
    int32_t idx = 0;
    for (int32_t i = 0; i < count; i++) {
        BSTR elem = VB6_SA_AT(BSTR, source, i);
        int32_t found = 0;
        if (elem && match) {
            int32_t elemLen = vb6_BSTR_Len(elem);
            int32_t matchLen = vb6_BSTR_Len(match);
            if (matchLen == 0) { found = 1; }
            else for (int32_t j = 0; j <= elemLen - matchLen; j++) {
                if (memcmp(elem + j, match, matchLen * sizeof(wchar_t)) == 0) { found = 1; break; }
            }
        }
        if (found == include) {
            VB6_SA_AT(BSTR, result, idx) = elem ? SysAllocString(elem) : vb6_BSTR_Empty();
            idx++;
        }
    }
    return result;
}

void* vb6_StrPtr(BSTR s) {
    return (void*)s;  // BSTR points directly to string data
}

uintptr_t vb6_ObjPtr(void* obj) {
    return (uintptr_t)obj;
}

BSTR vb6_LSet(BSTR str, int32_t length) {
    if (length <= 0) return vb6_BSTR_Empty();
    wchar_t* buf = (wchar_t*)calloc(length + 1, sizeof(wchar_t));
    if (!buf) return vb6_BSTR_Empty();
    /* Fill with spaces */
    for (int i = 0; i < length; i++) buf[i] = L' ';
    /* Copy string (left-justified, truncated if too long) */
    if (str) {
        int32_t slen = vb6_BSTR_Len(str);
        if (slen > length) slen = length;
        memcpy(buf, str, slen * sizeof(wchar_t));
    }
    BSTR result = vb6_BSTR_FromStr(buf);
    free(buf);
    return result;
}

BSTR vb6_RSet(BSTR str, int32_t length) {
    if (length <= 0) return vb6_BSTR_Empty();
    wchar_t* buf = (wchar_t*)calloc(length + 1, sizeof(wchar_t));
    if (!buf) return vb6_BSTR_Empty();
    /* Fill with spaces */
    for (int i = 0; i < length; i++) buf[i] = L' ';
    /* Copy string (right-justified, truncated if too long) */
    if (str) {
        int32_t slen = vb6_BSTR_Len(str);
        if (slen > length) slen = length;
        /* Right-justify: copy to end of buffer */
        memcpy(buf + (length - slen), str + (vb6_BSTR_Len(str) - slen), slen * sizeof(wchar_t));
    }
    BSTR result = vb6_BSTR_FromStr(buf);
    free(buf);
    return result;
}


BSTR vb6_WeekdayName(int32_t weekday, int32_t abbreviate, int32_t firstDayOfWeek) {
    /* firstDayOfWeek: 1=Sunday(default), 2=Monday, ..., 7=Saturday
       weekday is relative to firstDayOfWeek */
    static const wchar_t* fullNames[] = { L"Sunday", L"Monday", L"Tuesday", L"Wednesday", L"Thursday", L"Friday", L"Saturday" };
    static const wchar_t* abbrNames[] = { L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat" };
    if (firstDayOfWeek < 1 || firstDayOfWeek > 7) firstDayOfWeek = 1;
    /* Convert weekday (relative to firstDayOfWeek) to absolute (1=Sunday) */
    int32_t idx = (weekday - 1 + (firstDayOfWeek - 1)) % 7;
    if (idx < 0) idx += 7;
    const wchar_t* name = abbreviate ? abbrNames[idx] : fullNames[idx];
    return vb6_BSTR_FromStr(name);
}

BSTR vb6_MonthName(int32_t month, int32_t abbreviate) {
    static const wchar_t* fullNames[] = { L"January", L"February", L"March", L"April", L"May", L"June",
        L"July", L"August", L"September", L"October", L"November", L"December" };
    static const wchar_t* abbrNames[] = { L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun",
        L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec" };
    if (month < 1 || month > 12) return vb6_BSTR_Empty();
    const wchar_t* name = abbreviate ? abbrNames[month - 1] : fullNames[month - 1];
    return vb6_BSTR_FromStr(name);
}

BSTR vb6_FormatCurrency(double value, int32_t numDigits, int32_t incLeading, int32_t useParens, int32_t groupDigits) {
    (void)incLeading; (void)useParens; (void)groupDigits;
    wchar_t buf[64];
    swprintf(buf, 64, L"%%.%df", numDigits >= 0 ? numDigits : 2);
    /* Use currency symbol prefix */
    wchar_t fmtBuf[80];
    swprintf(fmtBuf, 80, buf, value);
    wchar_t result[84] = L"\x00a5";  /* Yen/ Yuan sign as default currency */
    wcscat(result, fmtBuf);
    return vb6_BSTR_FromStr(result);
}

BSTR vb6_FormatNumber(double value, int32_t numDigits, int32_t incLeading, int32_t useParens, int32_t groupDigits) {
    (void)incLeading; (void)useParens; (void)groupDigits;
    wchar_t buf[64];
    if (numDigits < 0) numDigits = 2;
    /* Format with grouping if requested */
    if (groupDigits) {
        /* Simple grouping: insert commas every 3 digits */
        swprintf(buf, 64, L"%%.%df", numDigits);
        wchar_t numBuf[64];
        swprintf(numBuf, 64, buf, value);
        /* Find decimal point */
        wchar_t* dot = wcschr(numBuf, L'.');
        int intLen = dot ? (int)(dot - numBuf) : (int)wcslen(numBuf);
        /* Build with commas */
        wchar_t outBuf[80];
        int outIdx = 0;
        int signLen = (numBuf[0] == L'-') ? 1 : 0;
        for (int i = signLen; i < intLen; i++) {
            if (i > signLen && (intLen - i) % 3 == 0) outBuf[outIdx++] = L',';
            outBuf[outIdx++] = numBuf[i];
        }
        /* Copy decimal part */
        if (dot) { wcscpy(outBuf + outIdx, dot); } else { outBuf[outIdx] = 0; }
        if (signLen) { memmove(outBuf + 1, outBuf, (wcslen(outBuf) + 1) * sizeof(wchar_t)); outBuf[0] = L'-'; }
        return vb6_BSTR_FromStr(outBuf);
    }
    swprintf(buf, 64, L"%%.%df", numDigits);
    swprintf(buf, 64, buf, value);  /* reuse buf for result */
    return vb6_BSTR_FromStr(buf);
}

BSTR vb6_FormatPercent(double value, int32_t numDigits, int32_t incLeading, int32_t useParens, int32_t groupDigits) {
    (void)incLeading; (void)useParens; (void)groupDigits;
    wchar_t buf[64];
    if (numDigits < 0) numDigits = 2;
    swprintf(buf, 64, L"%%.%df%%%%", numDigits);
    swprintf(buf, 64, buf, value * 100.0);
    return vb6_BSTR_FromStr(buf);
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

