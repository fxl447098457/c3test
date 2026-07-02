#pragma once
// VB6 工程文件 (.vbp) 解析器
// .vbp 是纯文本行导向的INI风格文件, 每行 Key=Value

#include <string>
#include <vector>
#include <filesystem>
#include "common/encoding.hpp"

namespace vb6c3 {

// 源文件条目类型
enum class VbpSourceType {
    Unknown,        // 未识别的源文件类型
    Form,           // Form=xxx.frm
    Module,         // Module=Name; xxx.bas
    Class,          // Class=Name; xxx.cls
    UserControl,    // UserControl=Name; xxx.ctl
    PropertyPage,   // PropertyPage=Name; xxx.pag
    Designer,       // Designer=Name; xxx.dsr
};

// 源文件条目
struct VbpSourceEntry {
    VbpSourceType type;
    std::string moduleName;     // 模块名 (Form无此字段, Module/Class有)
    std::string filePath;       // 文件路径 (相对或绝对)
    std::string clsidStr;       // P6.8: CLSID (仅Class, 格式: Class=Name; File.cls; {CLSID})
};

// 项目类型
enum class VbpProjectType {
    StandardExe,       // Type=Exe
    ActiveXExe,        // Type=OLE Exe
    ActiveXDLL,        // Type=DLL
    ActiveXControl,    // Type=Control
    Unknown,
};

// VBP 工程信息
struct VbpProject {
    // 项目基本信息
    VbpProjectType projectType = VbpProjectType::Unknown;
    std::string projectName;       // Name="xxx"
    std::string title;             // Title="xxx"
    std::string exeName;           // ExeName32="xxx.exe"
    std::string outputPath;        // Path32="xxx"
    std::string startupObject;     // Startup="Sub Main" 或 "Form1"
    std::string iconForm;          // IconForm="Form1"
    std::string helpFile;          // HelpFile=""
    std::string commandLine;       // Command32=""

    // 编译选项
    int compilationType = 0;       // 0=P-Code, 1=Native Code
    int optimizationType = 0;

    // 源文件列表
    std::vector<VbpSourceEntry> sources;

    // 引用 (类型库)
    struct Reference {
        std::string guid;
        int major = 0;
        int minor = 0;
        std::string path;
        std::string description;
    };
    std::vector<Reference> references;

    // Object引用 (OCX控件)
    struct ObjectRef {
        std::string guid;
        int major = 0;
        int minor = 0;
        std::string fileName;
    };
    std::vector<ObjectRef> objects;

    // 资源文件
    std::string resFile;

    // 版本信息 (P20-22)
    int majorVer = 1;
    int minorVer = 0;
    int revisionVer = 0;
    int autoIncrementVer = 0;
    std::string companyName;
    std::string fileDescription;
    std::string legalCopyright;
    std::string productName;
    std::string comments;
    std::string legalTrademarks;
    std::string originalFileName;

    // P22: ActiveX 二进制兼容和启动模式
    int compatibleMode = 0;  // 0=None, 1=Project, 2=Binary
    int startMode = 0;       // 0=Standalone, 1=ActiveX

    // .vbp 文件路径 (用于解析相对路径)
    std::filesystem::path vbpFilePath;

    // 获取源文件的绝对路径
    std::filesystem::path resolvePath(const std::string& relativePath) const {
        // relativePath is UTF-8 (from VBP parser GBK→UTF-8 conversion)
        // Must use utf8ToPath() to avoid ACP reinterpretation on Windows
        if (utf8ToPath(relativePath).is_absolute()) {
            return utf8ToPath(relativePath);
        }
        return vbpFilePath.parent_path() / utf8ToPath(relativePath);
    }
};

// VBP 文件解析器
class VbpParser {
public:
    // 解析 .vbp 文件
    static VbpProject parse(const std::string& vbpFilePath);

    // 解析 .vbp 内容字符串
    static VbpProject parseString(const std::string& content, const std::string& vbpFilePath = "");

private:
    static VbpProjectType parseProjectType(const std::string& value);
    static VbpSourceType parseSourceType(const std::string& key);
    static std::string unquote(const std::string& s);
};

} // namespace vb6c3
