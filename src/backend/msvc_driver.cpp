#include "backend/msvc_driver.hpp"

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <sstream>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

namespace vb6c3 {

MsvcDriver::MsvcDriver() {}
MsvcDriver::~MsvcDriver() = default;

bool MsvcDriver::isMsvcAvailable() {
    // 检查环境变量是否已设置vcvarsall
    const char* clPath = std::getenv("VCINSTALLDIR");
    if (clPath && clPath[0] != '\0') {
        return true;
    }
    // 尝试直接运行cl.exe
    int ret = std::system("cl.exe >nul 2>&1");
    return ret == 0;
}

std::string MsvcDriver::findClExe() const {
    // 如果vcvarsall已设置, cl.exe应该在PATH中
    return "cl.exe";
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

    // RTL实现文件
    if (!options.rtlDir.empty()) {
        cmd << " \"" << options.rtlDir << "\\vb6rtl.c\"";
        cmd << " \"" << options.rtlDir << "\\vb6com.c\"";
        if (options.isDll) {
            cmd << " \"" << options.rtlDir << "\\vb6comserver.c\"";  // P6.6: COM服务端运行时
        }
        if (options.isGui) {
            cmd << " \"" << options.rtlDir << "\\vb6forms.c\"";  // P7: 窗体运行时
        }
    }

    // 链接选项
    if (options.isDll) {
        // P6.6: ActiveX DLL链接
        cmd << " /link /DLL";
        if (!options.typelibResFile.empty()) {
            cmd << " \"" << options.typelibResFile << "\"";
        }
        if (!options.defFile.empty()) {
            cmd << " /DEF:\"" << options.defFile << "\"";
        }
        cmd << " ole32.lib oleaut32.lib uuid.lib advapi32.lib user32.lib shell32.lib";
    } else if (options.isGui) {
        // P7: GUI程序 (Win32窗口)
        cmd << " /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib";
    } else {
        // 控制台程序
        cmd << " /link /SUBSYSTEM:CONSOLE ole32.lib oleaut32.lib uuid.lib user32.lib shell32.lib";
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
    std::string fullCmd = cmd.str() + " > \"" + tmpLogPath + "\" 2>&1";

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
