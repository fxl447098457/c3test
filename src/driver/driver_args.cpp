// driver_args.cpp - C3 编译器驱动: 命令行解析与帮助/版本输出
// 2026-09-17 从 src/driver/driver.cpp 纯搬移（逐行未改）：
//   原第 40~181 行
//   原第 2421~2493 行

#include "driver/driver.hpp"
#include "common/diagnostics.hpp"
#include "common/encoding.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>

namespace vb6c3 {

// === 命令行解析 ===

std::pair<CompileOptions, int> Driver::parseArgs(int argc, char* argv[]) {
    CompileOptions opts;
    int resultCode = 0;

    // 无任何参数时显示帮助 (等价于 -h), 而不是报"未指定源文件"
    if (argc <= 1) {
        printHelp();
        return {opts, 0};
    }

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printHelp();
            return {opts, 0}; // 0 = 显示帮助后退出
        }
        if (arg == "-V" || arg == "--version") {
            printVersion();
            return {opts, 0};
        }
        else if (arg == "-o" && i + 1 < argc) {
            opts.outputFile = argv[++i];
        }
        else if (arg == "--output-dir" && i + 1 < argc) {
            opts.outputDir = argv[++i];
        }
        else if (arg == "--target" && i + 1 < argc) {
            opts.target = argv[++i];
        }
        else if (arg == "--arch" && i + 1 < argc) {
            opts.arch = argv[++i];
            if (opts.arch != "x86" && opts.arch != "x64") {
                std::cerr << "C3: --arch must be x86 or x64 (got: " << opts.arch << ")" << std::endl;
                resultCode = 1;
            }
        }
        else if (arg == "--gui" && i + 1 < argc) {
            opts.guiMode = argv[++i];
        }
        else if (arg == "--dump-tokens") {
            opts.dumpTokens = true;
        }
        else if (arg == "--dump-ast") {
            opts.dumpAST = true;
        }
        else if (arg == "--dump-symbols") {
            opts.dumpSymbols = true;
        }
        else if (arg == "--dump-preprocess") {
            opts.dumpPreprocess = true;
        }
        else if (arg == "--dump-ir") {
            opts.dumpIR = true;
        }
        else if (arg == "--emit-llvm") {
            opts.emitLLVM = true;
        }
        else if (arg == "--emit-c") {
            opts.emitC = true;
        }
        else if (arg == "--incremental") {
            opts.incremental = true;  // opt3: 增量编译 (obj级缓存)
        }
        else if (arg == "--trim-includes") {
            opts.trimIncludes = true;  // opt4: 裁剪未实际引用的跨模块include
        }
        else if (arg == "--keep-for-debug") {
            opts.keepTemps = true;  // 保留中间文件便于调试
        }
        else if (arg == "--syntax-only") {
            opts.syntaxOnly = true;
        }
        else if (arg == "--typelib" && i + 1 < argc) {
            opts.typelibRefs.push_back(argv[++i]);  // P6.3: 显式TypeLib引用
        }
        else if (arg == "--no-auto-typelib") {
            opts.autoTypelib = false;  // P6.3: 禁用自动TypeLib加载
        }
        else if (arg == "--dll") {
            opts.isDll = true;  // P6.6: 编译为ActiveX DLL
        }
        else if (arg == "--progid" && i + 1 < argc) {
            opts.dllProgId = argv[++i];  // P6.6: ProgID前缀
        }
        else if (arg == "--libid" && i + 1 < argc) {
            opts.libidStr = argv[++i];  // C3 扩展: 显式指定 TypeLib LibID
        }
        else if (arg == "--dump-frm") {
            opts.dumpFrm = true;  // P7: 输出.frm窗体描述
        }
        else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        }
        else if (arg == "-O" && i + 1 < argc) {
            opts.optimizationLevel = std::stoi(argv[++i]);
        }
        else if (arg == "-g" || arg == "--debug") {
            opts.debugInfo = true;
        }
        else if (arg == "--compat-check") {
            opts.compatCheck = true;
        }
        else if (arg == "--no-warn" && i + 1 < argc) {
            // 抑制指定ID的警告, 逗号分隔 (如 --no-warn 3001,3003)
            std::string list = argv[++i];
            std::stringstream ss(list);
            std::string item;
            while (std::getline(ss, item, ',')) {
                if (item.empty()) continue;
                try {
                    opts.suppressedWarningIds.push_back(std::stoi(item));
                } catch (...) {
                    std::cerr << "C3: 无效的警告ID: " << item << std::endl;
                    resultCode = 1;
                }
            }
        }
        else if (arg == "-d" || arg == "--define") {
            if (i + 1 < argc) {
                opts.defines.push_back(argv[++i]);
            }
        }
        else if (arg[0] == '-') {
            std::cerr << "C3: 未知选项: " << arg << std::endl;
            resultCode = 1;
        }
        else {
            opts.sourceFiles.push_back(arg);
        }
    }

    if (opts.sourceFiles.empty() && resultCode == 0) {
        std::cerr << "C3: 错误: 未指定源文件" << std::endl;
        resultCode = 1;
    }

    return {opts, resultCode};
}


