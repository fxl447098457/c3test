#pragma once
// VB6 窗体文件 (.frm) 解析器
// .frm 文件结构:
//   VERSION 5.00
//   Begin VB.Form FormName
//      Caption = "Hello"
//      ClientHeight = 3000
//      ...
//      Begin VB.CommandButton Command1
//         Caption = "OK"
//         ...
//      End
//   End
//   Attribute VB_Name = "Form1"
//   <VB代码段>

#include <string>
#include <vector>
#include <map>
#include <filesystem>

namespace vb6c3 {

// ============================================================
// 窗体属性值 — 支持多种类型
// ============================================================

enum class FrmValueType {
    Integer,        // 整数: 3000, -1
    Float,          // 浮点: 8.25
    String,         // 字符串: "Hello"
    Identifier,     // 标识符: vbModal, vbUpperCase
};

struct FrmValue {
    FrmValueType type;
    std::string rawText;        // 原始文本（字符串含引号）
    long long intValue = 0;     // type==Integer时有效
    double floatValue = 0.0;    // type==Float时有效

    // 便捷构造
    static FrmValue fromInt(long long v, const std::string& raw) {
        return {FrmValueType::Integer, raw, v, 0.0};
    }
    static FrmValue fromFloat(double v, const std::string& raw) {
        return {FrmValueType::Float, raw, 0, v};
    }
    static FrmValue fromString(const std::string& raw) {
        return {FrmValueType::String, raw, 0, 0.0};
    }
    static FrmValue fromIdent(const std::string& raw) {
        return {FrmValueType::Identifier, raw, 0, 0.0};
    }
};

// ============================================================
// 复合属性块 (BeginProperty ... EndProperty)
// ============================================================

struct FrmPropertyBlock {
    std::string blockName;          // "Font", "FontTrans", 等
    std::map<std::string, FrmValue> properties;   // Name="MS Sans Serif", Size=8.25, 等
};

// ============================================================
// 控件描述
// ============================================================

enum class FrmControlType {
    // VB6标准控件 (VB.xxx)
    Form,               // VB.Form — 窗体自身
    CommandButton,      // VB.CommandButton
    TextBox,            // VB.TextBox
    Label,              // VB.Label
    CheckBox,           // VB.CheckBox
    OptionButton,       // VB.OptionButton
    ListBox,            // VB.ListBox
    ComboBox,           // VB.ComboBox
    Frame,              // VB.Frame
    PictureBox,         // VB.PictureBox
    Timer,              // VB.Timer
    HScrollBar,         // VB.HScrollBar
    VScrollBar,         // VB.VScrollBar
    Image,              // VB.Image
    Shape,              // VB.Shape
    Line,               // VB.Line
    Data,               // VB.Data
    OLE,                // VB.OLE
    DriveListBox,       // VB.DriveListBox
    DirListBox,         // VB.DirListBox
    FileListBox,        // VB.FileListBox
    Menu,               // VB.Menu (菜单项)
    Toolbar,            // MSComctlLib.Toolbar 等 (第三方)
    StatusBar,          // MSComctlLib.StatusBar
    CommonDialog,       // MSComDlg.CommonDialog
    Unknown,            // 未识别的控件类型
};

struct FrmControl {
    FrmControlType controlType = FrmControlType::Unknown;
    std::string controlTypeName;       // 原始类型名: "VB.CommandButton", "MSComctlLib.Toolbar"
    std::string controlName;           // 控件实例名: "Command1", "Text1"
    int index = -1;                    // 控件数组索引 (-1=非数组)

    // 简单属性: Caption="OK", Left=120, Top=240, ...
    std::map<std::string, FrmValue> properties;

    // 复合属性块: Font, MouseIcon, etc.
    std::vector<FrmPropertyBlock> propertyBlocks;

    // 嵌套子控件 (Frame内的控件, Menu子项等)
    std::vector<FrmControl> children;

    // 事件处理桩标记 (从属性推断)
    // VB6中控件的事件处理器格式: Sub ControlName_EventName()
};

// ============================================================
// 窗体描述 (解析结果)
// ============================================================

struct FrmFormDesc {
    std::string formName;              // 窗体名: "Form1"
    FrmControl formControl;            // 窗体自身的属性 (Caption, ClientHeight, 等)
                                       // formControl.controlType == FrmControlType::Form

    // 顶层控件列表 (直接放在窗体上的控件)
    // formControl.children 就是顶层控件

    // .frx 二进制资源引用 (图片等)
    // 格式: 属性值中出现的 $begin...$end 块 或 :X 形式的偏移引用
    std::map<std::string, std::string> frxReferences;  // key=偏移, value=资源描述
};

// ============================================================
// .frm 文件完整解析结果
// ============================================================

struct FrmFile {
    std::string version;               // "5.00"
    FrmFormDesc form;                  // 窗体描述
    std::string codeSection;           // VB代码段 (Attribute和VB代码, 不含Begin...End)
    std::filesystem::path frmFilePath; // 文件路径
};

// ============================================================
// .frm 解析器
// ============================================================

class FrmParser {
public:
    // 解析 .frm 文件
    static FrmFile parse(const std::string& frmFilePath);

    // 解析 .frm 内容字符串
    static FrmFile parseString(const std::string& content, const std::string& frmFilePath = "");

    // 控件类型名 → FrmControlType
    static FrmControlType parseControlType(const std::string& typeName);

    // FrmControlType → Win32窗口类名 (用于代码生成)
    static const char* controlTypeToWin32Class(FrmControlType type);

    // FrmControlType → VB6运行时类型名 (用于符号表)
    static const char* controlTypeToVb6Name(FrmControlType type);

private:
    // 行级解析辅助
    static std::string trim(const std::string& s);
    static std::string unquote(const std::string& s);
    static FrmValue parseValue(const std::string& s);

    // 解析 Begin...End 块 (控件或窗体)
    static FrmControl parseControlBlock(
        const std::vector<std::string>& lines,
        size_t& lineIdx,
        const std::string& parentIndent = "");

    // 解析 BeginProperty...EndProperty 块
    static FrmPropertyBlock parsePropertyBlock(
        const std::vector<std::string>& lines,
        size_t& lineIdx);

    // 解析属性行: Key = Value 或 Key = Value ' Comment
    static bool parsePropertyLine(const std::string& line,
        std::string& key, FrmValue& value);

    // 提取代码段 (Begin...End之后的内容)
    static std::string extractCodeSection(
        const std::vector<std::string>& lines,
        size_t startIdx);
};

} // namespace vb6c3
