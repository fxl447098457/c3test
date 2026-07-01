// vb6c3 - VB6窗体文件(.frm)解析器
// .frm 文件结构: VERSION行 + Begin...End窗体描述块 + Attribute行 + VB代码段
// 窗体描述块包含: 窗体属性 + 嵌套控件(递归Begin...End) + 复合属性(BeginProperty...EndProperty)

#include "project/frm_parser.hpp"
#include "common/source_manager.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace vb6c3 {

// ============================================================
// 工具函数
// ============================================================

std::string FrmParser::trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string FrmParser::unquote(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

FrmValue FrmParser::parseValue(const std::string& s) {
    std::string t = trim(s);
    if (t.empty()) return FrmValue::fromInt(0, "");

    // 字符串字面量
    if (t.size() >= 2 && t.front() == '"') {
        return FrmValue::fromString(t);
    }

    // 负数
    bool negative = false;
    size_t numStart = 0;
    if (t[0] == '-' || t[0] == '+') {
        negative = (t[0] == '-');
        numStart = 1;
    }

    // 检查是否为数字
    bool hasDot = false;
    bool allDigit = true;
    for (size_t i = numStart; i < t.size(); i++) {
        char c = t[i];
        if (c == '.' && !hasDot) {
            hasDot = true;
        } else if (!std::isdigit(static_cast<unsigned char>(c))) {
            allDigit = false;
            break;
        }
    }

    if (allDigit && numStart < t.size()) {
        if (hasDot) {
            double v = std::stod(t);
            return FrmValue::fromFloat(v, t);
        } else {
            long long v = std::stoll(t);
            return FrmValue::fromInt(v, t);
        }
    }

    // 标识符 (vbModal, vbUpperCase, True, False 等)
    return FrmValue::fromIdent(t);
}

bool FrmParser::parsePropertyLine(const std::string& line,
    std::string& key, FrmValue& value) {
    // 格式: Key = Value 或 Key = Value ' Comment
    // VB6 .frm属性行格式: 键名(可能含空格) = 值
    // 例如: "Caption         =   "Form1""
    // 注意: 等号前后可能有大量空格(对齐)

    auto eqPos = line.find('=');
    if (eqPos == std::string::npos) return false;

    key = trim(line.substr(0, eqPos));
    if (key.empty()) return false;

    // 跳过属性值后的注释: ' Windows Default
    std::string valuePart = line.substr(eqPos + 1);
    auto commentPos = valuePart.find('\'');
    if (commentPos != std::string::npos) {
        valuePart = valuePart.substr(0, commentPos);
    }

    value = parseValue(valuePart);
    return true;
}

// ============================================================
// 控件类型映射
// ============================================================

FrmControlType FrmParser::parseControlType(const std::string& typeName) {
    // typeName格式: "VB.CommandButton", "MSComctlLib.Toolbar", 等
    std::string lower = typeName;
    for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    // VB6标准控件 (VB.xxx)
    if (lower.find("vb.mdiform") != std::string::npos) return FrmControlType::MDIForm;
    if (lower.find("vb.form") != std::string::npos) return FrmControlType::Form;
    if (lower.find("vb.commandbutton") != std::string::npos) return FrmControlType::CommandButton;
    if (lower.find("vb.textbox") != std::string::npos) return FrmControlType::TextBox;
    if (lower.find("vb.label") != std::string::npos && lower.find("vb.labellistbox") == std::string::npos) return FrmControlType::Label;
    if (lower.find("vb.checkbox") != std::string::npos) return FrmControlType::CheckBox;
    if (lower.find("vb.optionbutton") != std::string::npos) return FrmControlType::OptionButton;
    if (lower.find("vb.listbox") != std::string::npos) return FrmControlType::ListBox;
    if (lower.find("vb.combobox") != std::string::npos) return FrmControlType::ComboBox;
    if (lower.find("vb.frame") != std::string::npos) return FrmControlType::Frame;
    if (lower.find("vb.picturebox") != std::string::npos) return FrmControlType::PictureBox;
    if (lower.find("vb.timer") != std::string::npos) return FrmControlType::Timer;
    if (lower.find("vb.hscrollbar") != std::string::npos) return FrmControlType::HScrollBar;
    if (lower.find("vb.vscrollbar") != std::string::npos) return FrmControlType::VScrollBar;
    if (lower.find("vb.image") != std::string::npos) return FrmControlType::Image;
    if (lower.find("vb.shape") != std::string::npos) return FrmControlType::Shape;
    if (lower.find("vb.line") != std::string::npos) return FrmControlType::Line;
    if (lower.find("vb.data") != std::string::npos) return FrmControlType::Data;
    if (lower.find("vb.ole") != std::string::npos) return FrmControlType::OLE;
    if (lower.find("vb.drivelistbox") != std::string::npos) return FrmControlType::DriveListBox;
    if (lower.find("vb.dirlistbox") != std::string::npos) return FrmControlType::DirListBox;
    if (lower.find("vb.filelistbox") != std::string::npos) return FrmControlType::FileListBox;
    if (lower.find("vb.menu") != std::string::npos) return FrmControlType::Menu;

    // WebBrowser (WebView2宿主)
    if (lower.find("webbrowser") != std::string::npos) return FrmControlType::WebBrowser;
    if (lower.find("shdocvw") != std::string::npos) return FrmControlType::WebBrowser;

    // 常见第三方控件
    if (lower.find("toolbar") != std::string::npos) return FrmControlType::Toolbar;
    if (lower.find("statusbar") != std::string::npos) return FrmControlType::StatusBar;
    if (lower.find("commondialog") != std::string::npos) return FrmControlType::CommonDialog;

    return FrmControlType::Unknown;
}

const char* FrmParser::controlTypeToWin32Class(FrmControlType type) {
    switch (type) {
        case FrmControlType::MDIForm:       return "#32770";     // MDI父窗体 (用RegisterClass+MDICLIENT)
        case FrmControlType::Form:         return "#32770";     // 对话框类 (实际用RegisterClass)
        case FrmControlType::CommandButton: return "BUTTON";
        case FrmControlType::TextBox:      return "EDIT";
        case FrmControlType::Label:        return "STATIC";
        case FrmControlType::CheckBox:     return "BUTTON";     // BS_AUTOCHECKBOX样式
        case FrmControlType::OptionButton: return "BUTTON";     // BS_AUTORADIOBUTTON样式
        case FrmControlType::ListBox:      return "LISTBOX";
        case FrmControlType::ComboBox:     return "COMBOBOX";
        case FrmControlType::Frame:        return "BUTTON";     // BS_GROUPBOX样式
        case FrmControlType::PictureBox:   return "STATIC";     // SS_BITMAP样式
        case FrmControlType::HScrollBar:   return "SCROLLBAR";  // SBS_HORZ样式
        case FrmControlType::VScrollBar:   return "SCROLLBAR";  // SBS_VERT样式
        case FrmControlType::Timer:        return nullptr;       // 不可见控件, 无窗口
        case FrmControlType::Image:        return "STATIC";     // SS_BITMAP
        case FrmControlType::Menu:         return nullptr;       // 菜单, 非窗口
        case FrmControlType::WebBrowser:  return nullptr;       // WebView2, 运行时动态创建
        default:                           return nullptr;
    }
}

const char* FrmParser::controlTypeToVb6Name(FrmControlType type) {
    switch (type) {
        case FrmControlType::MDIForm:       return "MDIForm";
        case FrmControlType::Form:         return "Form";
        case FrmControlType::CommandButton: return "CommandButton";
        case FrmControlType::TextBox:      return "TextBox";
        case FrmControlType::Label:        return "Label";
        case FrmControlType::CheckBox:     return "CheckBox";
        case FrmControlType::OptionButton: return "OptionButton";
        case FrmControlType::ListBox:      return "ListBox";
        case FrmControlType::ComboBox:     return "ComboBox";
        case FrmControlType::Frame:        return "Frame";
        case FrmControlType::PictureBox:   return "PictureBox";
        case FrmControlType::Timer:        return "Timer";
        case FrmControlType::HScrollBar:   return "HScrollBar";
        case FrmControlType::VScrollBar:   return "VScrollBar";
        case FrmControlType::Image:        return "Image";
        case FrmControlType::Shape:        return "Shape";
        case FrmControlType::Line:         return "Line";
        case FrmControlType::Menu:         return "Menu";
        case FrmControlType::WebBrowser:  return "WebBrowser";
        default:                           return "Control";
    }
}

// ============================================================
// 复合属性块解析 (BeginProperty...EndProperty)
// ============================================================

FrmPropertyBlock FrmParser::parsePropertyBlock(
    const std::vector<std::string>& lines,
    size_t& lineIdx) {
    // 当前行: BeginProperty Font
    //   Name = "MS Sans Serif"
    //   Size = 8.25
    // EndProperty

    FrmPropertyBlock block;

    // 解析 BeginProperty 行
    std::string line = trim(lines[lineIdx]);
    // "BeginProperty Font" → blockName = "Font"
    auto spacePos = line.find(' ');
    if (spacePos != std::string::npos) {
        block.blockName = trim(line.substr(spacePos));
    }

    lineIdx++;  // 进入块体

    while (lineIdx < lines.size()) {
        std::string curLine = trim(lines[lineIdx]);

        if (curLine.empty()) {
            lineIdx++;
            continue;
        }

        // EndProperty — 结束
        if (curLine.find("EndProperty") == 0) {
            lineIdx++;
            break;
        }

        // 属性行
        std::string key;
        FrmValue value;
        if (parsePropertyLine(curLine, key, value)) {
            block.properties[key] = value;
        }

        lineIdx++;
    }

    return block;
}

// ============================================================
// 控件块解析 (Begin...End)
// ============================================================

FrmControl FrmParser::parseControlBlock(
    const std::vector<std::string>& lines,
    size_t& lineIdx,
    const std::string& parentIndent) {
    // 当前行: Begin VB.CommandButton Command1
    //   Caption = "OK"
    //   ...
    //   Begin VB.Label Label1
    //      ...
    //   End
    // End

    FrmControl ctrl;

    // 解析 Begin 行: "Begin TypeName InstanceName" 或 "Begin TypeName InstanceName(Index)"
    std::string line = trim(lines[lineIdx]);

    // 去掉 "Begin " 前缀
    auto spacePos = line.find(' ');
    if (spacePos != std::string::npos) {
        std::string rest = trim(line.substr(spacePos));

        // TypeName 和 InstanceName 分离
        // 例如: "VB.CommandButton Command1" 或 "VB.CommandButton Command1(0)"
        auto namePos = rest.find(' ');
        if (namePos != std::string::npos) {
            ctrl.controlTypeName = rest.substr(0, namePos);
            std::string instancePart = trim(rest.substr(namePos));

            // 检查控件数组: Command1(0)
            auto parenPos = instancePart.find('(');
            if (parenPos != std::string::npos) {
                ctrl.controlName = instancePart.substr(0, parenPos);
                auto closeParen = instancePart.find(')', parenPos);
                if (closeParen != std::string::npos) {
                    std::string idxStr = instancePart.substr(parenPos + 1, closeParen - parenPos - 1);
                    try { ctrl.index = std::stoi(idxStr); } catch (...) { ctrl.index = -1; }
                }
            } else {
                ctrl.controlName = instancePart;
            }
        } else {
            ctrl.controlTypeName = rest;
        }
    }

    ctrl.controlType = parseControlType(ctrl.controlTypeName);

    lineIdx++;  // 进入块体

    while (lineIdx < lines.size()) {
        std::string curLine = trim(lines[lineIdx]);

        if (curLine.empty()) {
            lineIdx++;
            continue;
        }

        // End — 结束当前控件块
        if (curLine == "End") {
            lineIdx++;
            break;
        }

        // Begin — 嵌套子控件
        if (curLine.find("Begin ") == 0 || curLine == "Begin") {
            auto child = parseControlBlock(lines, lineIdx, parentIndent + "   ");
            ctrl.children.push_back(std::move(child));
            continue;
        }

        // BeginProperty — 复合属性块
        if (curLine.find("BeginProperty") == 0) {
            auto propBlock = parsePropertyBlock(lines, lineIdx);
            ctrl.propertyBlocks.push_back(std::move(propBlock));
            continue;
        }

        // 普通属性行
        std::string key;
        FrmValue value;
        if (parsePropertyLine(curLine, key, value)) {
            ctrl.properties[key] = value;
            // P7.6: Index属性设置控件数组索引
            if (key == "Index" && value.type == FrmValueType::Integer) {
                ctrl.index = (int)value.intValue;
            }
        }

        lineIdx++;
    }

    return ctrl;
}

// ============================================================
// 代码段提取
// ============================================================

std::string FrmParser::extractCodeSection(
    const std::vector<std::string>& lines,
    size_t startIdx) {
    // 从 startIdx 开始收集所有后续行作为VB代码段
    // 这些行包含: Attribute语句, Option语句, Sub/Function定义等
    std::ostringstream oss;
    for (size_t i = startIdx; i < lines.size(); i++) {
        oss << lines[i] << "\n";
    }
    return oss.str();
}

// ============================================================
// 主解析入口
// ============================================================

FrmFile FrmParser::parse(const std::string& frmFilePath) {
    FrmFile frmFile;
    frmFile.frmFilePath = std::filesystem::absolute(frmFilePath);

    // M22: 使用编码检测+转换读取, 确保GBK等非UTF-8文件正确解码
    auto readResult = SourceBuffer::readAndConvertToUtf8(frmFilePath);
    if (readResult.content.empty()) {
        return frmFile;
    }

    return parseString(readResult.content, frmFilePath);
}

FrmFile FrmParser::parseString(const std::string& content, const std::string& frmFilePath) {
    FrmFile frmFile;
    if (!frmFilePath.empty()) {
        frmFile.frmFilePath = std::filesystem::absolute(frmFilePath);
    }

    // 按行分割
    std::vector<std::string> lines;
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        // 统一换行: 去掉行尾 \r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }

    size_t lineIdx = 0;

    // 1. VERSION 行
    if (lineIdx < lines.size()) {
        std::string verLine = trim(lines[lineIdx]);
        if (verLine.find("VERSION") == 0) {
            auto spacePos = verLine.find(' ');
            if (spacePos != std::string::npos) {
                frmFile.version = trim(verLine.substr(spacePos));
            }
            lineIdx++;
        }
    }

    // 2. Begin VB.Form ... End 窗体描述块
    if (lineIdx < lines.size()) {
        std::string beginLine = trim(lines[lineIdx]);
        if (beginLine.find("Begin ") == 0) {
            // 解析窗体控件块 (递归解析包含嵌套控件)
            auto formCtrl = parseControlBlock(lines, lineIdx);

            // 提取窗体名
            if (formCtrl.controlType == FrmControlType::Form) {
                frmFile.form.formName = formCtrl.controlName;
            } else {
                // 可能不是VB.Form (如VB.MDIForm), 仍保留
                frmFile.form.formName = formCtrl.controlName;
            }

            frmFile.form.formControl = std::move(formCtrl);

            // P7.7: 检测MDIChild属性
            auto mdiChildIt = frmFile.form.formControl.properties.find("MDIChild");
            if (mdiChildIt != frmFile.form.formControl.properties.end() &&
                mdiChildIt->second.type == FrmValueType::Integer && mdiChildIt->second.intValue != 0) {
                frmFile.form.isMDIChild = true;
            }
        }
    }

    // 3. 代码段 (Attribute + VB代码)
    frmFile.codeSection = extractCodeSection(lines, lineIdx);

    // 从代码段的 Attribute VB_Name 中提取窗体名 (优先)
    // Attribute VB_Name = "Form1"
    std::istringstream codeStream(frmFile.codeSection);
    while (std::getline(codeStream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::string t = trim(line);
        if (t.find("Attribute") == 0 && t.find("VB_Name") != std::string::npos) {
            auto eqPos = t.find('=');
            if (eqPos != std::string::npos) {
                std::string nameVal = unquote(trim(t.substr(eqPos + 1)));
                if (!nameVal.empty()) {
                    frmFile.form.formName = nameVal;
                }
            }
        }
    }

    return frmFile;
}

} // namespace vb6c3
