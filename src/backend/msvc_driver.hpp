#pragma once
// vb6c3 - MSVC编译驱动
// 探测vcvarsall环境，调用cl.exe编译C代码，link.exe链接

#include <string>
#include <vector>

namespace vb6c3 {

struct MsvcDriverOptions {
    std::vector<std::string> sourceFiles;  // .c 文件路径
    std::string outputFile;                 // 输出 .exe/.dll 路径
    std::string rtlDir;                     // vb6rtl.h / vb6rtl.c 所在目录
    bool verbose = false;
    bool debugInfo = false;
    int optimizationLevel = 0;
    bool isDll = false;                     // P6.6: ActiveX DLL模式
    std::string defFile;                    // P6.6: DLL导出定义文件(.def)路径
    bool isGui = false;                     // P7: GUI程序 (Win32窗口, 非控制台)
    std::string typelibResFile;              // P6.13: .res文件路径 (已编译好的资源)
    std::string objDir;                      // P11.2: .obj intermediate directory
    std::string srcDir;                      // P11.2: generated .c/.h directory (/I include path)
};

class MsvcDriver {
public:
    MsvcDriver();
    ~MsvcDriver();

    // 编译+链接: .c → .exe 或 .dll (取决于isDll)
    bool compileAndLink(const MsvcDriverOptions& options);

    // 检测cl.exe是否可用
    static bool isMsvcAvailable();

private:
    // 查找cl.exe路径
    std::string findClExe() const;

    // 执行命令行
    int executeCommand(const std::string& cmd) const;
};

} // namespace vb6c3
