#pragma once
// 编译器驱动 - 命令行解析 + 编译流程编排

#include "preprocessor/preprocessor.hpp"
#include <string>
#include <vector>
#include <memory>

namespace vb6c3 {

class Diagnostics;
class SourceBuffer;

// 编译选项
struct CompileOptions {
    std::vector<std::string> sourceFiles;   // 源文件列表
    std::string outputFile;                  // 输出文件路径

    // 目标平台
    std::string target = "win-x86";          // win-x86, win-x64, linux-x64, etc.

    // GUI模式
    std::string guiMode = "native";          // native, webview, none

    // 输出控制
    bool dumpTokens = false;
    bool dumpAST = false;
    bool dumpIR = false;
    bool dumpPreprocess = false;              // 输出预处理后的token列表
    bool emitLLVM = false;                   // 输出.ll文件
    bool syntaxOnly = false;                 // 只做语法检查
    bool verbose = false;

    // 条件编译
    std::vector<std::string> defines;        // -d:NAME=VALUE 或 --define NAME=VALUE

    // 优化
    int optimizationLevel = 0;               // 0=无, 1/2/3

    // 调试
    bool debugInfo = false;

    // 兼容性
    bool compatCheck = false;                // 跨平台兼容性检查
};

// 编译结果
struct CompileResult {
    bool success = false;
    std::string outputFile;
    int errorCount = 0;
    int warningCount = 0;
};

// 编译器驱动
class Driver {
public:
    Driver();
    ~Driver();

    // 从命令行参数编译
    CompileResult compile(int argc, char* argv[]);

    // 从选项编译
    CompileResult compile(const CompileOptions& options);

    // 解析命令行参数
    static std::pair<CompileOptions, int> parseArgs(int argc, char* argv[]);

    // 打印帮助/版本
    static void printHelp();
    static void printVersion();

private:
    std::unique_ptr<Diagnostics> diag_;

    // 编译流水线各阶段
    bool runLexer(const CompileOptions& options);
    bool runPreprocess(const CompileOptions& options);
    bool runParser(const CompileOptions& options);
    bool runSemanticAnalysis(const CompileOptions& options);
    bool runCodeGeneration(const CompileOptions& options);
    bool runLinker(const CompileOptions& options);
};

} // namespace vb6c3
