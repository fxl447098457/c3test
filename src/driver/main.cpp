#include "driver/driver.hpp"
#include "common/encoding.hpp"
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>  // CommandLineToArgvW
#include <dbghelp.h>
#include <cstdio>
#include <cstdlib>

// ---------------------------------------------------------------------------
// 崩溃栈追踪 (诊断用)
//
// C3 在特定输入下会自身崩溃 (0xC0000005), 而 ASAN 构建不崩, 无法用 ASAN report
// 定位. 这里提供一个基于 dbghelp 的进程内崩溃栈打印:
//
//   set C3_CRASH_TRACE=1
//   C3.exe <vbp> ...        → 崩溃时 stderr 输出异常码/地址 + 符号化调用栈
//
// 未设置该环境变量时不会安装任何处理器, 行为与原先完全一致.
// 注意: 需要带 PDB 的构建 (RelWithDebInfo / Debug) 才能解析出 函数+文件:行.
// ---------------------------------------------------------------------------
namespace {

LONG WINAPI c3CrashHandler(EXCEPTION_POINTERS* ep) {
    const DWORD code = ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0;
    std::fprintf(stderr, "\n[C3-CRASH] exception=0x%08lX address=%p\n",
                 (unsigned long)code,
                 ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionAddress : nullptr);

    HANDLE proc = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    if (SymInitialize(proc, nullptr, TRUE)) {
        void* frames[64];
        USHORT n = CaptureStackBackTrace(0, 64, frames, nullptr);
        char symBuf[sizeof(SYMBOL_INFO) + 512];
        for (USHORT i = 0; i < n; ++i) {
            SYMBOL_INFO* si = reinterpret_cast<SYMBOL_INFO*>(symBuf);
            si->SizeOfStruct = sizeof(SYMBOL_INFO);
            si->MaxNameLen = 512;
            DWORD64 symDisp = 0;
            IMAGEHLP_LINE64 line{};
            line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
            DWORD lineDisp = 0;
            const BOOL hasSym = SymFromAddr(proc, reinterpret_cast<DWORD64>(frames[i]), &symDisp, si);
            const BOOL hasLine = SymGetLineFromAddr64(proc, reinterpret_cast<DWORD64>(frames[i]), &lineDisp, &line);
            if (hasSym && hasLine) {
                std::fprintf(stderr, "  #%02u %s+0x%llX  (%s:%lu)\n", (unsigned)i, si->Name,
                             (unsigned long long)symDisp, line.FileName, (unsigned long)line.LineNumber);
            } else if (hasSym) {
                std::fprintf(stderr, "  #%02u %s+0x%llX\n", (unsigned)i, si->Name,
                             (unsigned long long)symDisp);
            } else {
                std::fprintf(stderr, "  #%02u %p\n", (unsigned)i, frames[i]);
            }
        }
        SymCleanup(proc);
    }
    std::fflush(stderr);
    TerminateProcess(GetCurrentProcess(), static_cast<UINT>(code));
    return EXCEPTION_EXECUTE_HANDLER;
}

void installCrashTraceIfRequested() {
    char buf[8] = {0};
    if (GetEnvironmentVariableA("C3_CRASH_TRACE", buf, sizeof(buf)) > 0 && buf[0] != '0') {
        SetUnhandledExceptionFilter(c3CrashHandler);
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// 控制台代码页守卫
//
// C3 内部统一 UTF-8 (源文件读入即转 UTF-8, /utf-8 编译, argv 转 UTF-8),
// std::cout 输出的是 UTF-8 字节, 而中文 cmd 默认代码页 936(GBK) 会显示乱码。
// 进入时把控制台输入/输出代码页切到 UTF-8, 退出时恢复原值:
//   1. 不需要用户手动 chcp 65001;
//   2. 不污染用户会话 —— C3 退出后 cmd 恢复 GBK, 之后在同一 cmd 运行
//      C3 编译出的 exe (vb6rtl 按 ACP 输出) 仍正常显示。
// 输出重定向到文件/管道时写入的始终是原始 UTF-8 字节, 不受影响。
// ---------------------------------------------------------------------------
class ConsoleCodePageGuard {
public:
    ConsoleCodePageGuard() {
        if (GetConsoleWindow() != nullptr) {  // 仅当实际附加到控制台时才切换
            oldOut_ = GetConsoleOutputCP();
            oldIn_ = GetConsoleCP();
            SetConsoleOutputCP(CP_UTF8);
            SetConsoleCP(CP_UTF8);
        }
    }
    ~ConsoleCodePageGuard() {
        if (oldOut_ != 0) SetConsoleOutputCP(oldOut_);
        if (oldIn_ != 0) SetConsoleCP(oldIn_);
    }
    ConsoleCodePageGuard(const ConsoleCodePageGuard&) = delete;
    ConsoleCodePageGuard& operator=(const ConsoleCodePageGuard&) = delete;

private:
    UINT oldOut_ = 0;
    UINT oldIn_ = 0;
};
#endif

namespace {

int runCompile(vb6c3::Driver& driver, int argc, char* argv[]) {
    auto result = driver.compile(argc, argv);

    // success=false 且 errorCount=0 = 编译未成功却无错误计数 (如 GUI 工程链接未产出
    // exe) — 必须判为失败. -h/--help/--version 走的是 compile() 里 success=true 的
    // "正常退出" 分支, 不会落到这里, 因此脚本里 c3 --version 退出码为 0.
    if (!result.success && result.errorCount == 0) {
        return 1;
    }

    return result.errorCount > 0 ? 1 : 0;
}

}  // namespace

int main(int argc, char* argv[]) {
#ifdef _WIN32
    installCrashTraceIfRequested();
    ConsoleCodePageGuard consoleCpGuard;  // 控制台切 UTF-8, 退出时恢复
#endif

    vb6c3::Driver driver;

#ifdef _WIN32
    // Windows 上 main() 的 argv 是 ACP(中文系统为GBK) 编码, 而 C3 内部统一按 UTF-8
    // 处理字符串。直接把 argv 交给 utf8ToPath() 会把 GBK 字节当作 UTF-8 解析,
    // 中文路径因此损坏 -> 文件打不开 -> 报 ".vbp文件中没有源文件"。
    // 这里改用 Unicode 命令行重新取参, 统一转成 UTF-8。
    int wargc = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (wargv && wargc > 0) {
        std::vector<std::string> utf8Args;
        std::vector<char*> argvUtf8;
        utf8Args.reserve(wargc);
        argvUtf8.reserve(wargc);
        for (int i = 0; i < wargc; ++i) {
            utf8Args.push_back(vb6c3::wideToUtf8(wargv[i]));
        }
        for (auto& s : utf8Args) {
            argvUtf8.push_back(&s[0]);
        }
        LocalFree(wargv);
        return runCompile(driver, wargc, argvUtf8.data());
    }
    if (wargv) LocalFree(wargv);
#endif

    return runCompile(driver, argc, argv);
}
