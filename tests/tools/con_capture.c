/* con_capture.c - 把一条命令行跑在**真控制台**里, 再把它的屏幕缓冲读回来按 UTF-8 打到 stdout。


 *


 * 为什么需要它: 重定向(stdout 落盘/进管道)只能量到**字节**, 量不到"在 cmd 里到底显示成什么" ——


 * 控制台那条路是另一支 (CRT/程序按句柄类型选 WriteConsoleW 还是 WriteConsoleA + 代码页),


 * 与重定向完全不是同一条。做法:


 *   ① 子进程用 CREATE_NEW_CONSOLE 起 (它有自己的控制台, 代码页 = 系统 ANSI, 中文机 936);


 *   ② 本进程 FreeConsole() 后重试 AttachConsole(子pid) —— 子进程跑起来才有控制台,


 *      快进程会抢在挂载前退出, 那时用 -hold 让命令行尾巴挂一会儿 (见下);


 *   ③ 挂上后每 100ms 把整屏读一遍, 直到子进程退出; 退出后我仍挂着, 缓冲不丢。


 *


 * 用法: con_capture [-hold] <exe> [args...]


 *   -hold : 用 `cmd /c "<exe> [args] & ping -n 4 127.0.0.1 >nul"` 起 (≈3s), 给挂载留时间;


 *           目标进程太快时用它。屏幕内容只多一行 cmd 的提示行, 读数按 LINE| 逐行给出。


 * 环境: CON_CAPTURE_CP 覆盖控制台输出代码页 (默认 936 = 中文 cmd 现场)。


 * 输出: CONSOLE_CP=<n> / LINE|<utf8 文本> / EXIT=<code>


 */


#define WIN32_LEAN_AND_MEAN


#include <windows.h>


#include <stdio.h>


#include <string.h>


#include <stdlib.h>





static HANDLE g_out;              /* 本进程**原始** stdout, 挂到别人控制台后要用它回写 */


static void say(const char *utf8) {


    DWORD n = 0;


    if (g_out && g_out != INVALID_HANDLE_VALUE) WriteFile(g_out, utf8, (DWORD)strlen(utf8), &n, NULL);


}





int main(int argc, char **argv) {


    STARTUPINFOW si;


    PROCESS_INFORMATION pi;


    wchar_t cmdline[4096];


    wchar_t inner[3584];


    HANDLE hCon = INVALID_HANDLE_VALUE;


    CONSOLE_SCREEN_BUFFER_INFO csbi;


    UINT cp = 936;


    DWORD exitCode = 0;


    int row, rows, i, attempts;


    int hold = 0;


    int nowin = 0;


    DWORD createFlags;


    char *cpEnv;


    char buf[4096];





    g_out = GetStdHandle(STD_OUTPUT_HANDLE);


    setvbuf(stdout, NULL, _IONBF, 0);





    i = 1;


    for (;;) {   /* 开关可任意顺序、任意组合 */
        if (i < argc && strcmp(argv[i], "-hold") == 0) { hold = 1; i++; continue; }
        if (i < argc && strcmp(argv[i], "-nowin") == 0) { nowin = 1; i++; continue; }
        break;
    }


    /* -nowin: 子进程用 CREATE_NO_WINDOW —— 它有控制台(std 句柄可用)但没有窗口,


     * 于是 GetConsoleWindow() 返回 NULL, 正是 Windows Terminal/ConPTY 下"代码页守卫


     * 判不出来"的那种现场 */




    if (i >= argc) { say("USAGE=[-hold] [-nowin] <exe> [args...]\n"); return 2; }


    cpEnv = getenv("CON_CAPTURE_CP");


    if (cpEnv && cpEnv[0]) cp = (UINT)atoi(cpEnv);





    inner[0] = 0;


    MultiByteToWideChar(CP_ACP, 0, argv[i], -1, inner, 3584);


    for (i = i + 1; i < argc; i++) {


        wchar_t one[1024];


        MultiByteToWideChar(CP_ACP, 0, argv[i], -1, one, 1024);


        if (wcscat_s(inner, 3584, L" ") != 0) break;


        if (wcscat_s(inner, 3584, one) != 0) break;


    }


    if (hold) {


        wcscpy_s(cmdline, 4096, L"cmd.exe /c \"");


        wcscat_s(cmdline, 4096, inner);


        wcscat_s(cmdline, 4096, L" & ping -n 4 127.0.0.1 >nul\"");


    } else {


        wcscpy_s(cmdline, 4096, inner);


    }





    memset(&si, 0, sizeof(si));


    si.cb = sizeof(si);


    createFlags = nowin ? CREATE_NO_WINDOW : CREATE_NEW_CONSOLE;

    if (nowin) {

        /* 无窗口控制台: 句柄继承即可, 不用挂屏 */

    }

    if (!CreateProcessW(NULL, cmdline, NULL, NULL, TRUE, createFlags, NULL, NULL, &si, &pi)) {


        sprintf(buf, "CREATE=FAIL gle=%lu\n", (unsigned long)GetLastError());


        say(buf);


        return 1;


    }





    FreeConsole();


    for (attempts = 0; attempts < 400; attempts++) {     /* 最多 ~2s */


        if (AttachConsole(pi.dwProcessId)) break;


        if (WaitForSingleObject(pi.hProcess, 5) != WAIT_TIMEOUT) break;   /* 子进程已退出 */


    }


    if (attempts >= 400 || !GetConsoleWindow()) {


        sprintf(buf, "ATTACH=FAIL gle=%lu attempts=%d\n", (unsigned long)GetLastError(), attempts);


        say(buf);


        TerminateProcess(pi.hProcess, 1);


        return 1;


    }


    SetConsoleOutputCP(cp);


    sprintf(buf, "CONSOLE_CP=%u\n", (unsigned)GetConsoleOutputCP());


    say(buf);





    hCon = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,


                       NULL, OPEN_EXISTING, 0, NULL);


    if (hCon == INVALID_HANDLE_VALUE) {


        sprintf(buf, "CONOUT=FAIL gle=%lu\n", (unsigned long)GetLastError());


        say(buf);


        TerminateProcess(pi.hProcess, 1);


        return 1;


    }





    /* 轮询读屏直到子进程退出 (我挂着, 它的控制台不会随它消失) */


    for (;;) {


        if (WaitForSingleObject(pi.hProcess, 100) != WAIT_TIMEOUT) break;


    }


    GetExitCodeProcess(pi.hProcess, &exitCode);


    CloseHandle(pi.hThread);


    CloseHandle(pi.hProcess);





    if (!GetConsoleScreenBufferInfo(hCon, &csbi)) {


        say("CSBI=FAIL\n");


        return 1;


    }


    rows = csbi.dwSize.Y;


    for (row = 0; row < rows; row++) {


        wchar_t line[1024];


        COORD c;


        DWORD got = 0;


        int end;


        char utf8[4200];


        char out[4300];


        c.X = 0;


        c.Y = (SHORT)row;


        if (!ReadConsoleOutputCharacterW(hCon, line, 1023, c, &got) || got == 0) continue;


        line[got] = 0;


        end = (int)wcslen(line);


        while (end > 0 && (line[end - 1] == L' ' || line[end - 1] == L'\t')) end--;


        line[end] = 0;


        if (end == 0) continue;


        if (WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8, 4200, NULL, NULL) <= 0) continue;


        sprintf(out, "LINE|%s\n", utf8);


        say(out);


    }


    sprintf(buf, "EXIT=%lu\n", (unsigned long)exitCode);


    say(buf);


    CloseHandle(hCon);


    return 0;


}


