#pragma once
// 编译器驱动 - 命令行解析 + 编译流程编排

#include "preprocessor/preprocessor.hpp"
#include "semantics/symbol_table.hpp"
#include "semantics/type_system.hpp"
#include "com/typelib_parser.hpp"
#include "project/frm_parser.hpp"
#include <string>
#include <vector>
#include <memory>
#include <map>

namespace vb6c3 {

class Diagnostics;
class SourceBuffer;
class Module;
class SemanticAnalyzer;

// 编译选项
struct CompileOptions {
    std::vector<std::string> sourceFiles;   // 源文件列表
    std::string outputFile;                  // 输出文件路径
    std::string outputDir;                   // 输出目录 (默认: output)

    // 目标平台
    std::string target = "win-x86";          // win-x86, win-x64, linux-x64, etc.

    // GUI模式
    std::string guiMode = "native";          // native, webview, none

    // 输出控制
    bool dumpTokens = false;
    bool dumpAST = false;
    bool dumpIR = false;
    bool dumpPreprocess = false;              // 输出预处理后的token列表
    bool dumpSymbols = false;                 // 输出符号表
    bool emitC = false;                       // 输出C代码 (.h/.c)
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

    // COM TypeLib引用 (P6.3, 前期绑定)
    std::vector<std::string> typelibRefs;    // TypeLib路径或ProgID列表
    bool autoTypelib = true;                 // 自动从源码中提取COM类型并加载TypeLib

    // ActiveX DLL (P6.6)
    bool isDll = false;                      // 编译为ActiveX DLL (而非EXE)
    std::string dllProgId;                   // DLL的ProgID前缀 (如 "MyLib")

    // 窗体调试 (P7)
    bool dumpFrm = false;                    // 输出.frm窗体描述解析结果
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
    std::vector<std::unique_ptr<Module>> modules_;  // 解析产出的AST

    // 语义分析产出 (供代码生成使用)
    std::vector<std::unique_ptr<SemanticAnalyzer>> analyzers_;

    // 窗体描述 (P7, .frm文件解析结果, 按模块名索引)
    std::map<std::string, FrmFile> frmFiles_;

    // P6.8: VBP指定的类CLSID映射 (模块名小写 -> CLSID字符串)
    std::unordered_map<std::string, std::string> classClsidMap_;

    // TypeLib解析器 (P6.3, COM类型导入)
    std::unique_ptr<TypeLibParser> typelibParser_;

    // 编译流水线各阶段
    bool runLexer(const CompileOptions& options);
    bool runPreprocess(const CompileOptions& options);
    bool runParser(const CompileOptions& options);
    bool runTypeLibImport(const CompileOptions& options);  // P6.3: 加载TypeLib+注册COM类型
    bool runSemanticAnalysis(const CompileOptions& options);
    bool runCrossModuleResolution();  // 跨模块符号链接
    bool runCodeGeneration(const CompileOptions& options, const std::string& outputDir);
    bool runLinker(const CompileOptions& options, const std::string& outputDir);
};

} // namespace vb6c3