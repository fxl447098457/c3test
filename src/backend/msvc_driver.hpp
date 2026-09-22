#pragma once
// vb6c3 - MSVC compile driver
// Detect vcvarsall environment (vswhere/registry fallback), invoke cl.exe to compile C code, link.exe to link

#include <string>
#include <vector>

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
};

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
