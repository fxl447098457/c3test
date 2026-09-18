#include "backend/msvc_driver.hpp"
#include "common/encoding.hpp"

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <filesystem>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#endif

namespace vb6c3 {

MsvcDriver::MsvcDriver() {}
MsvcDriver::~MsvcDriver() = default;

// 以隐藏窗口执行命令行, 返回退出码。
// 必须隐藏: C3 常被从**无控制台**的宿主拉起 (Git Bash/mintty、GUI 启动器),
// 这种父进程下 cmd.exe 子进程会被分配一个新的可见控制台 -> 屏幕上连闪黑框。
int MsvcDriver::executeCommand(const std::string& cmd) {
#ifdef _WIN32
    // M22-IssueB: Use CreateProcessW to pass UTF-16 command line to cmd.exe
    // This preserves Chinese/Unicode characters in file paths (e.g. /Fe"工程1.exe")
    // std::system() converts char* via CRT codepage, which corrupts UTF-8 paths
    
    // Convert UTF-8 command to wide string
    int wlen = MultiByteToWideChar(CP_UTF8, 0, cmd.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return std::system(cmd.c_str());
    std::wstring wcmd(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, cmd.c_str(), -1, &wcmd[0], wlen);
    wcmd.pop_back(); // remove trailing null from MultiByteToWideChar
    
    // Build full command: cmd.exe /c <command>
    std::wstring fullCmd = L"cmd.exe /c " + wcmd;
    
    STARTUPINFOW si = { sizeof(si) };
    // 隐藏窗口: 不设这两项时, 无控制台宿主下的 cmd.exe 会新建可见控制台窗口
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    
    // CreateProcessW requires mutable command line buffer
    std::wstring mutableCmd = fullCmd;
    
    BOOL ok = CreateProcessW(
        nullptr,                // application name (nullptr = use command line)
        &mutableCmd[0],         // command line (mutable)
        nullptr,                // process security
        nullptr,                // thread security
        FALSE,                  // inherit handles
        CREATE_NO_WINDOW,       // 不分配控制台窗口 (见函数头注释)
        nullptr,                // environment
        nullptr,                // current directory
        &si,                    // startup info
        &pi                     // process info
    );
    
    if (!ok) {
        // Fallback to std::system if CreateProcessW fails
        return std::system(cmd.c_str());
    }
    
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return static_cast<int>(exitCode);
#else
    return std::system(cmd.c_str());
#endif
}

// 同 executeCommand, 但把子进程 stdout 收进 out。
// vswhere 这类「跑一下读输出」的探测用它, 替换原来的 _popen (同样会弹 cmd 窗口)。
int MsvcDriver::executeCommandCapture(const std::string& cmd, std::string& out) {
    out.clear();
#ifdef _WIN32
    int wlen = MultiByteToWideChar(CP_UTF8, 0, cmd.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return -1;
    std::wstring wcmd(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, cmd.c_str(), -1, &wcmd[0], wlen);
    wcmd.pop_back();

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return -1;
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;

    PROCESS_INFORMATION pi = {};
    std::wstring mutableCmd = L"cmd.exe /c " + wcmd;
    BOOL ok = CreateProcessW(nullptr, &mutableCmd[0], nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    // 父进程必须关掉写端, 否则子进程退出后 ReadFile 也等不到 EOF
    CloseHandle(hWrite);
    if (!ok) { CloseHandle(hRead); return -1; }

    char buf[512];
    DWORD n = 0;
    while (ReadFile(hRead, buf, sizeof(buf), &n, nullptr) && n > 0) {
        out.append(buf, n);
    }
    CloseHandle(hRead);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD ec = 1;
    GetExitCodeProcess(pi.hProcess, &ec);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(ec);
#else
    FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
    if (!pipe) return -1;
    char buf[512];
    while (fgets(buf, sizeof(buf), pipe)) out += buf;
    return pclose(pipe);
#endif
}


bool MsvcDriver::compileAndLink(const MsvcDriverOptions& options) {
    if (options.sourceFiles.empty()) {
        std::cerr << "C3: 没有源文件需要编译" << std::endl;
        return false;
    }

    // opt3: 增量编译路径 (基于内容哈希的obj级缓存)
    if (options.incremental) {
        return compileAndLinkIncremental(options);
    }

    std::string cl = findClExe();

    // 构建cl.exe命令行
    std::ostringstream cmd;
    cmd << cl;

    // 包含路径: RTL目录
    if (!options.rtlDir.empty()) {
        cmd << " /I\"" << options.rtlDir << "\"";
    }
    if (!options.srcDir.empty()) {
        cmd << " /I\"" << options.srcDir << "\"";
    }

    // 优化级别
    switch (options.optimizationLevel) {
        case 0: cmd << " /Od"; break;
        case 1: cmd << " /O1"; break;
        case 2: cmd << " /O2"; break;
        case 3: cmd << " /Ox"; break;
    }

    // 调试信息
    if (options.debugInfo) {
        cmd << " /Zi /DEBUG";
    }

    // C11标准, Unicode, UTF-8源码编码, 禁用MSVC安全警告
    cmd << " /std:c11 /DUNICODE /D_UNICODE /utf-8 /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_WARNINGS";

    // 警告级别
    cmd << " /W3";

    // P10 恢复: RTL 源码与生成代码一起编译 (非 .lib 按需拉取),
    // /Gy 启用函数级链接, 链接器 /OPT:REF 可按函数剔除未引用的 RTL 代码
    cmd << " /Gy";

    // 性能优化: 多处理器并行编译 (cl.exe /MP, 默认进程数=CPU核心数)
    // 大型项目(如vbman 389个.c文件)串行编译耗时极长, /MP 让每个源文件
    // 由独立 cl 进程并行编译。注意 /MP 与 /GL、/Yc、/Yu 不兼容(本项目未使用)。
    cmd << " /MP";

    // P24-09: x86 RTL libraries are built with /MT (static CRT); match it to avoid LNK2038
    if (options.arch == "x86") {
        cmd << " /MT";
    }

    // Output file and .obj directory (P11.2: intermediates go to objDir)
    if (!options.outputFile.empty()) {
        // M22-IssueB: UTF-8 path preserved through CreateProcessW UTF-16 conversion
    cmd << " /Fe\"" << options.outputFile << "\"";
        if (!options.objDir.empty()) {
            cmd << " /Fo\"" << options.objDir << "/\"";
        } else {
            std::filesystem::path outPath(utf8ToPath(options.outputFile));
            std::string objDir = pathToUtf8(outPath.parent_path());
            if (!objDir.empty()) {
                cmd << " /Fo\"" << objDir << "/\"";
            }
        }
    }

    // 源文件列表
    for (const auto& src : options.sourceFiles) {
        cmd << " \"" << src << "\"";
    }

    // P11.3 (reverted): RTL 以 .c 源码加入 sourceFiles 编译, 无 .lib 链接

    // 链接选项
    if (options.isDll) {
        // P6.6: ActiveX DLL链接
        cmd << " /link /DLL";
        if (!options.typelibResFile.empty()) {
            cmd << " \"" << options.typelibResFile << "\"";
        }
        if (!options.versionInfoResFile.empty()) {
            cmd << " \"" << options.versionInfoResFile << "\"";
        }
        if (!options.userResFile.empty()) {
            cmd << " \"" << options.userResFile << "\"";
        }
        if (!options.defFile.empty()) {
            cmd << " /DEF:\"" << options.defFile << "\"";
        }
        cmd << " ole32.lib oleaut32.lib uuid.lib advapi32.lib user32.lib shell32.lib gdi32.lib";
    } else if (options.isGui) {
        // P7: GUI程序 (Win32窗口)
        cmd << " /link /SUBSYSTEM:WINDOWS";
        if (!options.typelibResFile.empty()) {
            cmd << " \"" << options.typelibResFile << "\"";
        }
        if (!options.versionInfoResFile.empty()) {
            cmd << " \"" << options.versionInfoResFile << "\"";
        }
        if (!options.userResFile.empty()) {
            cmd << " \"" << options.userResFile << "\"";
        }
        cmd << " user32.lib gdi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib advapi32.lib";
    } else {
        // 控制台程序
        cmd << " /link /SUBSYSTEM:CONSOLE";
        if (!options.typelibResFile.empty()) {
            cmd << " \"" << options.typelibResFile << "\"";
        }
        if (!options.versionInfoResFile.empty()) {
            cmd << " \"" << options.versionInfoResFile << "\"";
        }
        if (!options.userResFile.empty()) {
            cmd << " \"" << options.userResFile << "\"";
        }
        cmd << " ole32.lib oleaut32.lib uuid.lib advapi32.lib user32.lib shell32.lib gdi32.lib";
    }

    // DualArch: /MACHINE flag for x86 target (x64 is default, no explicit flag needed)
    if (options.arch == "x86") {
        cmd << " /MACHINE:X86";
    }

    if (options.verbose) {
        std::cout << "C3: 执行: " << cmd.str() << std::endl;
    }

    // P11.2: MSVC output to temp file in objDir (intermediates dir)
    std::string tmpLogDir;
    if (!options.objDir.empty()) {
        tmpLogDir = options.objDir;
    } else if (!options.outputFile.empty()) {
        std::filesystem::path outP(utf8ToPath(options.outputFile));
        tmpLogDir = pathToUtf8(outP.parent_path());
    }
    if (tmpLogDir.empty()) tmpLogDir = ".";
    std::string tmpLogPath = tmpLogDir + "/_c3_msvc_out.txt";

    // Use response file to avoid cmd.exe command line length limit (8191 chars)
    // when compiling many source files (e.g. 125+ .c files in a large project)
    std::string rspPath = tmpLogDir + "/_c3_cl_args.rsp";
    {
        std::ofstream rspFile(rspPath, std::ios::out | std::ios::trunc);
        if (rspFile) {
            // Write arguments (everything after "cl.exe ")
            rspFile << cmd.str().substr(cl.length());
        }
    }

    // P11.4: Prepend vcvarsall.bat setup if cl.exe not in PATH
    std::string vcvarsPrefix = buildVcvarsPrefix(options.arch);
    std::string fullCmd = vcvarsPrefix + cl + " @\"" + rspPath + "\" > \"" + tmpLogPath + "\" 2>&1";

    int ret = executeCommand(fullCmd);
    if (ret != 0) {
        // c3-error.log goes to output dir (user project dir), not intermediates
        std::string outputDirForLog;
        if (!options.outputFile.empty()) {
            std::filesystem::path outP(utf8ToPath(options.outputFile));
            outputDirForLog = pathToUtf8(outP.parent_path());
        }
        if (outputDirForLog.empty()) outputDirForLog = ".";
        std::string errorLogPath = outputDirForLog + "/c3-error.log";
        std::ifstream tmpLog(tmpLogPath);
        std::ofstream errLog(errorLogPath, std::ios::out | std::ios::trunc);
        if (tmpLog && errLog) {
            errLog << "C3: Compilation failed (exit code " << ret << ")" << std::endl;
            errLog << "=== MSVC Output ===" << std::endl;
            std::string line;
            while (std::getline(tmpLog, line)) {
                errLog << line << "\n";
            }
        }
        // 将 MSVC 输出打印到 stderr
        if (tmpLog) {
            tmpLog.clear();
            tmpLog.seekg(0);
            std::string line;
            while (std::getline(tmpLog, line)) {
                std::cerr << line << std::endl;
            }
        }
        std::cerr << "C3: 编译失败 (exit code " << ret << ")" << std::endl;
        std::cerr << "C3: 错误日志已保存: " << errorLogPath << std::endl;
        std::filesystem::remove(tmpLogPath, std::error_code());
        std::filesystem::remove(rspPath, std::error_code());
        return false;
    }
    // 编译成功: 清理临时文件
    std::filesystem::remove(tmpLogPath, std::error_code());
    std::filesystem::remove(rspPath, std::error_code());

    if (options.verbose) {
        std::cout << "C3: 编译成功: " << options.outputFile << std::endl;
    }

    return true;
}

} // namespace vb6c3
