#pragma once
// vb6c3 - MSVC compile driver
// Detect vcvarsall environment (vswhere/registry fallback), invoke cl.exe to compile C code, link.exe to link

#include <string>
#include <vector>
#include <ostream>

namespace vb6c3 {

struct MsvcDriverOptions {
    std::vector<std::string> sourceFiles;  // .c file paths
    std::string outputFile;                 // output .exe/.dll path
    std::string rtlDir;                     // directory containing vb6rtl.h / .c (RTL sources)
    bool verbose = false;
    bool debugInfo = false;
    int optimizationLevel = 0;
    bool isDll = false;                     // P6.6: ActiveX DLL mode
    std::string defFile;                    // P6.6: DLL export definition file (.def) path
    bool isGui = false;                     // P7: GUI program (Win32 window, not console)
    // GUI 工程链接入口点取决于**代码生成了哪个**: 启动对象是窗体 → `WinMain`
    // (SUBSYSTEM:WINDOWS 的默认入口即 WinMainCRTStartup); 启动对象是 `Sub Main` →
    // `main` → 必须显式 /ENTRY:mainCRTStartup, 否则 LNK2019 main。
    // 二者写死任一个都会让另一类工程链接失败, 所以由 driver 按启动对象决定。
    bool entryIsMain = false;
    std::string typelibResFile;              // P6.13: .res file path (compiled resource)
    std::string versionInfoResFile;          // P23-05: VS_VERSION_INFO .res file path
    std::string userResFile;                // P23-03: User-specified .res file (from VBP ResFile=)
    std::string objDir;                      // P11.2: .obj intermediate directory
    std::string srcDir;                      // P11.2: generated .c/.h directory (/I include path)
    std::string arch = "x64";               // DualArch: x64 or x86 — target binary architecture

    // 增量编译 (opt3): 基于内容哈希的obj级缓存。
    // cacheDir 存放复用的 .obj 与 cache.txt 索引，跨运行持久。
    bool incremental = false;
    std::string incrementalCacheDir;         // 持久obj缓存目录 (如 <outputDir>/.c3obj)

    // === ai/024 T02: 用户静态库 (归档) ===
    // 由 driver 从 LibSearchPaths 解析得到 —— Declare 的 `Lib "x.lib"` 静态形态
    // (driver 侧 staticLibResolved_) 与 vbp `ExtraLib=` / CLI `--extra-lib`
    // (extraLibResolved_)。路径已是绝对路径; 放行给链接器的裸名也在这里
    // (让它走 LIB 环境变量 / 本结构的 libSearchPaths)。
    std::vector<std::string> userLibInputs;
    // 追加成 /LIBPATH:"<dir>"，顺序即静态库的搜索顺序 (LibDir= → --libdir → <proj>/Lib)
    std::vector<std::string> libSearchPaths;
    // ai/024 E4 (T04b): /alternatename 桥接指令体, 元素形如 "internal=real"
    // (driver_staticlib.cpp 的 alternatenameDirective 生成, driver_link.cpp 去重)。
    // 用途: `Alias "_foo@12"` 里 '@' 无法作 C 标识符, 发码侧只能发清洗后的内部名,
    // 这里把内部修饰名桥回真实归档符号。落点实验见 024 §十二 T04b。
    std::vector<std::string> alternatenames;

    // ai/vb-asm-extension-spec: ml64 汇编产物的 .obj 路径 (Asm 块过程的降级目标)。
    // 与 sourceFiles 分开: cl 不吃 .asm, 这些是**已经汇编好的**目标文件, 只进链接。
    std::vector<std::string> extraObjects;
};

// 把用户静态库输入与搜索根追加到链接命令行。
// 两处链接路径 (msvc_driver.cpp 的 cl /link 与 msvc_driver_incremental.cpp 的 link.exe)
// 共用这一个函数，避免两边的库列表漂移。
inline void appendUserLibInputs(std::ostream& os, const MsvcDriverOptions& options) {
    for (const auto& d : options.libSearchPaths) os << " /LIBPATH:\"" << d << "\"";
    for (const auto& l : options.userLibInputs)  os << " \"" << l << "\"";
    // ai/024 E4 (T04b): /alternatename:<internal>=<real>。符号名无空格, 不加引号;
    // 须写**完整修饰名** (x86 stdcall 带前导 '_' 与 @N), 落点实验已验证。
    for (const auto& a : options.alternatenames) os << " /alternatename:" << a;
}

// ai/vb-asm-extension-spec: 把 ml64 汇编出的 .obj 追加为链接输入。
// 同 appendUserLibInputs 的理由: cl /link 与 link.exe 两条路径共用, 避免漂移。
inline void appendExtraObjects(std::ostream& os, const MsvcDriverOptions& options) {
    for (const auto& o : options.extraObjects) os << " \"" << o << "\"";
}

class MsvcDriver {
public:
    MsvcDriver();
    ~MsvcDriver();

    // Compile + link: .c -> .exe or .dll (depends on isDll)
    bool compileAndLink(const MsvcDriverOptions& options);

    // Detect if cl.exe is available (directly or via vswhere)
    static bool isMsvcAvailable();

    // P11.4: Get vcvarsall.bat path (empty if not found)
    static std::string findVcvarsallBat();

    // ai/vb-asm-extension-spec: 定位 ml64.exe (与 cl.exe 同一个 MSVC bin 目录)。
    // Asm 块过程的 x64 降级走 `ml64 /c` → .obj → 链接 (见 driver_link.cpp)。
    static std::string findMl64Exe();

    // 以**隐藏窗口**(CREATE_NO_WINDOW + SW_HIDE)执行命令行, 返回退出码。
    // 公开且 static: isMsvcAvailable 的 cl 探测、driver 层的 rc.exe 调用都要复用,
    // 避免各自 std::system 弹 cmd 窗口。
    static int executeCommand(const std::string& cmd);

    // 同上, 但把子进程 stdout 收进 out (用于 vswhere 这类需要回读输出的命令)
    static int executeCommandCapture(const std::string& cmd, std::string& out);

private:
    // Find cl.exe path
    std::string findClExe() const;

    // P11.4: Find VS installation path via vswhere.exe or registry
    static std::string findVsInstallPath();

    // Build vcvarsall setup prefix for cl.exe command
    std::string buildVcvarsPrefix(const std::string& arch = "x64") const;

    // opt3: 增量编译 — 基于内容哈希跳过未变化的 .c 编译
    bool compileAndLinkIncremental(const MsvcDriverOptions& options);
};

} // namespace vb6c3
