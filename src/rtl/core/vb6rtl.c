// vb6rtl.c - VB6运行时库最小实现
// 仅支持 hello.bas 等简单程序运行

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
        case vb6_vtDouble: {
            wchar_t buf[64];
            swprintf(buf, 64, L"%g", expr.dblVal);
            return vb6_BSTR_FromStr(buf);
        }
        case vb6_vtDate: {
            // M22-Issue4: Format VT_DATE as date/time string using system locale
            // When fmt is NULL, VB6 uses system short date + time format
            SYSTEMTIME st;
            if (VariantTimeToSystemTime(expr.dblVal, &st)) {
                wchar_t dateBuf[64], timeBuf[64];
                // Get system short date format
                GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, dateBuf, 64);
                // Get system time format (without seconds for clean display like VB6)
                GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &st, NULL, timeBuf, 64);
                wchar_t fullBuf[128];
                swprintf(fullBuf, 128, L"%s %s", dateBuf, timeBuf);
                return vb6_BSTR_FromStr(fullBuf);
            }
            // Fallback: print as number
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
    vb6_VARIANT v; memset(&v, 0, sizeof(v)); v.vt = VT_I4; v.lVal = x;
    return vb6_Format(v, NULL);
}
BSTR vb6_CStrDbl(double x) {
    vb6_VARIANT v; memset(&v, 0, sizeof(v)); v.vt = VT_R8; v.dblVal = x;
    return vb6_Format(v, NULL);
}
BSTR vb6_CStrBool(int16_t x) {
    vb6_VARIANT v; memset(&v, 0, sizeof(v)); v.vt = VT_BOOL; v.boolVal = x;
    return vb6_Format(v, NULL);
}
BSTR vb6_CStrByte(uint8_t x) {
    vb6_VARIANT v; memset(&v, 0, sizeof(v)); v.vt = VT_UI1; v.bVal = x;
    return vb6_Format(v, NULL);
}
BSTR vb6_CStrDate(double x) {
    vb6_VARIANT v; memset(&v, 0, sizeof(v)); v.vt = VT_DATE; v.dblVal = x;
    return vb6_Format(v, NULL);
}
// ============================================================
// P18: Missing RTL functions (CCur/RGB/QBColor/FileDateTime/FileLen/SendKeys/AppActivate)
// ============================================================

int64_t vb6_CCur(double v) {
    /* CCur: convert to Currency (int64_t scaled by 10000) */
    return (int64_t)(v * 10000.0);
}

long vb6_RGB(int32_t r, int32_t g, int32_t b) {
    /* RGB: combine red/green/blue into OLE color */
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    return (long)(r | (g << 8) | (b << 16));
}

long vb6_QBColor(int32_t n) {
    /* QBColor: return RGB for 16 QB colors (0-15) */
    static const long qbc[16] = {
        0x000000, 0x800000, 0x008000, 0x808000,
        0x000080, 0x800080, 0x008080, 0xC0C0C0,
        0x808080, 0xFF0000, 0x00FF00, 0xFFFF00,
        0x0000FF, 0xFF00FF, 0x00FFFF, 0xFFFFFF
    };
    if (n >= 0 && n < 16) return qbc[n];
    return 0;
}

double vb6_FileDateTime(BSTR path) {
    /* FileDateTime: return VB6 date serial for file modification time */
    if (!path) return 0.0;
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &fad)) return 0.0;
    /* Convert FILETIME to VB6 date serial */
    SYSTEMTIME st;
    FileTimeToSystemTime(&fad.ftLastWriteTime, &st);
    /* Build date: days since 1899-12-30 */
    struct tm t = {0};
    t.tm_year = st.wYear - 1900;
    t.tm_mon = st.wMonth - 1;
    t.tm_mday = st.wDay;
    t.tm_hour = st.wHour;
    t.tm_min = st.wMinute;
    t.tm_sec = st.wSecond;
    t.tm_isdst = -1;
    /* VB6 serial = days from 1899-12-30 + time fraction */
    /* 1899-12-30 is day -1, 1900-01-01 is day 2 in VB6 serial system */
    /* Use difftime from 1899-12-30 00:00:00 */
    struct tm epoch = {0};
    epoch.tm_year = -1;  /* 1899 */
    epoch.tm_mon = 11;   /* December */
    epoch.tm_mday = 30;
    double secs = difftime(mktime(&t), mktime(&epoch));
    return secs / 86400.0;
}

int32_t vb6_FileLen(BSTR path) {
    /* FileLen: return file size in bytes */
    if (!path) return 0;
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &fad)) return 0;
    ULARGE_INTEGER size;
    size.LowPart = fad.nFileSizeLow;
    size.HighPart = fad.nFileSizeHigh;
    return (int32_t)size.QuadPart;  /* Truncate to Long for VB6 compatibility */
}
// P21-11: GetAttr — return file attributes
int32_t vb6_GetAttr(BSTR pathname) {
    if (!pathname) return 0;
    DWORD attrs = GetFileAttributesW(pathname);
    if (attrs == INVALID_FILE_ATTRIBUTES) return 0;
    return (int32_t)attrs;
}

// P21-12: SetAttr — set file attributes
void vb6_SetAttr(BSTR pathname, int32_t attributes) {
    if (!pathname) return;
    SetFileAttributesW(pathname, (DWORD)attributes);
}


/* Helper: send a single virtual key press+release */
static void vb6_SendKeyVk(WORD vk) {
    INPUT inputs[2] = {0};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = vk;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = vk;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}

/* Helper: lookup VB6 SendKeys special key name to VK code */
static WORD vb6_SendKeysLookup(const wchar_t* name, size_t len) {
    /* F1-F24 */
    if ((len == 2 || len == 3) && (name[0]|0x20) == L'f') {
        int fnum = 0;
        for (size_t i = 1; i < len; i++) {
            if (name[i] >= L'0' && name[i] <= L'9') fnum = fnum * 10 + (name[i] - L'0');
            else { fnum = 0; break; }
        }
        if (fnum >= 1 && fnum <= 24) return (WORD)(VK_F1 + fnum - 1);
    }
    if (len == 3 && _wcsnicmp(name, L"TAB", 3) == 0) return VK_TAB;
    if (len == 5 && _wcsnicmp(name, L"ENTER", 5) == 0) return VK_RETURN;
    if (len == 3 && _wcsnicmp(name, L"ESC", 3) == 0) return VK_ESCAPE;
    if (len == 4 && _wcsnicmp(name, L"BACK", 4) == 0) return VK_BACK;
    if (len == 3 && _wcsnicmp(name, L"DEL", 3) == 0) return VK_DELETE;
    if (len == 6 && _wcsnicmp(name, L"DELETE", 6) == 0) return VK_DELETE;
    if (len == 3 && _wcsnicmp(name, L"INS", 3) == 0) return VK_INSERT;
    if (len == 4 && _wcsnicmp(name, L"HOME", 4) == 0) return VK_HOME;
    if (len == 3 && _wcsnicmp(name, L"END", 3) == 0) return VK_END;
    if (len == 4 && _wcsnicmp(name, L"PGUP", 4) == 0) return VK_PRIOR;
    if (len == 4 && _wcsnicmp(name, L"PGDN", 4) == 0) return VK_NEXT;
    if (len == 2 && _wcsnicmp(name, L"UP", 2) == 0) return VK_UP;
    if (len == 4 && _wcsnicmp(name, L"DOWN", 4) == 0) return VK_DOWN;
    if (len == 4 && _wcsnicmp(name, L"LEFT", 4) == 0) return VK_LEFT;
    if (len == 5 && _wcsnicmp(name, L"RIGHT", 5) == 0) return VK_RIGHT;
    if (len == 8 && _wcsnicmp(name, L"CAPSLOCK", 8) == 0) return VK_CAPITAL;
    if (len == 7 && _wcsnicmp(name, L"NUMLOCK", 7) == 0) return VK_NUMLOCK;
    if (len == 10 && _wcsnicmp(name, L"SCROLLLOCK", 10) == 0) return VK_SCROLL;
    if (len == 5 && _wcsnicmp(name, L"BREAK", 5) == 0) return VK_CANCEL;
    if (len == 4 && _wcsnicmp(name, L"HELP", 4) == 0) return VK_HELP;
    if (len == 6 && _wcsnicmp(name, L"PRTSC", 5) == 0) return VK_SNAPSHOT;
    if (len == 11 && _wcsnicmp(name, L"PRINTSCREEN", 11) == 0) return VK_SNAPSHOT;
    if (len == 5 && _wcsnicmp(name, L"SPACE", 5) == 0) return VK_SPACE;
    if (len == 8 && _wcsnicmp(name, L"BACKSPACE", 9) == 0) return VK_BACK;
    if (len == 8 && _wcsnicmp(name, L"CLEAR", 5) == 0) return VK_CLEAR;
    return 0;
}

void vb6_SendKeys(BSTR keys, int32_t wait) {
    /* SendKeys: supports single chars, {ENTER}/{TAB}/{ESC}/{F1}-{F24} etc., +^% modifiers */
    (void)wait;
    if (!keys) return;
    size_t len = vb6_BSTR_Len(keys);
    size_t i = 0;
    while (i < len) {
        int modShift = 0, modCtrl = 0, modAlt = 0;
        while (i < len && (keys[i] == L'+' || keys[i] == L'^' || keys[i] == L'%')) {
            if (keys[i] == L'+') modShift = 1;
            else if (keys[i] == L'^') modCtrl = 1;
            else if (keys[i] == L'%') modAlt = 1;
            i++;
        }
        if (i >= len) break;
        WORD vk = 0;
        int isUnicode = 0;
        wchar_t ch = 0;
        if (keys[i] == L'{') {
            size_t start = i + 1;
            size_t end = start;
            while (end < len && keys[end] != L'}') end++;
            if (end < len) {
                size_t klen = end - start;
                if (klen == 1 && keys[start] == L'~') {
                    vk = VK_RETURN;
                } else {
                    vk = vb6_SendKeysLookup(&keys[start], klen);
                }
            }
            i = end + 1;
        } else {
            isUnicode = 1;
            ch = keys[i];
            i++;
        }
        /* Press modifier keys */
        if (modShift) { INPUT mi = {0}; mi.type = INPUT_KEYBOARD; mi.ki.wVk = VK_SHIFT; SendInput(1, &mi, sizeof(INPUT)); }
        if (modCtrl) { INPUT mi = {0}; mi.type = INPUT_KEYBOARD; mi.ki.wVk = VK_CONTROL; SendInput(1, &mi, sizeof(INPUT)); }
        if (modAlt) { INPUT mi = {0}; mi.type = INPUT_KEYBOARD; mi.ki.wVk = VK_MENU; SendInput(1, &mi, sizeof(INPUT)); }
        if (isUnicode) {
            INPUT inputs[2] = {0};
            inputs[0].type = INPUT_KEYBOARD;
            inputs[0].ki.wScan = ch;
            inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
            inputs[1].type = INPUT_KEYBOARD;
            inputs[1].ki.wScan = ch;
            inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
            SendInput(2, inputs, sizeof(INPUT));
        } else if (vk) {
            vb6_SendKeyVk(vk);
        }
        /* Release modifier keys */
        if (modAlt) { INPUT mi = {0}; mi.type = INPUT_KEYBOARD; mi.ki.wVk = VK_MENU; mi.ki.dwFlags = KEYEVENTF_KEYUP; SendInput(1, &mi, sizeof(INPUT)); }
        if (modCtrl) { INPUT mi = {0}; mi.type = INPUT_KEYBOARD; mi.ki.wVk = VK_CONTROL; mi.ki.dwFlags = KEYEVENTF_KEYUP; SendInput(1, &mi, sizeof(INPUT)); }
        if (modShift) { INPUT mi = {0}; mi.type = INPUT_KEYBOARD; mi.ki.wVk = VK_SHIFT; mi.ki.dwFlags = KEYEVENTF_KEYUP; SendInput(1, &mi, sizeof(INPUT)); }
    }
}