// === 帮助/版本 ===

void Driver::writeErrorLog(const std::string& logPath, const std::string& stage) {
    // Prepend fail info (also creates file if not exists from MSVC driver)
    if (!std::filesystem::exists(utf8ToPath(logPath))) {
        std::ofstream createLog(logPath, std::ios::out);
        createLog << "C3: Compilation failed (stage: " << stage << ")" << std::endl;
    }
    std::ofstream errLog(logPath, std::ios::out | std::ios::app);
    if (!errLog) return;
    errLog << "\n=== C3 Diagnostics (" << stage << ") ===" << std::endl;
    errLog << diag_->toString();
}


void Driver::printHelp() {
    std::cout << "C3 - Visual Basic 6.0 Compiler\n"
              << "\n"
              << "用法: C3 [选项] <源文件...>\n"
              << "\n"
              << "选项:\n"
              << "  -o <文件>          输出文件路径\n"
              << "  --output-dir <目录> 输出目录 (默认: 源文件所在目录)\n"
              << "  --target <平台>     目标平台 (win-x86, win-x64, linux-x64, macos-arm64)\n"
              << "  --arch <架构>      目标架构 x64 (默认) 或 x86 (用于32位COM组件)\n"
              << "  --gui <模式>        GUI模式 (native, webview, none)\n"
              << "  --syntax-only       只做语法检查\n"
              << "  -d, --define <N=V>  定义条件编译常量 (如 -d:DEBUG=-1)\n"
              << "  -O <级别>           优化级别 (0-3)\n"
              << "  -g, --debug         生成调试信息\n"
              << "  --dll               编译为ActiveX DLL\n"
              << "  -v, --verbose       详细输出\n"
              << "  -h, --help          显示帮助\n"
              << "  -V, --version       显示版本\n"
              << "\n"
              << "调试/转储选项:\n"
              << "  --dump-tokens      输出词法分析后的Token流\n"
              << "  --dump-ast         输出抽象语法树\n"
              << "  --dump-symbols     输出符号表\n"
              << "  --dump-preprocess  输出预处理后的源码\n"
              << "  --dump-ir          输出中间表示\n"
              << "  --dump-frm         输出.frm窗体描述\n"
              << "  --emit-c           生成C代码 (输出到 --output-dir)\n"
              << "  --emit-llvm        生成LLVM IR\n"
              << "  --keep-for-debug   保留中间文件便于调试\n"
              << "  --compat-check     兼容性检查模式\n"
              << "\n"
              << "TypeLib/COM 选项:\n"
              << "  --typelib <文件>    显式引用TypeLib\n"
              << "  --no-auto-typelib   禁用自动TypeLib加载\n"
              << "  --progid <前缀>     ActiveX DLL的ProgID前缀\n"
              << "  --libid <字符串>    显式指定TypeLib的LibID\n"
              << "\n"
              << "性能/杂项选项:\n"
              << "  --incremental       增量编译 (obj级缓存, 跳过未变化的.c)\n"
              << "  --trim-includes     裁剪未实际引用的跨模块include\n"
              << "  --no-warn <ID列表>   抑制指定ID的警告 (逗号分隔, 如 3001,3003)\n"
              << "\n"
              << "示例:\n"
              << "  C3 hello.bas -o hello.exe\n"
              << "  C3 app.vbp --target win-x64\n"
              << "  C3 module.bas -d:DEBUG=-1\n";
}


// 版本号由 CMake 从根目录 VERSION 文件读出、经编译期宏注入，勿在此硬编码。
#ifndef VB6C3_VERSION_STRING
#define VB6C3_VERSION_STRING "(unknown)"
#endif

void Driver::printVersion() {
    std::cout << "C3 version " VB6C3_VERSION_STRING " (vb6.pro project)" << std::endl;
}

} // namespace vb6c3
