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
            // 仅在缓存中不存在时加载
            if (!typelibParser_->findCachedCoClass(progId)) {
                typelibParser_->loadByProgId(progId);
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
                            sym->comSourceIfaceIsDispatch = cc->defaultSourceIface->isDispatch;
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

    // 为每个模块计算基名（用于sourceModule标识）
    std::vector<std::string> moduleBaseNames;
    for (const auto& module : modules_) {
        std::filesystem::path p(utf8ToPath(module->filename));
        moduleBaseNames.push_back(pathToUtf8(p.stem()));
    }

    // 收集每个模块导出的Public符号: [模块索引] -> vector<Symbol*>
    std::vector<std::vector<const Symbol*>> exportedSymbols(modules_.size());
    for (size_t i = 0; i < analyzers_.size(); i++) {
        exportedSymbols[i] = analyzers_[i]->symbolTable().getPublicSymbols();
    }

    // 构建 "小写符号名 -> (模块索引, Symbol*)" 的全局查找表
    // VB6不区分大小写，所以用小写名做key
    std::unordered_map<std::string, std::pair<size_t, const Symbol*>> globalPublicSyms;
    for (size_t i = 0; i < exportedSymbols.size(); i++) {
        for (const Symbol* sym : exportedSymbols[i]) {
            std::string lowerName = Symbol::toLower(sym->name);
            // 如果多个模块导出同名Public符号，第一个遇到的优先（VB6行为：先声明的优先）
            if (globalPublicSyms.find(lowerName) == globalPublicSyms.end()) {
                globalPublicSyms[lowerName] = {i, sym};
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

        for (const auto& [lowerName, entry] : globalPublicSyms) {
            auto [srcIdx, srcSym] = entry;
            // 跳过本模块导出的符号
            if (srcIdx == i) continue;

            // 检查本模块是否已有此符号的本地定义
            Symbol* localSym = symTab.lookupModule(lowerName);
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
                extSym->isInterface = srcSym->isInterface;  // P6.4
                extSym->implementsNames = srcSym->implementsNames;  // P6.4
                extSym->interfaceMethodNames = srcSym->interfaceMethodNames;  // P6.4
                extSym->eventNames = srcSym->eventNames;  // P6.5
                extSym->comClsidStr = srcSym->comClsidStr;  // P6.8: CLSID
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
            // 多文件: 用源文件名作为基名
            std::filesystem::path p(utf8ToPath(module->filename));
            baseName = pathToUtf8(p.stem());
        }

        // 收集跨模块include需求（从符号表获取外部模块名）
        auto externalModules = analyzer->symbolTable().getExternalModuleNames();

        // 调用C代码生成器
        CCodeGen cgen(*diag_, analyzer->symbolTable(), analyzer->typeSystem(),
                      options.verbose);

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
        CCodeGen dllCgen(*diag_, lastAnalyzer->symbolTable(), lastAnalyzer->typeSystem(),
                         options.verbose);
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
                        if (symPtr->instancing == VBInstancing::Private) continue;
                        auto& clsSym = *symPtr;
                        // Deduplicate: each class only once across all symbol tables
                        if (processedClasses.count(clsSym.name)) continue;
                        processedClasses.insert(clsSym.name);

                        std::vector<TypeLibBuilder::MethodInfo> methods;
                        for (auto& memberName : clsSym.memberNames) {
                            // 同名属性可能有 PropertyGet/Let/Set 多个符号
                            // memberNames 中同名属性只存一次, 需要分别查找各变体
                            auto* subSym = symTab->lookupModuleByKind(memberName, SymbolKind::Sub);
                            auto* fnSym = symTab->lookupModuleByKind(memberName, SymbolKind::Function);
                            auto* propGetSym = symTab->lookupModuleByKind(memberName, SymbolKind::PropertyGet);
                            auto* propLetSym = symTab->lookupModuleByKind(memberName, SymbolKind::PropertyLet);
                            auto* propSetSym = symTab->lookupModuleByKind(memberName, SymbolKind::PropertySet);

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
            std::filesystem::path p(utf8ToPath(modules_[i]->filename));
            baseName = pathToUtf8(p.stem());
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
              << "示例:\n"
              << "  C3 hello.bas -o hello.exe\n"
              << "  C3 app.vbp --target win-x64\n"
              << "  C3 module.bas -d:DEBUG=-1\n";
}


void Driver::printVersion() {
    std::cout << "C3 version 0.10.0 (vb6.pro project)" << std::endl;
}

} // namespace vb6c3