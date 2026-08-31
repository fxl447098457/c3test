#include "driver/driver.hpp"
#include "common/diagnostics.hpp"
#include "common/encoding.hpp"
#include "common/source_manager.hpp"
#include "lexer/lexer.hpp"
#include "lexer/token.hpp"
#include "preprocessor/preprocessor.hpp"
#include "parser/parser.hpp"
#include "ast/ast_printer.hpp"
#include "semantics/semantic_analyzer.hpp"
#include "backend/cgen.hpp"
#include "backend/msvc_driver.hpp"
#include "typelib/typelib_builder.hpp"
#include "driver/rtl_embedded.hpp"
#include "project/vbp_parser.hpp"
#include "project/frm_parser.hpp"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <cstdlib>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#endif

namespace vb6c3 {

// pathToUtf8/utf8ToWide/utf8ToPath moved to common/encoding.hpp

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
        else if (arg == "--keep-for-debug") {
            opts.keepTemps = true;  // 隐藏参数: 保留中间文件便于调试
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

    // === 阶段0: VBP工程文件解析 ===
    // 如果输入是.vbp文件, 展开源文件列表
    CompileOptions effectiveOpts = options;
    if (options.sourceFiles.size() == 1) {
        const auto& srcFile = options.sourceFiles[0];
        if (srcFile.size() >= 4 &&
            (srcFile.compare(srcFile.size()-4, 4, ".vbp") == 0 ||
             srcFile.compare(srcFile.size()-4, 4, ".VBP") == 0)) {

            VbpProject project = VbpParser::parse(srcFile);
            if (project.sources.empty()) {
                std::cerr << "C3: 错误: .vbp文件中没有源文件: " << srcFile << std::endl;
                result.errorCount = 1;
                return result;
            }

            if (options.verbose) {
                std::cout << "C3: 加载工程: " << project.projectName
                          << " (" << project.sources.size() << " 个源文件)" << std::endl;
            }

            // 展开源文件列表 (将相对路径转为绝对路径)
            effectiveOpts.sourceFiles.clear();
            for (const auto& entry : project.sources) {
                auto absPath = project.resolvePath(entry.filePath);
                effectiveOpts.sourceFiles.push_back(pathToUtf8(absPath));
            }

            // P6.8: 收集VBP中的CLSID映射
            for (const auto& entry : project.sources) {
                if (!entry.clsidStr.empty()) {
                    std::string lowerName = entry.moduleName;
                    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                    classClsidMap_[lowerName] = entry.clsidStr;
                }
            }

            // 如果没有指定输出文件, 使用工程名
            // 保存VBP工程基名 (用于输出文件命名)
            // 优先使用ExeName32的stem, 否则用VBP文件名
            if (!project.exeName.empty()) {
                // project.exeName is already UTF-8 from VBP parser (GBK→UTF-8)
                // Must construct path from wstring to avoid ACP reinterpretation
                std::filesystem::path exePath(utf8ToPath(project.exeName));
                projectBaseName_ = pathToUtf8(exePath.stem());
            } else {
                std::filesystem::path vbpPath(srcFile);
                projectBaseName_ = pathToUtf8(vbpPath.stem());
            }
            // 注意: 不再设置effectiveOpts.outputFile, 让runLinker通过projectBaseName_统一处理
            // 这样确保输出路径始终包含outputDir前缀
            // P11.1: Save VBP Path32 for output directory resolution
            if (!project.outputPath.empty()) {
                projectPath32_ = pathToUtf8(project.resolvePath(project.outputPath));
            }


            // P6.6: 从VBP工程类型推断是否为ActiveX DLL
            if (!effectiveOpts.isDll && project.projectType == VbpProjectType::ActiveXDLL) {
                effectiveOpts.isDll = true;
                if (effectiveOpts.verbose) {
                    std::cout << "C3: 检测到ActiveX DLL工程 (Type=DLL)" << std::endl;
                }
            }
            // ProgID前缀: 优先CLI指定, 否则用工程名
            if (effectiveOpts.isDll && effectiveOpts.dllProgId.empty()) {
                effectiveOpts.dllProgId = project.projectName.empty() ? "VB6DLL" : project.projectName;
            }
            // LibID: 优先CLI --libid, 其次VBP的LibID=字段, 都空则编译期自动生成
            // (typelib_builder.cpp:125 的 fallback)
            if (effectiveOpts.isDll && effectiveOpts.libidStr.empty() && !project.libidStr.empty()) {
                effectiveOpts.libidStr = project.libidStr;
                if (effectiveOpts.verbose) {
                    std::cout << "C3: 使用VBP指定的LibID: " << project.libidStr << std::endl;
                }
            }
            // P23-01: Feed VBP Reference= and Object= entries into TypeLib import pipeline
            // Reference= GUID -> loadByClsid, path -> loadByPath (fallback when GUID not in registry)
            for (const auto& ref : project.references) {
                if (!ref.guid.empty()) {
                    effectiveOpts.typelibRefs.push_back(ref.guid);
                }
                // P24-04: 同时推送resolved path作为fallback
                // 当GUID未注册(regsvr32未运行)时, loadByClsid会失败, 此时可loadByPath
                if (!ref.path.empty()) {
                    auto resolvedPath = project.resolvePath(ref.path);
                    if (std::filesystem::exists(resolvedPath)) {
                        effectiveOpts.typelibRefs.push_back(resolvedPath.u8string());
                    }
                }
            }
            // Object= GUID (ActiveX controls) -> loadByClsid
            for (const auto& obj : project.objects) {
                if (!obj.guid.empty()) {
                    effectiveOpts.typelibRefs.push_back(obj.guid);
                }
            }

            // P23-05: Collect version info from VBP for VS_VERSION_INFO resource
            verMajor_ = project.majorVer;
            verMinor_ = project.minorVer;
            verRevision_ = project.revisionVer;
            verCompanyName_ = project.companyName;
            verFileDescription_ = project.fileDescription;
            verLegalCopyright_ = project.legalCopyright;
            verProductName_ = project.productName;
            verComments_ = project.comments;
            verLegalTrademarks_ = project.legalTrademarks;
            verOriginalFileName_ = project.originalFileName;
            verTitle_ = project.title;
            // P23-03: Collect ResFile path
            if (!project.resFile.empty()) {
                userResFile_ = pathToUtf8(project.resolvePath(project.resFile));
            }
        }
    }

    // === P7: .frm窗体描述解析 ===
    if (effectiveOpts.dumpFrm) {
        bool hasFrm = false;
        for (const auto& srcFile : effectiveOpts.sourceFiles) {
            if (srcFile.size() >= 4 &&
                (srcFile.compare(srcFile.size()-4, 4, ".frm") == 0 ||
                 srcFile.compare(srcFile.size()-4, 4, ".FRM") == 0)) {
                hasFrm = true;
                auto frm = FrmParser::parse(srcFile);

                std::cout << "=== FrmParser: " << srcFile << " ===\n";
                std::cout << "Version: " << frm.version << "\n";
                std::cout << "FormName: " << frm.form.formName << "\n";
                std::cout << "ControlType: " << FrmParser::controlTypeToVb6Name(frm.form.formControl.controlType) << "\n";

                // 窗体属性
                std::cout << "Properties:\n";
                for (const auto& [k, v] : frm.form.formControl.properties) {
                    std::cout << "  " << k << " = " << v.rawText << "\n";
                }

                // 控件列表
                std::cout << "Controls (" << frm.form.formControl.children.size() << "):\n";
                for (const auto& ctrl : frm.form.formControl.children) {
                    std::cout << "  " << FrmParser::controlTypeToVb6Name(ctrl.controlType)
                              << " " << ctrl.controlName;
                    if (ctrl.index >= 0) std::cout << "(" << ctrl.index << ")";
                    std::cout << " [" << ctrl.controlTypeName << "]\n";
                    for (const auto& [k, v] : ctrl.properties) {
                        std::cout << "    " << k << " = " << v.rawText << "\n";
                    }
                    // 子控件
                    for (const auto& child : ctrl.children) {
                        std::cout << "    " << FrmParser::controlTypeToVb6Name(child.controlType)
                                  << " " << child.controlName << "\n";
                    }
                }

                // 复合属性块
                for (const auto& block : frm.form.formControl.propertyBlocks) {
                    std::cout << "PropertyBlock: " << block.blockName << "\n";
                    for (const auto& [k, v] : block.properties) {
                        std::cout << "  " << k << " = " << v.rawText << "\n";
                    }
                }

                std::cout << "CodeSection: " << frm.codeSection.size() << " chars\n";
                std::cout << "\n";
            }
        }
        if (!hasFrm) {
            std::cerr << "C3: --dump-frm: 未找到.frm文件\n";
        }
    result.success = true;
        return result;
    }

    // === 阶段1: 词法分析 ===
    if (!runLexer(effectiveOpts)) {
        std::cerr << diag_->toString();
        result.errorCount = diag_->errorCount();
        result.warningCount = diag_->warningCount();
        return result;
    }

    // dump-tokens模式: 输出完token就结束, 不继续流水线
    if (effectiveOpts.dumpTokens) {
        result.success = true;
        return result;
    }

    // === 阶段1.5: 预处理 (条件编译) ===
    if (!runPreprocess(effectiveOpts)) {
        std::cerr << diag_->toString();
        result.errorCount = diag_->errorCount();
        result.warningCount = diag_->warningCount();
        return result;
    }

    // dump-preprocess模式: 输出完预处理token就结束
    if (effectiveOpts.dumpPreprocess) {
        result.success = true;
        return result;
    }

    // === 阶段2: 语法分析 ===
    if (!runParser(effectiveOpts)) {
        std::cerr << diag_->toString();
        result.errorCount = diag_->errorCount();
        result.warningCount = diag_->warningCount();
        return result;
    }

    // dump-ast模式: 已在runParser中输出, 结束
    if (effectiveOpts.dumpAST) {
        result.success = true;
        return result;
    }

    // === 阶段2.5: TypeLib导入 (P6.3, 前期绑定) ===
    if (!runTypeLibImport(effectiveOpts)) {
        // TypeLib加载失败不阻断编译, 仅降级为后期绑定
        if (effectiveOpts.verbose) {
            std::cerr << "C3: note: TypeLib import skipped, using late binding" << std::endl;
        }
    }

    // === 阶段3: 语义分析 ===
    if (!runSemanticAnalysis(effectiveOpts)) {
        std::cerr << diag_->toString();
        result.errorCount = diag_->errorCount();
        result.warningCount = diag_->warningCount();
        return result;
    }

    // === 阶段3.5: 跨模块符号链接 ===
    // 多模块项目: 解析跨模块Public符号引用
    if (modules_.size() > 1) {
        if (!runCrossModuleResolution()) {
            std::cerr << diag_->toString();
            result.errorCount = diag_->errorCount();
            result.warningCount = diag_->warningCount();
            return result;
        }
    }

    // === 阶段3.6: P6.4 标记接口类 ===
    // 遍历所有模块的类符号, 将被Implements引用的类标记为isInterface
    for (size_t i = 0; i < analyzers_.size(); i++) {
        SymbolTable& symTab = analyzers_[i]->symbolTable();
        for (auto& [key, sym] : symTab.moduleScope()->symbols()) {
            if (sym->kind == SymbolKind::Class && !sym->implementsNames.empty()) {
                // 这个类有Implements列表, 它实现的接口需要标记
                for (const auto& ifaceName : sym->implementsNames) {
                    // 在所有模块中查找并标记接口类
                    for (size_t j = 0; j < analyzers_.size(); j++) {
                        SymbolTable& otherSymTab = analyzers_[j]->symbolTable();
                        Symbol* ifaceSym = otherSymTab.lookupModule(ifaceName);
                        if (ifaceSym && ifaceSym->kind == SymbolKind::Class) {
                            ifaceSym->isInterface = true;
                        }
                    }
                }
            }
        }
    }

    // 如果只做语法检查, 到此结束
    if (effectiveOpts.syntaxOnly) {
        result.success = true;
        result.errorCount = diag_->errorCount();
        result.warningCount = diag_->warningCount();
        return result;
    }

    // === P11.1: Determine output directory ===
    // Priority: user-specified --output-dir > VBP Path32 > source file directory
    std::string outputDir;
    if (!effectiveOpts.outputDir.empty()) {
        // User explicitly specified --output-dir
        outputDir = effectiveOpts.outputDir;
    } else if (!projectPath32_.empty()) {
        // VBP specified Path32
        outputDir = projectPath32_;
    } else {
        // Default: source file directory
        if (effectiveOpts.sourceFiles.size() == 1) {
            std::filesystem::path srcPath(utf8ToPath(effectiveOpts.sourceFiles[0]));
            outputDir = pathToUtf8(srcPath.parent_path());
            // parent_path() returns empty for bare filename (e.g. "hello.bas")
            if (outputDir.empty()) outputDir = ".";
        } else {
            outputDir = ".";
        }
    }
    outputDir = pathToUtf8(std::filesystem::absolute(utf8ToPath(outputDir)));
    if (!std::filesystem::exists(utf8ToPath(outputDir))) {
        std::filesystem::create_directories(utf8ToPath(outputDir));
    }

    // === P11.2: Create session for intermediates ===
    SessionManager session;
    std::string rtlDir = session.create(effectiveOpts.arch);
    if (rtlDir.empty()) {
        std::cerr << "C3: error: failed to create session directory" << std::endl;
        result.errorCount = 1;
        return result;
    }
    // Intermediates (.c/.h/.obj) go to session root dir; RTL is in session_dir/rtl/
    std::string intermediatesDir = session.sessionDir();
    // === Stage 4: Code Generation ===
    if (!runCodeGeneration(effectiveOpts, intermediatesDir)) {
        std::cerr << diag_->toString();
        writeErrorLog(outputDir + "/c3-error.log", "code-generation");
        result.errorCount = diag_->errorCount();
        result.warningCount = diag_->warningCount();
        session.release();
        return result;
    }

    // === Stage 5: Link ===
    if (!runLinker(effectiveOpts, outputDir, intermediatesDir, session)) {
        std::cerr << diag_->toString();
        writeErrorLog(outputDir + "/c3-error.log", "linking");
        result.errorCount = diag_->errorCount();
        result.warningCount = diag_->warningCount();
        std::cout << "C3: [debug] intermediates kept at: " << intermediatesDir << std::endl;
        session.release();
        return result;
    }

    // Success: clean up intermediates (unless --keep-for-debug)
    if (!effectiveOpts.keepTemps) {
        session.cleanup();
    } else {
        session.release();  // clear paths so destructor won't delete
        std::cout << "C3: [debug] intermediates kept at: " << intermediatesDir << std::endl;
    }

    result.success = true;
    result.outputFile = effectiveOpts.outputFile;
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
        ppOpts.is64Bit = (options.arch == "x64");  // Fix 081h
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
    ppOpts.is64Bit = (options.arch == "x64");  // Fix 081h
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
        // 根据文件扩展名判断模块类型
        bool isClassModule = false;
        bool isFormModule = false;
        FrmFile frmDesc;  // P7: 窗体描述 (仅.frm有效)
        if (filePath.size() >= 4) {
            std::string ext = filePath.substr(filePath.size() - 4);
            for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            isClassModule = (ext == ".cls");
            isFormModule = (ext == ".frm");  // P7: 窗体模块
        }

        // M22: 统一编码处理 — .frm经由FrmParser读取(已含编码转换+代码段提取),
        // .bas/.cls经由SourceBuffer::fromFile读取(含编码转换)
        std::unique_ptr<SourceBuffer> buffer;
        if (isFormModule) {
            // P7: 解析.frm窗体描述, 提取VB代码段
            // M22: FrmParser.parse已使用readAndConvertToUtf8, 返回的codeSection是UTF-8
            frmDesc = FrmParser::parse(filePath);
            // P24: 设置 .frx 文件路径 (与 .frm 同目录同名)
            {
                auto frxPath = frmDesc.frmFilePath;
                frxPath.replace_extension(".frx");
                if (std::filesystem::exists(frxPath)) {
                    frmDesc.form.frxFilePath = frxPath;
                }
            }
            if (!frmDesc.codeSection.empty()) {
                buffer = SourceBuffer::fromString(filePath, frmDesc.codeSection);
            } else {
                // 无代码段的.frm: 用fromFile读取完整内容
                buffer = SourceBuffer::fromFile(filePath);
            }
        } else {
            buffer = SourceBuffer::fromFile(filePath);
        }

        if (!buffer) {
            SourceLocation loc{filePath, 0, 0};
            diag_->error(DiagnosticID::LexFileEncodingError, loc,
                "无法打开文件: " + filePath);
            return false;
        }

        Parser parser(std::move(buffer), *diag_, ppOpts);
        auto module = parser.parseModule(isClassModule);

        // P7: 设置窗体模块标志
        if (module && isFormModule) {
            module->isFormModule = true;

        }

        if (options.dumpAST && module) {
            ASTPrinter printer(std::cout);
            printer.print(*module);
        }

        if (module) {
            // 从 Attribute VB_Name 提取模块名
            // VB6 模块名来自 Attribute VB_Name = "ModuleName"
            for (const auto& attr : module->attributes) {
                if (attr->attrName == "VB_Name" && attr->value) {
                    // 值应为字符串字面量
                    if (attr->value->kind == ASTNodeKind::LiteralExpr) {
                        auto& lit = static_cast<LiteralExpr&>(*attr->value);
                        if (lit.literalKind == LiteralKind::String && !lit.rawText.empty()) {
                            // rawText包含引号, 去掉首尾引号
                            std::string name = lit.rawText;
                            if (name.size() >= 2 && name.front() == '"' && name.back() == '"') {
                                name = name.substr(1, name.size() - 2);
                            }
                            module->moduleName = name;
                        }
                    }
                }
            }
            // 如果没有 VB_Name 属性，使用文件名（去掉扩展名）作为模块名
            if (module->moduleName.empty()) {
                std::filesystem::path p(utf8ToPath(filePath));
                module->moduleName = pathToUtf8(p.stem());
            }
            // P7: 保存窗体描述 (此时moduleName已从Attribute VB_Name或文件名确定)
            if (isFormModule && module->isFormModule) {
                frmFiles_[module->moduleName] = std::move(frmDesc);
            }

            modules_.push_back(std::move(module));
        }

        if (diag_->hasErrors()) {
            return false;
        }
    }
    return !diag_->hasErrors();
}

