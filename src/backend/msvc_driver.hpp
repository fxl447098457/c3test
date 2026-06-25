#pragma once
// vb6c3 - MSVC编译驱动
// 探测vcvarsall环境，调用cl.exe编译C代码，link.exe链接

#include <string>
#include <vector>

namespace vb6c3 {

struct MsvcDriverOptions {
    std::vector<std::string> sourceFiles;  // .c 文件路径
    std::string outputFile;                 // 输出 .exe 路径
    std::string rtlDir;                     // vb6rtl.h / vb6rtl.c 所在目录
    bool verbose = false;
    bool debugInfo = false;
    int optimizationLevel = 0;
};

class MsvcDriver {
public:
    MsvcDriver();
    ~MsvcDriver();

    // 编译+链接: .c → .exe
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
