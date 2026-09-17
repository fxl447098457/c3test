// vb6rtl_file.c - VB6 运行时库: 文件家族：文件 I/O 通道表 + 目录/文件操作 + Get/Put + 文件锁
// 2026-09-17 从 src/rtl/core/vb6rtl/vb6rtl.c 按家族拆出（纯搬移，逐行未改）:
//   原第 3618~3918 行
//   原第 4115~4181 行
//   原第 4595~4666 行

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
        case 1: modeStr = "rb"; break;   // Input (binary: VB6 uses CRLF explicitly)
        case 2: modeStr = "wb"; break;   // Output
        case 4: modeStr = "r+b"; break; // Random
        case 8: modeStr = "ab"; break;   // Append
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
            if (ch == '\r' || ch == '\n') {
                // VB6: string content CR/LF written as-is (binary mode, no text-mode conversion)
                fputc(ch, f);
                vb6_col_table[filenumber] = 0;
            } else {
                if (w > 0 && vb6_col_table[filenumber] >= w) {
                    fputc('\r', f);
                    fputc('\n', f);
                    vb6_col_table[filenumber] = 0;
                }
                fputc(ch, f);
                vb6_col_table[filenumber]++;
            }
        }
    }
    // VB6 Print always terminates with CRLF
    fputc('\r', f);
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