bool Driver::runTypeLibImport(const CompileOptions& options) {
    // P6.3: 编译期TypeLib导入, 提取COM类型信息用于前期绑定
    // 此阶段在语义分析之前运行, 将TypeLib中的coclass/接口注册为全局符号

    typelibParser_ = std::make_unique<TypeLibParser>(*diag_);

    // 1. 加载显式引用的TypeLib
    for (const auto& ref : options.typelibRefs) {
        // 判断是文件路径还是ProgID
        if (options.verbose) std::cerr << "C3: Loading TypeLib ref: " << ref << std::endl;
        if (ref.find('.') != std::string::npos && ref.find('\\') == std::string::npos && ref.find('/') == std::string::npos) {
            // 含点但不含路径分隔符 → ProgID
            typelibParser_->loadByProgId(ref);
        } else if (ref.find('{') != std::string::npos) {
            // 含花括号 → CLSID
            typelibParser_->loadByClsid(ref);
        } else {
            // 否则视为文件路径
            typelibParser_->loadByPath(ref);
        }
    }

    // 2. 自动加载常用TypeLib (可被--no-auto-typelib关闭)
    if (options.autoTypelib) {
        // 常用COM组件ProgID列表
        static const std::vector<std::string> commonProgIds = {
            "Scripting.FileSystemObject",   // Scripting Runtime
            "Scripting.Dictionary",         // Scripting Runtime (同一TypeLib)
            "ADODB.Connection",             // ADO
            "Excel.Application",            // Excel
            "Word.Application",             // Word
            "Shell.Application",            // Shell
            "WScript.Shell",                // WScript
            "MSXML2.DOMDocument",           // MSXML
        };

        for (const auto& progId : commonProgIds) {
            // 仅在缓存中不存在时加载; silent=true: 缺失是预期(用户未显式引用),
            // 不报VB4001避免污染c3-error.log
            if (!typelibParser_->findCachedCoClass(progId)) {
                typelibParser_->loadByProgId(progId, true);
            }
        }
    }

    // 3. 输出加载结果 (verbose模式)
    if (options.verbose) {
        std::cerr << "C3: TypeLib import: ";
        int totalCoClasses = 0, totalIfaces = 0, totalModules = 0;
        for (auto& tl : typelibParser_->cachedResults()) {
            totalCoClasses += (int)tl->coclasses.size();
            totalIfaces += (int)tl->interfaces.size();
            totalModules += (int)tl->modules.size();
            // P24-04: verbose module details
            if (!tl->modules.empty()) {
                for (auto& m : tl->modules) {
                    std::cerr << "\n  MODULE: " << m->name << " dllPath=" << m->dllPath
                              << " funcs=" << m->functions.size() << " consts=" << m->constants.size();
                    for (auto& f : m->functions) {
                        std::cerr << "\n    func: " << f.realName << " memid=" << f.memid;
                    }
                }
            }
        }
        std::cerr << totalCoClasses << " coclasses, " << totalIfaces
                  << " interfaces, " << totalModules << " modules from "
                  << typelibParser_->cachedResults().size()
                  << " type libraries" << std::endl;
    }

    // P24-04: verbose - show each cached TypeLib info
    if (options.verbose) {
        for (auto& tl : typelibParser_->cachedResults()) {
            std::cerr << "  TL: " << tl->tlbPath << " cclasses=" << tl->coclasses.size() << " ifaces=" << tl->interfaces.size() << " mods=" << tl->modules.size() << std::endl;
        }
    }
    return true;  // TypeLib加载失败不阻断编译
}

