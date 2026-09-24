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
#include <algorithm>
#include <cctype>
#include <unordered_set>

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
    // vb6forms_axsite.c 按功能家族拆 5 个编译单元 (2026-09-20): 伞文件本身不参与编译
    // 注意: axsite/ 下的 .c 解包后是平铺目录, 故这里写 basename 而非带子目录路径
    opts.sourceFiles.push_back(rtlDir + "/ax_site.c");
    opts.sourceFiles.push_back(rtlDir + "/ax_site_ext.c");
    opts.sourceFiles.push_back(rtlDir + "/ax_propbag.c");
    opts.sourceFiles.push_back(rtlDir + "/ax_load.c");
    opts.sourceFiles.push_back(rtlDir + "/ax_host.c");
    // Fix 112: 工程内 UserControl 实例宿主 + 宿主对象模型 (2026-09-19 按族拆 6 单元)
    // 注意: uc/ 下的 .c 解包后是平铺目录，故这里写 basename 而非带子目录路径
    opts.sourceFiles.push_back(rtlDir + "/uc_host.c");
    opts.sourceFiles.push_back(rtlDir + "/uc_host_window.c");
    opts.sourceFiles.push_back(rtlDir + "/uc_hostmodel.c");
    opts.sourceFiles.push_back(rtlDir + "/uc_controls.c");
    opts.sourceFiles.push_back(rtlDir + "/uc_collection.c");
    opts.sourceFiles.push_back(rtlDir + "/uc_debug.c");
    // Fix 120-141 移植 (2026-09-20): PropertyBag (IDispatch) 编译单元
    opts.sourceFiles.push_back(rtlDir + "/uc_propbag.c");
    // Fix 148: OCX 真宿主 (免注册 LoadLibrary + DllGetClassObject) —— NewTab 等第三方 32 位 OCX
    opts.sourceFiles.push_back(rtlDir + "/vb6forms_axcontainer.c");
}

// ============================================================
// ai/vb-asm-extension-spec: Asm 过程 → MASM (.asm) → ml64 → .obj
//   v1 (x64): 每个「函数体 = 单个 Asm 块」的过程降级为独立 MASM 过程。
//   按名引用 `[param]` → Win64 ABI 寄存器 (RCX,RDX,R8,R9), `[Function]` → RAX;
//   `'` 注释 → MASM `;`; `.name:` 局部标签 → `<proc>_<name>` (MASM 无 proc 局部标签,
//   且一个 .asm 里多个 PROC 的裸标签会撞名)。
// ============================================================

static void toLowerAscii(std::string& s) {
    for (auto& c : s) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
}

// 把一个 Asm 过程写成 MASM PROC 体
static void emitMasmProc(std::ostream& os, const AsmProcInfo& p) {
    // [name] → ABI 寄存器 / [Function] → 与返回类型同宽的返回寄存器。
    // 替换表在 asm_proc.hpp (asmBuildX64Subs) —— codegen 期的宽度校验 (3038) 用的是
    // 同一张表, 改映射两边自动一致, 不会再各写一份走偏。
    auto subs = asmBuildX64Subs(p);

    std::vector<std::string> body = asmRewriteLines(p.lines, subs, p.cName);

    // callee-saved 自动保存 (spec §5 第 3 条 / §7): 扫描块内实际用到的 + clobber 声明的。
    // `<Naked>` 下不生成任何保存代码 —— 用户全权负责 (含自己 ret)。
    std::vector<std::string> saved =
        p.naked ? std::vector<std::string>()
                : asmSavedRegsForArch(p.lines, p.clobbers, /*x64=*/true);

    os << "; Win64 ABI: RCX,RDX,R8,R9 = 整型参数; RAX = 返回\n";
    if (!saved.empty()) {
        os << "; callee-saved 自动保存:";
        for (auto& r : saved) os << " " << r;
        os << " (块内使用/ clobber 声明)\n";
    }
    os << p.cName << " PROC\n";
    for (auto& r : saved) os << "    push " << r << "\n";

    bool lastWasRet = false;
    for (auto& line : body) {
        os << "    " << line << "\n";
        std::string tline = line; toLowerAscii(tline);
        size_t s = tline.find_first_not_of(" \t");
        lastWasRet = (s != std::string::npos && tline.compare(s, 3, "ret") == 0 &&
                      (s + 3 >= tline.size() || tline[s + 3] == ' ' || tline[s + 3] == ';'));
    }
    for (auto it = saved.rbegin(); it != saved.rend(); ++it) os << "    pop " << *it << "\n";
    if (!p.naked && !lastWasRet) os << "    ret\n";   // 非 Naked: 叶函数, 编译器补返回
    os << p.cName << " ENDP\n";
}

