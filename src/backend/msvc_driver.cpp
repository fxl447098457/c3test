#include "backend/msvc_driver.hpp"

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

namespace vb6c3 {

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
        std::string cmd = "\"" + vswhere + "\" -all -latest -property installationPath";
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

    return "";
}

// P11.4: Find vcvarsall.bat path
std::string MsvcDriver::findVcvarsallBat() {
    // 1. Check VCINSTALLDIR (already set from previous vcvarsall)
    const char* vcDir = std::getenv("VCINSTALLDIR");
    if (vcDir && vcDir[0] != '\0') {
        std::string bat = std::string(vcDir) + "Auxiliary\\Build\\vcvarsall.bat";
        if (std::filesystem::exists(bat)) return bat;
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
std::string MsvcDriver::buildVcvarsPrefix() const {
    // If vcvarsall already set, no prefix needed
    const char* vcDir = std::getenv("VCINSTALLDIR");
    if (vcDir && vcDir[0] != '\0') {
        return "";
    }

    // Try to find vcvarsall.bat
    std::string vcvars = findVcvarsallBat();
    if (!vcvars.empty()) {
        return "call \"" + vcvars + "\" x64 >nul 2>&1 && ";
    }

    return "";
}

int MsvcDriver::executeCommand(const std::string& cmd) const {
#ifdef _WIN32
    // 使用cmd /c执行, 避免路径问题
    return std::system(cmd.c_str());
#else
    return std::system(cmd.c_str());
#endif
}
bool MsvcDriver::compileAndLink(const MsvcDriverOptions& options) {
    if (options.sourceFiles.empty()) {
        std::cerr << "C3: 没有源文件需要编译" << std::endl;
        return false;
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

    // Output file and .obj directory (P11.2: intermediates go to objDir)
    if (!options.outputFile.empty()) {
        cmd << " /Fe\"" << options.outputFile << "\"";
        if (!options.objDir.empty()) {
            cmd << " /Fo\"" << options.objDir << "/\"";
        } else {
            std::filesystem::path outPath(options.outputFile);
            std::string objDir = outPath.parent_path().string();
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
            cmd << " \"" << options.rtlDir << "\\vb6rtl.lib\""
                << " \"" << options.rtlDir << "\\vb6rtl_dll.lib\"";
        }
        if (!options.typelibResFile.empty()) {
            cmd << " \"" << options.typelibResFile << "\"";
        }
        if (!options.defFile.empty()) {
            cmd << " /DEF:\"" << options.defFile << "\"";
        }
        cmd << " ole32.lib oleaut32.lib uuid.lib advapi32.lib user32.lib shell32.lib";
    } else if (options.isGui) {
        // P7: GUI程序 (Win32窗口)
        cmd << " /link /SUBSYSTEM:WINDOWS";
        if (!options.rtlDir.empty()) {
            cmd << " \"" << options.rtlDir << "\\vb6rtl.lib\""
                << " \"" << options.rtlDir << "\\vb6rtl_gui.lib\"";
        }
        cmd << " user32.lib gdi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib";
    } else {
        // 控制台程序
        cmd << " /link /SUBSYSTEM:CONSOLE";
        if (!options.rtlDir.empty()) {
            cmd << " \"" << options.rtlDir << "\\vb6rtl.lib\"";
        }
        cmd << " ole32.lib oleaut32.lib uuid.lib user32.lib shell32.lib";
    }


    if (options.verbose) {
        std::cout << "C3: 执行: " << cmd.str() << std::endl;
    }

    // P11.2: MSVC output to temp file in objDir (intermediates dir)
    std::string tmpLogDir;
    if (!options.objDir.empty()) {
        tmpLogDir = options.objDir;
    } else if (!options.outputFile.empty()) {
        std::filesystem::path outP(options.outputFile);
        tmpLogDir = outP.parent_path().string();
    }
    if (tmpLogDir.empty()) tmpLogDir = ".";
    std::string tmpLogPath = tmpLogDir + "/_c3_msvc_out.txt";
    // P11.4: Prepend vcvarsall.bat setup if cl.exe not in PATH
    std::string vcvarsPrefix = buildVcvarsPrefix();
    std::string fullCmd = vcvarsPrefix + cmd.str() + " > \"" + tmpLogPath + "\" 2>&1";

    int ret = executeCommand(fullCmd);
    if (ret != 0) {
        // c3-error.log goes to output dir (user project dir), not intermediates
        std::string outputDirForLog;
        if (!options.outputFile.empty()) {
            std::filesystem::path outP(options.outputFile);
            outputDirForLog = outP.parent_path().string();
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
        return false;
    }
    // 编译成功: 清理临时文件
    std::filesystem::remove(tmpLogPath, std::error_code());

    if (options.verbose) {
        std::cout << "C3: 编译成功: " << options.outputFile << std::endl;
    }

    return true;
}

} // namespace vb6c3