bool Driver::runSemanticAnalysis(const CompileOptions& options) {
    analyzers_.clear();
    for (auto& module : modules_) {
        auto analyzer = std::make_unique<SemanticAnalyzer>(*diag_, options.verbose);

        // P6.3: 如果有TypeLib解析结果, 注入COM类型信息到符号表
        if (typelibParser_) {
            for (auto& tl : typelibParser_->cachedResults()) {
                for (auto& cc : tl->coclasses) {
                    // 注册ComClass符号
                    auto sym = std::make_unique<Symbol>(
                        SymbolKind::ComClass, cc->name, Vb6Type::Object,
                        SourceLocation{}, AccessLevel::Public);
                    sym->isBuiltin = true;
                    sym->comClsidStr = cc->clsidStr;
                    sym->comProgId = cc->progId;
                    sym->comDefaultIfaceName = cc->defaultIfaceName;

                    // 复制默认接口的方法签名到ComClass
                    if (cc->defaultIface) {
                        sym->comIidStr = cc->defaultIface->iidStr;
                        sym->comIsDual = cc->defaultIface->isDual;
                        sym->comVtblBase = cc->defaultIface->isDispatch ? 7 : 3;

                        // P24-10: 传播默认成员名 (DISPID_VALUE=0)
                        sym->comDefaultMemberName = cc->defaultIface->defaultMemberName;
                        sym->comDefaultMemberRealName = cc->defaultIface->defaultMemberRealName;

                        for (auto& member : cc->defaultIface->members) {
                            Symbol::ComMethodSig sig;
                            sig.realName = member.realName;
                            sig.memid = member.memid;
                            sig.vtableIndex = member.vtableIndex;
                            sig.returnType = member.returnType;
                            sig.isPropertyGet = (member.kind == ComMemberKind::PropertyGet);
                            sig.isPropertyPut = (member.kind == ComMemberKind::PropertyPut);
                            sig.isPropertyPutRef = (member.kind == ComMemberKind::PropertyPutRef);
                            for (auto& param : member.params) {
                                ParameterInfo pi;
                                pi.name = param.name;
                                pi.type = param.type;
                                pi.isByVal = (param.direction == ComParamDir::In);
                                pi.isOptional = param.isOptional;
                                sig.params.push_back(std::move(pi));
                            }
                            sym->comMethods[member.name] = std::move(sig);
                        }
                        // 填充memberNames (类成员名列表, 兼容现有逻辑)
                        for (auto& member : cc->defaultIface->members) {
                            sym->memberNames.push_back(member.realName);
                        }
                    }

                    // P13.23: 填充事件源接口信息 (用于外部COM WithEvents)
                    if (!cc->defaultSourceIfaceName.empty()) {
                        sym->comHasSourceIface = true;
                        sym->comSourceIfaceName = cc->defaultSourceIfaceName;
                        if (cc->defaultSourceIface) {
                            sym->comSourceIfaceIid = cc->defaultSourceIface->iidStr;
                            sym->comSourceIfaceIsDispOnly = cc->defaultSourceIface->isDispatch;
                            for (auto& member : cc->defaultSourceIface->members) {
                                sym->eventNames.push_back(member.realName);
                                sym->comEventDispids[Symbol::toLower(member.name)] = member.memid;
                                Symbol::ComMethodSig sig;
                                sig.realName = member.realName;
                                sig.memid = member.memid;
                                sig.vtableIndex = member.vtableIndex;
                                sig.returnType = member.returnType;
                                for (auto& param : member.params) {
                                    ParameterInfo pi;
                                    pi.name = param.name;
                                    pi.type = param.type;
                                    pi.isByVal = (param.direction == ComParamDir::In);
                                    pi.isOptional = param.isOptional;
                                    sig.params.push_back(pi);
                                }
                                sym->comSourceMethods[Symbol::toLower(member.name)] = std::move(sig);
                            }
                        }
                    }
                    analyzer->symbolTable().define(std::move(sym));
                }

                // 也注册ComInterface符号
                for (auto& iface : tl->interfaces) {
                    auto sym = std::make_unique<Symbol>(
                        SymbolKind::ComInterface, iface->name, Vb6Type::Object,
                        SourceLocation{}, AccessLevel::Public);
                    sym->isBuiltin = true;
                    sym->comIidStr = iface->iidStr;
                    sym->comIsDual = iface->isDual;
                    sym->comVtblBase = iface->isDispatch ? 7 : 3;

                    for (auto& member : iface->members) {
                        Symbol::ComMethodSig sig;
                        sig.realName = member.realName;
                        sig.memid = member.memid;
                        sig.vtableIndex = member.vtableIndex;
                        sig.returnType = member.returnType;
                        sig.isPropertyGet = (member.kind == ComMemberKind::PropertyGet);
                        sig.isPropertyPut = (member.kind == ComMemberKind::PropertyPut);
                        sig.isPropertyPutRef = (member.kind == ComMemberKind::PropertyPutRef);
                        for (auto& param : member.params) {
                            ParameterInfo pi;
                            pi.name = param.name;
                            pi.type = param.type;
                            pi.isByVal = (param.direction == ComParamDir::In);
                            pi.isOptional = param.isOptional;
                            sig.params.push_back(std::move(pi));
                        }
                        sym->comMethods[member.name] = std::move(sig);
                    }

                    analyzer->symbolTable().define(std::move(sym));
                }

                // Fix 018: 注册COM枚举成员为EnumMember符号 (TextCompare/adStateClosed 等)
                // COM 类型库的 TKIND_ENUM 解析后, 成员作为全局可见命名常量注入符号表.
                // hasConstValue=true 使 cgen 发出数值 (经 Fix 017-P1/P3 路径), 避免裸名 C2065.
                // Volume guard: 跳过枚举成员过多的类型库 (如 MSHTML 数千成员), 避免命名空间
                // 污染、内存膨胀 (125 模块 × N 成员) 和跨 enum 同名碰撞. 小型库 (Scripting ~30,
                // ADO ~200) 正常注册, 覆盖 TextCompare / adXXX 等已知 C2065.
                size_t totalEnumMembers = 0;
                for (auto& en : tl->enums) totalEnumMembers += en->members.size();
                if (totalEnumMembers <= 1000) {
                    for (auto& en : tl->enums) {
                        for (auto& m : en->members) {
                            auto enumSym = std::make_unique<Symbol>(
                                SymbolKind::EnumMember, m.name, Vb6Type::Long,
                                SourceLocation{}, AccessLevel::Public);
                            enumSym->isBuiltin = true;
                            enumSym->hasConstValue = true;
                            enumSym->constIntValue = m.value;
                            enumSym->constType = Vb6Type::Long;
                            analyzer->symbolTable().define(std::move(enumSym));
                        }
                    }
                }
            }
        }
            // P24-04: 注册ComModule符号 (TKIND_MODULE → ActiveX DLL全局函数命名空间)
            // VBMAN.Version() 这种调用: VBMAN是工程名/模块名, Version是全局函数
            for (auto& tl : typelibParser_->cachedResults()) {
                for (auto& mod : tl->modules) {
                    auto sym = std::make_unique<Symbol>(
                        SymbolKind::ComModule, mod->name, Vb6Type::Object,
                        SourceLocation{}, AccessLevel::Public);
                    sym->isBuiltin = true;
                    sym->comModuleDllPath = mod->dllPath;
                    for (auto& func : mod->functions) {
                        Symbol::ComMethodSig sig;
                        sig.realName = func.realName;
                        sig.memid = func.memid;
                        sig.vtableIndex = -1;  // 模块函数无vtable
                        sig.returnType = func.returnType;
                        sig.isPropertyGet = false;
                        sig.isPropertyPut = false;
                        sig.isPropertyPutRef = false;
                        for (auto& param : func.params) {
                            ParameterInfo pi;
                            pi.name = param.name;
                            pi.type = param.type;
                            pi.isByVal = (param.direction == ComParamDir::In);
                            pi.isOptional = param.isOptional;
                            sig.params.push_back(std::move(pi));
                        }
                        sym->comModuleFunctions[func.name] = std::move(sig);
                    }
                    analyzer->symbolTable().define(std::move(sym));
                }
            }

            // P24-04: 注册VB_GlobalNameSpace promoted函数为ComGlobalNs符号
            // GlobalNameSpace coclass (如sGlobal) 的默认接口Public方法提升为全局符号
            // 示例: VBMAN库的sGlobal._sGlobal.VBMAN() → 全局"VBMAN"函数 (SymbolKind::ComGlobalNs)
            // 代码生成: VBMAN() → vb6_ComCallObject(vb6_CreateObject(L"VBMANLIB.sGlobal"), L"VBMAN", NULL, 0)
            for (auto& tl : typelibParser_->cachedResults()) {
                for (auto& cc : tl->coclasses) {
                    if (!cc->isGlobalNamespace || !cc->defaultIface) continue;
                    // 只提升非标准方法 (排除IUnknown/IDispatch的7个标准方法)
                    // IDispatch dispatch接口: 前7个是IUnknown(3)+IDispatch(4)标准方法
                    size_t standardMethods = cc->defaultIface->isDispatch ? 7 : 3;
                    for (size_t mi = standardMethods; mi < cc->defaultIface->members.size(); mi++) {
                        auto& member = cc->defaultIface->members[mi];
                        // 每个promoted方法注册为独立的ComGlobalNs符号
                        auto sym = std::make_unique<Symbol>(
                            SymbolKind::ComGlobalNs, member.realName, member.returnType,
                            SourceLocation{}, AccessLevel::Public);
                        sym->isBuiltin = true;
                        sym->comClsidStr = cc->clsidStr;
                        sym->comProgId = cc->progId;
                        sym->comDefaultIfaceName = cc->defaultIfaceName;
                        sym->comDefaultIfaceIid = cc->defaultIface->iidStr;
                        sym->comDefaultMemberName = cc->defaultIface->defaultMemberName;
                        sym->comDefaultMemberRealName = cc->defaultIface->defaultMemberRealName;
                        sym->comGlobalNsMethodName = member.realName;
                        // 复制方法签名
                        Symbol::ComMethodSig sig;
                        sig.realName = member.realName;
                        sig.memid = member.memid;
                        sig.vtableIndex = member.vtableIndex;
                        sig.returnType = member.returnType;
                        sig.isPropertyGet = (member.kind == ComMemberKind::PropertyGet);
                        sig.isPropertyPut = (member.kind == ComMemberKind::PropertyPut);
                        sig.isPropertyPutRef = (member.kind == ComMemberKind::PropertyPutRef);
                        for (auto& param : member.params) {
                            ParameterInfo pi;
                            pi.name = param.name;
                            pi.type = param.type;
                            pi.isByVal = (param.direction == ComParamDir::In);
                            pi.isOptional = param.isOptional;
                            sig.params.push_back(std::move(pi));
                        }
                        sym->comMethods[member.name] = std::move(sig);
                        analyzer->symbolTable().define(std::move(sym));
                    }
                }
            }
        // P7.5: 注册窗体控件名为符号 (否则语义分析报"未声明的标识符")
        auto frmIt = frmFiles_.find(module->moduleName);
        if (frmIt != frmFiles_.end()) {
            const auto& frmDesc = frmIt->second.form;
            // 注册窗体名本身 (Form1.Caption 访问)
            {
                auto sym = std::make_unique<Symbol>(
                    SymbolKind::Variable, module->moduleName, Vb6Type::Object,
                    SourceLocation{}, AccessLevel::Public);
                sym->isBuiltin = true;
                analyzer->symbolTable().define(std::move(sym));
            }
            // 注册控件名 (去重: 控件数组只注册一次)
            std::unordered_set<std::string> registeredCtrls;
            for (const auto& ctrl : frmDesc.formControl.children) {
                std::string ctrlLower = ctrl.controlName;
                for (auto& c : ctrlLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (registeredCtrls.count(ctrlLower)) continue;
                registeredCtrls.insert(ctrlLower);
                auto sym = std::make_unique<Symbol>(
                    SymbolKind::Variable, ctrl.controlName, Vb6Type::Object,
                    SourceLocation{}, AccessLevel::Public);
                sym->isBuiltin = true;
                analyzer->symbolTable().define(std::move(sym));
            }
        }

        // Fix 047: Pre-register Public Enum and UDT types from all other modules
        // so that parameter types like "As UcsAsyncSocketEventMaskEnum" resolve
        // to Long/UserDefinedType during Pass 1 (before runCrossModuleResolution).
        // Without this, cross-module Enum/UDT parameter types are resolved as Variant,
        // causing false-positive vb6_VariantFromValue() wrapping at call sites → C2440.
        for (auto& otherModule : modules_) {
            if (otherModule.get() == module.get()) continue;
            for (auto& decl : otherModule->declarations) {
                if (decl->kind == ASTNodeKind::EnumDecl) {
                    auto& enumDecl = static_cast<EnumDecl&>(*decl);
                    // AccessLevel::Default == Public, so all non-Private enums are public
                    if (enumDecl.access != AccessLevel::Private) {
                        std::string lower = Symbol::toLower(enumDecl.name);
                        if (!analyzer->symbolTable().lookupModule(lower)) {
                            auto sym = std::make_unique<Symbol>(
                                SymbolKind::EnumType, enumDecl.name,
                                Vb6Type::Long, enumDecl.loc, AccessLevel::Public
                            );
                            sym->isExternal = true;
                            sym->sourceModule = otherModule->moduleName;
                            analyzer->symbolTable().defineExternal(std::move(sym));
                        }
                    }
                } else if (decl->kind == ASTNodeKind::TypeDecl) {
                    auto& typeDecl = static_cast<TypeDecl&>(*decl);
                    if (typeDecl.access != AccessLevel::Private) {
                        std::string lower = Symbol::toLower(typeDecl.name);
                        if (!analyzer->symbolTable().lookupModule(lower)) {
                            auto sym = std::make_unique<Symbol>(
                                SymbolKind::UserDefinedType, typeDecl.name,
                                Vb6Type::UserDefinedType, typeDecl.loc, AccessLevel::Public
                            );
                            sym->isExternal = true;
                            sym->sourceModule = otherModule->moduleName;
                            analyzer->symbolTable().defineExternal(std::move(sym));
                        }
                    }
                }
            }
        }

        bool ok = analyzer->analyze(*module);

        if (options.dumpSymbols) {
            analyzer->dumpSymbols(std::cout);
        }

        if (!ok) return false;
        analyzers_.push_back(std::move(analyzer));
    }
    return !diag_->hasErrors();
}

// === 跨模块符号链接 ===
// 遍历每个模块的符号表，查找未定义的标识符，在其他模块的Public符号中查找匹配
// 为匹配到的符号注入 isExternal=true + sourceModule 的外部符号

bool Driver::runCrossModuleResolution() {
    if (modules_.size() != analyzers_.size()) return false;

    // Fix 013: 为每个模块计算模块名 — 使用 module.moduleName (VB_Name)
    // 而非文件名stem, 确保与 clsSym->name / mapTypeRef 生成的类型名一致
    std::vector<std::string> moduleBaseNames;
    for (const auto& module : modules_) {
        moduleBaseNames.push_back(module->moduleName);
    }

    // 收集每个模块导出的Public符号: [模块索引] -> vector<Symbol*>
    std::vector<std::vector<const Symbol*>> exportedSymbols(modules_.size());
    for (size_t i = 0; i < analyzers_.size(); i++) {
        exportedSymbols[i] = analyzers_[i]->symbolTable().getPublicSymbols();
    }

    // 构建 "存储键 -> (模块索引, Symbol*)" 的全局查找表
    // Fix 010r-12: 使用storageKey()而非lowerName做去重键
    // 原因: Property Get/Let/Set同名但有不同storageKey ($pg/$pl/$ps)
    // 用lowerName去重会导致只有第一个变体(通常Get)被保留, Let/Set丢失
    // 消费模块的P6.7查找 lookupModuleByKind(name, PropertyLet) 会失败
    std::unordered_map<std::string, std::pair<size_t, const Symbol*>> globalPublicSyms;
    for (size_t i = 0; i < exportedSymbols.size(); i++) {
        for (const Symbol* sym : exportedSymbols[i]) {
            std::string sKey = sym->storageKey();
            // 同一个storageKey只保留第一个 (VB6行为: 先声明的优先)
            if (globalPublicSyms.find(sKey) == globalPublicSyms.end()) {
                globalPublicSyms[sKey] = {i, sym};
            }
        }
    }

    // 对每个模块，检查其模块级作用域中的所有符号
    // 找到未定义引用（在visit(IdentifierExpr)中可能失败的标识符）
    // 策略：遍历模块级作用域中尚未定义（但被引用的地方找不到）的标识符
    // 实际上更简单的做法：扫描每个模块的AST，找到所有IdentifierExpr引用的名称，
    // 如果在本地符号表中找不到，就在全局Public表中查找并注入外部符号

    // 但为了避免修改AST遍历，采用更简洁的方式：
    // 对每个模块，遍历全局Public表，如果该符号在本模块没有本地定义，且名称匹配
    // 某些被引用但未在本模块定义的标识符，就注入外部符号
    //
    // 更精确的方案：只遍历在visit(IdentifierExpr)中可能需要跨模块的符号类型
    // (Sub/Function/Variable/Constant)

    for (size_t i = 0; i < analyzers_.size(); i++) {
        SymbolTable& symTab = analyzers_[i]->symbolTable();

        for (const auto& [sKey, entry] : globalPublicSyms) {
            auto [srcIdx, srcSym] = entry;
            // 跳过本模块导出的符号
            if (srcIdx == i) continue;

            // 检查本模块是否已有此符号的本地定义
            // Fix 010r-12: Property变体需按kind分别检查 (Get/Let/Set各自独立)
            Symbol* localSym = nullptr;
            if (srcSym->kind == SymbolKind::PropertyGet ||
                srcSym->kind == SymbolKind::PropertyLet ||
                srcSym->kind == SymbolKind::PropertySet) {
                localSym = symTab.lookupModuleByKind(srcSym->name, srcSym->kind);
            } else {
                localSym = symTab.lookupModule(srcSym->name);
            }
            if (localSym) continue;  // 已有本地定义，不需要外部符号

            // 注入外部符号
            auto extSym = std::make_unique<Symbol>(
                srcSym->kind, srcSym->name, srcSym->type,
                srcSym->location, srcSym->access
            );
            extSym->isExternal = true;
            extSym->sourceModule = moduleBaseNames[srcIdx];
            extSym->params = srcSym->params;  // 复制参数列表（函数调用需要）
            extSym->isArray = srcSym->isArray;
            // 类符号: 复制instancing、memberNames、isInterface、implementsNames
            if (srcSym->kind == SymbolKind::Class) {
                extSym->instancing = srcSym->instancing;
                extSym->memberNames = srcSym->memberNames;
                extSym->memberReturnTypes = srcSym->memberReturnTypes;  // Fix 015: 链式调用返回类型表
                extSym->memberProcKinds = srcSym->memberProcKinds;       // Fix 016: 成员过程类型表
                extSym->memberParams = srcSym->memberParams;             // Fix 033: 成员参数表 (calleeParams 跨模块精确查找)
                extSym->isInterface = srcSym->isInterface;  // P6.4
                extSym->implementsNames = srcSym->implementsNames;  // P6.4
                extSym->interfaceMethodNames = srcSym->interfaceMethodNames;  // P6.4
                extSym->eventNames = srcSym->eventNames;  // P6.5
                extSym->comClsidStr = srcSym->comClsidStr;  // P6.8: CLSID
            }
            // Fix 010r-11 / Fix 015: 复制变量/返回类型名
            // - Variable: 用于跨模块类实例变量识别 (knownClassVars_)
            // - Function / PropertyGet: 用于 method chaining 的返回类型推断
            //   (getClassMethodReturnType 读取该字段判断链式调用能继续到哪一层)
            // 对其它 kind 该字段为空, 无副作用. 原先把此赋值放在 Variable-only 分支内,
            // 导致 Function/PropertyGet 外部符号丢失返回类型名, 链式调用解析失败.
            extSym->variableTypeName = srcSym->variableTypeName;
            // Fix 017: 复制常量值 (EnumMember / Constant 跨模块注入后需保留值).
            // 外部 EnumMember 若 hasConstValue=false, cgen 会发出裸标识符 (如
            // HASH_ALG_SHA256) 而非数值 → C2065. 此前外部符号构造只复制
            // kind/name/type/location/access, 丢失 hasConstValue/constIntValue.
            // Fix 081d: 也复制 constStringValue/constFloatValue, 否则字符串常量跨模块时丢失.
            extSym->hasConstValue = srcSym->hasConstValue;
            extSym->constIntValue = srcSym->constIntValue;
            extSym->constType = srcSym->constType;
            extSym->constStringValue = srcSym->constStringValue;
            extSym->constFloatValue = srcSym->constFloatValue;
            extSym->constBoolValue = srcSym->constBoolValue;
            if (srcSym->kind == SymbolKind::Variable) {
                extSym->dimCount = srcSym->dimCount;
            }

            symTab.defineExternal(std::move(extSym));
        }
    }

    if (diag_->hasErrors()) return false;
    return true;
}

bool Driver::runCodeGeneration(const CompileOptions& options, const std::string& outputDir) {
    if (modules_.size() != analyzers_.size()) {
        std::cerr << "C3: 内部错误: 模块数与分析器数不匹配" << std::endl;
        return false;
    }

    // Fix 023: 预扫描所有模块的 AST, 构建 className → void* 字段集 映射.
    // 这个映射在 cgen_expr.cpp 的 knownClassVars_ fallback 路径用于判断
    // obj->member 是否返回 void* COM 指针 (例: cDataBase.Rs As New ADODB.Recordset,
    // Rs 字段在 C 结构体中是 void*). 若是, 在 emit 的注释中加入 "voidptr" 标记,
    // 由外层 MemberAccessExpr 识别并切换为 COM dispatch (vb6_ComCall/ComGet*Prop).
    // 不依赖模块编译顺序 — driver 在主 codegen 循环前一次性扫描所有模块.
    std::unordered_map<std::string, std::set<std::string>> classVoidFieldMap;
    // void* 判定规则与 cgen_base.cpp::mapTypeRef 保持一致:
    //   - var.isNew (As New ClassName) → void*
    //   - vb6 内置对象类型 (Collection/ErrObject/...) → void*
    //   - 已知 ADODB 等 COM 对象类型 (Connection/Recordset/...) → void*
    //   - 兜底: 无法解析为 Class/ComClass/UDT/Enum/内置类型 的名称 → void*
    auto isVoidFieldType = [](const VariableDecl& var,
                              const TypeSystem& typeSys,
                              const SymbolTable& symTab) -> bool {
        if (var.isNew) return true;
        if (!var.asType || var.asType->kind != ASTNodeKind::SimpleTypeRef) return false;
        std::string typeName = static_cast<SimpleTypeRef*>(var.asType.get())->name;
        if (typeName == "Any" || typeName == "any") return true;
        if (typeName.size() > 4 && typeName.compare(0, 4, "VBA.") == 0) typeName = typeName.substr(4);
        static const std::unordered_set<std::string> vb6BuiltinObj = {
            "Collection", "Forms", "ErrObject", "App", "Screen", "Printer", "Clipboard"
        };
        if (vb6BuiltinObj.count(typeName)) return true;
        Vb6Type t = typeSys.resolveTypeName(typeName);
        if (t != Vb6Type::Unknown) return false;  // 基础类型可解析 → 非 void*
        std::string lookupName = typeName;
        size_t dotPos = typeName.find('.');
        if (dotPos != std::string::npos) {
            std::string shortName = typeName.substr(dotPos + 1);
            auto* dotSym = symTab.lookupModule(shortName);
            if (dotSym) lookupName = shortName;
        }
        auto* clsSym = symTab.lookupModule(lookupName);
        if (clsSym) {
            if (clsSym->kind == SymbolKind::Class) return false;        // 真实类实例指针
            if (clsSym->kind == SymbolKind::ComClass ||
                clsSym->kind == SymbolKind::ComInterface) return false; // 类型化 COM 接口指针
        }
        auto* udtSym = symTab.lookup(lookupName);
        if (udtSym) {
            if (udtSym->kind == SymbolKind::UserDefinedType) return false;
            if (udtSym->kind == SymbolKind::EnumType) return false;
        }
        if (lookupName == "LongPtr" || lookupName == "Longptr") return false;
        if (lookupName.size() >= 2 && lookupName.compare(0, 2, "Vb") == 0) return false;
        if (lookupName.size() >= 4 && lookupName.compare(0, 4, "OLE_") == 0) return false;
        if (lookupName.size() >= 4 &&
            lookupName.compare(lookupName.size() - 4, 4, "Enum") == 0) return false;
        static const std::unordered_set<std::string> comObjTypes = {
            "Connection", "Recordset", "Command", "Parameter",
            "Field", "Fields", "Error", "Errors", "Property",
            "Properties", "Stream"
        };
        if (comObjTypes.count(lookupName)) return true;
        static const std::unordered_set<std::string> vb6EnumAliases = {
            "CompareMethod", "TriState", "FirstDayOfWeek", "FirstWeekOfYear",
            "MsgBoxResult", "MsgBoxStyle", "FileAttribute", "DateFormat",
            "Calendar", "DateTimeFormat", "CallType", "VariantType",
            "VarType", "QueryDef", "EditModeEnum", "FieldAttributeEnum"
        };
        if (vb6EnumAliases.count(lookupName)) return false;
        return true;  // 兜底: 未知类型 → void*
    };
    for (size_t i = 0; i < modules_.size(); i++) {
        const auto& module = modules_[i];
        // 类模块和窗体模块都有结构体字段; 标准模块无 struct 不需要扫描
        if (!module->isClassModule && !module->isFormModule) continue;
        const std::string& className = module->moduleName;
        std::set<std::string> voidFields;
        for (const auto& decl : module->declarations) {
            if (decl->kind != ASTNodeKind::VariableDecl) continue;
            const auto& var = static_cast<const VariableDecl&>(*decl);
            if (var.isDynamicArray || !var.dimensions.empty()) continue;  // 数组字段不是 void*
            if (!isVoidFieldType(var, analyzers_[i]->typeSystem(),
                                 analyzers_[i]->symbolTable())) continue;
            std::string oLower = var.name;
            std::transform(oLower.begin(), oLower.end(), oLower.begin(),
                           [](unsigned char c) { return (char)std::tolower(c); });
            std::string mLower = "m_" + oLower;
            voidFields.insert(oLower);
            voidFields.insert(mLower);
        }
        if (!voidFields.empty()) {
            classVoidFieldMap[className] = std::move(voidFields);
        }
    }

    // Fix 037b: 构建 classTypedFieldMap — 非 void* 的 typed 对象字段类型表.
    // 扫描每个类模块/窗体模块的字段声明, 对 isVoidFieldType 返回 false 但
    // 类型为 Class/ComClass/ComInterface 的字段, 记录其 VB6 类型名.
    // 用于 IndexOrCallExpr 中 obj.typedField(idx) 模式: typedField 是对象数据字段
    // (非函数), VB6 语义为调用其默认 Item 属性.
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> classTypedFieldMap;
    for (size_t i = 0; i < modules_.size(); i++) {
        const auto& module = modules_[i];
        if (!module->isClassModule && !module->isFormModule) continue;
        const std::string& className = module->moduleName;
        std::unordered_map<std::string, std::string> typedFields;
        for (const auto& decl : module->declarations) {
            if (decl->kind != ASTNodeKind::VariableDecl) continue;
            const auto& var = static_cast<const VariableDecl&>(*decl);
            if (var.isDynamicArray || !var.dimensions.empty()) continue;
            if (!var.asType || var.asType->kind != ASTNodeKind::SimpleTypeRef) continue;
            // 跳过 void* 字段 (已在 classVoidFieldMap 中)
            if (isVoidFieldType(var, analyzers_[i]->typeSystem(),
                               analyzers_[i]->symbolTable())) continue;
            std::string typeName = static_cast<SimpleTypeRef*>(var.asType.get())->name;
            // 在符号表中查找类型, 区分项目类 vs COM 接口
            std::string lookupName = typeName;
            size_t dotPos = typeName.find('.');
            if (dotPos != std::string::npos) {
                std::string shortName = typeName.substr(dotPos + 1);
                auto* dotSym = analyzers_[i]->symbolTable().lookupModule(shortName);
                if (dotSym) lookupName = shortName;
            }
            auto* clsSym = analyzers_[i]->symbolTable().lookupModule(lookupName);
            if (!clsSym) continue;
            std::string fieldTypeMarker;
            if (clsSym->kind == SymbolKind::Class) {
                fieldTypeMarker = clsSym->name;  // 项目类名 (如 "cCollection")
            } else if (clsSym->kind == SymbolKind::ComClass ||
                       clsSym->kind == SymbolKind::ComInterface) {
                fieldTypeMarker = "COM:" + typeName;  // COM 标记
            } else {
                continue;  // UDT/Enum 等非对象类型跳过
            }
            std::string oLower = var.name;
            std::transform(oLower.begin(), oLower.end(), oLower.begin(),
                           [](unsigned char c) { return (char)std::tolower(c); });
            std::string mLower = "m_" + oLower;
            typedFields[oLower] = fieldTypeMarker;
            typedFields[mLower] = fieldTypeMarker;
        }
        if (!typedFields.empty()) {
            classTypedFieldMap[className] = std::move(typedFields);
        }
    }

    // Fix 045: 预扫描所有模块, 构建返回 Variant 的项目函数 C 名集合.
    // 遍历所有模块的 FunctionDecl 和 PropertyDecl(PropertyGet),
    // 检查 returnType 是否为 Variant (nullptr=隐式Variant, 或 SimpleTypeRef name="Variant").
    // 构建 C 函数名: vb6_<moduleName>_<procName> (与 cProcName 多模块模式一致).
    // 供 cExprIsVariant() 检测项目类函数返回 Variant 的表达式.
    std::unordered_set<std::string> variantReturnFuncs;
    for (size_t i = 0; i < modules_.size(); i++) {
        const auto& module = modules_[i];
        const std::string& modName = module->moduleName;
        for (const auto& decl : module->declarations) {
            std::string procName;
            bool isVariant = false;

            if (decl->kind == ASTNodeKind::FunctionDecl) {
                auto& fn = static_cast<const FunctionDecl&>(*decl);
                procName = fn.name;
                if (!fn.returnType) {
                    isVariant = true;  // 隐式 Variant
                } else if (fn.returnType->kind == ASTNodeKind::SimpleTypeRef) {
                    auto& tr = static_cast<const SimpleTypeRef&>(*fn.returnType);
                    std::string lower = tr.name;
                    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                    if (lower == "variant") isVariant = true;
                }
            } else if (decl->kind == ASTNodeKind::PropertyDecl) {
                auto& prop = static_cast<const PropertyDecl&>(*decl);
                if (prop.propKind != ProcKind::PropertyGet) continue;
                procName = "prop_get_" + prop.name;
                if (!prop.returnType) {
                    isVariant = true;
                } else if (prop.returnType->kind == ASTNodeKind::SimpleTypeRef) {
                    auto& tr = static_cast<const SimpleTypeRef&>(*prop.returnType);
                    std::string lower = tr.name;
                    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                    if (lower == "variant") isVariant = true;
                }
            }

            if (isVariant && !procName.empty()) {
                // cIdent 等价: 替换特殊字符为 _
                auto cIdentS = [](const std::string& vb6Name) -> std::string {
                    std::string name = vb6Name;
                    if (!name.empty() && name.front() == '[' && name.back() == ']') {
                        name = name.substr(1, name.size() - 2);
                    }
                    for (auto& ch : name) {
                        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') {
                            ch = '_';
                        }
                    }
                    return name;
                };
                std::string cName = "vb6_" + cIdentS(modName) + "_" + cIdentS(procName);
                variantReturnFuncs.insert(cName);
                // Fix 066: 同模块调用 Private 函数时 C 函数名使用 vb6_pv_ 前缀,
                // 也需加入 variantReturnFuncs 以便 cExprIsVariant 正确检测.
                bool isPrivate = false;
                if (decl->kind == ASTNodeKind::FunctionDecl) {
                    isPrivate = (static_cast<const FunctionDecl&>(*decl).access == AccessLevel::Private);
                } else if (decl->kind == ASTNodeKind::PropertyDecl) {
                    isPrivate = (static_cast<const PropertyDecl&>(*decl).access == AccessLevel::Private);
                }
                if (isPrivate) {
                    std::string privateCName = "vb6_pv_" + cIdentS(procName);
                    variantReturnFuncs.insert(privateCName);
                }
            }
        }
    }

    for (size_t i = 0; i < modules_.size(); i++) {
        auto& module = modules_[i];
        auto& analyzer = analyzers_[i];

        // 确定输出基名
        std::string baseName;
        if (modules_.size() == 1 && !options.outputFile.empty()) {
            // 单文件: 用输出文件名作为基名
            std::filesystem::path p(options.outputFile);
            baseName = pathToUtf8(p.stem());
        } else {
            // Fix 013: 多文件: 用 module.moduleName (VB_Name) 作为基名
            // 确保输出文件名与 #include 指令、跨模块 sourceModule 一致
            baseName = module->moduleName;
        }

        // 收集跨模块include需求（从符号表获取外部模块名）
        auto externalModules = analyzer->symbolTable().getExternalModuleNames();

        // Fix 075: 多模块项目中，所有模块的 isMultiModule_ 必须为 true，
        // 否则函数定义名不带模块前缀而跨模块调用带模块前缀，导致 LNK2019。
        // 将项目中所有其他模块名也加入 externalModules，确保 isMultiModule_=true。
        if (modules_.size() > 1) {
            for (size_t j = 0; j < modules_.size(); j++) {
                if (j != i) {
                    externalModules.insert(modules_[j]->moduleName);
                }
            }
        }

        // 调用C代码生成器
        // Fix 023: 传入 void* 字段表供 cgen_expr.cpp fallback 路径查询
        CCodeGen cgen(*diag_, analyzer->symbolTable(), analyzer->typeSystem(),
                      &classVoidFieldMap, &classTypedFieldMap, &variantReturnFuncs, options.verbose);

        // 传入模块基名和外部模块列表
        // P6.6: 传递ActiveX DLL模式信息
        // P7: 传递窗体描述 (仅.frm模块有效)
        const FrmFormDesc* frmDescPtr = nullptr;
        auto frmIt = frmFiles_.find(module->moduleName);
        if (frmIt != frmFiles_.end()) {
            frmDescPtr = &frmIt->second.form;
        }
        bool ok = cgen.generate(*module, baseName, externalModules,
                                options.isDll, options.dllProgId, frmDescPtr);
        if (!ok) return false;

        // 写 .h 文件
        std::string hPath = outputDir + "/" + baseName + ".h";
        {
            std::ofstream ofs(hPath, std::ios::out | std::ios::trunc);
            if (!ofs) {
                std::cerr << "C3: 无法写入文件: " << hPath << std::endl;
                return false;
            }
            ofs << cgen.headerCode();
        }

        // 写 .c 文件
        std::string cPath = outputDir + "/" + baseName + ".c";
        {
            std::ofstream ofs(cPath, std::ios::out | std::ios::trunc);
            if (!ofs) {
                std::cerr << "C3: 无法写入文件: " << cPath << std::endl;
                return false;
            }
            ofs << cgen.sourceCode();
        }

        if (options.verbose) {
            std::cout << "C3: 生成 " << hPath << " (" << cgen.headerCode().size() << " bytes)" << std::endl;
            std::cout << "C3: 生成 " << cPath << " (" << cgen.sourceCode().size() << " bytes)" << std::endl;
        }

        // --emit-c 模式: 输出C代码后结束
        if (options.emitC) {
            std::cout << cgen.headerCode() << std::endl;
            std::cout << cgen.sourceCode() << std::endl;
        }
    }

    // P6.6: ActiveX DLL模式, 额外生成 dll_entry.c (包含DLL导出函数)
    // 这个文件始终由driver生成, 而不是在单个模块的.c中生成
    // 原因: DLL工程可能只有类模块而没有标准模块, emitActiveXDll()在类模块中不会被执行
    if (options.isDll && !analyzers_.empty()) {
        // 使用最后一个analyzer的符号表 (已包含跨模块符号)
        auto& lastAnalyzer = analyzers_.back();
        // Fix 023: 传入 void* 字段表 (与主 codegen 循环一致)
        CCodeGen dllCgen(*diag_, lastAnalyzer->symbolTable(), lastAnalyzer->typeSystem(),
                         &classVoidFieldMap, &classTypedFieldMap, &variantReturnFuncs, options.verbose);
        // Collect all symbol tables for cross-module Property lookup
        std::vector<SymbolTable*> allSymTabs;
        for (auto& analyzer : analyzers_) {
            allSymTabs.push_back(&analyzer->symbolTable());
        }
        // P6.8: 注入VBP指定的CLSID到类符号
        for (auto& [key, sym] : lastAnalyzer->symbolTable().moduleScope()->symbols()) {
            if (sym->kind == SymbolKind::Class && !sym->isInterface) {
                std::string lowerName = sym->name;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                auto it = classClsidMap_.find(lowerName);
                if (it != classClsidMap_.end() && sym->comClsidStr.empty()) {
                    sym->comClsidStr = it->second;
                }
            }
        }
        // P6.6.6: TypeLib building MUST run before generateDllEntry()
        // Reason: TypeLib builder writes back comDefaultIfaceIid and comClsidStr
        // to the symbol table, which cgen's generateDllEntry() reads

        // P9: Build TypeLib using CreateTypeLib2 API (replaces MIDL)
        {
            TypeLibBuilder tlbBuilder;
            std::string tlbPath = pathToUtf8(std::filesystem::absolute(utf8ToPath(outputDir + "/" + options.dllProgId + ".tlb")));
            std::string libId = options.libidStr.empty()
                ? TypeLibBuilder::generateUuid(options.dllProgId + ".TypeLib")
                : options.libidStr;

            if (tlbBuilder.beginLib(tlbPath, libId, options.dllProgId + " TypeLib", options.dllProgId + ".TypeLib")) {
                int dispidCounter = 1;
                std::set<std::string> processedClasses;
                for (auto* symTab : allSymTabs) {
                    // 遍历模块级作用域中的符号, 找出 Public Class (ActiveX DLL 的 coclass)
                    auto* modScope = symTab->moduleScope();
                    if (!modScope) continue;
                    for (auto& [key, symPtr] : modScope->symbols()) {
                        if (!symPtr || symPtr->kind != SymbolKind::Class) continue;
                        // 跳过跨模块注入的外部类副本 (isExternal=true), 只处理宿主表中的本地类,
                        // 否则其成员查找会命中本模块/其他模块的同名方法导致参数污染
                        if (symPtr->isExternal) continue;
                        if (symPtr->instancing == VBInstancing::Private) continue;
                        auto& clsSym = *symPtr;
                        // Deduplicate: each class only once across all symbol tables
                        if (processedClasses.count(clsSym.name)) continue;
                        processedClasses.insert(clsSym.name);

                        std::vector<TypeLibBuilder::MethodInfo> methods;
                        // 类→宿主符号表映射 (与 cgen_util.cpp Fix 016b 一致):
                        // 只记录 isExternal=false 的本地 Class 所在表, 成员查找优先用宿主表,
                        // 彻底避免跨表 lookupModuleByKind 命中其他类同名方法 (参数污染,
                        // 如 cCollection.Add 被查成 cHttpServerRouterAfter.Add 变成
                        // RouteName/Handler/MethodLimit, 导致 SetFuncAndParamNames 因参数
                        // 个数不匹配返回 TYPE_E_TYPEMISMATCH 0x800280EC)。
                        std::unordered_map<std::string, SymbolTable*> classOwningSymTab;
                        for (auto* st_ : allSymTabs) {
                            if (!st_ || !st_->moduleScope()) continue;
                            for (auto& [k, cs] : st_->moduleScope()->symbols()) {
                                if (cs && cs->kind == SymbolKind::Class && !cs->isExternal) {
                                    classOwningSymTab.emplace(Symbol::toLower(cs->name), st_);
                                }
                            }
                        }
                        auto findMemberInClass = [&](const std::string& name, SymbolKind kind) -> Symbol* {
                            // 1) 类宿主表精确查找 (无跨类碰撞)。
                            //    类宿主表 (classOwningSymTab) 由语义分析注册, 包含该类全部成员变体
                            //    (Sub/Function/Property Get/Let/Set), 跨表 fallback 反而会命中
                            //    其他类的同名方法 (如 cCollection 的 Sub Add(Item,Key) 之外, 又
                            //    从其他类找到 Function Add(RouteName,Handler,MethodLimit), 导致
                            //    SetFuncAndParamNames 因参数个数不匹配返回 TYPE_E_TYPEMISMATCH)。
                            auto itOwn = classOwningSymTab.find(Symbol::toLower(clsSym.name));
                            if (itOwn != classOwningSymTab.end() && itOwn->second && itOwn->second->moduleScope()) {
                                // Fix 048: 排除 isBuiltin — 内置函数 (IsEmpty/Filter/Timer 等)
                                // 注册于每个模块符号表 (key=无后缀名), 若类成员查找命中它们,
                                // TypeLib 会出现幽灵 Function 变体 (与真实属性 $pg/$pl 同名同
                                // dispid, SetFuncAndParamNames 返回 TYPE_E_TYPEMISMATCH)。
                                Symbol* s = itOwn->second->lookupModuleByKind(name, kind);
                                if (s && !s->isExternal && !s->isBuiltin) {
                                    return s;
                                }
                            }
                            // 2) 当前表兜底 (只接受本地符号)
                            Symbol* s2 = symTab->lookupModuleByKind(name, kind);
                            if (s2 && !s2->isExternal && !s2->isBuiltin) {
                                return s2;
                            }
                            return nullptr;
                        };
                        // memberNames 可能含大小写不同的重复名 (VB6 标识符不区分大小写),
                        // 重复处理会导致同一方法被添加多次, 且第二次查找可能命中其他类的同名
                        // 方法符号 (参数污染)。按大小写不敏感去重。
                        std::unordered_set<std::string> seenMemberNames;
                        for (auto& memberName : clsSym.memberNames) {
                            if (!seenMemberNames.insert(Symbol::toLower(memberName)).second) continue;
                            // 同名属性可能有 PropertyGet/Let/Set 多个符号
                            // memberNames 中同名属性只存一次, 需要分别查找各变体
                            // M29: cross-symTab fallback - the class may be found in a "merged" analyzer
                            // (e.g., lastAnalyzer), but its Property Let/Set variants might only exist in
                            // the original per-module analyzer's table. Fall back to allSymTabs to find them.
                            auto* subSym = findMemberInClass(memberName, SymbolKind::Sub);
                            auto* fnSym = findMemberInClass(memberName, SymbolKind::Function);
                            auto* propGetSym = findMemberInClass(memberName, SymbolKind::PropertyGet);
                            auto* propLetSym = findMemberInClass(memberName, SymbolKind::PropertyLet);
                            auto* propSetSym = findMemberInClass(memberName, SymbolKind::PropertySet);

                            // 收集所有 Public 成员变体 (PropertyGet 必须在 PropertyLet 前面)
                            struct MemberRef { Symbol* sym; bool isGet; bool isPut; bool isPutRef; };
                            std::vector<MemberRef> refs;
                            if (subSym && subSym->access == AccessLevel::Public)
                                refs.push_back({subSym, false, false, false});
                            if (fnSym && fnSym->access == AccessLevel::Public)
                                refs.push_back({fnSym, false, false, false});
                            if (propGetSym && propGetSym->access == AccessLevel::Public)
                                refs.push_back({propGetSym, true, false, false});
                            if (propLetSym && propLetSym->access == AccessLevel::Public)
                                refs.push_back({propLetSym, false, true, false});
                            if (propSetSym && propSetSym->access == AccessLevel::Public)
                                refs.push_back({propSetSym, false, false, true});

                            for (auto& mr : refs) {
                                TypeLibBuilder::MethodInfo mi;
                                mi.name = mr.sym->name;
                                mi.returnType = mr.sym->type;
                                mi.isPropertyGet = mr.isGet;
                                mi.isPropertyPut = mr.isPut;
                                mi.isPropertyPutRef = mr.isPutRef;
                                // Property Get/Let/Set with same name share DISPID
                                bool foundExistingDispid = false;
                                for (const auto& existing : methods) {
                                    if (existing.name == mi.name) {
                                        mi.dispid = existing.dispid;
                                        foundExistingDispid = true;
                                        break;
                                    }
                                }
                                if (!foundExistingDispid) {
                                    mi.dispid = dispidCounter++;
                                }
                                // M29: 回写dispid到method symbol, 供cgen生成dll_entry.c方法表使用, 确保TypeLib与dll_entry.c dispid一致
                                mr.sym->comDispid = mi.dispid;
                                for (auto& param : mr.sym->params) {
                                    mi.params.push_back(param);
                                }
                                methods.push_back(mi);
                            }
                        }

                        std::string iid = TypeLibBuilder::generateUuid("_" + clsSym.name);
                        std::string ifaceName = "_" + clsSym.name;
                        bool ifaceOk = tlbBuilder.addDispInterface(ifaceName, iid, methods);
                        if (!ifaceOk) {
                            std::cerr << "C3: TypeLib addDispInterface failed for " << ifaceName << ": " << tlbBuilder.lastError() << std::endl;
                            for (size_t mi_ = 0; mi_ < methods.size(); mi_++) {
                                auto& mm = methods[mi_];
                                fprintf(stderr, "  [%zu] %s get=%d put=%d putref=%d dispid=%d nparams=%zu\n",
                                        mi_, mm.name.c_str(), (int)mm.isPropertyGet, (int)mm.isPropertyPut,
                                        (int)mm.isPropertyPutRef, mm.dispid, mm.params.size());
                            }
                        }

                        // Only write back IID if addDispInterface succeeded
                        if (ifaceOk) {
                            clsSym.comDefaultIfaceIid = iid;
                            clsSym.comDefaultIfaceName = ifaceName;
                        }

                        // If class has events, create source dispinterface (_ClassNameEvents)
                        std::string sourceIfaceName;
                        bool sourceIfaceOk = false;
                        if (!clsSym.eventNames.empty()) {
                            sourceIfaceName = "_" + clsSym.name + "Events";
                            std::string sourceIid = TypeLibBuilder::generateUuid(sourceIfaceName);
                            std::vector<TypeLibBuilder::MethodInfo> eventMethods;
                            for (size_t ei = 0; ei < clsSym.eventNames.size(); ei++) {
                                TypeLibBuilder::MethodInfo emi;
                                emi.name = clsSym.eventNames[ei];
                                emi.dispid = (int32_t)(ei + 1);
                                emi.returnType = Vb6Type::Void;
                                eventMethods.push_back(emi);
                            }
                            sourceIfaceOk = tlbBuilder.addDispInterface(sourceIfaceName, sourceIid, eventMethods);
                            if (!sourceIfaceOk) {
                                std::cerr << "C3: TypeLib addDispInterface failed for " << sourceIfaceName << ": " << tlbBuilder.lastError() << std::endl;
                            }

                            // Only write back sourceIfaceIid if addDispInterface succeeded
                            if (sourceIfaceOk) {
                                clsSym.comSourceIfaceIid = sourceIid;
                                clsSym.comSourceIfaceName = sourceIfaceName;
                            }

                            // Write event DISPIDs to symbol table
                            for (size_t ei = 0; ei < clsSym.eventNames.size(); ei++) {
                                std::string evtLower = Symbol::toLower(clsSym.eventNames[ei]);
                                clsSym.comEventDispids[evtLower] = eventMethods[ei].dispid;
                            }
                        }

                        // Only add CoClass if primary interface was added successfully
                        if (ifaceOk) {
                            std::string clsid = clsSym.comClsidStr;
                            if (clsid.empty()) {
                                auto it = classClsidMap_.find(clsSym.lowerName);
                                if (it != classClsidMap_.end()) clsid = it->second;
                            }
                            if (clsid.empty()) clsid = TypeLibBuilder::generateUuid(clsSym.name);
                            if (clsSym.comClsidStr.empty()) clsSym.comClsidStr = clsid;
                            if (!tlbBuilder.addCoClass(clsSym.name, clsid, ifaceName, sourceIfaceName)) {
                                std::cerr << "C3: TypeLib addCoClass failed for " << clsSym.name << ": " << tlbBuilder.lastError() << std::endl;
                            }
                        }
                    }
                }

                if (!tlbBuilder.endLib(tlbPath)) {
                    std::cerr << "C3: TypeLib generation failed: " << tlbBuilder.lastError() << std::endl;
                } else if (options.verbose) {
                    std::cout << "C3: TypeLib generated: " << tlbPath << std::endl;
                }
            } else {
                std::cerr << "C3: TypeLib init failed: " << tlbBuilder.lastError() << std::endl;
            }
        }

        // P6.6.6: Sync comDefaultIfaceIid/comClsidStr from allSymTabs to lastAnalyzer's symbol table
        // (TypeLib builder writes to per-module Symbol objects, but cgen uses lastAnalyzer's merged table)
        for (auto& [key, sym] : lastAnalyzer->symbolTable().moduleScope()->symbols()) {
            if (sym->kind == SymbolKind::Class && !sym->isInterface && sym->comDefaultIfaceIid.empty()) {
                // Search all symbol tables for a matching class with IID info
                for (auto* st : allSymTabs) {
                    auto* srcSym = st->lookupModule(sym->name);
                    if (srcSym && srcSym->kind == SymbolKind::Class && !srcSym->comDefaultIfaceIid.empty()) {
                        sym->comDefaultIfaceIid = srcSym->comDefaultIfaceIid;
                        sym->comDefaultIfaceName = srcSym->comDefaultIfaceName;
                        sym->comDefaultMemberName = srcSym->comDefaultMemberName;
                        sym->comDefaultMemberRealName = srcSym->comDefaultMemberRealName;
                    }
                    if (srcSym && srcSym->kind == SymbolKind::Class && !srcSym->comClsidStr.empty() && sym->comClsidStr.empty()) {
                        sym->comClsidStr = srcSym->comClsidStr;
                    }
                    if (srcSym && srcSym->kind == SymbolKind::Class && !srcSym->comSourceIfaceIid.empty() && sym->comSourceIfaceIid.empty()) {
                        sym->comSourceIfaceIid = srcSym->comSourceIfaceIid;
                        sym->comSourceIfaceName = srcSym->comSourceIfaceName;
                    }
                }
            }
        }

        // M29: Sync comDispid from per-module method symbols to lastAnalyzer's merged table
        // (TypeLib builder writes comDispid to per-module Symbol* in allSymTabs, but cgen's
        //  generateDllEntry reads via lastAnalyzer.symbolTable().lookupModule[ByKind], which
        //  may return a DIFFERENT Symbol* than what driver TypeLib phase wrote to. Same pattern
        //  as the comDefaultIfaceIid sync block above.)
        for (auto& [mKey, mClsSym] : lastAnalyzer->symbolTable().moduleScope()->symbols()) {
            if (!mClsSym || mClsSym->kind != SymbolKind::Class || mClsSym->isInterface) continue;
            if (mClsSym->isExternal) continue;  // 跳过外部注入副本
            for (auto& memberName : mClsSym->memberNames) {
                // Sync each possible method kind independently (Sub/Function are exclusive;
                // Property Get/Let/Set share name but are distinct Symbol objects)
                for (auto mKind : {SymbolKind::Sub, SymbolKind::Function,
                                   SymbolKind::PropertyGet, SymbolKind::PropertyLet,
                                   SymbolKind::PropertySet}) {
                    // Find any Symbol* with comDispid set (the "source of truth" written by driver TypeLib phase)
                    int knownDispid = 0;
                    for (auto* st : allSymTabs) {
                        Symbol* s = st->lookupModuleByKind(memberName, mKind);
                        if (s && !s->isExternal && s->comDispid != 0) { knownDispid = s->comDispid; break; }
                    }
                    if (knownDispid == 0) continue;  // method not present, or driver didn't set dispid
                    // Propagate to ALL matching Symbol* across all symbol tables (only fill zeros)
                    // (cgen's generateDllEntry may read via a DIFFERENT Symbol* than what driver wrote to,
                    //  because lastAnalyzer's merged table can have its own copy. Update every copy.)
                    for (auto* st : allSymTabs) {
                        Symbol* s = st->lookupModuleByKind(memberName, mKind);
                        if (s && !s->isExternal && s->comDispid == 0) s->comDispid = knownDispid;
                    }
                }
            }
        }
        // P6.6.6: generateDllEntry() runs after TypeLib building so IIDs/CLSIDs are available
        std::string dllEntryCode = dllCgen.generateDllEntry(options.dllProgId, allSymTabs);

        std::string dllEntryPath = outputDir + "/dll_entry.c";
        {
            std::ofstream ofs(dllEntryPath, std::ios::out | std::ios::trunc);
            if (!ofs) {
                std::cerr << "C3: 无法写入文件: " << dllEntryPath << std::endl;
                return false;
            }
            ofs << dllEntryCode;
        }

        if (options.verbose) {
            std::cout << "C3: 生成 " << dllEntryPath << " (" << dllEntryCode.size() << " bytes)" << std::endl;
        }
    }

    return !diag_->hasErrors();
}