// driver_link.cpp 内的落盘 + 汇编 + 收集
static bool assembleAsmProcs(const std::vector<EmittedAsmProc>& procs,
                             const std::string& intermediatesDir, MsvcDriverOptions& msvcOpts,
                             bool verbose) {
    namespace fs = std::filesystem;
    std::string ml64 = MsvcDriver::findMl64Exe();

    // 按模块基名分组 → 一个模块一个 .asm (多 PROC 同文件)
    std::vector<std::string> order;
    std::map<std::string, std::vector<const AsmProcInfo*>> byBase;
    for (auto& ep : procs) {
        std::string key = ep.moduleBase;
        if (byBase.find(key) == byBase.end()) order.push_back(key);
        byBase[key].push_back(&ep.info);
    }

    for (auto& base : order) {
        std::string asmPath = intermediatesDir + "/" + base + "_asm.asm";
        std::string objPath = intermediatesDir + "/" + base + "_asm.obj";
        {
            std::ofstream ofs(asmPath, std::ios::out | std::ios::trunc);
            if (!ofs) {
                std::cerr << "C3: error: 无法写入汇编文件: " << asmPath << std::endl;
                return false;
            }
            ofs << "; === C3 auto-generated (ai/vb-asm-extension-spec) ===\n";
            ofs << "; Asm 块过程降级为独立 MASM 过程 (Win64 ABI)\n";
            ofs << "_TEXT SEGMENT\n";
            for (auto* p : byBase[base]) emitMasmProc(ofs, *p);
            ofs << "_TEXT ENDS\n";
            ofs << "END\n";
        }
        // ml64 /c /Fo <obj> <asm> —— 外层多包一层引号: executeCommand 走
        // "cmd /c <cmd>", cmd 在 /c 后首字符是引号时会剥首尾各一个 (同 rc.exe 的先例)。
        std::string args = "\"" + ml64 + "\" /nologo /c /Fo \"" + objPath + "\" \"" + asmPath + "\"";
        if (verbose) std::cout << "C3: ml64: " << args << std::endl;
        int ret = MsvcDriver::executeCommand("\"" + args + "\"");
        if (ret != 0 || !fs::exists(utf8ToPath(objPath))) {
            std::cerr << "C3: error: ml64 汇编失败 (" << asmPath << "), 退出码 " << ret << std::endl;
            return false;
        }
        msvcOpts.extraObjects.push_back(objPath);
    }
    return true;
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
        // 泛型模板类 (G4): 未发码 (见 driver_codegen_module_loop), 无 .c 可链
        if (!modules_[i]->classTypeParams.empty()) continue;
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
    // Fix 165: 入口点跟着启动对象走 — 见 msvc_driver.hpp 的 entryIsMain 注释。
    // 与 cgen_base_generate_entry.inc:54 用的是同一个判定 (`sub main`), 两处若改须同改。
    if (msvcOpts.isGui) {
        std::string so165 = startupObject_;
        std::transform(so165.begin(), so165.end(), so165.begin(), ::tolower);
        msvcOpts.entryIsMain = (so165 == "sub main");
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
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6com_collection.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6com_collection_enum.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6_di_stubs.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6_di_win32_stubs.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6_di_user32_stubs.c");
    // gdiplus 二级拆分 (2026-09-20): 解包后是平铺目录, 故写 basename
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6_di_gdiplus_draw_stubs.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6_di_gdiplus_text_stubs.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6_di_gdiplus_image_stubs.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6_di_gdiplus_brush_stubs.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6_di_gdiplus_path_stubs.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6_di_crypto_stubs.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6_di_com_stubs.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6_di_net_stubs.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6_di_shell_stubs.c");
    // Fix 160y: unknown 族 (无 vb6_di_lib 标记的杂项符号: Imm/version/TransparentBlt/msvbvm60)
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6_di_unknown_stubs.c");
    // 092z-3 + Fix 112: vb6forms 组对**所有**工程类型都链接 —— vb6rtl/vb6com 的
    // 宿主对象分派挂接点 (vb6_Host_*) 引用 vb6forms_uc, 不能只在 GUI/DLL 下链接.
    // (原先 Fix 096 的按需扫描已不需要: 一律链接.)
    addFormsSources(msvcOpts, rtlDir);
    // ExeComBridge 01: vb6comserver 组无条件链入 (原先仅 DLL).
    //   EXE 的工程类实例也要能被包装成 IDispatch (vb6_ComObject_FromInstance /
    //   vb6_FindCoClassDesc 定义在 vb6comserver_obj.c); 纯 EXE 下无调用方,
    //   不产生新的外部库依赖 (advapi32.lib 三种链接分支本就都带).
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6comserver.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6comserver_obj.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6comserver_factory.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6comserver_cp.c");
    msvcOpts.sourceFiles.push_back(rtlDir + "/vb6comserver_pci.c");
    // ExeComBridge 01: com_entry.c = EXE 模式的 coclass 表 (driver_codegen_dll_entry.inc
    //   生成于中间目录), 提供 g_vb6_coclasses / g_vb6_coclassCount (vb6comserver_obj.c
    //   的 vb6_FindCoClassDesc 引用). DLL 模式由 dll_entry.c 提供同名符号且不生成
    //   com_entry.c → 仅 EXE 链入, 无条件加会让 DLL 的 cl 报 C1083.
    if (!msvcOpts.isDll) {
        msvcOpts.sourceFiles.push_back(intermediatesDir + "/com_entry.c");
    }

    msvcOpts.verbose = options.verbose;
    msvcOpts.debugInfo = options.debugInfo;
    msvcOpts.optimizationLevel = options.optimizationLevel;
    msvcOpts.arch = options.arch;  // DualArch: pass target architecture

    // === ai/024 T02: 用户静态库 → 链接输入 ===
    // 两类来源合并:
    //   A) Declare 的 `Lib "x.lib"` 静态形态 → staticLibResolved_ (T01 已解析, 全是绝对路径)
    //   B) vbp `ExtraLib=` / CLI `--extra-lib` → extraLibResolved_ (绝对路径, 或"放行给
    //      链接器"的裸名 —— 靠 LIB 环境变量与下面的 /LIBPATH 找)
    // 发码侧对静态 Declare 只发 `extern <真实导出名>` 而**不发** `#pragma comment(lib,...)`
    // (见 cgen_decl_api.cpp 的 isStaticDecl), 所以这两份清单就是静态库进入链接的**唯一**通路。
    {
        std::unordered_set<std::string> seen;
        auto pushInput = [&](const std::string& p) {
            if (p.empty()) return;
            std::string key = p;
            for (size_t i = 0; i < key.size(); i++) {
                key[i] = static_cast<char>(::tolower(static_cast<unsigned char>(key[i])));
            }
            if (!seen.insert(key).second) return;
            msvcOpts.userLibInputs.push_back(p);
        };
        for (const auto& kv : staticLibResolved_) pushInput(kv.second);
        for (const auto& p : extraLibResolved_) pushInput(p);

        // 搜索根 → /LIBPATH:, 顺序即静态库搜索顺序:
        // vbp `LibDir=` → CLI `--libdir` → `<工程目录>/Lib` (显式总赢过隐式)。
        for (const auto& r : staticLibPaths_.roots()) msvcOpts.libSearchPaths.push_back(r);

        // ai/024 E4 (T04b): Alias "_foo@12" 逃生舱的 /alternatename 桥接指令。
        // 同名 Declare 在多模块重复出现会生成重复指令 → 按整串去重 (符号名
        // 大小写在链接器眼里有区分, 这里保序保原文)。
        {
            std::unordered_set<std::string> altSeen;
            for (const auto& a : staticLibAlternatenames_) {
                if (a.empty() || !altSeen.insert(a).second) continue;
                msvcOpts.alternatenames.push_back(a);
            }
        }

        if (options.verbose && (!msvcOpts.userLibInputs.empty() || !msvcOpts.libSearchPaths.empty())) {
            std::cout << "C3: link inputs -- user libs (" << msvcOpts.userLibInputs.size() << "):";
            for (const auto& l : msvcOpts.userLibInputs) std::cout << " " << l;
            std::cout << std::endl;
        }
    }

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
                // executeCommand 内部拼的是 "cmd.exe /c <cmd>", 且带 CREATE_NO_WINDOW (不弹窗口)。
                // 必须再包一层引号: cmd 在「/c 之后首字符是引号」时会剥掉首尾各一个引号。
                // rcArgs 本身以 "<rc.exe 路径>" 开头 (路径含空格), 少这层外层引号时 cmd 会把
                // rc 路径剥成 C:\...\rc.exe" -> 找不到命令 -> rcRet != 0 被静默跳过,
                // TypeLib 资源不再嵌入 (实测 DLL 少 141KB)。多包一层让 cmd 剥外层、留下 rc 自己的引号。
                int rcRet = MsvcDriver::executeCommand("\"" + rcArgs.str() + "\"");
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
            // 同 rcArgs: 必须多包一层引号, 否则 cmd 剥引号后 rc 路径被破坏 (见上文注释)
            int verRcRet = MsvcDriver::executeCommand("\"" + verRcArgs.str() + "\"");
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

    // ai/vb-asm-extension-spec: Asm 块过程 → .asm → ml64 → .obj → 链接输入
    if (!asmProcs_.empty()) {
        if (!assembleAsmProcs(asmProcs_, intermediatesDir, msvcOpts, options.verbose)) {
            return false;
        }
    }

    MsvcDriver msvc;
    // 编译前自愈: 会话 rtl/ 下的 RTL 源文件可能已被兄弟进程的 cleanupOldSessions
    // 误删 (并发编译下 cl /MP 尚未处理的那些源文件会报 C1083). 缺什么补什么.
    if (!session.restoreMissing()) {
        std::cerr << "C3: error: RTL sources incomplete before compile (" << rtlDir << ")" << std::endl;
        return false;
    }
    return msvc.compileAndLink(msvcOpts);
}

} // namespace vb6c3
