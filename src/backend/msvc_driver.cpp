#include "backend/msvc_driver.hpp"

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <sstream>

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
        std::cerr << "c3: 没有C源文件需要编译" << std::endl;
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

    // C11标准, Unicode, UTF-8源码编码
    cmd << " /std:c11 /DUNICODE /D_UNICODE /utf-8";

    // 警告级别
    cmd << " /W3";

    // 输出对象文件名
    if (!options.outputFile.empty()) {
        cmd << " /Fe\"" << options.outputFile << "\"";
    }

    // 源文件列表
    for (const auto& src : options.sourceFiles) {
        cmd << " \"" << src << "\"";
    }

    // RTL实现文件
    if (!options.rtlDir.empty()) {
        cmd << " \"" << options.rtlDir << "\\vb6rtl.c\"";
    }

    // 链接: 控制台程序
    cmd << " /link /SUBSYSTEM:CONSOLE";

    if (options.verbose) {
        std::cout << "c3: 执行: " << cmd.str() << std::endl;
    }

    int ret = executeCommand(cmd.str());
    if (ret != 0) {
        std::cerr << "c3: MSVC编译失败 (exit code " << ret << ")" << std::endl;
        return false;
    }

    if (options.verbose) {
        std::cout << "c3: 编译成功: " << options.outputFile << std::endl;
    }

    return true;
}

} // namespace vb6c3
