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
#include "project/vbp_parser.hpp"
#include "project/frm_parser.hpp"

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
                std::cerr << "c3: 错误: .vbp文件中没有源文件: " << srcFile << std::endl;
                result.errorCount = 1;
                return result;
            }

            if (options.verbose) {
                std::cout << "c3: 加载工程: " << project.projectName
                          << " (" << project.sources.size() << " 个源文件)" << std::endl;
            }

            // 展开源文件列表 (将相对路径转为绝对路径)
            effectiveOpts.sourceFiles.clear();
            for (const auto& entry : project.sources) {
                auto absPath = project.resolvePath(entry.filePath);
                effectiveOpts.sourceFiles.push_back(absPath.string());
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
                std::filesystem::path exePath(project.exeName);
                projectBaseName_ = exePath.stem().string();
            } else {
                std::filesystem::path vbpPath(srcFile);
                projectBaseName_ = vbpPath.stem().string();
            }
            // 注意: 不再设置effectiveOpts.outputFile, 让runLinker通过projectBaseName_统一处理
            // 这样确保输出路径始终包含outputDir前缀

            // P6.6: 从VBP工程类型推断是否为ActiveX DLL
            if (!effectiveOpts.isDll && project.projectType == VbpProjectType::ActiveXDLL) {
                effectiveOpts.isDll = true;
                if (effectiveOpts.verbose) {
                    std::cout << "c3: 检测到ActiveX DLL工程 (Type=DLL)" << std::endl;
                }
            }
            // ProgID前缀: 优先CLI指定, 否则用工程名
            if (effectiveOpts.isDll && effectiveOpts.dllProgId.empty()) {
                effectiveOpts.dllProgId = project.projectName.empty() ? "VB6DLL" : project.projectName;
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
            std::cerr << "c3: --dump-frm: 未找到.frm文件\n";
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
            std::cerr << "c3: note: TypeLib import skipped, using late binding" << std::endl;
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

    // === 阶段4: 代码生成 ===
    // 确定输出目录
    std::string outputDir = effectiveOpts.outputDir.empty() ? "output" : effectiveOpts.outputDir;
    if (!std::filesystem::exists(outputDir)) {
        std::filesystem::create_directories(outputDir);
    }

    if (!runCodeGeneration(effectiveOpts, outputDir)) {
        std::cerr << diag_->toString();
        result.errorCount = diag_->errorCount();
        result.warningCount = diag_->warningCount();
        return result;
    }

    // === 阶段5: 链接 ===
    if (!runLinker(effectiveOpts, outputDir)) {
        std::cerr << diag_->toString();
        result.errorCount = diag_->errorCount();
        result.warningCount = diag_->warningCount();
        return result;
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
        auto buffer = SourceBuffer::fromFile(filePath);
        if (!buffer) {
            SourceLocation loc{filePath, 0, 0};
            diag_->error(DiagnosticID::LexFileEncodingError, loc,
                "无法打开文件: " + filePath);
            return false;
        }

        // 根据文件扩展名判断模块类型
        bool isClassModule = false;
        bool isFormModule = false;
        FrmFile frmDesc;  // P7: 窗体描述 (仅.frm有效)
        if (filePath.size() >= 4) {
            std::string ext = filePath.substr(filePath.size() - 4);
            for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            isClassModule = (ext == ".cls");
            isFormModule = (ext == ".frm");  // P7: 窗体模块

            // P7: 解析.frm窗体描述, 提取VB代码段
            if (isFormModule) {
                frmDesc = FrmParser::parse(filePath);
                // 用代码段替换原始源文件内容 (窗体描述块不是VB代码)
                if (!frmDesc.codeSection.empty()) {
                    buffer = SourceBuffer::fromString(filePath, frmDesc.codeSection);
                }
            }
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
                std::filesystem::path p(filePath);
                module->moduleName = p.stem().string();
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
        std::cerr << "c3: TypeLib import: ";
        int totalCoClasses = 0, totalIfaces = 0;
        for (auto& tl : typelibParser_->cachedResults()) {
            totalCoClasses += (int)tl->coclasses.size();
            totalIfaces += (int)tl->interfaces.size();
        }
        std::cerr << totalCoClasses << " coclasses, " << totalIfaces
                  << " interfaces from " << typelibParser_->cachedResults().size()
                  << " type libraries" << std::endl;
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
        std::filesystem::path p(module->filename);
        moduleBaseNames.push_back(p.stem().string());
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
        std::string dllEntryCode = dllCgen.generateDllEntry(options.dllProgId, allSymTabs);

        std::string dllEntryPath = outputDir + "/dll_entry.c";
        {
            std::ofstream ofs(dllEntryPath, std::ios::out | std::ios::trunc);
            if (!ofs) {
                std::cerr << "c3: 无法写入文件: " << dllEntryPath << std::endl;
                return false;
            }
            ofs << dllEntryCode;
        }

        if (options.verbose) {
            std::cout << "c3: 生成 " << dllEntryPath << " (" << dllEntryCode.size() << " bytes)" << std::endl;
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

    // P6.6: ActiveX DLL模式, 加入 dll_entry.c
    if (options.isDll) {
        std::string dllEntryPath = outputDir + "/dll_entry.c";
        msvcOpts.sourceFiles.push_back(dllEntryPath);
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
    std::string outputExt = options.isDll ? ".dll" : ".exe";
    if (!options.outputFile.empty()) {
        // 用户指定了绝对/相对路径
        msvcOpts.outputFile = options.outputFile;
        // P6.6: 如果是DLL模式且用户指定了.exe后缀, 自动改为.dll
        if (options.isDll && msvcOpts.outputFile.size() >= 4 &&
            msvcOpts.outputFile.compare(msvcOpts.outputFile.size()-4, 4, ".exe") == 0) {
            msvcOpts.outputFile.replace(msvcOpts.outputFile.size()-4, 4, ".dll");
        }
    } else if (!projectBaseName_.empty()) {
        // VBP工程: 使用工程基名
        msvcOpts.outputFile = outputDir + "/" + projectBaseName_ + outputExt;
    } else if (modules_.size() == 1) {
        std::filesystem::path p(modules_[0]->filename);
        msvcOpts.outputFile = outputDir + "/" + p.stem().string() + outputExt;
    } else {
        msvcOpts.outputFile = outputDir + "/a" + outputExt;
    }

    msvcOpts.isDll = options.isDll;  // P6.6: DLL编译模式
    // P7: 检测是否为GUI程序 (包含窗体模块)
    for (auto& module : modules_) {
        if (module->isFormModule) {
            msvcOpts.isGui = true;
            break;
        }
    }
    msvcOpts.verbose = options.verbose;
    msvcOpts.debugInfo = options.debugInfo;
    msvcOpts.optimizationLevel = options.optimizationLevel;

    // P6.6: ActiveX DLL模式, 生成.def导出文件
    if (options.isDll) {
        std::string defPath = outputDir + "/activex_dll.def";
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
                std::cout << "c3: 生成导出定义: " << defPath << std::endl;
            }
        }
    }

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
              << "  --dll               编译为ActiveX DLL (P6.6)\n"
              << "  --progid <前缀>     ActiveX DLL的ProgID前缀\n"
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