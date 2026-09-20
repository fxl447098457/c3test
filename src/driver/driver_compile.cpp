// driver_compile.cpp - C3 编译器驱动: 编译入口与主流程编排
// 2026-09-17 从 src/driver/driver.cpp 纯搬移（逐行未改）：
//   原第 182~544 行

#include "driver/driver.hpp"
#include "common/diagnostics.hpp"
#include "common/encoding.hpp"
#include "common/source_manager.hpp"
#include "ast/ast.hpp"
#include "semantics/semantic_analyzer.hpp"
#include "driver/rtl_embedded.hpp"
#include "project/vbp_parser.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace vb6c3 {

// === 编译入口 ===

CompileResult Driver::compile(int argc, char* argv[]) {
    auto [opts, code] = parseArgs(argc, argv);
    if (code != 0) {
        return {false, "", 1, 0};          // 参数错误 (未知选项 / 未指定源文件)
    }
    if (opts.sourceFiles.empty()) {
        // -h / --help / --version: 已显示帮助或版本, 属"正常退出"而非失败.
        // success=true 是这里的必要标记: 真正的编译失败也会给出
        // success=false 且 errorCount=0 (如 GUI 工程链接未产出 exe), main.cpp 正是
        // 依据 success 区分两者 — 若此处留 false 则帮助/版本会被判为失败 (退出码 1).
        return {true, "", 0, 0};
    }
    return compile(opts);
}

CompileResult Driver::compile(const CompileOptions& options) {
    CompileResult result;
    diag_->clear();

    // 应用警告抑制 (性能优化: 减少大型项目的日志I/O)
    for (int id : options.suppressedWarningIds) {
        diag_->suppress(static_cast<DiagnosticID>(id));
    }

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
                // srcFile 是 UTF-8, 必须用 utf8ToPath 构造; 直接用 path(const char*)
                // 会让 Windows 按 ACP 解释, 中文被破坏成 '?' -> 输出文件名非法 -> LNK1104
                std::filesystem::path vbpPath(utf8ToPath(srcFile));
                projectBaseName_ = pathToUtf8(vbpPath.stem());
            }
            // 注意: 不再设置effectiveOpts.outputFile, 让runLinker通过projectBaseName_统一处理
            // 这样确保输出路径始终包含outputDir前缀
            // P11.1: Save VBP Path32 for output directory resolution
            if (!project.outputPath.empty()) {
                projectPath32_ = pathToUtf8(project.resolvePath(project.outputPath));
            }

            // Fix 142: 保存 Startup= 启动对象, 供代码生成阶段决定哪个模块生成入口点
            startupObject_ = project.startupObject;


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
                    } else {
                        // P24-04 续: VBP 里写死的类型库路径在本机不存在时原为静默丢弃,
                        // 只留 GUID 去查注册表; 两者都失败就完全没有该类型库信息,
                        // 早绑定 / GlobalNameSpace 语法随即退化成未定义符号 (LNK2019).
                        // 明确提示路径无效, 便于判断是否需先注册该类型库.
                        diag_->warn(DiagnosticID::CodeGenUnsupportedFeature, SourceLocation{},
                                    "TypeLib reference path not found: " + resolvedPath.u8string());
                    }
                }
            }
            // Object= GUID (ActiveX controls) -> loadByClsid
            for (const auto& obj : project.objects) {
                if (!obj.guid.empty()) {
                    effectiveOpts.typelibRefs.push_back(obj.guid);
                    // Fix 143: 记录 OCX 文件路径, 供运行时 LoadLibrary+DllGetClassObject 免注册实例化第三方 OCX 控件.
                    // 注意区分两种路径:
                    //   - typelibRefs(下方) 用【绝对】路径: 那是【编译期】读取 OCX 类型库用, 必须在构建机找得到;
                    //   - ocxFiles_/ocxRefs_(下方) 烘焙【相对 exe】的路径: 那是【运行期】用, 支持把 OCX 放进
                    //     exe 旁的子目录 (如 bin\) 后在任意机器免注册加载, 不依赖构建机的绝对目录.
                    if (!obj.fileName.empty()) {
                        auto ocxPath = project.resolvePath(obj.fileName);
                        if (std::filesystem::exists(ocxPath)) {
                            effectiveOpts.typelibRefs.push_back(pathToUtf8(std::filesystem::absolute(ocxPath)));
                            std::string g143 = obj.guid;
                            std::transform(g143.begin(), g143.end(), g143.begin(), ::tolower);
                            g143.erase(std::remove(g143.begin(), g143.end(), '{'), g143.end());
                            g143.erase(std::remove(g143.begin(), g143.end(), '}'), g143.end());
                            ocxFiles_[g143] = obj.fileName;   // 相对 exe 的路径 (如 "bin\\NewTab01.ocx")
                            ocxRefs_.push_back({g143, obj.fileName});
                        }
                    }
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
    std::string rtlDir = session.create();
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

} // namespace vb6c3