/* AppActivate EnumWindows callback context */
typedef struct { DWORD pid; HWND result; } vb6_AppActivateCtx;
static BOOL CALLBACK vb6_AppActivateEnumProc(HWND hwnd, LPARAM lParam) {
    vb6_AppActivateCtx* ctx = (vb6_AppActivateCtx*)lParam;
    DWORD winPid = 0;
    GetWindowThreadProcessId(hwnd, &winPid);
    if (winPid == ctx->pid && IsWindowVisible(hwnd) && GetParent(hwnd) == NULL) {
        ctx->result = hwnd;
        return FALSE;
    }
    return TRUE;
}
void vb6_AppActivate(BSTR title, int32_t wait) {
    /* AppActivate: activate window by title or processID */
    (void)wait;
    if (!title) return;
    
    /* Check if title is a numeric processID (pure digits) */
    int32_t isPid = 1;
    int32_t pidVal = 0;
    size_t tLen = vb6_BSTR_Len(title);
    if (tLen == 0) return;
    for (size_t i = 0; i < tLen; i++) {
        if (title[i] >= L'0' && title[i] <= L'9') {
            pidVal = pidVal * 10 + (title[i] - L'0');
        } else {
            isPid = 0;
            break;
        }
    }
    
    if (isPid && pidVal > 0) {
        vb6_AppActivateCtx ctx = { (DWORD)pidVal, NULL };
        EnumWindows(vb6_AppActivateEnumProc, (LPARAM)&ctx);
        if (ctx.result) SetForegroundWindow(ctx.result);
    } else {
        HWND hwnd = FindWindowW(NULL, title);
        if (hwnd) SetForegroundWindow(hwnd);
    }
}

void vb6_AppActivateByPid(int32_t pid, int32_t wait) {
    /* AppActivate by processID */
    (void)wait;
    if (pid <= 0) return;
    vb6_AppActivateCtx ctx = { (DWORD)pid, NULL };
    EnumWindows(vb6_AppActivateEnumProc, (LPARAM)&ctx);
    if (ctx.result) SetForegroundWindow(ctx.result);
}
vb6_VARIANT vb6_CDec(vb6_VARIANT v) {
    /* P20-07: CDec - convert to Decimal (VT_DECIMAL=14) */
    vb6_VARIANT result;
    memset(&result, 0, sizeof(result));
    result.vt = vb6_vtDecimal;
    switch (v.vt) {
        case vb6_vtInteger: case vb6_vtLong: case vb6_vtByte: {
            int32_t ival = (v.vt == vb6_vtInteger) ? v.iVal : (v.vt == vb6_vtByte) ? (int32_t)v.bVal : v.lVal;
            result.decVal.Lo32 = (uint32_t)(ival < 0 ? -ival : ival);
            result.decVal.Mid32 = 0;
            result.decVal.Hi32 = 0;
            result.decVal.scale = 0;
            result.decVal.sign = (ival < 0) ? 0x80 : 0;
            break;
        }
        case vb6_vtSingle: case vb6_vtDouble: case vb6_vtCurrency: {
            double dval = (v.vt == vb6_vtSingle) ? (double)v.fltVal :
                          (v.vt == vb6_vtCurrency) ? (double)v.cyVal / 10000.0 : v.dblVal;
            DECIMAL winDec;
            if (VarDecFromR8(dval, &winDec) == S_OK) {
                memcpy(&result.decVal, &winDec, sizeof(winDec));
            } else {
                int64_t i64 = (int64_t)dval;
                result.decVal.Lo32 = (uint32_t)(i64 & 0xFFFFFFFF);
                result.decVal.Mid32 = (uint32_t)((i64 >> 32) & 0xFFFFFFFF);
                result.decVal.Hi32 = 0;
                result.decVal.scale = 0;
                result.decVal.sign = (dval < 0) ? 0x80 : 0;
            }
            break;
        }
        case vb6_vtBSTR: {
            DECIMAL winDec;
            if (v.bstrVal && VarDecFromStr(v.bstrVal, LOCALE_USER_DEFAULT, 0, &winDec) == S_OK) {
                memcpy(&result.decVal, &winDec, sizeof(winDec));
            }
            break;
        }
        case vb6_vtDecimal: {
            result = v;
            break;
        }
        default: break;
    }
    return result;
}

void vb6_MidSet(BSTR* target, int32_t start, int32_t len, BSTR replacement) {
    /* P18-A: Mid$ statement assignment — Mid$(target, start, len) = replacement */
    /* Replaces len characters of target starting at position start (1-based) */
    if (!target || !*target || start < 1) return;
    int32_t tLen = vb6_BSTR_Len(*target);
    if (start > tLen) return;
    int32_t rLen = replacement ? vb6_BSTR_Len(replacement) : 0;
    /* If len <= 0, use replacement length (VB6 behavior when length omitted) */
    if (len <= 0) len = rLen;
    /* Clamp to available characters */
    int32_t avail = tLen - start + 1;
    if (len > avail) len = avail;
    if (len > rLen) len = rLen;
    if (len <= 0) return;
    /* Build new string: prefix + replacement[0..len-1] + suffix */
    int32_t newLen = tLen;  /* Mid$ doesn't change length, only replaces in-place */
    BSTR result = SysAllocStringLen(NULL, newLen);
    if (!result) return;
    /* Copy prefix (before start) */
    if (start > 1) memcpy(result, *target, (start - 1) * sizeof(wchar_t));
    /* Copy replacement */
    if (replacement && len > 0) memcpy(result + start - 1, replacement, len * sizeof(wchar_t));
    /* Copy suffix (after start+len-1) */
    int32_t afterStart = start - 1 + len;
    if (afterStart < tLen) memcpy(result + afterStart, *target + afterStart, (tLen - afterStart) * sizeof(wchar_t));
    result[newLen] = L'\0';
    /* Replace target */
    SysFreeString(*target);
    *target = result;
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


// P21-10: FormatDateTime — named date/time formatting
// namedFormat: 0=vbGeneralDate, 1=vbLongDate, 2=vbShortDate, 3=vbLongTime, 4=vbShortTime
BSTR vb6_FormatDateTime(double dateSerial, int32_t namedFormat) {
    SYSTEMTIME st;
    double intPart, fracPart;
    fracPart = modf(dateSerial, &intPart);
    if (dateSerial == 0.0) {
        GetLocalTime(&st);
    } else {
        if (!VariantTimeToSystemTime(dateSerial, &st)) {
            GetLocalTime(&st);
        }
    }
    wchar_t buf[256] = {0};
    int len = 0;
    switch (namedFormat) {
        case 1: // vbLongDate
            len = GetDateFormatW(LOCALE_USER_DEFAULT, DATE_LONGDATE, &st, NULL, buf, 256);
            break;
        case 2: // vbShortDate
            len = GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, buf, 256);
            break;
        case 3: // vbLongTime
            len = GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &st, NULL, buf, 256);
            break;
        case 4: // vbShortTime
            len = GetTimeFormatW(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &st, NULL, buf, 256);
            break;
        case 0: // vbGeneralDate
        default: {
            // General date: date + time if time part nonzero
            len = GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, buf, 256);
            if (len > 0) buf[len-1] = L' ';
            if (fracPart != 0.0) {
                GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &st, NULL, buf + len, 256 - len);
            }
            len = (int)wcslen(buf);
            break;
        }
    }
    if (len <= 0) buf[0] = L'\0';
    return SysAllocString(buf);
}
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
// P14.3.4: App全局对象属性
// ============================================================

/* App.Path: 返回EXE所在目录 (去掉文件名部分) */
BSTR vb6_App_Path(void) {
    wchar_t buf[1024];
    DWORD len = GetModuleFileNameW(NULL, buf, 1024);
    if (len == 0) return vb6_BSTR_FromStr(L".");
    for (DWORD i = len; i > 0; i--) {
        if (buf[i-1] == L'\\' || buf[i-1] == L'/') {
            buf[i-1] = L'\0';
            return vb6_BSTR_FromStr(buf);
        }
    }
    return vb6_BSTR_FromStr(L".");
}

/* App.EXEName: 返回EXE文件名(不含路径和扩展名) */
BSTR vb6_App_EXEName(void) {
    wchar_t buf[1024];
    DWORD len = GetModuleFileNameW(NULL, buf, 1024);
    if (len == 0) return vb6_BSTR_Empty();
    wchar_t* fname = buf;
    for (DWORD i = 0; i < len; i++) {
        if (buf[i] == L'\\' || buf[i] == L'/') fname = &buf[i+1];
    }
    wchar_t* dot = wcsrchr(fname, L'.');
    if (dot) *dot = L'\0';
    return vb6_BSTR_FromStr(fname);
}

/* App.hInstance: 返回模块实例句柄 */
int32_t vb6_App_hInstance(void) {
    return (int32_t)(intptr_t)GetModuleHandleW(NULL);
}

// ============================================================
// P18-C: Clipboard 对象
// ============================================================
void vb6_Clipboard_SetText(BSTR text) {
    if (!OpenClipboard(NULL)) return;
    EmptyClipboard();
    int len = SysStringLen(text);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(wchar_t));
    if (hMem) {
        wchar_t* p = (wchar_t*)GlobalLock(hMem);
        if (p) {
            memcpy(p, text, len * sizeof(wchar_t));
            p[len] = 0;
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        } else {
            GlobalFree(hMem);
        }
    }
    CloseClipboard();
}

