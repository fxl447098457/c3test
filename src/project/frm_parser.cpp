// vb6c3 - VB6窗体文件(.frm)解析器
// .frm 文件结构: VERSION行 + Begin...End窗体描述块 + Attribute行 + VB代码段
// 窗体描述块包含: 窗体属性 + 嵌套控件(递归Begin...End) + 复合属性(BeginProperty...EndProperty)

#include "project/frm_parser.hpp"
#include "common/encoding.hpp"
#include "common/source_manager.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace vb6c3 {

// ============================================================
// 工具函数
// ============================================================


// ============================================================
// 复合属性块解析 (BeginProperty...EndProperty)
// ============================================================

FrmPropertyBlock FrmParser::parsePropertyBlock(
    const std::vector<std::string>& lines,
    size_t& lineIdx) {
    // 当前行格式:
    //   BeginProperty Font
    //   BeginProperty Images {2C247F25-8591-11D1-B16A-00C0F0283628}
    //     NumListImages = 5
    //     BeginProperty ListImage1 {2C247F27-8591-11D1-B16A-00C0F0283628}
    //       Picture = "Form1.frx":1A89
    //       Key = ""
    //     EndProperty
    //   EndProperty

    FrmPropertyBlock block;

    // 解析 BeginProperty 行: "BeginProperty Name" 或 "BeginProperty Name {GUID}"
    std::string line = trim(lines[lineIdx]);
    // "BeginProperty Font" → rest = "Font"
    // "BeginProperty Images {GUID}" → rest = "Images {GUID}"
    auto spacePos = line.find(' ');
    if (spacePos != std::string::npos) {
        std::string rest = trim(line.substr(spacePos));
        // 分离 blockName 和 blockGuid
        auto bracePos = rest.find('{');
        if (bracePos != std::string::npos) {
            block.blockName = trim(rest.substr(0, bracePos));
            auto endBrace = rest.find('}', bracePos);
            if (endBrace != std::string::npos) {
                block.blockGuid = rest.substr(bracePos, endBrace - bracePos + 1);
            }
        } else {
            block.blockName = rest;
        }
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

        // 嵌套 BeginProperty — 递归解析
        if (curLine.find("BeginProperty") == 0) {
            auto nested = parsePropertyBlock(lines, lineIdx);
            block.nestedBlocks.push_back(std::move(nested));
            continue;
        }

        // 属性行: Key = Value (包括 "Object.Tag" 等带点号的键)
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
    frmFile.frmFilePath = std::filesystem::absolute(utf8ToPath(frmFilePath));

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
        frmFile.frmFilePath = std::filesystem::absolute(utf8ToPath(frmFilePath));
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

    // 1.5 Skip Object= lines (ActiveX control references in .frm header)
    // e.g. Object = "{831FDD16-0C5C-11D2-A9FC-0000F8754DA1}#2.0#0"; "MSCOMCTL.OCX"
    while (lineIdx < lines.size()) {
        std::string objLine = trim(lines[lineIdx]);
        if (objLine.empty() ||
            objLine.find("Object ") == 0 ||
            objLine.find("Object=") == 0) {
            lineIdx++;
            continue;
        }
        break;
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

// ============================================================
// Fix 195: 外部二进制资源 (.frx/.ctx/.pgx) 的收集与定位
// ============================================================

namespace {

void collectRefsInBlocks(const std::vector<FrmPropertyBlock>& blocks,
                         const std::string& prefix,
                         std::vector<FrmResourceRef>& out) {
    for (const auto& blk : blocks) {
        std::string blkPrefix = prefix + "." + blk.blockName;
        for (const auto& kv : blk.properties) {
            if (kv.second.type != FrmValueType::FrxReference) continue;
            out.push_back({blkPrefix + "." + kv.first, kv.second.frxFile, kv.second.frxOffset,
                           kv.second.rawText});
        }
        collectRefsInBlocks(blk.nestedBlocks, blkPrefix, out);
    }
}

} // namespace

void collectFrmResourceRefs(const FrmControl& ctrl, std::vector<FrmResourceRef>& out) {
    // 控件数组元素在 VB 里写作 `Label1(0)`, 路径里带上索引, 这样 where 本身
    // 就是一句合法的 VB 左值 (导出器直接照抄)。
    const std::string self = ctrl.controlName
        + (ctrl.index >= 0 ? "(" + std::to_string(ctrl.index) + ")" : std::string());
    for (const auto& kv : ctrl.properties) {
        if (kv.second.type != FrmValueType::FrxReference) continue;
        out.push_back({self + "." + kv.first, kv.second.frxFile, kv.second.frxOffset,
                       kv.second.rawText});
    }
    collectRefsInBlocks(ctrl.propertyBlocks, self, out);
    // 子控件 (含 Frame 内的) 一律按自己的名字拼路径 —— VB6 就是这么寻址的。
    for (const auto& child : ctrl.children) collectFrmResourceRefs(child, out);
}

std::filesystem::path resolveFrmResourceFile(const FrmFile& frm,
                                             const std::string& fallbackExt) {
    auto candidate = frm.frmFilePath;
    candidate.replace_extension(fallbackExt);

    std::vector<FrmResourceRef> refs;
    collectFrmResourceRefs(frm.form.formControl, refs);

    // 1) 属性行里的引用名是权威 —— 且能容忍 .frm 被改名的场景
    //    (Form1.frm 改名成 Form1_copy.frm 后, 属性里写的仍是 "Form1.frx")
    if (!refs.empty() && !refs.front().fileName.empty()) {
        auto named = candidate.parent_path() / refs.front().fileName;
        std::error_code ec;
        if (std::filesystem::exists(named, ec)) return named;
    }
    // 2) 退回"同基名"
    std::error_code ec2;
    if (std::filesystem::exists(candidate, ec2)) return candidate;
    return {};
}

} // namespace vb6c3
