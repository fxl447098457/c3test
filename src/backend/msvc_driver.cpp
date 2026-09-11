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

namespace {

// opt3: FNV-1a 64位哈希 (跨运行稳定, 不依赖标准库实现)
std::string fnv1aHex(const void* data, size_t len) {
    uint64_t h = 1469598103934665603ULL;
    const unsigned char* p = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < len; ++i) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    std::ostringstream oss;
    oss << std::hex << h;
    return oss.str();
}

std::string hashString(const std::string& s) {
    return fnv1aHex(s.data(), s.size());
}

// 文件内容哈希; 读取失败返回空串
std::string hashFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    uint64_t h = 1469598103934665603ULL;
    char buf[65536];
    while (f.read(buf, sizeof(buf)) || f.gcount() > 0) {
        std::streamsize n = f.gcount();
        const unsigned char* p = reinterpret_cast<const unsigned char*>(buf);
        for (std::streamsize i = 0; i < n; ++i) {
            h ^= p[i];
            h *= 1099511628211ULL;
        }
    }
    std::ostringstream oss;
    oss << std::hex << h;
    return oss.str();
}

// 解析 .c 文件中的本地头文件 #include "xxx.h"，返回绝对路径列表
// (opt3: 依赖跟踪 — 任一依赖头文件变化即触发该 .c 重编译)
std::vector<std::string> collectLocalIncludes(const std::string& cPath, const std::string& srcDir) {
    std::vector<std::string> incs;
    std::ifstream f(cPath);
    if (!f) return incs;
    std::string line;
    while (std::getline(f, line)) {
        size_t hash = line.find('#');
        if (hash == std::string::npos) continue;
        size_t q1 = line.find('"', hash);
        if (q1 == std::string::npos) continue;
        size_t q2 = line.find('"', q1 + 1);
        if (q2 == std::string::npos) continue;
        std::string name = line.substr(q1 + 1, q2 - q1 - 1);
        if (name.size() < 3) continue;
        if (name.compare(name.size() - 2, 2, ".h") != 0) continue;
        // 绝对路径 (srcDir 下)
        incs.push_back(srcDir + "/" + name);
    }
    return incs;
}

} // namespace

MsvcDriver::MsvcDriver() {}
MsvcDriver::~MsvcDriver() = default;

bool MsvcDriver::isMsvcAvailable() {
    // 1. Check if vcvarsall environment already set
    const char* vcDir = std::getenv("VCINSTALLDIR");
    if (vcDir && vcDir[0] != '\0') {
        return true;
    }
    // 2. Try running cl.exe directly (might be in PATH from other setup)
    int ret = std::system("cl.exe >nul 2>&1");
    if (ret == 0) return true;
    // 3. P11.4: Check if vswhere can find VS installation
    std::string vcvars = findVcvarsallBat();
    return !vcvars.empty();
}

// P11.4: Find VS installation path via vswhere.exe or registry
std::string MsvcDriver::findVsInstallPath() {
    // Method 1: vswhere.exe (VS2017+)
    const char* pf_x86 = std::getenv("ProgramFiles(x86)");
    if (!pf_x86) pf_x86 = "C:\\Program Files (x86)";
    std::string vswhere = std::string(pf_x86) + "\\Microsoft Visual Studio\\Installer\\vswhere.exe";

    if (std::filesystem::exists(vswhere)) {
        // Run vswhere to get installation path
        // -products * : include BuildTools (not just full VS editions)
        std::string cmd = "\"" + vswhere + "\" -all -latest -products * -property installationPath";
        // Use _popen to capture output
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (pipe) {
            char buffer[512];
            std::string result;
            while (fgets(buffer, sizeof(buffer), pipe)) {
                result += buffer;
            }
            _pclose(pipe);
            // Trim whitespace
            while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' '))
                result.pop_back();
            if (!result.empty() && std::filesystem::exists(result)) {
                return result;
            }
        }
    }

    // Method 2: Registry fallback (VS2015 and earlier)
#ifdef _WIN32
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\VisualStudio\\SxS\\VS7",
                       0, KEY_READ | KEY_WOW64_32KEY, &hKey) == ERROR_SUCCESS) {
        char value[512];
        DWORD size = sizeof(value);
        // Check for VS2022 (17.0), VS2019 (16.0), VS2017 (15.0), VS2015 (14.0)
        const char* versions[] = {"17.0", "16.0", "15.0", "14.0"};
        for (const char* ver : versions) {
            size = sizeof(value);
            if (RegQueryValueExA(hKey, ver, nullptr, nullptr, (LPBYTE)value, &size) == ERROR_SUCCESS) {
                std::string path(value);
                // Trim trailing backslash
                while (!path.empty() && path.back() == '\\') path.pop_back();
                RegCloseKey(hKey);
                if (!path.empty()) return path;
            }
        }
        RegCloseKey(hKey);
    }