BSTR vb6_Clipboard_GetText(void) {
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) return SysAllocString(L"");
    if (!OpenClipboard(NULL)) return SysAllocString(L"");
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    BSTR result = SysAllocString(L"");
    if (hData) {
        wchar_t* p = (wchar_t*)GlobalLock(hData);
        if (p) {
            result = SysAllocString(p);
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();
    return result;
}

void vb6_Clipboard_Clear(void) {
    if (OpenClipboard(NULL)) { EmptyClipboard(); CloseClipboard(); }
}

int32_t vb6_Clipboard_GetFormat(int32_t format) {
    /* format: 1=vbCFText, 2=vbCFBitmap, 3=vbCFMetafile, 8=vbCFDIB, 9=vbCFPalette, &HBF00+=vbCFRtf */
    UINT cf = CF_UNICODETEXT;
    if (format == 1 || format == 2) cf = CF_UNICODETEXT;
    else if (format == 2) cf = CF_BITMAP;
    else if (format == 3) cf = CF_METAFILEPICT;
    else if (format == 8) cf = CF_DIB;
    else if (format == 9) cf = CF_PALETTE;
    else if (format == 0xBF00) cf = RegisterClipboardFormatW(L"Rich Text Format");
    return IsClipboardFormatAvailable(cf) ? -1 : 0;
}

// ============================================================
// P18-C: Screen 对象
// ============================================================
#define VB6_TWIPS_PER_INCH 1440

int32_t vb6_Screen_Width(void) {
    HDC dc = GetDC(NULL);
    int w = GetDeviceCaps(dc, HORZRES);
    int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(NULL, dc);
    return dpi > 0 ? (int32_t)(w * (double)VB6_TWIPS_PER_INCH / dpi) : w * 15;
}

int32_t vb6_Screen_Height(void) {
    HDC dc = GetDC(NULL);
    int h = GetDeviceCaps(dc, VERTRES);
    int dpi = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(NULL, dc);
    return dpi > 0 ? (int32_t)(h * (double)VB6_TWIPS_PER_INCH / dpi) : h * 15;
}

int32_t vb6_Screen_MouseX(void) {
    POINT pt; GetCursorPos(&pt);
    HDC dc = GetDC(NULL);
    int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(NULL, dc);
    return dpi > 0 ? (int32_t)(pt.x * (double)VB6_TWIPS_PER_INCH / dpi) : pt.x * 15;
}

int32_t vb6_Screen_MouseY(void) {
    POINT pt; GetCursorPos(&pt);
    HDC dc = GetDC(NULL);
    int dpi = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(NULL, dc);
    return dpi > 0 ? (int32_t)(pt.y * (double)VB6_TWIPS_PER_INCH / dpi) : pt.y * 15;
}

void* vb6_Screen_ActiveControl(void) {
    return (void*)GetFocus();
}

void* vb6_Screen_ActiveForm(void) {
    HWND hwnd = GetFocus();
    while (hwnd && !IsWindowVisible(hwnd)) hwnd = GetParent(hwnd);
    while (hwnd) {
        HWND parent = GetParent(hwnd);
        if (!parent || parent == GetDesktopWindow()) break;
        if (GetWindowLongPtrA(hwnd, GWLP_HWNDPARENT) == 0) break;
        hwnd = (HWND)GetWindowLongPtrA(hwnd, GWLP_HWNDPARENT);
        if (!hwnd) break;
    }
    return (void*)hwnd;
}

int32_t vb6_Screen_TwipsPerPixelX(void) {
    HDC dc = GetDC(NULL);
    int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(NULL, dc);
    return dpi > 0 ? (int32_t)((double)VB6_TWIPS_PER_INCH / dpi + 0.5) : 15;
}

int32_t vb6_Screen_TwipsPerPixelY(void) {
    HDC dc = GetDC(NULL);
    int dpi = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(NULL, dc);
    return dpi > 0 ? (int32_t)((double)VB6_TWIPS_PER_INCH / dpi + 0.5) : 15;
}

// ============================================================
// P18-C: Printer 对象
// ============================================================
static HDC g_printerDC = NULL;
static int g_printerCurrentX = 0;
static int g_printerCurrentY = 0;
static DOCINFOW g_docInfo = {0};
static int g_printerStarted = 0;

static void vb6_Printer_EnsureDC(void) {
    if (!g_printerDC) {
        g_printerDC = CreateDCW(L"WINSPOOL", NULL, NULL, NULL);
    }
    if (g_printerDC && !g_printerStarted) {
        memset(&g_docInfo, 0, sizeof(g_docInfo));
        g_docInfo.cbSize = sizeof(g_docInfo);
        g_docInfo.lpszDocName = L"VB6 Print Job";
        StartDocW(g_printerDC, &g_docInfo);
        StartPage(g_printerDC);
        g_printerStarted = 1;
        g_printerCurrentX = 0;
        g_printerCurrentY = 0;
    }
}

void vb6_Printer_Print(BSTR text) {
    vb6_Printer_EnsureDC();
    if (!g_printerDC) return;
    TextOutW(g_printerDC, g_printerCurrentX, g_printerCurrentY, text, SysStringLen(text));
    SIZE sz;
    GetTextExtentPoint32W(g_printerDC, text, SysStringLen(text), &sz);
    g_printerCurrentY += sz.cy;
}

void vb6_Printer_EndDoc(void) {
    if (g_printerDC && g_printerStarted) {
        EndPage(g_printerDC);
        EndDoc(g_printerDC);
        g_printerStarted = 0;
    }
    if (g_printerDC) { DeleteDC(g_printerDC); g_printerDC = NULL; }
}

void vb6_Printer_NewPage(void) {
    if (g_printerDC && g_printerStarted) {
        EndPage(g_printerDC);
        StartPage(g_printerDC);
        g_printerCurrentX = 0;
        g_printerCurrentY = 0;
    }
}

int32_t vb6_Printer_Width(void) {
    vb6_Printer_EnsureDC();
    if (!g_printerDC) return 0;
    // P20-44: VB6 Printer.Width返回twips(1440/inch), 不是pixels
    int px = GetDeviceCaps(g_printerDC, PHYSICALWIDTH);
    int dpi = GetDeviceCaps(g_printerDC, LOGPIXELSX);
    return dpi > 0 ? (int32_t)((int64_t)px * 1440 / dpi) : px;
}

int32_t vb6_Printer_Height(void) {
    vb6_Printer_EnsureDC();
    if (!g_printerDC) return 0;
    // P20-44: VB6 Printer.Height返回twips(1440/inch), 不是pixels
    int px = GetDeviceCaps(g_printerDC, PHYSICALHEIGHT);
    int dpi = GetDeviceCaps(g_printerDC, LOGPIXELSY);
    return dpi > 0 ? (int32_t)((int64_t)px * 1440 / dpi) : px;
}

int32_t vb6_Printer_CurrentX(void) { return g_printerCurrentX; }
int32_t vb6_Printer_CurrentY(void) { return g_printerCurrentY; }
void vb6_Printer_SetCurrentX(int32_t x) { g_printerCurrentX = x; }
void vb6_Printer_SetCurrentY(int32_t y) { g_printerCurrentY = y; }

// ============================================================
// P18-C: Forms 集合
// ============================================================
#define VB6_MAX_FORMS 64
static HWND g_formList[VB6_MAX_FORMS] = {0};
static int g_formCount = 0;

void vb6_Forms_Register(void* hwnd_) {
    if (g_formCount < VB6_MAX_FORMS) { g_formList[g_formCount++] = (HWND)hwnd_; }
}
void vb6_Forms_Unregister(void* hwnd_) {
    HWND hwnd = (HWND)hwnd_;
    for (int i = 0; i < g_formCount; i++) {
        if (g_formList[i] == hwnd) {
            g_formList[i] = g_formList[--g_formCount];
            g_formList[g_formCount] = NULL;
            return;
        }
    }
}

int32_t vb6_Forms_Count(void) { return g_formCount; }
void* vb6_Forms_Item(int32_t index) {
    if (index >= 0 && index < g_formCount) return (void*)g_formList[index];
    return NULL;
}


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

int32_t vb6_StrPtr(BSTR s) {
    return (int32_t)(intptr_t)s;
}

int32_t vb6_ObjPtr(void* obj) {
    return (int32_t)(intptr_t)obj;
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
// P14.2.4: IIf / InputBox
// ============================================================

/* IIf: VB6内联条件函数 - 通过函数调用确保两个分支都求值(C函数参数求值顺序不影响"都求值")
 * VB6 IIf 不短路，两个分支都必须计算，然后按条件选一个
 * 通过函数调用而非C三元运算符实现，保证VB6语义正确 */
BSTR vb6_IIfBSTR(int32_t cond, BSTR truepart, BSTR falsepart) {
    BSTR result = cond ? truepart : falsepart;
    /* 释放未被选中的分支(防止BSTR泄漏) */
    if (cond) { vb6_BSTR_Free(falsepart); }
    else      { vb6_BSTR_Free(truepart); }
    return result;
}
int32_t vb6_IIfLong(int32_t cond, int32_t truepart, int32_t falsepart) {
    (void)falsepart;
    return cond ? truepart : falsepart;
}
double vb6_IIfDouble(int32_t cond, double truepart, double falsepart) {
    (void)falsepart;
    return cond ? truepart : falsepart;
}
vb6_VARIANT vb6_IIfVariant(int32_t cond, vb6_VARIANT truepart, vb6_VARIANT falsepart) {
    if (cond) { vb6_VariantClear(&falsepart); return truepart; }
    else      { vb6_VariantClear(&truepart);  return falsepart; }
}

/* InputBox: 简化实现 - 使用控制台输入 (非GUI环境) */
BSTR vb6_InputBox(BSTR prompt, BSTR title, BSTR defaultstr, int32_t xpos, int32_t ypos, BSTR helpfile, int32_t context) {
    (void)title; (void)xpos; (void)ypos; (void)helpfile; (void)context;
    /* 输出提示 */
    if (prompt) {
        fwprintf(stdout, L"%ls", prompt);
        fwprintf(stdout, L"\r\n");
    }
    /* 显示默认值提示 */
    if (defaultstr && vb6_BSTR_Len(defaultstr) > 0) {
        fwprintf(stdout, L"[%ls] ", defaultstr);
    }
    fwprintf(stdout, L"> ");
    fflush(stdout);
    /* 读取一行输入 */
    wchar_t buf[1024];
    if (fgetws(buf, 1024, stdin)) {
        /* 去掉尾部换行 */
        int32_t len = (int32_t)wcslen(buf);
        while (len > 0 && (buf[len-1] == L'\n' || buf[len-1] == L'\r')) {
            buf[--len] = L'\0';
        }
        if (len == 0 && defaultstr) return vb6_BSTR_FromStr(defaultstr);
        return vb6_BSTR_FromStr(buf);
    }
    /* 读取失败则返回默认值 */
    if (defaultstr) return vb6_BSTR_FromStr(defaultstr);
    return vb6_BSTR_Empty();
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

void vb6_DateSet(BSTR dateStr) {
    if (!dateStr || SysStringLen(dateStr) == 0) return;
    char buf[32] = {0};
    WideCharToMultiByte(CP_ACP, 0, dateStr, -1, buf, 31, NULL, NULL);
    int m = 0, d = 0, y = 0;
    char sep = strchr(buf, '/') ? '/' : '-';
    char *ctx = NULL;
    char buf2[32]; memcpy(buf2, buf, 32);
    char *p1 = strtok_s(buf2, &sep, &ctx);
    char *p2 = p1 ? strtok_s(NULL, &sep, &ctx) : NULL;
    char *p3 = p2 ? strtok_s(NULL, &sep, &ctx) : NULL;
    if (p1 && p2 && p3) {
        m = atoi(p1); d = atoi(p2); y = atoi(p3);
        SYSTEMTIME st; GetLocalTime(&st);
        st.wYear = (WORD)y; st.wMonth = (WORD)m; st.wDay = (WORD)d;
        SetLocalTime(&st);
    }
}

void vb6_TimeSet(BSTR timeStr) {
    if (!timeStr || SysStringLen(timeStr) == 0) return;
    char buf[32] = {0};
    WideCharToMultiByte(CP_ACP, 0, timeStr, -1, buf, 31, NULL, NULL);
    int h = 0, mi = 0, s = 0;
    char *ctx = NULL;
    char buf2[32]; memcpy(buf2, buf, 32);
    char *p1 = strtok_s(buf2, ":", &ctx);
    char *p2 = p1 ? strtok_s(NULL, ":", &ctx) : NULL;
    char *p3 = p2 ? strtok_s(NULL, ":", &ctx) : NULL;
    if (p1) h = atoi(p1);
    if (p2) mi = atoi(p2);
    if (p3) s = atoi(p3);
    SYSTEMTIME st; GetLocalTime(&st);
    st.wHour = (WORD)h; st.wMinute = (WORD)mi; st.wSecond = (WORD)s; st.wMilliseconds = 0;
    SetLocalTime(&st);
}

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
double vb6_DateSerial(int32_t year, int32_t month, int32_t day) {
    int32_t jd_date = vb6_date_to_serial(year, month, day);
    int32_t jd_base = vb6_date_to_serial(1899, 12, 30);
    return (double)(jd_date - jd_base);
}

// P14.2.4: DateAdd/DateDiff/DatePart 日期函数
// ============================================================

// 解析VB6间隔字符串为枚举
typedef enum {
    vb6_di_year, vb6_di_quarter, vb6_di_month, vb6_di_dayofyear,
    vb6_di_day, vb6_di_weekday, vb6_di_week, vb6_di_hour,
    vb6_di_minute, vb6_di_second
} vb6_date_interval;

static vb6_date_interval vb6_parse_interval(BSTR interval) {
    if (!interval || vb6_BSTR_Len(interval) == 0) return vb6_di_day;
    wchar_t ch = interval[0];
    switch (ch) {
        case L'y': case L'Y':
            if (vb6_BSTR_Len(interval) >= 4)
                return vb6_di_year;  // "yyyy" = year
            return vb6_di_dayofyear;  // "y" = DayOfYear
        case L'q': case L'Q': return vb6_di_quarter;
        case L'm': case L'M': return vb6_di_month;
        case L'd': case L'D': return vb6_di_day;
        case L'w': case L'W':
            if (vb6_BSTR_Len(interval) >= 2 && (interval[1] == L'w' || interval[1] == L'W'))
                return vb6_di_week;
            return vb6_di_weekday;
        case L'h': case L'H': return vb6_di_hour;
        case L'n': case L'N': return vb6_di_minute;
        case L's': case L'S': return vb6_di_second;
        default: return vb6_di_day;
    }
}

double vb6_DateAdd(BSTR interval, double number, double date) {
    vb6_date_interval di = vb6_parse_interval(interval);
    int32_t datePart = (int32_t)date;
    double timePart = date - (double)datePart;
    if (timePart < 0) { timePart += 1.0; datePart--; }
    
    int32_t y, m, d;
    vb6_serial_to_date(datePart, &y, &m, &d);
    int32_t hh = (int32_t)(timePart * 24.0) % 24;
    int32_t mm = (int32_t)(timePart * 1440.0) % 60;
    int32_t ss = (int32_t)(timePart * 86400.0) % 60;
    
    int32_t n = (int32_t)number;
    
    switch (di) {
        case vb6_di_year:
            y += n;
            break;
        case vb6_di_quarter:
            m += n * 3;
            while (m > 12) { m -= 12; y++; }
            while (m < 1)  { m += 12; y--; }
            break;
        case vb6_di_month:
            m += n;
            while (m > 12) { m -= 12; y++; }
            while (m < 1)  { m += 12; y--; }
            break;
        case vb6_di_day:
        case vb6_di_dayofyear:
        case vb6_di_weekday:
            datePart += n;
            goto rebuild;
        case vb6_di_week:
            datePart += n * 7;
            goto rebuild;
        case vb6_di_hour:
            hh += n;
            while (hh >= 24) { hh -= 24; datePart++; }
            while (hh < 0)   { hh += 24; datePart--; }
            goto rebuild;
        case vb6_di_minute:
            mm += n;
            while (mm >= 60) { mm -= 60; hh++; }
            while (mm < 0)   { mm += 60; hh--; }
            while (hh >= 24) { hh -= 24; datePart++; }
            while (hh < 0)   { hh += 24; datePart--; }
            goto rebuild;
        case vb6_di_second:
            ss += n;
            while (ss >= 60) { ss -= 60; mm++; }
            while (ss < 0)   { ss += 60; mm--; }
            while (mm >= 60) { mm -= 60; hh++; }
            while (mm < 0)   { mm += 60; hh--; }
            while (hh >= 24) { hh -= 24; datePart++; }
            while (hh < 0)   { hh += 24; datePart--; }
            goto rebuild;
    }
    // For year/quarter/month: clamp day to valid range for the new month
    {
        static const int32_t daysInMonth[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
        int32_t maxDay = daysInMonth[m];
        if (m == 2 && (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0))) maxDay = 29;
        if (d > maxDay) d = maxDay;
        // Rebuild serial
        int32_t jd_date = vb6_date_to_serial(y, m, d);
        int32_t jd_base = vb6_date_to_serial(1899, 12, 30);
        datePart = jd_date - jd_base;
    }
rebuild:
    {
        double result = (double)datePart + (hh * 3600.0 + mm * 60.0 + ss) / 86400.0;
        return result;
    }
}

int64_t vb6_DateDiff(BSTR interval, double date1, double date2, int32_t firstDayOfWeek, int32_t firstWeekOfYear) {
    (void)firstDayOfWeek;
    (void)firstWeekOfYear;
    vb6_date_interval di = vb6_parse_interval(interval);
    
    int32_t d1 = (int32_t)date1, d2 = (int32_t)date2;
    
    switch (di) {
        case vb6_di_year: {
            int32_t y1, m1, dd1, y2, m2, dd2;
            vb6_serial_to_date(d1, &y1, &m1, &dd1);
            vb6_serial_to_date(d2, &y2, &m2, &dd2);
            return (int64_t)(y2 - y1);
        }
        case vb6_di_quarter: {
            int32_t y1, m1, dd1, y2, m2, dd2;
            vb6_serial_to_date(d1, &y1, &m1, &dd1);
            vb6_serial_to_date(d2, &y2, &m2, &dd2);
            return (int64_t)((y2 * 4 + (m2 - 1) / 3) - (y1 * 4 + (m1 - 1) / 3));
        }
        case vb6_di_month: {
            int32_t y1, m1, dd1, y2, m2, dd2;
            vb6_serial_to_date(d1, &y1, &m1, &dd1);
            vb6_serial_to_date(d2, &y2, &m2, &dd2);
            return (int64_t)((y2 * 12 + m2) - (y1 * 12 + m1));
        }
        case vb6_di_day:
        case vb6_di_dayofyear:
            return (int64_t)(d2 - d1);
        case vb6_di_weekday:
            return (int64_t)(d2 - d1);
        case vb6_di_week:
            return (int64_t)((d2 - d1) / 7);
        case vb6_di_hour:
            return (int64_t)((date2 - date1) * 24.0);
        case vb6_di_minute:
            return (int64_t)((date2 - date1) * 1440.0);
        case vb6_di_second:
            return (int64_t)((date2 - date1) * 86400.0);
    }
    return 0;
}

int32_t vb6_DatePart(BSTR interval, double date, int32_t firstDayOfWeek, int32_t firstWeekOfYear) {
    (void)firstDayOfWeek;
    (void)firstWeekOfYear;
    vb6_date_interval di = vb6_parse_interval(interval);
    int32_t d = (int32_t)date;
    
    switch (di) {
        case vb6_di_year: {
            int32_t y, m, dd;
            vb6_serial_to_date(d, &y, &m, &dd);
            return y;
        }
        case vb6_di_quarter: {
            int32_t y, m, dd;
            vb6_serial_to_date(d, &y, &m, &dd);
            return (m - 1) / 3 + 1;
        }
        case vb6_di_month: {
            int32_t y, m, dd;
            vb6_serial_to_date(d, &y, &m, &dd);
            return m;
        }
        case vb6_di_day:
        case vb6_di_dayofyear: {
            int32_t y, m, dd;
            vb6_serial_to_date(d, &y, &m, &dd);
            // Day of year: simple approximation (days since start of year)
            int32_t jd_this = vb6_date_to_serial(y, m, dd);
            int32_t jd_jan1 = vb6_date_to_serial(y, 1, 1);
            return jd_this - jd_jan1 + 1;
        }
        case vb6_di_weekday: {
            // Sunday=1, Monday=2, ..., Saturday=7
            // 1899-12-30 (serial 0) was a Saturday (day 7)
            int32_t wd = (d % 7 + 7) % 7;  // 0=Sat,1=Sun,...,6=Fri
            int32_t weekday = wd + 1;        // Sat=1,Sun=2,...,Fri=7
            // Remap: Sun=1,Mon=2,...,Sat=7
            // wd: 0=Sat,1=Sun,2=Mon,3=Tue,4=Wed,5=Thu,6=Fri
            if (wd == 0) return 7;  // Saturday
            return wd;              // Sunday=1, Monday=2, etc.
        }
        case vb6_di_week: {
            // ISO 8601 simplified: week 1 contains Jan 4
            int32_t y, m, dd;
            vb6_serial_to_date(d, &y, &m, &dd);
            int32_t jd_this = vb6_date_to_serial(y, m, dd);
            int32_t jd_jan1 = vb6_date_to_serial(y, 1, 1);
            int32_t dayOfYear = jd_this - jd_jan1 + 1;
            return (dayOfYear - 1) / 7 + 1;
        }
        case vb6_di_hour:
            return vb6_Hour(date);
        case vb6_di_minute:
            return vb6_Minute(date);
        case vb6_di_second:
            return vb6_Second(date);
    }
    return 0;
}
// ============================================================

// ============================================================
// P21-B: Weekday/DateValue/TimeSerial/TimeValue/Array
// ============================================================

int32_t vb6_Weekday(double date, int32_t firstDayOfWeek) {
    /* VB6 Weekday(date[, firstDayOfWeek])
       Returns the day of the week as an integer.
       firstDayOfWeek: 1=vbSunday(default), 2=vbMonday, ..., 7=vbSaturday
       Return: 1-based relative to firstDayOfWeek.
       For default (vbSunday=1): Sunday=1, Monday=2, ..., Saturday=7 */
    int32_t d = (int32_t)date;
    /* 1899-12-30 (serial 0) was a Saturday */
    int32_t wd = (d % 7 + 7) % 7;  /* 0=Sat,1=Sun,2=Mon,3=Tue,4=Wed,5=Thu,6=Fri */
    /* Convert to Sunday=1,Monday=2,...,Saturday=7 */
    int32_t weekday;
    if (wd == 0) weekday = 7;  /* Saturday */
    else weekday = wd;         /* Sunday=1,Monday=2,... */
    
    /* Adjust for firstDayOfWeek */
    if (firstDayOfWeek <= 0) firstDayOfWeek = 1;  /* default vbSunday */
    if (firstDayOfWeek > 7) firstDayOfWeek = 1;
    if (firstDayOfWeek == 1) return weekday;  /* vbSunday: no shift needed */
    /* Shift: e.g. vbMonday(2) -> Monday=1,...,Sunday=7 */
    int32_t shifted = weekday - (firstDayOfWeek - 1);
    if (shifted < 1) shifted += 7;
    return shifted;
}

double vb6_DateValue(BSTR dateStr) {
    /* VB6 DateValue(datestring) -> date serial (double)
       Parses a date string using Windows OLE Automation */
    if (!dateStr) return 0.0;
    DATE result = 0.0;
    if (SUCCEEDED(VarDateFromStr(dateStr, LOCALE_USER_DEFAULT, 0, &result))) {
        return (double)(int32_t)result;
    }
    return 0.0;
}

double vb6_TimeSerial(int32_t hour, int32_t minute, int32_t second) {
    /* VB6 TimeSerial(hour, minute, second) -> date serial (double)
       The date part is 0 (Dec 30, 1899), time part is the fraction of day */
    int32_t totalSeconds = hour * 3600 + minute * 60 + second;
    while (totalSeconds < 0) totalSeconds += 86400;
    int32_t extraDays = totalSeconds / 86400;
    int32_t timeSeconds = totalSeconds % 86400;
    return (double)extraDays + (double)timeSeconds / 86400.0;
}

double vb6_TimeValue(BSTR timeStr) {
    /* VB6 TimeValue(timestring) -> date serial (double)
       Parses a time string using Windows OLE Automation */
    if (!timeStr) return 0.0;
    DATE result = 0.0;
    if (SUCCEEDED(VarDateFromStr(timeStr, LOCALE_USER_DEFAULT, 0, &result))) {
        double datePart = (double)(int32_t)result;
        double timePart = result - datePart;
        if (timePart < 0) timePart += 1.0;
        return timePart;
    }
    return 0.0;
}

vb6_SafeArray1D* vb6_ArrayCreate(int32_t count) {
    /* VB6 Array(arglist) helper: creates a Variant SafeArray with count elements.
       Caller (cgen) sets each element directly via VB6_SA_AT. */
    if (count <= 0) count = 0;
    vb6_SafeArray1D* arr = vb6_SafeArrayCreate1D(vb6_sa_variant, 0, count > 0 ? count - 1 : 0);
    return arr;
}

void vb6_ArraySetLong(vb6_SafeArray1D* arr, int32_t index, int32_t val) {
    if (!arr || index < arr->lBound || index > arr->uBound) return;
    vb6_VARIANT* v = &VB6_SA_AT(vb6_VARIANT, arr, index);
    vb6_VariantClear(v);
    v->vt = vb6_vtLong;
    v->lVal = val;
}

void vb6_ArraySetDouble(vb6_SafeArray1D* arr, int32_t index, double val) {
    if (!arr || index < arr->lBound || index > arr->uBound) return;
    vb6_VARIANT* v = &VB6_SA_AT(vb6_VARIANT, arr, index);
    vb6_VariantClear(v);
    v->vt = vb6_vtDouble;
    v->dblVal = val;
}

void vb6_ArraySetBSTR(vb6_SafeArray1D* arr, int32_t index, BSTR val) {
    if (!arr || index < arr->lBound || index > arr->uBound) return;
    vb6_VARIANT* v = &VB6_SA_AT(vb6_VARIANT, arr, index);
    vb6_VariantClear(v);
    v->vt = vb6_vtBSTR;
    v->bstrVal = vb6_BSTR_FromBSTR(val);
}

void vb6_ArraySetVariant(vb6_SafeArray1D* arr, int32_t index, vb6_VARIANT val) {
    if (!arr || index < arr->lBound || index > arr->uBound) return;
    vb6_VARIANT* v = &VB6_SA_AT(vb6_VARIANT, arr, index);
    vb6_VariantClear(v);
    vb6_VariantCopy(v, &val);
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
        case vb6_sa_currency: return (int32_t)sizeof(int64_t);  /* P1-9修复: VB6 Currency = 8bytes */
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

    // P1-12修复: BSTR/VARIANT - 旧区域中被丢弃的元素需要释放
    if (arr->data && (arr->elemType == vb6_sa_bstr || arr->elemType == vb6_sa_variant)) {
        for (int32_t linear = 0; linear < arr->totalElements; linear++) {
            int32_t tmp = linear;
            int32_t indices[16];
            int32_t d;
            for (d = arr->dimCount - 1; d >= 0; d--) {
                indices[d] = tmp % arr->bounds[d].cElements;
                tmp /= arr->bounds[d].cElements;
            }
            int32_t inNewBounds = 1;
            for (d = 0; d < arr->dimCount && d < dimCount; d++) {
                if (indices[d] >= newBounds[d].cElements) { inNewBounds = 0; break; }
            }
            if (arr->dimCount > dimCount) inNewBounds = 0;
            if (!inNewBounds) {
                if (arr->elemType == vb6_sa_bstr) {
                    BSTR* slot = (BSTR*)((char*)arr->data + linear * arr->elemSize);
                    if (*slot) vb6_BSTR_Free(*slot);
                } else if (arr->elemType == vb6_sa_variant) {
                    vb6_VARIANT* slot = (vb6_VARIANT*)((char*)arr->data + linear * arr->elemSize);
                    vb6_VariantClear(slot);
                }
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
static int32_t vb6_width_table[VB6_MAX_FILES] = {0}; // P22-08: Width# 行宽 (0=不限)
static int32_t vb6_col_table[VB6_MAX_FILES] = {0};   // P22-08: 当前列位置

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
    vb6_width_table[filenumber] = 0;  // P22-08: reset width on open
    vb6_col_table[filenumber] = 0;    // P22-08: reset column on open
    return -1;  // True
}

int32_t vb6_Close(int32_t filenumber) {
    if (filenumber < 1 || filenumber >= VB6_MAX_FILES) return 0;
    if (vb6_file_table[filenumber]) {
        fclose(vb6_file_table[filenumber]);
        vb6_file_table[filenumber] = NULL;
        vb6_file_mode[filenumber] = 0;
        vb6_file_reclen[filenumber] = 0;
        vb6_width_table[filenumber] = 0;  // P22-08
        vb6_col_table[filenumber] = 0;    // P22-08
    }
    return -1;
}

int32_t vb6_CloseAll() {
    int count = 0;
    for (int i = 1; i < VB6_MAX_FILES; i++) {
        if (vb6_file_table[i]) {
            fclose(vb6_file_table[i]);
            vb6_file_table[i] = NULL;
            vb6_file_mode[i] = 0;
            vb6_file_reclen[i] = 0;
            vb6_width_table[i] = 0;  // P22-08
            vb6_col_table[i] = 0;    // P22-08
            count++;
        }
    }
    return count;
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

// P21-13: Seek function — return current file position
int32_t vb6_SeekFunc(int32_t filenumber) {
    if (filenumber < 1 || filenumber > 255 || !vb6_file_table[filenumber]) return 0;
    return (int32_t)(ftell(vb6_file_table[filenumber]) + 1);  // VB6 is 1-based
}

// P21-13: Seek statement — set file position
void vb6_SeekStmt(int32_t filenumber, int32_t position) {
    if (filenumber < 1 || filenumber > 255 || !vb6_file_table[filenumber]) return;
    fseek(vb6_file_table[filenumber], (long)(position - 1), SEEK_SET);  // VB6 is 1-based
}


// P22-08: Width# — 设置文件输出行宽
void vb6_Width(int32_t filenumber, int32_t width) {
    if (filenumber < 1 || filenumber >= VB6_MAX_FILES) return;
    vb6_width_table[filenumber] = width;
}

void vb6_Print(int32_t filenumber, BSTR s) {
    if (filenumber < 1 || filenumber >= VB6_MAX_FILES || !vb6_file_table[filenumber]) return;
    FILE* f = vb6_file_table[filenumber];
    int32_t w = vb6_width_table[filenumber];  // P22-08: Width# supported line width
    if (s) {
        int32_t len = vb6_BSTR_Len(s);
        for (int32_t i = 0; i < len; i++) {
            char ch = (char)s[i];
            if (ch == '\n') {
                fputc('\n', f);
                vb6_col_table[filenumber] = 0;
            } else {
                if (w > 0 && vb6_col_table[filenumber] >= w) {
                    fputc('\n', f);
                    vb6_col_table[filenumber] = 0;
                }
                fputc(ch, f);
                vb6_col_table[filenumber]++;
            }
        }
    }
    fputc('\n', f);
    vb6_col_table[filenumber] = 0;
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

// P15.4: Input function - reads count characters from file
BSTR vb6_InputString(int32_t filenumber, int32_t count) {
    if (filenumber < 1 || filenumber > 511 || !vb6_file_table[filenumber]) return NULL;
    if (count <= 0) return vb6_BSTR_Empty();
    wchar_t* buf = (wchar_t*)malloc((count + 1) * sizeof(wchar_t));
    if (!buf) return NULL;
    int32_t read = 0;
    for (int32_t i = 0; i < count; i++) {
        int ch = fgetc(vb6_file_table[filenumber]);
        if (ch == EOF) break;
        buf[read++] = (wchar_t)(unsigned char)ch;
    }
    buf[read] = L'\0';
    BSTR result = vb6_BSTR_FromStr(buf);
    free(buf);
    return result;
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
    // P20-49: 尝试切换驱动器, 失败也返回成功(空操作兼容)
#ifdef _WIN32
    if (drive && SysStringLen(drive) > 0) {
        int driveNum = towupper(drive[0]) - 'A' + 1;
        _chdrive(driveNum);
    }
#endif
    return 0;  // VB6 ChDrive是Sub, 无返回值, 始终返回0
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


// ============================================================
// ParamArray runtime support (P14.1.5)
// ============================================================

SAFEARRAY* vb6_PA_Create(int32_t count) {
    if (count <= 0) return NULL;
    SAFEARRAYBOUND bound;
    bound.lLbound = 0;
    bound.cElements = (ULONG)count;
    SAFEARRAY* psa = SafeArrayCreate(VT_VARIANT, 1, &bound);
    return psa;
}

void vb6_PA_Destroy(SAFEARRAY* psa) {
    if (psa) SafeArrayDestroy(psa);
}

void vb6_PA_SetVariant(SAFEARRAY* psa, int32_t index, VARIANT* pv) {
    if (!psa || !pv) return;
    long idx = (long)index;
    SafeArrayPutElement(psa, &idx, pv);
}

void vb6_PA_SetLong(SAFEARRAY* psa, int32_t index, int32_t val) {
    if (!psa) return;
    VARIANT v;
    VariantInit(&v);
    v.vt = VT_I4;
    v.lVal = val;
    long idx = (long)index;
    SafeArrayPutElement(psa, &idx, &v);
    // SafeArrayPutElement copies the VARIANT, no need to keep v alive
}

void vb6_PA_SetDouble(SAFEARRAY* psa, int32_t index, double val) {
    if (!psa) return;
    VARIANT v;
    VariantInit(&v);
    v.vt = VT_R8;
    v.dblVal = val;
    long idx = (long)index;
    SafeArrayPutElement(psa, &idx, &v);
}

void vb6_PA_SetBSTR(SAFEARRAY* psa, int32_t index, BSTR val) {
    if (!psa) return;
    VARIANT v;
    VariantInit(&v);
    v.vt = VT_BSTR;
    v.bstrVal = SysAllocString(val);  // SafeArrayPutElement doesn't copy BSTR
    long idx = (long)index;
    SafeArrayPutElement(psa, &idx, &v);
    // v.bstrVal is now owned by the array element
}

VARIANT vb6_PA_GetVariant(SAFEARRAY* psa, int32_t index) {
    VARIANT v;
    VariantInit(&v);
    if (!psa) return v;
    long idx = (long)index;
    SafeArrayGetElement(psa, &idx, &v);
    return v;
}

int32_t vb6_PA_GetLong(SAFEARRAY* psa, int32_t index) {
    if (!psa) return 0;
    VARIANT v;
    VariantInit(&v);
    long idx = (long)index;
    SafeArrayGetElement(psa, &idx, &v);
    int32_t result = 0;
    if (v.vt == VT_I4) result = v.lVal;
    else if (v.vt == VT_I2) result = (int32_t)v.iVal;
    else if (v.vt == VT_R8) result = (int32_t)v.dblVal;
    VariantClear(&v);
    return result;
}

double vb6_PA_GetDouble(SAFEARRAY* psa, int32_t index) {
    if (!psa) return 0.0;
    VARIANT v;
    VariantInit(&v);
    long idx = (long)index;
    SafeArrayGetElement(psa, &idx, &v);
    double result = 0.0;
    if (v.vt == VT_R8) result = v.dblVal;
    else if (v.vt == VT_I4) result = (double)v.lVal;
    else if (v.vt == VT_BSTR && v.bstrVal) result = wcstod(v.bstrVal, NULL);
    VariantClear(&v);
    return result;
}

BSTR vb6_PA_GetBSTR(SAFEARRAY* psa, int32_t index) {
    if (!psa) return NULL;
    VARIANT v;
    VariantInit(&v);
    long idx = (long)index;
    SafeArrayGetElement(psa, &idx, &v);
    BSTR result = NULL;
    if (v.vt == VT_BSTR && v.bstrVal) {
        result = SysAllocString(v.bstrVal);
    } else if (v.vt == VT_I4) {
        wchar_t buf[32];
        _ltow_s(v.lVal, buf, 32, 10);
        result = SysAllocString(buf);
    } else if (v.vt == VT_R8) {
        wchar_t buf[64];
        swprintf_s(buf, 64, L"%g", v.dblVal);
        result = SysAllocString(buf);
    }
    VariantClear(&v);
    return result;
}

int32_t vb6_IsMissing(SAFEARRAY* psa) {
    // IsMissing returns True if no arguments were passed to ParamArray
    return (psa == NULL) ? -1 : 0;  // VB6 True = -1, False = 0
}

int32_t vb6_PA_UBound(SAFEARRAY* psa) {
    if (!psa) return -1;  // empty ParamArray: UBound returns -1 (VB6 behavior)
    long ubound = 0;
    SafeArrayGetUBound(psa, 1, &ubound);
    return (int32_t)ubound;
}

int32_t vb6_PA_LBound(SAFEARRAY* psa) {
    if (!psa) return 0;
    long lbound = 0;
    SafeArrayGetLBound(psa, 1, &lbound);
    return (int32_t)lbound;
}

// P14.3.5: CallByName - 按名称动态调用方法/属性
// calltype: 1=VbLet(set property), 2=VbMethod(call method), 3=VbGet(get property)
vb6_VARIANT vb6_CallByName(void* obj, const wchar_t* procName, int32_t callType,
                           void* args, int32_t argc) {
    vb6_VARIANT result;
    memset(&result, 0, sizeof(result));
    result.vt = VT_EMPTY;

    if (!obj || !procName) return result;

    IDispatch* disp = (IDispatch*)obj;
    DISPID dispid = 0;
    HRESULT hr;

    // Get DISPID
    hr = disp->lpVtbl->GetIDsOfNames(disp, &IID_NULL, (LPOLESTR*)&procName, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) return result;

    // Determine INVOKE_KIND from calltype
    INVOKEKIND invKind;
    switch (callType) {
        case 1: invKind = DISPATCH_PROPERTYPUT; break;  // VbLet
        case 2: invKind = DISPATCH_METHOD; break;        // VbMethod
        case 3: invKind = DISPATCH_PROPERTYGET; break;   // VbGet
        default: invKind = DISPATCH_METHOD; break;
    }

    // Build DISPPARAMS from args array
    DISPPARAMS dp;
    memset(&dp, 0, sizeof(dp));
    VARIANT* pArgs = NULL;

    if (argc > 0 && args) {
        pArgs = (VARIANT*)CoTaskMemAlloc(argc * sizeof(VARIANT));
        if (pArgs) {
            for (int32_t i = 0; i < argc; i++) {
                memcpy(&pArgs[i], (char*)args + i * sizeof(VARIANT), sizeof(VARIANT));
            }
            dp.cArgs = (UINT)argc;
            dp.rgvarg = pArgs;
            // Reverse args for DISPPARAMS (COM expects right-to-left)
            // For PropertyPut, also set named arg
            if (invKind == DISPATCH_PROPERTYPUT) {
                DISPID putId = DISPID_PROPERTYPUT;
                dp.cNamedArgs = 1;
                dp.rgdispidNamedArgs = &putId;
            }
        }
    }

    // Invoke
    VARIANT retVal;
    VariantInit(&retVal);
    EXCEPINFO excep;
    memset(&excep, 0, sizeof(excep));
    UINT argErr = 0;

    hr = disp->lpVtbl->Invoke(disp, dispid, &IID_NULL, LOCALE_USER_DEFAULT,
                              invKind, &dp, &retVal, &excep, &argErr);

    if (SUCCEEDED(hr)) {
        result.vt = (vb6_vartype)retVal.vt;
        // Copy value from COM VARIANT to vb6_VARIANT
        switch (retVal.vt) {
            case VT_I2: result.iVal = retVal.iVal; break;
            case VT_I4: result.lVal = retVal.lVal; break;
            case VT_R4: result.fltVal = retVal.fltVal; break;
            case VT_R8: result.dblVal = retVal.dblVal; break;
            case VT_BSTR: result.bstrVal = retVal.bstrVal; VariantInit(&retVal); break;
            case VT_DISPATCH: result.pdispVal = retVal.pdispVal; VariantInit(&retVal); break;
            case VT_BOOL: result.boolVal = retVal.boolVal; break;
            case VT_UI1: result.bVal = retVal.bVal; break;
            default: result.lVal = retVal.lVal; break;
        }
    }

    // Cleanup
    if (pArgs) CoTaskMemFree(pArgs);
    VariantClear(&retVal);
    if (excep.bstrSource) SysFreeString(excep.bstrSource);
    if (excep.bstrDescription) SysFreeString(excep.bstrDescription);
    if (excep.bstrHelpFile) SysFreeString(excep.bstrHelpFile);

    return result;
}


// ============================================================
// P18-E: Financial Functions
// ============================================================

// SLN - Straight Line Depreciation
double vb6_SLN(double cost, double salvage, double life) {
    if (life == 0.0) return 0.0;
    return (cost - salvage) / life;
}

// SYD - Sum of Years' Digits Depreciation
double vb6_SYD(double cost, double salvage, double life, double period) {
    if (life == 0.0) return 0.0;
    return (cost - salvage) * (life - period + 1.0) * 2.0 / (life * (life + 1.0));
}

// DDB - Double Declining Balance Depreciation
double vb6_DDB(double cost, double salvage, double life, double period, double factor) {
    if (life == 0.0) return 0.0;
    double bookValue = cost;
    double depreciation = 0.0;
    int pmax = (int)period;
    for (int p = 1; p <= pmax; p++) {
        depreciation = bookValue * factor / life;
        if (bookValue - depreciation < salvage) {
            depreciation = bookValue - salvage;
        }
        if (p == pmax) break;
        bookValue -= depreciation;
        if (bookValue <= salvage) {
            if (p == pmax) break;
            depreciation = 0.0;
            break;
        }
    }
    return depreciation;
}

// FV - Future Value of an annuity
double vb6_FV(double rate, double nper, double pmt, double pv, int32_t type) {
    if (rate == 0.0) {
        return -(pv + pmt * nper);
    }
    double factor = pow(1.0 + rate, nper);
    return -(pv * factor + pmt * (factor - 1.0) / rate * (1.0 + rate * (double)type));
}

// PV - Present Value of an annuity
double vb6_PV(double rate, double nper, double pmt, double fv, int32_t type) {
    if (rate == 0.0) {
        return -(fv + pmt * nper);
    }
    double factor = pow(1.0 + rate, nper);
    return -(fv / factor + pmt * (1.0 - 1.0 / factor) / rate * (1.0 + rate * (double)type));
}

// Pmt - Periodic Payment
double vb6_Pmt(double rate, double nper, double pv, double fv, int32_t type) {
    if (rate == 0.0) {
        if (nper == 0.0) return 0.0;
        return -(pv + fv) / nper;
    }
    double factor = pow(1.0 + rate, nper);
    return -(pv * factor + fv) * rate / ((factor - 1.0) * (1.0 + rate * (double)type));
}

// IPmt - Interest Payment for a specific period
double vb6_IPmt(double rate, double per, double nper, double pv, double fv, int32_t type) {
    (void)nper; (void)fv;
    double pmt = vb6_Pmt(rate, nper, pv, fv, type);
    double n = per - 1.0;
    if (type == 1) n -= 1.0;
    if (n < 0.0) return 0.0;

    double balance;
    if (rate == 0.0) {
        balance = pv + pmt * n;
    } else {
        double factor = pow(1.0 + rate, n);
        balance = pv * factor + pmt * (factor - 1.0) / rate;
    }
    return -balance * rate;
}

// PPmt - Principal Payment for a specific period
double vb6_PPmt(double rate, double per, double nper, double pv, double fv, int32_t type) {
    double pmt = vb6_Pmt(rate, nper, pv, fv, type);
    double ipmt = vb6_IPmt(rate, per, nper, pv, fv, type);
    return pmt - ipmt;
}

// RATE - Interest rate per period (Newton's method iteration)
double vb6_RATE(double nper, double pmt, double pv, double fv, int32_t type, double guess) {
    double rate = guess;
    if (rate == 0.0) rate = 0.1;

    for (int iter = 0; iter < 100; iter++) {
        double factor = pow(1.0 + rate, nper);
        double f;
        if (rate == 0.0) {
            f = pv + pmt * nper * (1.0 + rate * (double)type) + fv;
        } else {
            f = pv * factor + pmt * (1.0 + rate * (double)type) * (factor - 1.0) / rate + fv;
        }

        double delta = rate * 0.0001;
        if (delta < 1e-10) delta = 1e-10;
        double rate2 = rate + delta;
        double factor2 = pow(1.0 + rate2, nper);
        double f2;
        if (rate2 == 0.0) {
            f2 = pv + pmt * nper * (1.0 + rate2 * (double)type) + fv;
        } else {
            f2 = pv * factor2 + pmt * (1.0 + rate2 * (double)type) * (factor2 - 1.0) / rate2 + fv;
        }
        double fp = (f2 - f) / delta;

        if (fabs(fp) < 1e-15) break;

        double newRate = rate - f / fp;
        if (fabs(newRate - rate) < 1e-10) {
            rate = newRate;
            break;
        }
        rate = newRate;
    }
    return rate;
}

// NPV - Net Present Value
double vb6_NPV(double rate, struct vb6_SafeArray1D* values) {
    if (!values || !values->data || values->count <= 0) return 0.0;
    double npv = 0.0;

    for (int32_t i = 0; i < values->count; i++) {
        double val = 0.0;
        switch (values->elemType) {
            case 6: /* vb6_sa_double */
                val = ((double*)values->data)[i];
                break;
            case 5: /* vb6_sa_single */
                val = (double)((float*)values->data)[i];
                break;
            case 4: /* vb6_sa_long */
                val = (double)((int32_t*)values->data)[i];
                break;
            case 3: /* vb6_sa_int */
                val = (double)((int16_t*)values->data)[i];
                break;
            case 2: /* vb6_sa_byte */
                val = (double)((uint8_t*)values->data)[i];
                break;
            case 8: /* vb6_sa_variant */
                val = vb6_VariantToDouble(((vb6_VARIANT*)values->data)[i]);
                break;
            default:
                val = 0.0;
                break;
        }
        npv += val / pow(1.0 + rate, (double)(i + 1));
    }
    return npv;
}

// ============================================================
// P18-E: File Locking
// ============================================================

void vb6_Lock(int32_t filenum, int64_t start, int64_t end) {
#ifdef _WIN32
    if (filenum < 1 || filenum >= VB6_MAX_FILES) return;
    FILE* f = vb6_file_table[filenum];
    if (!f) return;

    intptr_t osfhandle = _get_osfhandle(_fileno(f));
    if (osfhandle == -1) return;
    HANDLE h = (HANDLE)osfhandle;

    DWORD64 offset, length;
    if (start <= 0 && end <= 0) {
        offset = 0;
        length = 0x7FFFFFFF;
    } else if (end <= 0) {
        offset = (DWORD64)start;
        length = 0x7FFFFFFF;
    } else {
        offset = (DWORD64)start;
        length = (DWORD64)(end - start + 1);
    }

    OVERLAPPED ov;
    ZeroMemory(&ov, sizeof(ov));
    ov.Offset = (DWORD)offset;
    ov.OffsetHigh = (DWORD)(offset >> 32);
    LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK, 0, (DWORD)length, (DWORD)(length >> 32), &ov);
#else
    (void)filenum; (void)start; (void)end;
#endif
}

void vb6_Unlock(int32_t filenum, int64_t start, int64_t end) {
#ifdef _WIN32
    if (filenum < 1 || filenum >= VB6_MAX_FILES) return;
    FILE* f = vb6_file_table[filenum];
    if (!f) return;

    intptr_t osfhandle = _get_osfhandle(_fileno(f));
    if (osfhandle == -1) return;
    HANDLE h = (HANDLE)osfhandle;

    DWORD64 offset, length;
    if (start <= 0 && end <= 0) {
        offset = 0;
        length = 0x7FFFFFFF;
    } else if (end <= 0) {
        offset = (DWORD64)start;
        length = 0x7FFFFFFF;
    } else {
        offset = (DWORD64)start;
        length = (DWORD64)(end - start + 1);
    }

    OVERLAPPED ov;
    ZeroMemory(&ov, sizeof(ov));
    ov.Offset = (DWORD)offset;
    ov.OffsetHigh = (DWORD)(offset >> 32);
    UnlockFileEx(h, 0, (DWORD)length, (DWORD)(length >> 32), &ov);
#else
    (void)filenum; (void)start; (void)end;
#endif
}

void vb6_Reset(void) {
    vb6_CloseAll();
}

// ============================================================
// P18-E: Partition Function
// ============================================================

BSTR vb6_Partition(int64_t number, int64_t start, int64_t stop, int64_t interval) {
    int64_t rangeStart = 0, rangeEnd = 0;
    int leftBlank = 0, rightBlank = 0;

    if (number < start) {
        leftBlank = 1;
        rangeEnd = start - 1;
    } else if (number > stop) {
        rightBlank = 1;
        rangeStart = stop + 1;
    } else {
        int64_t idx = (number - start) / interval;
        rangeStart = start + idx * interval;
        rangeEnd = rangeStart + interval - 1;
        if (rangeEnd > stop) rangeEnd = stop;
    }

    wchar_t buf[64];
    if (leftBlank) {
        swprintf(buf, 64, L"%10ls: %10lld", L"", (long long)rangeEnd);
    } else if (rightBlank) {
        swprintf(buf, 64, L"%10lld: %10ls", (long long)rangeStart, L"");
    } else {
        swprintf(buf, 64, L"%10lld: %10lld", (long long)rangeStart, (long long)rangeEnd);
    }
    return vb6_BSTR_FromStr(buf);
}

// ============================================================
// P20-02: IEnumVARIANT support for COM For Each
// ============================================================

void* vb6_ComNewEnum(void* disp) {
    if (!disp) return NULL;
    IDispatch* pDisp = (IDispatch*)disp;

    // _NewEnum has DISPID = -4 (DISPID_NEWENUM)
    DISPID dispidNewEnum = -4;
    OLECHAR* wszNewEnum = L"_NewEnum";
    HRESULT hr = pDisp->lpVtbl->GetIDsOfNames(pDisp, &IID_NULL, &wszNewEnum, 1,
                                                LOCALE_USER_DEFAULT, &dispidNewEnum);
    if (FAILED(hr)) {
        // Try known DISPID directly
        dispidNewEnum = -4;
    }

    DISPPARAMS dp = {0};
    VARIANT result;
    VariantInit(&result);
    EXCEPINFO exInfo = {0};

    hr = pDisp->lpVtbl->Invoke(pDisp, dispidNewEnum, &IID_NULL, LOCALE_USER_DEFAULT,
                                DISPATCH_PROPERTYGET | DISPATCH_METHOD,
                                &dp, &result, &exInfo, NULL);
    if (FAILED(hr)) return NULL;

    // QI for IEnumVARIANT
    IEnumVARIANT* pEnum = NULL;
    if (result.vt == VT_UNKNOWN) {
        hr = result.punkVal->lpVtbl->QueryInterface(result.punkVal, &IID_IEnumVARIANT, (void**)&pEnum);
        result.punkVal->lpVtbl->Release(result.punkVal);
    } else if (result.vt == VT_DISPATCH) {
        hr = result.pdispVal->lpVtbl->QueryInterface(result.pdispVal, &IID_IEnumVARIANT, (void**)&pEnum);
        result.pdispVal->lpVtbl->Release(result.pdispVal);
    } else {
        VariantClear(&result);
        return NULL;
    }
    return (void*)pEnum;
}

int vb6_EnumNext(void* penum, vb6_VARIANT* outElem) {
    if (!penum || !outElem) return 0;
    IEnumVARIANT* pEnum = (IEnumVARIANT*)penum;

    VARIANT varElem;
    VariantInit(&varElem);
    ULONG fetched = 0;
    HRESULT hr = pEnum->lpVtbl->Next(pEnum, 1, &varElem, &fetched);

    if (hr != S_OK || fetched == 0) {
        VariantClear(&varElem);
        return 0;
    }

    // Convert COM VARIANT to vb6_VARIANT
    memset(outElem, 0, sizeof(vb6_VARIANT));
    switch (varElem.vt) {
        case VT_EMPTY:  outElem->vt = vb6_vtEmpty; break;
        case VT_NULL:   outElem->vt = vb6_vtNull; break;
        case VT_I2:     outElem->vt = vb6_vtInteger; outElem->iVal = varElem.iVal; break;
        case VT_I4:     outElem->vt = vb6_vtLong; outElem->lVal = varElem.lVal; break;
        case VT_R4:     outElem->vt = vb6_vtSingle; outElem->fltVal = varElem.fltVal; break;
        case VT_R8:     outElem->vt = vb6_vtDouble; outElem->dblVal = varElem.dblVal; break;
        case VT_BSTR:   outElem->vt = vb6_vtBSTR; outElem->bstrVal = varElem.bstrVal; break;
        case VT_BOOL:   outElem->vt = vb6_vtBoolean; outElem->boolVal = varElem.boolVal ? -1 : 0; VariantClear(&varElem); break;
        case VT_DISPATCH: outElem->vt = vb6_vtDispatch; outElem->pdispVal = varElem.pdispVal; break;
        case VT_DATE:   outElem->vt = vb6_vtDate; outElem->dblVal = varElem.date; VariantClear(&varElem); break;
        default: {
            // Convert to BSTR for unknown types
            VariantChangeType(&varElem, &varElem, 0, VT_BSTR);
            if (varElem.vt == VT_BSTR) {
                outElem->vt = vb6_vtBSTR;
                outElem->bstrVal = varElem.bstrVal;
            } else {
                VariantClear(&varElem);
            }
            return 1;
        }
    }
    // Cleanup for types we didn't transfer ownership of
    if (varElem.vt != VT_BSTR && varElem.vt != VT_DISPATCH && varElem.vt != VT_UNKNOWN) {
        VariantClear(&varElem);
    }
    return 1;
}

void vb6_EnumRelease(void* penum) {
    if (!penum) return;
    IEnumVARIANT* pEnum = (IEnumVARIANT*)penum;
    pEnum->lpVtbl->Release(pEnum);
}

// ============================================================
// P20-37: Registry functions (VB6: SaveSetting/GetSetting/DeleteSetting/GetAllSettings)
// VB6 registry path: HKEY_CURRENT_USER\Software\VB and VBA Program Settings\
// ============================================================

static BSTR vb6_RegBuildKey(BSTR appName, BSTR section) {
    const wchar_t* base = L"Software\\VB and VBA Program Settings\\";
    int baseLen = (int)wcslen(base);
    int appLen = appName ? (int)wcslen(appName) : 0;
    int secLen = section ? (int)wcslen(section) : 0;
    int totalLen = baseLen + appLen + 1 + secLen + 1;
    wchar_t* buf = (wchar_t*)calloc(totalLen + 1, sizeof(wchar_t));
    if (!buf) return vb6_BSTR_Empty();
    memcpy(buf, base, baseLen * sizeof(wchar_t));
    if (appName) memcpy(buf + baseLen, appName, appLen * sizeof(wchar_t));
    buf[baseLen + appLen] = L'\\';
    if (section) memcpy(buf + baseLen + appLen + 1, section, secLen * sizeof(wchar_t));
    buf[baseLen + appLen + 1 + secLen] = L'\0';
    BSTR result = SysAllocString(buf);
    free(buf);
    return result;
}

void vb6_SaveSetting(BSTR appName, BSTR section, BSTR key, BSTR setting) {
    if (!appName || !section || !key) return;
    BSTR keyPath = vb6_RegBuildKey(appName, section);
    HKEY hKey;
    DWORD disp;
    LONG rc = RegCreateKeyExW(HKEY_CURRENT_USER, keyPath, 0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, &disp);
    SysFreeString(keyPath);
    if (rc != ERROR_SUCCESS) return;
    if (setting) {
        RegSetValueExW(hKey, key, 0, REG_SZ, (const BYTE*)setting, (DWORD)(wcslen(setting) + 1) * sizeof(wchar_t));
    } else {
        RegSetValueExW(hKey, key, 0, REG_SZ, (const BYTE*)L"", sizeof(wchar_t));
    }
    RegCloseKey(hKey);
}

BSTR vb6_GetSetting(BSTR appName, BSTR section, BSTR key, BSTR default_) {
    if (!appName || !section || !key) return default_ ? SysAllocString(default_) : vb6_BSTR_Empty();
    BSTR keyPath = vb6_RegBuildKey(appName, section);
    HKEY hKey;
    LONG rc = RegOpenKeyExW(HKEY_CURRENT_USER, keyPath, 0, KEY_QUERY_VALUE, &hKey);
    SysFreeString(keyPath);
    if (rc != ERROR_SUCCESS) return default_ ? SysAllocString(default_) : vb6_BSTR_Empty();
    
    DWORD dataSize = 0;
    rc = RegQueryValueExW(hKey, key, NULL, NULL, NULL, &dataSize);
    if (rc != ERROR_SUCCESS || dataSize == 0) {
        RegCloseKey(hKey);
        return default_ ? SysAllocString(default_) : vb6_BSTR_Empty();
    }
    
    wchar_t* buf = (wchar_t*)calloc(dataSize + sizeof(wchar_t), 1);
    if (!buf) {
        RegCloseKey(hKey);
        return default_ ? SysAllocString(default_) : vb6_BSTR_Empty();
    }
    rc = RegQueryValueExW(hKey, key, NULL, NULL, (LPBYTE)buf, &dataSize);
    RegCloseKey(hKey);
    if (rc != ERROR_SUCCESS) {
        free(buf);
        return default_ ? SysAllocString(default_) : vb6_BSTR_Empty();
    }
    BSTR result = SysAllocString(buf);
    free(buf);
    return result;
}

void vb6_DeleteSetting(BSTR appName, BSTR section, BSTR key) {
    if (!appName || !section) return;
    BSTR keyPath = vb6_RegBuildKey(appName, section);
    if (key && wcslen(key) > 0) {
        HKEY hKey;
        LONG rc = RegOpenKeyExW(HKEY_CURRENT_USER, keyPath, 0, KEY_SET_VALUE, &hKey);
        SysFreeString(keyPath);
        if (rc == ERROR_SUCCESS) {
            RegDeleteValueW(hKey, key);
            RegCloseKey(hKey);
        }
    } else {
        RegDeleteKeyW(HKEY_CURRENT_USER, keyPath);
        SysFreeString(keyPath);
    }
}

vb6_VARIANT vb6_GetAllSettings(BSTR appName, BSTR section) {
    vb6_VARIANT result;
    memset(&result, 0, sizeof(result));
    /* VB6 returns a 2D Variant array, but we simplify to 1D with alternating key/value */
    result.vt = (vb6_vtVariant); /* simplified: not true 2D array */
    
    if (!appName || !section) return result;
    
    BSTR keyPath = vb6_RegBuildKey(appName, section);
    HKEY hKey;
    LONG rc = RegOpenKeyExW(HKEY_CURRENT_USER, keyPath, 0, KEY_QUERY_VALUE, &hKey);
    SysFreeString(keyPath);
    if (rc != ERROR_SUCCESS) return result;
    
    DWORD numValues = 0;
    DWORD maxNameLen = 0;
    DWORD maxDataLen = 0;
    RegQueryInfoKeyW(hKey, NULL, NULL, NULL, NULL, NULL, NULL, &numValues, &maxNameLen, &maxDataLen, NULL, NULL);
    
    if (numValues == 0) {
        RegCloseKey(hKey);
        return result;
    }
    
    /* Create 1D variant array with numValues*2 elements (key, value pairs) */
    struct vb6_SafeArray1D* sa = vb6_SafeArrayCreate1D(vb6_sa_variant, 0, (int32_t)(numValues * 2) - 1);
    if (!sa) {
        RegCloseKey(hKey);
        return result;
    }
    
    wchar_t* nameBuf = (wchar_t*)calloc(maxNameLen + 2, sizeof(wchar_t));
    BYTE* dataBuf = (BYTE*)calloc(maxDataLen + sizeof(wchar_t), 1);
    if (!nameBuf || !dataBuf) {
        free(nameBuf);
        free(dataBuf);
        vb6_SafeArrayDestroy1D(sa);
        RegCloseKey(hKey);
        return result;
    }
    
    for (DWORD i = 0; i < numValues; i++) {
        DWORD nameSize = maxNameLen + 2;
        DWORD dataSize = maxDataLen + sizeof(wchar_t);
        DWORD type;
        RegEnumValueW(hKey, i, nameBuf, &nameSize, NULL, &type, dataBuf, &dataSize);
        
        /* Key (name) - index i*2 */
        vb6_VARIANT varKey;
        memset(&varKey, 0, sizeof(varKey));
        varKey.vt = vb6_vtBSTR;
        varKey.bstrVal = SysAllocString(nameBuf);
        vb6_ArraySetVariant(sa, (int32_t)(i * 2), varKey);
        
        /* Value - index i*2+1 */
        vb6_VARIANT varVal;
        memset(&varVal, 0, sizeof(varVal));
        varVal.vt = vb6_vtBSTR;
        if (type == REG_SZ && dataSize >= sizeof(wchar_t)) {
            varVal.bstrVal = SysAllocString((wchar_t*)dataBuf);
        } else {
            varVal.bstrVal = vb6_BSTR_Empty();
        }
        vb6_ArraySetVariant(sa, (int32_t)(i * 2 + 1), varVal);
    }
    
    free(nameBuf);
    free(dataBuf);
    RegCloseKey(hKey);
    
    result.parray = sa;
    return result;
}

// P21-18: LoadPictureEx - OleLoadPicturePath for all image types
void* vb6_LoadPictureEx(BSTR pathname) {
    if (!pathname) return NULL;
    IPicture* pPicture = NULL;
    HRESULT hr = OleLoadPicturePath(pathname, NULL, 0, 0, &IID_IPicture, (void**)&pPicture);
    if (FAILED(hr)) return NULL;
    OLE_HANDLE hHandle = 0;
    pPicture->lpVtbl->get_Handle(pPicture, &hHandle);
    /* Note: we release IPicture but the HBITMAP handle remains valid while
       the picture is kept by GDI. For a more correct implementation we'd
       DuplicateHandle, but this matches VB6's LoadPicture behavior well enough. */
    pPicture->lpVtbl->Release(pPicture);
    return (void*)(intptr_t)hHandle;
}

// ============================================================
// COM调用结果→vb6_VARIANT转换
// 将后期绑定COM调用的VARIANT*结果转为vb6_VARIANT并释放原指针
// 用于: v = dic.Item("hello") 等场景
// 此函数消耗VARIANT*所有权, 调用后原指针不可再使用
// ============================================================
vb6_VARIANT vb6_VariantFromComResult(void* variant_ptr) {
    vb6_VARIANT result;
    memset(&result, 0, sizeof(result));
    if (!variant_ptr) { result.vt = vb6_vtEmpty; return result; }
    VARIANT* pv = (VARIANT*)variant_ptr;
    // Windows VARENUM 与 vb6_vartype 值一致, 可直接赋值
    result.vt = (vb6_vartype)pv->vt;
    switch (pv->vt) {
        case VT_EMPTY: break;
        case VT_NULL:  break;
        case VT_I2:    result.iVal = pv->iVal; break;
        case VT_I4:    result.lVal = pv->lVal; break;
        case VT_R4:    result.fltVal = pv->fltVal; break;
        case VT_R8:    result.dblVal = pv->dblVal; break;
        case VT_CY:    result.cyVal = *(int64_t*)&pv->cyVal; break;
        case VT_DATE:  result.dblVal = pv->date; break;
        case VT_BSTR:
            result.bstrVal = pv->bstrVal;
            pv->bstrVal = NULL;  // 转移所有权, 防止VariantClear释放
            break;
        case VT_DISPATCH:
            result.pdispVal = pv->pdispVal;
            pv->pdispVal = NULL;  // 转移所有权
            break;
        case VT_BOOL:   result.boolVal = pv->boolVal; break;
        case VT_UI1:    result.bVal = pv->bVal; break;
        case VT_ERROR:  result.lVal = pv->scode; break;
        case VT_DECIMAL:
            memcpy(&result.decVal, &pv->decVal, sizeof(result.decVal));
            break;
        default:
            // 未知类型: 尝试转换为BSTR
            {
                VARIANT vBSTR;
                VariantInit(&vBSTR);
                if (SUCCEEDED(VariantChangeType(&vBSTR, pv, 0, VT_BSTR))) {
                    result.vt = vb6_vtBSTR;
                    result.bstrVal = vBSTR.bstrVal;
                    vBSTR.bstrVal = NULL;
                } else {
                    result.vt = vb6_vtEmpty;
                }
                VariantClear(&vBSTR);
            }
            break;
    }
    VariantClear(pv);  // 清理原始VARIANT (BSTR/pdispVal已转移)
    free(pv);
    return result;
}
