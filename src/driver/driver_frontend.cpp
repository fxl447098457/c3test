// driver_frontend.cpp - C3 编译器驱动: 前端流水线（词法/预处理/解析/TypeLib 导入）
// 2026-09-17 从 src/driver/driver.cpp 纯搬移（逐行未改）：
//   原第 547~865 行

#include "driver/driver.hpp"
#include "common/diagnostics.hpp"
#include "common/encoding.hpp"
#include "common/source_manager.hpp"
#include "lexer/lexer.hpp"
#include "lexer/token.hpp"
#include "parser/parser.hpp"
#include "ast/ast_printer.hpp"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cctype>

namespace vb6c3 {

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

    // 类模块必须先于标准模块做语义分析。
    // 原因: 标准模块中引用项目类 (如 Dim WithEvents btn As Button) 时, 需要 Button
    // 的类符号已经在符号表中; 若类模块尚未分析, 类型解析会把 btn 退化为 void*,
    // 方法调用随之退化为 COM 后期绑定 (vb6_ComCall(btn, L"DoClick", ...)),
    // 而项目类实例实际是纯 C 结构体 (vb6_cls_Button*) → 运行期解引用 vtable 崩溃
    // (0xC0000005)。vbp 中 Module 完全可能排在 Class 之前 (如 test_events.vbp),
    // 故在此把类模块稳定前移 (同类之间、Form 与标准模块之间的相对顺序保持不变)。
    std::stable_sort(modules_.begin(), modules_.end(),
                     [](const auto& a, const auto& b) {
                         int rankA = a->isClassModule ? 0 : 1;
                         int rankB = b->isClassModule ? 0 : 1;
                         return rankA < rankB;
                     });

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

} // namespace vb6c3