#endif

    // Method 3: Check well-known filesystem paths directly (fallback for BuildTools
    // when vswhere is absent or doesn't report the product)
    const char* pf_x86_2 = std::getenv("ProgramFiles(x86)");
    if (!pf_x86_2) pf_x86_2 = "C:\\Program Files (x86)";
    const char* pf_64 = std::getenv("ProgramFiles");
    if (!pf_64) pf_64 = "C:\\Program Files";
    const char* editions[] = {"BuildTools", "Community", "Professional", "Enterprise"};
    const char* versions[] = {"2022", "2019"};
    const char* bases[] = {pf_x86_2, pf_64};
    for (const char* base : bases) {
        for (const char* ver : versions) {
            for (const char* ed : editions) {
                std::string path = std::string(base) + "\\Microsoft Visual Studio\\" + ver + "\\" + ed;
                if (std::filesystem::exists(path + "\\VC\\Auxiliary\\Build\\vcvarsall.bat")) {
                    return path;
                }
            }
        }
    }

    return "";
}

// P11.4: Find vcvarsall.bat path
std::string MsvcDriver::findVcvarsallBat() {
    // 1. Check VCINSTALLDIR (already set from previous vcvarsall)
    const char* vcDir = std::getenv("VCINSTALLDIR");
    if (vcDir && vcDir[0] != '\0') {
        // M30-AX86: Normalize to guarantee a trailing backslash.
        // install_msvc.bat writes VCINSTALLDIR without one (e.g.
        // "C:\pro\c3\msvc") while the portable vcvars.bat writes it WITH one.
        // Without normalization the concatenations below yield
        // "C:\pro\c3\msvcAuxiliary\..." (missing separator), which silently
        // breaks --arch x86 on the mini toolchain deployed via install_msvc.bat
        // (LNK1112: env stays x64 because findVcvarsallBat returns "").
        std::string base = vcDir;
        if (!base.empty() && base.back() != '\\') base.push_back('\\');
        // Standard VS layout
        std::string bat = base + "Auxiliary\\Build\\vcvarsall.bat";
        if (std::filesystem::exists(bat)) return bat;
        // P24-AX86: Portable C3 mini toolchain layout (vcvars.bat lives in
        // VCINSTALLDIR root, accepts x64/x86 arg). Enables --arch x86 even when
        // the user env was permanently set to x64 by install_msvc.bat.
        std::string portable = base + "vcvars.bat";
        if (std::filesystem::exists(portable)) return portable;
    }

    // 2. Find via vswhere/registry
    std::string vsPath = findVsInstallPath();
    if (!vsPath.empty()) {
        std::string bat = vsPath + "\\VC\\Auxiliary\\Build\\vcvarsall.bat";
        if (std::filesystem::exists(bat)) return bat;
    }

    return "";
}

std::string MsvcDriver::findClExe() const {
    return "cl.exe";
}

// P11.4: Build vcvarsall.bat prefix if needed
std::string MsvcDriver::buildVcvarsPrefix(const std::string& arch) const {
    // P24-05: Check if vcvarsall already set AND target arch matches current env
    const char* vcDir = std::getenv("VCINSTALLDIR");
    if (vcDir && vcDir[0] != '\0') {
        // VSCMD_ARG_TGT_ARCH is set by VS2017+ vcvarsall.bat ("x86" or "x64")
        const char* tgtArch = std::getenv("VSCMD_ARG_TGT_ARCH");
        if (tgtArch && tgtArch[0] != '\0') {
            if (arch == tgtArch) {
                return "";  // env already matches target arch
            }
            // Mismatch: must re-call vcvarsall with correct arch (e.g. x64 env but --arch x86)
            // Fall through to findVcvarsallBat below
        } else {
            // VSCMD_ARG_TGT_ARCH not set (e.g. portable toolchain via install_msvc.bat)
            // P24-AX86: x64 is the common default - assume env already matches x64.
            // For x86 target we must reconfigure env (call vcvars.bat x86) to switch
            // to the Hostx64 cross compiler and lib\x86; fall through.
            if (arch == "x64") {
                return "";
            }
            // non-x64 target with no arch hint: fall through to findVcvarsallBat
        }
    }

    // Try to find vcvarsall.bat
    std::string vcvars = findVcvarsallBat();
    if (!vcvars.empty()) {
        return "call \"" + vcvars + "\" " + arch + " >nul 2>&1 && ";
    }

    return "";
}