bool Driver::runLinker(const CompileOptions& options, const std::string& outputDir,
                       const std::string& intermediatesDir, SessionManager& session) {
    // --emit-c mode: no linking needed
    if (options.emitC) {
        return true;
    }

    // Check MSVC availability
    if (!MsvcDriver::isMsvcAvailable()) {
        std::cerr << "C3: error: MSVC not found (install Visual Studio 2017+ with C++ workload)" << std::endl;
        std::cerr << "C3: Use --emit-c to generate C code only" << std::endl;
        return false;
    }

    // Collect generated .c files from intermediatesDir
    // MUST match baseName logic in runCodeGeneration (single-file + -o uses output stem)
    MsvcDriverOptions msvcOpts;
    for (size_t i = 0; i < modules_.size(); i++) {
        std::string baseName;
        if (modules_.size() == 1 && !options.outputFile.empty()) {
            std::filesystem::path p(options.outputFile);
            baseName = pathToUtf8(p.stem());
        } else {
            // Fix 013: 用 module.moduleName (VB_Name) 作为基名, 与 runCodeGeneration 一致
            baseName = modules_[i]->moduleName;
        }
        std::string cPath = intermediatesDir + "/" + baseName + ".c";
        msvcOpts.sourceFiles.push_back(cPath);
    }

    // P6.6: ActiveX DLL mode, add dll_entry.c
    if (options.isDll) {
        std::string dllEntryPath = intermediatesDir + "/dll_entry.c";
        msvcOpts.sourceFiles.push_back(dllEntryPath);
    }

    // P10: Get RTL directory from session
    std::string rtlDir = session.rtlDir();
    if (rtlDir.empty()) {
        std::cerr << "C3: error: RTL runtime not available" << std::endl;
        return false;
    }
    msvcOpts.rtlDir = rtlDir;

    // P11.1+P11.2: Set intermediate directories
    msvcOpts.srcDir = intermediatesDir;   // /I for generated .h files
    msvcOpts.objDir = intermediatesDir;   // /Fo for .obj files

    // Output file path (in user's output directory, not intermediates)
    std::string outputExt = options.isDll ? ".dll" : ".exe";
    if (!options.outputFile.empty()) {
        msvcOpts.outputFile = options.outputFile;
        if (options.isDll && msvcOpts.outputFile.size() >= 4 &&
            msvcOpts.outputFile.compare(msvcOpts.outputFile.size()-4, 4, ".exe") == 0) {
            msvcOpts.outputFile.replace(msvcOpts.outputFile.size()-4, 4, ".dll");
        }
    } else if (!projectBaseName_.empty()) {
        msvcOpts.outputFile = outputDir + "/" + projectBaseName_ + outputExt;
    } else if (modules_.size() == 1) {
        std::filesystem::path p(utf8ToPath(modules_[0]->filename));
        msvcOpts.outputFile = outputDir + "/" + pathToUtf8(p.stem()) + outputExt;
    } else {
        msvcOpts.outputFile = outputDir + "/a" + outputExt;
    }

    msvcOpts.isDll = options.isDll;
    // P7: Detect GUI program
    for (auto& module : modules_) {
        if (module->isFormModule) {
            msvcOpts.isGui = true;
            break;
        }
    }
    msvcOpts.verbose = options.verbose;
    msvcOpts.debugInfo = options.debugInfo;
    msvcOpts.optimizationLevel = options.optimizationLevel;
    msvcOpts.arch = options.arch;  // DualArch: pass target architecture

    // P6.6: ActiveX DLL - generate .def export file (in intermediatesDir)
    if (options.isDll) {
        std::string defPath = intermediatesDir + "/activex_dll.def";
        std::ofstream defFile(defPath, std::ios::out | std::ios::trunc);
        if (defFile) {
            defFile << "LIBRARY\n";
            defFile << "EXPORTS\n";
            defFile << "    DllGetClassObject\n";
            defFile << "    DllCanUnloadNow\n";
            defFile << "    DllRegisterServer\n";
            defFile << "    DllUnregisterServer\n";
            defFile << "    DllMain\n";
            defFile.close();
            msvcOpts.defFile = defPath;
            if (options.verbose) {
                std::cout << "C3: Generated export definition: " << defPath << std::endl;
            }
        }
    }

    // P9: Embed TypeLib into DLL resource
    if (options.isDll && !options.dllProgId.empty()) {
        std::string tlbPath = pathToUtf8(std::filesystem::absolute(utf8ToPath(intermediatesDir + "/" + options.dllProgId + ".tlb")));
        if (std::filesystem::exists(tlbPath)) {
            std::string absInterDir = pathToUtf8(std::filesystem::absolute(utf8ToPath(intermediatesDir)));
            std::string rcPath = absInterDir + "\\activex_dll_typelib.rc";
            {
                std::ofstream rcFile(rcPath, std::ios::out | std::ios::trunc);
                if (rcFile) {
                    std::string tlbPathForRc = tlbPath;
                    for (auto& c : tlbPathForRc) { if (c == '\\') c = '/'; }
                    rcFile << "1 TYPELIB \"" << tlbPathForRc << "\"\n";

                }
            }

            // Find rc.exe
            std::string rcExePath;
            std::filesystem::path toolsRc = std::filesystem::current_path() / "tools" / "rc.exe";
            if (std::filesystem::exists(toolsRc)) {
                rcExePath = toolsRc.string();
            } else {
                std::string sdkBinDir;
                const char* sdkDir = std::getenv("WindowsSdkDir");
                if (sdkDir && sdkDir[0] != '\0') {
                    std::string sdkRoot = sdkDir;
                    while (!sdkRoot.empty() && sdkRoot.back() == '\\') sdkRoot.pop_back();
                    sdkBinDir = sdkRoot + "\\bin";
                }
                if (sdkBinDir.empty() || !std::filesystem::exists(sdkBinDir)) {
                    static const char* commonSdkBin = "C:\\Program Files (x86)\\Windows Kits\\10\\bin";
                    if (std::filesystem::exists(commonSdkBin)) sdkBinDir = commonSdkBin;
                }
                if (!sdkBinDir.empty() && std::filesystem::exists(sdkBinDir)) {
                    for (auto& entry : std::filesystem::directory_iterator(sdkBinDir)) {
                        if (!entry.is_directory()) continue;
                        std::filesystem::path candidate = entry.path() / "x64" / "rc.exe";
                        if (std::filesystem::exists(candidate)) {
                            rcExePath = candidate.string();
                        }
                    }
                }
            }

            if (!rcExePath.empty()) {
                std::string resPath = absInterDir + "\\activex_dll_typelib.res";
                std::ostringstream rcArgs;
                rcArgs << "\"" << rcExePath << "\" /r /fo \"" << resPath << "\" \"" << rcPath << "\"";
                if (options.verbose) {
                    std::cout << "C3: RC: " << rcArgs.str() << std::endl;
                }
                std::string rcFullCmd = std::string("cmd /c \"") + rcArgs.str() + "\"";
                int rcRet = std::system(rcFullCmd.c_str());
                if (rcRet == 0 && std::filesystem::exists(resPath)) {
                    msvcOpts.typelibResFile = resPath;
                    if (options.verbose) {
                        std::cout << "C3: TypeLib resource embedded: " << resPath << std::endl;
                    }
                }
            } else if (options.verbose) {
                std::cout << "C3: rc.exe not found, TypeLib will not be embedded in DLL" << std::endl;
            }
        } else if (options.verbose) {
            std::cout << "C3: TypeLib file not found: " << tlbPath << std::endl;
        }
    }
    // P23-05: Generate VS_VERSION_INFO resource if version info is available
    if (verMajor_ > 0 || verMinor_ > 0 || !verCompanyName_.empty() || !verFileDescription_.empty()) {
        std::string absInterDir2 = pathToUtf8(std::filesystem::absolute(utf8ToPath(intermediatesDir)));
        std::string verRcPath = absInterDir2 + "\\version_info.rc";
        {
            std::ofstream rcFile(verRcPath, std::ios::out | std::ios::trunc);
            if (rcFile) {
                // Determine internal name from project base name or output file
                std::string internalName = projectBaseName_.empty() ? "VB6App" : projectBaseName_;
                std::string originalName = verOriginalFileName_.empty() ? (internalName + ".exe") : verOriginalFileName_;
                std::string prodName = verProductName_.empty() ? internalName : verProductName_;
                std::string fileDesc = verFileDescription_.empty() ? internalName : verFileDescription_;
                std::string company = verCompanyName_;
                std::string copyright = verLegalCopyright_;
                std::string comments = verComments_;
                std::string trademarks = verLegalTrademarks_;

                // Escape backslashes for RC string values
                auto escapeRc = [](std::string s) -> std::string {
                    std::string result;
                    for (char c : s) {
                        if (c == '\\') result += "\\\\";
                        else if (c == '"') result += "\\\"";
                        else result += c;
                    }
                    return result;
                };

                int fileVerMs = verMajor_;
                int fileVerLs = verMinor_;
                int prodVerMs = verMajor_;
                int prodVerLs = verMinor_;

                                rcFile << "\n";
                rcFile << "#pragma code_page(65001)\n";
                rcFile << "1 VERSIONINFO\n";
                rcFile << "FILEVERSION " << fileVerMs << "," << fileVerLs << ",0," << verRevision_ << "\n";
                rcFile << "PRODUCTVERSION " << prodVerMs << "," << prodVerLs << ",0," << verRevision_ << "\n";
                rcFile << "FILEFLAGSMASK 0x3fL\n";
                rcFile << "FILEFLAGS 0x0L\n";
                rcFile << "FILEOS 0x00040004L\n";
                rcFile << "FILETYPE 0x00000001L\n";
                rcFile << "FILESUBTYPE 0x00000000L\n";
                rcFile << "BEGIN\n";
                rcFile << "  BLOCK \"StringFileInfo\"\n";
                rcFile << "  BEGIN\n";
                rcFile << "    BLOCK \"080404b0\"\n";
                rcFile << "    BEGIN\n";
                rcFile << "      VALUE \"CompanyName\", \"" << escapeRc(company) << "\"\n";
                rcFile << "      VALUE \"FileDescription\", \"" << escapeRc(fileDesc) << "\"\n";
                rcFile << "      VALUE \"FileVersion\", \"" << fileVerMs << "." << fileVerLs << ".0." << verRevision_ << "\"\n";
                rcFile << "      VALUE \"InternalName\", \"" << escapeRc(internalName) << "\"\n";
                rcFile << "      VALUE \"LegalCopyright\", \"" << escapeRc(copyright) << "\"\n";
                rcFile << "      VALUE \"LegalTrademarks\", \"" << escapeRc(trademarks) << "\"\n";
                rcFile << "      VALUE \"OriginalFilename\", \"" << escapeRc(originalName) << "\"\n";
                rcFile << "      VALUE \"ProductName\", \"" << escapeRc(prodName) << "\"\n";
                rcFile << "      VALUE \"ProductVersion\", \"" << prodVerMs << "." << prodVerLs << ".0." << verRevision_ << "\"\n";
                if (!comments.empty()) {
                    rcFile << "      VALUE \"Comments\", \"" << escapeRc(comments) << "\"\n";
                }
                rcFile << "    END\n";
                rcFile << "  END\n";
                rcFile << "  BLOCK \"VarFileInfo\"\n";
                rcFile << "  BEGIN\n";
                rcFile << "    VALUE \"Translation\", 0x0804, 1200\n";
                rcFile << "  END\n";
                rcFile << "END\n";
            }
        }

        // Find rc.exe (reuse same logic as TypeLib RC)
        std::string rcExePath2;
        std::filesystem::path toolsRc2 = std::filesystem::current_path() / "tools" / "rc.exe";
        if (std::filesystem::exists(toolsRc2)) {
            rcExePath2 = toolsRc2.string();
        } else {
            std::string sdkBinDir2;
            const char* sdkDir2 = std::getenv("WindowsSdkDir");
            if (sdkDir2 && sdkDir2[0] != '\0') {
                std::string sdkRoot2 = sdkDir2;
                while (!sdkRoot2.empty() && sdkRoot2.back() == '\\') sdkRoot2.pop_back();
                sdkBinDir2 = sdkRoot2 + "\\bin";
            }
            if (sdkBinDir2.empty() || !std::filesystem::exists(sdkBinDir2)) {
                static const char* commonSdkBin2 = "C:\\Program Files (x86)\\Windows Kits\\10\\bin";
                if (std::filesystem::exists(commonSdkBin2)) sdkBinDir2 = commonSdkBin2;
            }
            if (!sdkBinDir2.empty() && std::filesystem::exists(sdkBinDir2)) {
                for (auto& entry : std::filesystem::directory_iterator(sdkBinDir2)) {
                    if (!entry.is_directory()) continue;
                    std::filesystem::path candidate = entry.path() / "x64" / "rc.exe";
                    if (std::filesystem::exists(candidate)) {
                        rcExePath2 = candidate.string();
                    }
                }
            }
        }

        if (!rcExePath2.empty()) {
            std::string verResPath = absInterDir2 + "\\version_info.res";
            std::ostringstream verRcArgs;
            verRcArgs << "\"" << rcExePath2 << "\" /r /fo \"" << verResPath << "\" \"" << verRcPath << "\"";
            if (options.verbose) {
                std::cout << "C3: RC (version): " << verRcArgs.str() << std::endl;
            }
            std::string verRcFullCmd = std::string("cmd /c \"") + verRcArgs.str() + "\"";
            int verRcRet = std::system(verRcFullCmd.c_str());
            if (verRcRet == 0 && std::filesystem::exists(verResPath)) {
                msvcOpts.versionInfoResFile = verResPath;
                if (options.verbose) {
                    std::cout << "C3: VS_VERSION_INFO resource compiled: " << verResPath << std::endl;
                }
            }
        } else if (options.verbose) {
            std::cout << "C3: rc.exe not found, version info will not be embedded" << std::endl;
        }
    }

        // P23-03: Pass user .res file to linker
    if (!userResFile_.empty() && std::filesystem::exists(utf8ToPath(userResFile_))) {
        msvcOpts.userResFile = userResFile_;
        if (options.verbose) {
            std::cout << "C3: User resource file: " << userResFile_ << std::endl;
        }
    }

    MsvcDriver msvc;
    return msvc.compileAndLink(msvcOpts);
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
              << "  --typelib <文件>   显式引用TypeLib\n"
              << "  --no-auto-typelib  禁用自动TypeLib加载\n"
              << "  --progid <前缀>    ActiveX DLL的ProgID前缀\n"
              << "  --libid <字符串>   显式指定TypeLib的LibID\n"
              << "\n"
              << "示例:\n"
              << "  C3 hello.bas -o hello.exe\n"
              << "  C3 app.vbp --target win-x64\n"
              << "  C3 module.bas -d:DEBUG=-1\n";
}


void Driver::printVersion() {
    std::cout << "C3 version 0.10.0 (vb6.pro project)" << std::endl;
}

} // namespace vb6c3