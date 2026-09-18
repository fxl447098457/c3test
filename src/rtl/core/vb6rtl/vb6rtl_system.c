// vb6rtl_system.c - VB6 运行时库: 系统对象家族：Dir/CurDir/Shell/Environ/Command/FormatDateTime + App/Clipboard/Screen/Printer/Forms
// 2026-09-17 从 src/rtl/core/vb6rtl/vb6rtl.c 按家族拆出（纯搬移，逐行未改）:
//   原第 1790~1950 行
//   原第 1951~1993 行
//   原第 1994~2065 行
//   原第 2066~2133 行
//   原第 2134~2208 行
//   原第 2209~2236 行

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

/* App.HelpFile: 编译产物无 App COM 对象 → 返回空帮助文件名 (VB6默认同EXE名.hlp,
 * 语义上仅作错误/事件参数传递, 空串可编译且运行等价于无帮助文件) */
BSTR vb6_App_HelpFile(void) {
    return vb6_BSTR_FromStr(L"");
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

// Fix 056: Clipboard.SetData — copy IPicture bitmap to clipboard
void vb6_Clipboard_SetData(void* pPicture) {
    if (!pPicture) return;
    if (!OpenClipboard(NULL)) return;
    EmptyClipboard();
    // Try to get bitmap handle from IPicture
    HBITMAP hBmp = NULL;
    IPicture* pPic = (IPicture*)pPicture;
    HANDLE hPal = NULL;
    OLE_HANDLE hOleHandle = 0;
    pPic->lpVtbl->get_Handle(pPic, &hOleHandle);
    hBmp = (HBITMAP)(uintptr_t)hOleHandle;
    if (hBmp) {
        SetClipboardData(CF_BITMAP, hBmp);
    }
    CloseClipboard();
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
    /* Fix 112: 窗体 HWND 也是宿主对象 (Me.ScaleWidth / Me.Controls / Me.hwnd) */
    vb6_HostObj_Register(hwnd_, NULL, "Form", 1, -1);
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


