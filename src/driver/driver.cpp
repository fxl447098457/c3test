#include "driver/driver.hpp"
#include "common/diagnostics.hpp"
#include "common/source_manager.hpp"
#include "lexer/lexer.hpp"
#include "lexer/token.hpp"
#include "preprocessor/preprocessor.hpp"
#include "parser/parser.hpp"
#include "ast/ast_printer.hpp"
#include "semantics/semantic_analyzer.hpp"
#include "backend/cgen.hpp"
#include "backend/msvc_driver.hpp"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <cstdlib>

namespace vb6c3 {

Driver::Driver() : diag_(std::make_unique<Diagnostics>()) {}
Driver::~Driver() = default;

// === 命令行解析 ===

std::pair<CompileOptions, int> Driver::parseArgs(int argc, char* argv[]) {
    CompileOptions opts;
    int resultCode = 0;

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
        else if (arg == "--syntax-only") {
            opts.syntaxOnly = true;
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
        else if (arg == "-d" || arg == "--define") {
            if (i + 1 < argc) {
                opts.defines.push_back(argv[++i]);
            }
        }
        else if (arg[0] == '-') {
            std::cerr << "c3: 未知选项: " << arg << std::endl;
            resultCode = 1;
        }
        else {
            opts.sourceFiles.push_back(arg);
        }
    }

    if (opts.sourceFiles.empty() && resultCode == 0) {
        std::cerr << "c3: 错误: 未指定源文件" << std::endl;
        resultCode = 1;
    }

    return {opts, resultCode};
}

// === 编译入口 ===

CompileResult Driver::compile(int argc, char* argv[]) {
    auto [opts, code] = parseArgs(argc, argv);
    if (code != 0 || opts.sourceFiles.empty()) {
        return {false, "", 1, 0};
    }
    return compile(opts);
}

CompileResult Driver::compile(const CompileOptions& options) {
    CompileResult result;
    diag_->clear();

    // === 阶段1: 词法分析 ===
    if (!runLexer(options)) {
        std::cerr << diag_->toString();
        result.errorCount = diag_->errorCount();
        result.warningCount = diag_->warningCount();
        return result;
    }

    // dump-tokens模式: 输出完token就结束, 不继续流水线
    if (options.dumpTokens) {
        result.success = true;
        return result;
    }

    // === 阶段1.5: 预处理 (条件编译) ===
    if (!runPreprocess(options)) {
        std::cerr << diag_->toString();
        result.errorCount = diag_->errorCount();
        result.warningCount = diag_->warningCount();
        return result;
    }

    // dump-preprocess模式: 输出完预处理token就结束
    if (options.dumpPreprocess) {
        result.success = true;
        return result;
    }

    // === 阶段2: 语法分析 ===
    if (!runParser(options)) {
        std::cerr << diag_->toString();
        result.errorCount = diag_->errorCount();
        result.warningCount = diag_->warningCount();
        return result;
    }

    // dump-ast模式: 已在runParser中输出, 结束
    if (options.dumpAST) {
        result.success = true;
        return result;
    }

    // === 阶段3: 语义分析 ===
    if (!runSemanticAnalysis(options)) {
        std::cerr << diag_->toString();
        result.errorCount = diag_->errorCount();
        result.warningCount = diag_->warningCount();
        return result;
    }

    // 如果只做语法检查, 到此结束
    if (options.syntaxOnly) {
        result.success = true;
        result.errorCount = diag_->errorCount();
        result.warningCount = diag_->warningCount();
        return result;
    }

    // === 阶段4: 代码生成 ===
    // 确定输出目录
    std::string outputDir = options.outputDir.empty() ? "output" : options.outputDir;
    if (!std::filesystem::exists(outputDir)) {
        std::filesystem::create_directories(outputDir);
    }

    if (!runCodeGeneration(options, outputDir)) {
        std::cerr << diag_->toString();
        result.errorCount = diag_->errorCount();
        result.warningCount = diag_->warningCount();
        return result;
    }

    // === 阶段5: 链接 ===
    if (!runLinker(options, outputDir)) {
        std::cerr << diag_->toString();
        result.errorCount = diag_->errorCount();
        result.warningCount = diag_->warningCount();
        return result;
    }

    result.success = true;
    result.outputFile = options.outputFile;
    result.errorCount = diag_->errorCount();
    result.warningCount = diag_->warningCount();
    return result;
}

// === 词法分析阶段 ===

bool Driver::runLexer(const CompileOptions& options) {
    if (options.dumpTokens) {
        for (const auto& filePath : options.sourceFiles) {
            auto buffer = SourceBuffer::fromFile(filePath);
            if (!buffer) {
                SourceLocation loc{filePath, 0, 0};
                diag_->error(DiagnosticID::LexFileEncodingError, loc,
                    "无法打开文件: " + filePath);
                return false;
            }

            Lexer lexer(std::move(buffer), *diag_);
            Token tok;
            do {
                tok = lexer.nextToken();
                std::cout << tok.toString() << std::endl;
            } while (tok.kind != TokenKind::EndOfFile);
        }

        // dump-tokens模式下, 输出完token就结束
        return true;
    }

    // 正常模式: 词法分析结果传给语法分析
    // TODO: 将token流缓存供后续阶段使用
    return true;
}

// === 预处理阶段 (条件编译) ===

bool Driver::runPreprocess(const CompileOptions& options) {
    if (!options.dumpPreprocess && options.defines.empty()) {
        // 无需单独预处理, 由Parser内部的Preprocessor处理
        return true;
    }

    if (options.dumpPreprocess) {
        // 解析命令行定义
        PreprocessOptions ppOpts;
        for (const auto& def : options.defines) {
            // NAME=VALUE 格式
            auto eq = def.find('=');
            if (eq != std::string::npos) {
                std::string name = def.substr(0, eq);
                std::string valStr = def.substr(eq + 1);
                std::string nameLower = name;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                // 尝试解析为布尔或整数
                if (valStr == "True" || valStr == "true" || valStr == "-1") {
                    ppOpts.defines[nameLower] = CondCompileValue::fromBool(true);
                } else if (valStr == "False" || valStr == "false" || valStr == "0") {
                    ppOpts.defines[nameLower] = CondCompileValue::fromBool(false);
                } else {
                    try {
                        ppOpts.defines[nameLower] = CondCompileValue::fromLong(std::stoll(valStr));
                    } catch (...) {
                        ppOpts.defines[nameLower] = CondCompileValue::fromBool(false);
                    }
                }
            }
        }

        for (const auto& filePath : options.sourceFiles) {
            auto buffer = SourceBuffer::fromFile(filePath);
            if (!buffer) {
                SourceLocation loc{filePath, 0, 0};
                diag_->error(DiagnosticID::LexFileEncodingError, loc,
                    "无法打开文件: " + filePath);
                return false;
            }

            Preprocessor preproc(std::move(buffer), *diag_, ppOpts);
            Token tok;
            do {
                tok = preproc.nextToken();
                std::cout << tok.toString() << std::endl;
            } while (tok.kind != TokenKind::EndOfFile);

            // 输出条件编译常量表
            if (options.verbose) {
                std::cout << "\n--- 条件编译常量 ---" << std::endl;
                for (const auto& [name, val] : preproc.constants()) {
                    std::cout << "  " << name << " = ";
                    if (val.kind == CondCompileValue::Boolean) {
                        std::cout << (val.boolValue ? "True" : "False");
                    } else if (val.kind == CondCompileValue::Long) {
                        std::cout << val.longValue;
                    } else {
                        std::cout << "<undefined>";
                    }
                    std::cout << std::endl;
                }
            }
        }
    }

    return true;
}

// === 解析阶段 ===

bool Driver::runParser(const CompileOptions& options) {
    // 解析命令行定义
    PreprocessOptions ppOpts;
    for (const auto& def : options.defines) {
        auto eq = def.find('=');
        if (eq != std::string::npos) {
            std::string name = def.substr(0, eq);
            std::string valStr = def.substr(eq + 1);
            std::string nameLower = name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
            if (valStr == "True" || valStr == "true" || valStr == "-1") {
                ppOpts.defines[nameLower] = CondCompileValue::fromBool(true);
            } else if (valStr == "False" || valStr == "false" || valStr == "0") {
                ppOpts.defines[nameLower] = CondCompileValue::fromBool(false);
            } else {
                try {
                    ppOpts.defines[nameLower] = CondCompileValue::fromLong(std::stoll(valStr));
                } catch (...) {
                    ppOpts.defines[nameLower] = CondCompileValue::fromBool(false);
                }
            }
        }
    }

    for (const auto& filePath : options.sourceFiles) {
        auto buffer = SourceBuffer::fromFile(filePath);
        if (!buffer) {
            SourceLocation loc{filePath, 0, 0};
            diag_->error(DiagnosticID::LexFileEncodingError, loc,
                "无法打开文件: " + filePath);
            return false;
        }

        Parser parser(std::move(buffer), *diag_, ppOpts);
        auto module = parser.parseModule();

        if (options.dumpAST && module) {
            ASTPrinter printer(std::cout);
            printer.print(*module);
        }

        if (module) {
            modules_.push_back(std::move(module));
        }

        if (diag_->hasErrors()) {
            return false;
        }
    }
    return !diag_->hasErrors();
}

bool Driver::runSemanticAnalysis(const CompileOptions& options) {
    analyzers_.clear();
    for (auto& module : modules_) {
        auto analyzer = std::make_unique<SemanticAnalyzer>(*diag_, options.verbose);
        bool ok = analyzer->analyze(*module);

        if (options.dumpSymbols) {
            analyzer->dumpSymbols(std::cout);
        }

        if (!ok) return false;
        analyzers_.push_back(std::move(analyzer));
    }
    return !diag_->hasErrors();
}

bool Driver::runCodeGeneration(const CompileOptions& options, const std::string& outputDir) {
    if (modules_.size() != analyzers_.size()) {
        std::cerr << "c3: 内部错误: 模块数与分析器数不匹配" << std::endl;
        return false;
    }

    for (size_t i = 0; i < modules_.size(); i++) {
        auto& module = modules_[i];
        auto& analyzer = analyzers_[i];

        // 确定输出基名
        std::string baseName;
        if (modules_.size() == 1 && !options.outputFile.empty()) {
            // 单文件: 用输出文件名作为基名
            std::filesystem::path p(options.outputFile);
            baseName = p.stem().string();
        } else {
            // 多文件: 用源文件名作为基名
            std::filesystem::path p(module->filename);
            baseName = p.stem().string();
        }

        // 调用C代码生成器
        CCodeGen cgen(*diag_, analyzer->symbolTable(), analyzer->typeSystem(),
                      options.verbose);
        bool ok = cgen.generate(*module, baseName);
        if (!ok) return false;

        // 写 .h 文件
        std::string hPath = outputDir + "/" + baseName + ".h";
        {
            std::ofstream ofs(hPath, std::ios::out | std::ios::trunc);
            if (!ofs) {
                std::cerr << "c3: 无法写入文件: " << hPath << std::endl;
                return false;
            }
            ofs << cgen.headerCode();
        }

        // 写 .c 文件
        std::string cPath = outputDir + "/" + baseName + ".c";
        {
            std::ofstream ofs(cPath, std::ios::out | std::ios::trunc);
            if (!ofs) {
                std::cerr << "c3: 无法写入文件: " << cPath << std::endl;
                return false;
            }
            ofs << cgen.sourceCode();
        }

        if (options.verbose) {
            std::cout << "c3: 生成 " << hPath << " (" << cgen.headerCode().size() << " bytes)" << std::endl;
            std::cout << "c3: 生成 " << cPath << " (" << cgen.sourceCode().size() << " bytes)" << std::endl;
        }

        // --emit-c 模式: 输出C代码后结束
        if (options.emitC) {
            std::cout << cgen.headerCode() << std::endl;
            std::cout << cgen.sourceCode() << std::endl;
        }
    }

    return !diag_->hasErrors();
}

bool Driver::runLinker(const CompileOptions& options, const std::string& outputDir) {
    // 如果是 --emit-c 模式, 不需要链接
    if (options.emitC) {
        return true;
    }

    // 检查 MSVC 是否可用
    if (!MsvcDriver::isMsvcAvailable()) {
        std::cerr << "c3: 错误: 未检测到MSVC环境 (请先运行vcvarsall.bat)" << std::endl;
        std::cerr << "c3: 使用 --emit-c 选项可仅生成C代码" << std::endl;
        return false;
    }

    // 收集生成的 .c 文件
    MsvcDriverOptions msvcOpts;
    for (auto& module : modules_) {
        std::filesystem::path p(module->filename);
        std::string baseName = p.stem().string();
        std::string cPath = outputDir + "/" + baseName + ".c";
        msvcOpts.sourceFiles.push_back(cPath);
    }

    // 查找RTL目录: 优先VB6RTL_DIR环境变量, 其次尝试相对路径
    std::string rtlDir;
    const char* envRtl = std::getenv("VB6RTL_DIR");
    if (envRtl && envRtl[0] != '\0') {
        rtlDir = envRtl;
    } else {
        // 尝试从当前工作目录向上查找 src/rtl/core
        std::filesystem::path search = std::filesystem::current_path();
        for (int i = 0; i < 10; i++) {
            std::filesystem::path candidate = search / "src" / "rtl" / "core";
            if (std::filesystem::exists(candidate / "vb6rtl.h")) {
                rtlDir = candidate.string();
                break;
            }
            auto parent = search.parent_path();
            if (parent == search) break;
            search = parent;
        }
    }

    if (rtlDir.empty()) {
        std::cerr << "c3: 错误: 找不到VB6 RTL目录 (请设置VB6RTL_DIR环境变量)" << std::endl;
        return false;
    }
    msvcOpts.rtlDir = rtlDir;

    // 输出文件 - 放入 outputDir
    if (!options.outputFile.empty()) {
        // 用户指定了绝对/相对路径, 直接使用
        msvcOpts.outputFile = options.outputFile;
    } else if (modules_.size() == 1) {
        std::filesystem::path p(modules_[0]->filename);
        msvcOpts.outputFile = outputDir + "/" + p.stem().string() + ".exe";
    } else {
        msvcOpts.outputFile = outputDir + "/a.exe";
    }

    msvcOpts.verbose = options.verbose;
    msvcOpts.debugInfo = options.debugInfo;
    msvcOpts.optimizationLevel = options.optimizationLevel;

    MsvcDriver msvc;
    return msvc.compileAndLink(msvcOpts);
}

// === 帮助/版本 ===

void Driver::printHelp() {
    std::cout << "c3 - Visual Basic 6.0 Compiler\n"
              << "\n"
              << "用法: c3 [选项] <源文件...>\n"
              << "\n"
              << "选项:\n"
              << "  -o <文件>          输出文件路径\n"
              << "  --output-dir <目录> 输出目录 (默认: output)\n"
              << "  --target <平台>     目标平台 (win-x86, win-x64, linux-x64, macos-arm64)\n"
              << "  --gui <模式>        GUI模式 (native, webview, none)\n"
              << "  --dump-tokens       输出token列表\n"
              << "  --dump-preprocess   输出预处理后的token列表\n"
              << "  --dump-ast          输出AST\n"
              << "  --dump-symbols      输出符号表\n"
              << "  --dump-ir           输出IR\n"
              << "  --emit-c           输出C代码 (.h/.c)\n"
              << "  --emit-llvm         输出LLVM IR (.ll)\n"
              << "  --syntax-only       只做语法检查\n"
              << "  -d, --define <N=V>  定义条件编译常量 (如 -d:DEBUG=-1)\n"
              << "  -O <级别>           优化级别 (0-3)\n"
              << "  -g, --debug         生成调试信息\n"
              << "  --compat-check      跨平台兼容性检查\n"
              << "  -v, --verbose       详细输出\n"
              << "  -h, --help          显示帮助\n"
              << "  -V, --version       显示版本\n"
              << "\n"
              << "示例:\n"
              << "  c3 hello.bas -o hello.exe\n"
              << "  c3 module.bas --dump-tokens\n"
              << "  c3 app.vbp --target win-x64\n"
              << "  c3 module.bas -d:WIN64=-1 --dump-preprocess\n";
}

void Driver::printVersion() {
    std::cout << "c3 version 0.1.0 (vb6.pro project)" << std::endl;
}

} // namespace vb6c3
