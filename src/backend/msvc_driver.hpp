#pragma once
// vb6c3 - MSVC compile driver
// Detect vcvarsall environment (vswhere/registry fallback), invoke cl.exe to compile C code, link.exe to link

#include <string>
#include <vector>

namespace vb6c3 {

struct MsvcDriverOptions {
    std::vector<std::string> sourceFiles;  // .c file paths
    std::string outputFile;                 // output .exe/.dll path
    std::string rtlDir;                     // directory containing vb6rtl.h / .lib
    bool verbose = false;
    bool debugInfo = false;
    int optimizationLevel = 0;
    bool isDll = false;                     // P6.6: ActiveX DLL mode
    std::string defFile;                    // P6.6: DLL export definition file (.def) path
    bool isGui = false;                     // P7: GUI program (Win32 window, not console)
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

private:
    // Find cl.exe path
    std::string findClExe() const;

    // P11.4: Find VS installation path via vswhere.exe or registry
    static std::string findVsInstallPath();

    // Build vcvarsall setup prefix for cl.exe command
    std::string buildVcvarsPrefix(const std::string& arch = "x64") const;

    // Execute command line
    int executeCommand(const std::string& cmd) const;

    // opt3: 增量编译 — 基于内容哈希跳过未变化的 .c 编译
    bool compileAndLinkIncremental(const MsvcDriverOptions& options);
};

} // namespace vb6c3