int MsvcDriver::executeCommand(const std::string& cmd) const {
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
    PROCESS_INFORMATION pi = {};
    
    // CreateProcessW requires mutable command line buffer
    std::wstring mutableCmd = fullCmd;
    
    BOOL ok = CreateProcessW(
        nullptr,                // application name (nullptr = use command line)
        &mutableCmd[0],         // command line (mutable)
        nullptr,                // process security
        nullptr,                // thread security
        FALSE,                  // inherit handles
        0,                      // creation flags
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

    // P11.3: RTL pre-compiled .lib files linked via /link (no .c source compilation)
    // .lib paths are added to the /link section below

    // 链接选项
    if (options.isDll) {
        // P6.6: ActiveX DLL链接
        cmd << " /link /DLL";
        if (!options.rtlDir.empty()) {
            // 092z-3: DLL 也要链 vb6rtl_gui.lib —— VB6 的 ActiveX DLL 允许包含窗体
            // (VB6 支持在 ActiveX DLL 里放 Form/UserControl)，此时模块会引用窗体运行时
            // (vb6_CreateFormWindow / vb6_DoEvents / vb6_SetControlText ...)，只链
            // vb6rtl.lib 会在链接期报这 37 个符号未解析。
            // 静态库是按需拉取的：不需要窗体运行时的 DLL 不会拉入 vb6forms.obj，无副作用。
            cmd << " \"" << options.rtlDir << "\\vb6rtl.lib\""
                << " \"" << options.rtlDir << "\\vb6rtl_dll.lib\""
                << " \"" << options.rtlDir << "\\vb6rtl_gui.lib\"";
        }
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
        if (!options.rtlDir.empty()) {
            cmd << " \"" << options.rtlDir << "\\vb6rtl.lib\""
                << " \"" << options.rtlDir << "\\vb6rtl_gui.lib\"";
        }
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
        if (!options.rtlDir.empty()) {
            cmd << " \"" << options.rtlDir << "\\vb6rtl.lib\"";
        }
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

// opt3: 增量编译 — 基于内容哈希的 obj 级缓存。
// 每个 .c 的缓存记录 = 源文件哈希 + 依赖头文件哈希 + 编译选项指纹。
// 命中且缓存 obj 存在 → 跳过 cl 编译，直接复用 obj 链接。
bool MsvcDriver::compileAndLinkIncremental(const MsvcDriverOptions& options) {
    std::string cl = findClExe();
    std::string arch = options.arch;

    // obj 输出目录 (新编译的 obj 先落在这里, 链接成功后复制到缓存目录)
    std::string objDir = options.objDir;
    if (objDir.empty() && !options.outputFile.empty()) {
        std::filesystem::path outP(utf8ToPath(options.outputFile));
        objDir = pathToUtf8(outP.parent_path());
    }
    if (objDir.empty()) objDir = ".";
    std::string tmpLogPath = objDir + "/_c3_msvc_out.txt";

    // 持久缓存目录
    std::string cacheDir = options.incrementalCacheDir;
    if (cacheDir.empty()) cacheDir = objDir;
    std::error_code ec;
    std::filesystem::create_directories(utf8ToPath(cacheDir), ec);
    std::string cacheFile = cacheDir + "/cache.txt";

    // 编译选项指纹: 选项变化(优化级别/架构/DLL标志等)会使全部缓存失效
    std::string optsFp = hashString(
        options.arch + "|" +
        std::to_string(options.isDll ? 1 : 0) + "|" +
        std::to_string(options.isGui ? 1 : 0) + "|" +
        std::to_string(options.optimizationLevel) + "|" +
        std::to_string(options.debugInfo ? 1 : 0));

    // 读缓存: objName -> record
    std::unordered_map<std::string, std::string> cache;
    {
        std::ifstream f(cacheFile);
        std::string line;
        while (std::getline(f, line)) {
            size_t tab = line.find('\t');
            if (tab != std::string::npos) {
                cache[line.substr(0, tab)] = line.substr(tab + 1);
            }
        }
    }

    // 公共编译选项 (不含 /Fe /Fo /MP /c 与源文件)
    std::ostringstream common;
    common << cl;
    if (!options.rtlDir.empty()) common << " /I\"" << options.rtlDir << "\"";
    if (!options.srcDir.empty()) common << " /I\"" << options.srcDir << "\"";
    switch (options.optimizationLevel) {
        case 0: common << " /Od"; break;
        case 1: common << " /O1"; break;
        case 2: common << " /O2"; break;
        case 3: common << " /Ox"; break;
    }
    if (options.debugInfo) common << " /Zi";
    common << " /std:c11 /DUNICODE /D_UNICODE /utf-8 /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_WARNINGS";
    common << " /W3";
    if (options.arch == "x86") common << " /MT";

    // 增量判断
    std::vector<std::string> toCompile;   // 需要重编译的 .c
    std::vector<std::string> reusedObjs;  // 缓存命中的 obj 完整路径 (cacheDir)
    std::vector<std::string> newObjs;     // 新编译 obj 的预测路径 (objDir)
    std::unordered_map<std::string, std::string> newCache; // objName -> record
    int hitCount = 0;
    for (const auto& src : options.sourceFiles) {
        std::filesystem::path sp(utf8ToPath(src));
        std::string objName = pathToUtf8(sp.stem()) + ".obj";
        std::string srcHash = hashFile(src);
        if (srcHash.empty()) {  // 读不到源文件: 必须编译
            toCompile.push_back(src);
            newObjs.push_back(objDir + "/" + objName);
            continue;
        }
        auto incs = collectLocalIncludes(src, options.srcDir);
        std::string deps;
        for (auto& inc : incs) deps += hashFile(inc);
        std::string record = srcHash + " " + hashString(deps) + " " + optsFp;
        newCache[objName] = record;
        std::string cachedObj = cacheDir + "/" + objName;
        if (cache.count(objName) && cache[objName] == record &&
            std::filesystem::exists(utf8ToPath(cachedObj))) {
            reusedObjs.push_back(cachedObj);
            hitCount++;
        } else {
            toCompile.push_back(src);
            newObjs.push_back(objDir + "/" + objName);
        }
    }

    if (options.verbose) {
        std::cout << "C3: 增量编译: " << hitCount << "/" << options.sourceFiles.size()
                  << " 个源文件命中缓存, 跳过编译" << std::endl;
    }

    // === 编译需要重编的 .c (仅这些) ===
    if (!toCompile.empty()) {
        std::ostringstream compileCmd;
        compileCmd << common.str() << " /MP /c";
        if (!options.outputFile.empty()) {
            compileCmd << " /Fo\"" << objDir << "/\"";
        }
        for (const auto& src : toCompile) {
            compileCmd << " \"" << src << "\"";
        }
        std::string rspPath = objDir + "/_c3_cl_args.rsp";
        {
            std::ofstream rspFile(rspPath, std::ios::out | std::ios::trunc);
            if (rspFile) rspFile << compileCmd.str().substr(cl.length());
        }
        std::string vcvarsPrefix = buildVcvarsPrefix(arch);
        std::string fullCmd = vcvarsPrefix + cl + " @\"" + rspPath + "\" > \"" + tmpLogPath + "\" 2>&1";
        int ret = executeCommand(fullCmd);
        if (ret != 0) {
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
                while (std::getline(tmpLog, line)) errLog << line << "\n";
            }
            if (tmpLog) {
                tmpLog.clear();
                tmpLog.seekg(0);
                std::string line;
                while (std::getline(tmpLog, line)) std::cerr << line << std::endl;
            }
            std::cerr << "C3: 编译失败 (exit code " << ret << ")" << std::endl;
            std::cerr << "C3: 错误日志已保存: " << errorLogPath << std::endl;
            std::filesystem::remove(tmpLogPath, std::error_code());
            std::filesystem::remove(rspPath, std::error_code());
            return false;
        }
        std::filesystem::remove(tmpLogPath, std::error_code());
        std::filesystem::remove(rspPath, std::error_code());

        // 复制新 obj 到持久缓存目录
        for (const auto& src : toCompile) {
            std::filesystem::path sp(utf8ToPath(src));
            std::string objName = pathToUtf8(sp.stem()) + ".obj";
            std::string srcObj = objDir + "/" + objName;
            std::string dstObj = cacheDir + "/" + objName;
            if (std::filesystem::exists(utf8ToPath(srcObj))) {
                std::error_code ec2;
                std::filesystem::copy_file(utf8ToPath(srcObj), utf8ToPath(dstObj),
                                           std::filesystem::copy_options::overwrite_existing, ec2);
            }
        }
    }

    // 更新缓存索引 (总是写, 保证新模块/新记录持久)
    {
        std::ofstream f(cacheFile, std::ios::out | std::ios::trunc);
        for (auto& kv : newCache) {
            f << kv.first << "\t" << kv.second << "\n";
        }
    }

    // === 链接 (所有 obj: 复用的 + 新编译的) ===
    // 注意: 拆分为两步后编译阶段无源文件, cl /link 不会进入链接模式(D8003),
    // 因此链接阶段直接调用 link.exe (vcvarsall 后位于 PATH)。
    std::string linkExe = "link.exe";
    std::ostringstream linkCmd;
    linkCmd << linkExe << " /NOLOGO";
    if (options.isDll) {
        linkCmd << " /DLL";
    } else if (options.isGui) {
        linkCmd << " /SUBSYSTEM:WINDOWS";
    } else {
        linkCmd << " /SUBSYSTEM:CONSOLE";
    }
    if (!options.outputFile.empty()) linkCmd << " /OUT:\"" << options.outputFile << "\"";
    for (auto& o : reusedObjs) linkCmd << " \"" << o << "\"";
    for (auto& o : newObjs) linkCmd << " \"" << o << "\"";
    if (options.isDll) {
        if (!options.rtlDir.empty()) {
            linkCmd << " \"" << options.rtlDir << "\\vb6rtl.lib\""
                    << " \"" << options.rtlDir << "\\vb6rtl_dll.lib\"";
        }
        if (!options.typelibResFile.empty()) linkCmd << " \"" << options.typelibResFile << "\"";
        if (!options.versionInfoResFile.empty()) linkCmd << " \"" << options.versionInfoResFile << "\"";
        if (!options.userResFile.empty()) linkCmd << " \"" << options.userResFile << "\"";
        if (!options.defFile.empty()) linkCmd << " /DEF:\"" << options.defFile << "\"";
        linkCmd << " ole32.lib oleaut32.lib uuid.lib advapi32.lib user32.lib shell32.lib gdi32.lib";
    } else if (options.isGui) {
        if (!options.rtlDir.empty()) {
            linkCmd << " \"" << options.rtlDir << "\\vb6rtl.lib\""
                    << " \"" << options.rtlDir << "\\vb6rtl_gui.lib\"";
        }
        if (!options.typelibResFile.empty()) linkCmd << " \"" << options.typelibResFile << "\"";
        if (!options.versionInfoResFile.empty()) linkCmd << " \"" << options.versionInfoResFile << "\"";
        if (!options.userResFile.empty()) linkCmd << " \"" << options.userResFile << "\"";
        linkCmd << " user32.lib gdi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib advapi32.lib";
    } else {
        if (!options.rtlDir.empty()) linkCmd << " \"" << options.rtlDir << "\\vb6rtl.lib\"";
        if (!options.typelibResFile.empty()) linkCmd << " \"" << options.typelibResFile << "\"";
        if (!options.versionInfoResFile.empty()) linkCmd << " \"" << options.versionInfoResFile << "\"";
        if (!options.userResFile.empty()) linkCmd << " \"" << options.userResFile << "\"";
        linkCmd << " ole32.lib oleaut32.lib uuid.lib advapi32.lib user32.lib shell32.lib gdi32.lib";
    }
    if (options.arch == "x86") linkCmd << " /MACHINE:X86";
    if (options.debugInfo) linkCmd << " /DEBUG";

    std::string linkRsp = objDir + "/_c3_link_args.rsp";
    {
        std::ofstream rspFile(linkRsp, std::ios::out | std::ios::trunc);
        if (rspFile) rspFile << linkCmd.str().substr(linkExe.length());
    }
    std::string vcvarsPrefix2 = buildVcvarsPrefix(arch);
    std::string fullLinkCmd = vcvarsPrefix2 + linkExe + " @\"" + linkRsp + "\" > \"" + tmpLogPath + "\" 2>&1";

    if (options.verbose) {
        std::cout << "C3: 执行: " << fullLinkCmd << std::endl;
    }

    int ret2 = executeCommand(fullLinkCmd);
    if (ret2 != 0) {
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
            errLog << "C3: Compilation failed (exit code " << ret2 << ")" << std::endl;
            errLog << "=== MSVC Output ===" << std::endl;
            std::string line;
            while (std::getline(tmpLog, line)) errLog << line << "\n";
        }
        if (tmpLog) {
            tmpLog.clear();
            tmpLog.seekg(0);
            std::string line;
            while (std::getline(tmpLog, line)) std::cerr << line << std::endl;
        }
        std::cerr << "C3: 编译失败 (exit code " << ret2 << ")" << std::endl;
        std::cerr << "C3: 错误日志已保存: " << errorLogPath << std::endl;
        std::filesystem::remove(tmpLogPath, std::error_code());
        std::filesystem::remove(linkRsp, std::error_code());
        return false;
    }
    std::filesystem::remove(tmpLogPath, std::error_code());
    std::filesystem::remove(linkRsp, std::error_code());

    if (options.verbose) {
        std::cout << "C3: 编译成功: " << options.outputFile << std::endl;
    }
    return true;
}

} // namespace vb6c3
