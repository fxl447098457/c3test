// driver_link.cpp - C3 编译器驱动: 链接阶段
// 2026-09-17 从 src/driver/driver.cpp 纯搬移（逐行未改）：
//   原第 2080~2418 行

#include "driver/driver.hpp"
#include "common/diagnostics.hpp"
#include "common/encoding.hpp"
#include "ast/ast.hpp"
#include "backend/msvc_driver.hpp"
#include "driver/rtl_embedded.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdlib>

namespace vb6c3 {

// vb6forms RTL 源文件集 (Fix 096 从 runLinker 内联清单抽出复用)
static void addFormsSources(MsvcDriverOptions& opts, const std::string& rtlDir) {
    opts.sourceFiles.push_back(rtlDir + "/vb6forms.c");
    opts.sourceFiles.push_back(rtlDir + "/vb6forms_ctrl.c");
    opts.sourceFiles.push_back(rtlDir + "/vb6forms_list.c");
    opts.sourceFiles.push_back(rtlDir + "/vb6forms_style.c");
    opts.sourceFiles.push_back(rtlDir + "/vb6forms_scroll.c");
    opts.sourceFiles.push_back(rtlDir + "/vb6forms_picture.c");
    opts.sourceFiles.push_back(rtlDir + "/vb6forms_picture_prop.c");
    opts.sourceFiles.push_back(rtlDir + "/vb6forms_ctrlarr.c");
    opts.sourceFiles.push_back(rtlDir + "/vb6forms_webview.c");
    opts.sourceFiles.push_back(rtlDir + "/vb6forms_widget.c");
    opts.sourceFiles.push_back(rtlDir + "/vb6forms_widget_prop.c");
    opts.sourceFiles.push_back(rtlDir + "/vb6forms_shape.c");
    opts.sourceFiles.push_back(rtlDir + "/vb6forms_axsite.c");
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
            std::filesystem::path p(utf8ToPath(options.outputFile));
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

    // RTL 源码编译 (P10 恢复): 会话目录释放的 RTL .c 与生成代码一起编译,
    // 不再链接预编译 .lib —— 修改 RTL 源码后重编 C3.exe 即生效
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6rtl.c");
    // vb6rtl.c 按家族拆分 (2026-09-17): 13 个族实现与主文件同批编译
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6rtl_string.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6rtl_format.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6rtl_conv.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6rtl_misc.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6rtl_system.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6rtl_compat.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6rtl_date.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6rtl_array.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6rtl_file.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6rtl_paramarray.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6rtl_financial.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6rtl_com.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6rtl_registry.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6com.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6com_invoke.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6com_pack.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6com_wrap.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6com_sink.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6com_foreach.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6_di_stubs.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6_di_win32_stubs.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6_di_user32_stubs.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6_di_gdiplus_stubs.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6_di_crypto_stubs.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6_di_com_stubs.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6_di_net_stubs.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6_di_shell_stubs.c");
    if (msvcOpts.isGui || msvcOpts.isDll) {
        // 092z-3: ActiveX DLL 允许包含窗体 (Form/UserControl), 也要 vb6forms
        addFormsSources(msvcOpts, rtlDir);
    } else {
        // Fix 096: 非 GUI 工程也可能引用 vb6forms RTL —— 标准模块裸 Print
        // 生成 vb6_Form_Print(NULL, ...) (C3 扩展语义), 之前不链 vb6forms
        // 报 LNK2019。扫描生成的 .c 源码, 引用了 vb6_Form_ 符号即补链。
        bool usesFormsRtl = false;
        for (auto& sf : msvcOpts.sourceFiles) {
            std::ifstream f(utf8ToPath(sf));
            if (!f) continue;
            std::string line;
            while (std::getline(f, line)) {
                if (line.find("vb6_Form_") != std::string::npos) {
                    usesFormsRtl = true;
                    break;
                }
            }
            if (usesFormsRtl) break;
        }
        if (usesFormsRtl) {
            addFormsSources(msvcOpts, rtlDir);
        }
    }
    if (msvcOpts.isDll) {
        msvcOpts.sourceFiles.push_back(rtlDir + "/vb6comserver.c");
        msvcOpts.sourceFiles.push_back(rtlDir + "/vb6comserver_obj.c");
        msvcOpts.sourceFiles.push_back(rtlDir + "/vb6comserver_factory.c");
        msvcOpts.sourceFiles.push_back(rtlDir + "/vb6comserver_cp.c");
        msvcOpts.sourceFiles.push_back(rtlDir + "/vb6comserver_pci.c");
    }

    msvcOpts.verbose = options.verbose;
    msvcOpts.debugInfo = options.debugInfo;
    msvcOpts.optimizationLevel = options.optimizationLevel;
    msvcOpts.arch = options.arch;  // DualArch: pass target architecture

    // opt3: 增量编译 — obj级缓存目录放在输出目录下, 跨运行持久
    msvcOpts.incremental = options.incremental;
    msvcOpts.incrementalCacheDir = outputDir + "/.c3obj";

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

} // namespace vb6c3
