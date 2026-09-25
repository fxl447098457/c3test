// ============================================================
// Fix 195: .frx 设计期取值 → VB 代码 导出器实现
// ============================================================
#include "project/frx_extract.hpp"
#include "project/frx_reader.hpp"
#include "common/encoding.hpp"
#include <algorithm>
#include <cctype>

namespace vb6c3 {

namespace {

// UTF-8 字节里 0x0D/0x0A 不会出现在多字节序列内部, 所以可以按字节安全切行。
std::string escapeVbString(const std::string& s) {
    std::string r;
    for (char ch : s) {
        if (ch == '"') r += "\"\"";
        else r += ch;
    }
    return r;
}

// 生成 VB6 字符串字面量: 单行 -> "x"; 多行 -> "a" & vbCrLf & "b"
std::string vbStringLiteral(const std::string& text) {
    std::vector<std::string> lines;
    std::string cur;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (c == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') ++i;
            lines.push_back(cur);
            cur.clear();
        } else if (c == '\n') {
            lines.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    lines.push_back(cur);

    if (lines.size() == 1) return "\"" + escapeVbString(lines[0]) + "\"";
    std::string r;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i) r += " & vbCrLf & ";
        r += "\"" + escapeVbString(lines[i]) + "\"";
    }
    return r;
}

// 二进制资源属性: .frx 里存的是图片字节, 无法用 VB 赋值语句表达
bool isBinaryProp(const std::string& prop) {
    static const char* kNames[] = {
        "Picture", "Icon", "MouseIcon", "DragIcon", "Image",
        "DisabledPicture", "DownPicture", "MaskColor",
    };
    for (const char* n : kNames) {
        if (_stricmp(prop.c_str(), n) == 0) return true;
    }
    return false;
}

// 字符串取值: 先按"普通字符串属性"(长度前缀 + ANSI) 读, 失败再按
// "属性包 DocProperty"(dword 长度 + UTF-16) 读。两条都试过即可覆盖实测到的两种布局。
std::string readStringValue(size_t offset) {
    auto t = FrxReader::readText(offset);
    if (!t.text.empty()) return t.text;
    auto d = FrxReader::readDocString(offset);
    if (!d.text.empty()) return d.text;
    return {};
}

// 导出的内容看起来像不像真的文本 (防把二进制块当字符串导出去)
bool looksLikeText(const std::string& s) {
    if (s.empty()) return false;
    size_t ctrl = 0;
    for (unsigned char c : s) {
        if (c < 0x09) return false;
        if (c < 0x20 && c != 0x09 && c != 0x0A && c != 0x0D) ctrl++;
    }
    return ctrl == 0;
}

} // namespace

FrxExportResult frxExtractToVb(const FrmFile& frm, const std::filesystem::path& frxPath) {
    FrxExportResult result;
    if (frxPath.empty()) {
        result.error = "找不到二进制资源文件 (请确认 .frx 与 .frm 在同一目录)";
        return result;
    }
    if (!FrxReader::load(frxPath)) {
        result.error = FrxReader::lastError();
        return result;
    }

    std::vector<FrmResourceRef> refs;
    collectFrmResourceRefs(frm.form.formControl, refs);

    std::vector<std::string> body;   // 已生成的 VB 语句 (不含缩进)
    std::vector<std::string> itemDataLines;

    // 第一遍: ListBox/ComboBox 的 List —— 必须早于 ItemData 出现 (ItemData 依附于项)
    for (const auto& ref : refs) {
        size_t dot = ref.where.rfind('.');
        if (dot == std::string::npos) continue;
        std::string owner = ref.where.substr(0, dot);
        std::string prop = ref.where.substr(dot + 1);
        if (_stricmp(prop.c_str(), "List") != 0) continue;

        auto list = FrxReader::readStringList(ref.offset);
        for (const auto& item : list.items) {
            if (!looksLikeText(item)) continue;
            body.push_back(owner + ".AddItem " + vbStringLiteral(item));
        }
    }

    // 第二遍: 其余属性
    for (const auto& ref : refs) {
        size_t dot = ref.where.rfind('.');
        if (dot == std::string::npos) continue;
        std::string owner = ref.where.substr(0, dot);
        std::string prop = ref.where.substr(dot + 1);

        if (_stricmp(prop.c_str(), "List") == 0) continue;  // 第一遍已处理

        if (_stricmp(prop.c_str(), "ItemData") == 0) {
            auto ints = FrxReader::readIntList(ref.offset);
            for (size_t i = 0; i < ints.items.size(); ++i) {
                if (ints.items[i] == 0) continue;   // 0 是 VB6 默认值, 不必显式写出
                itemDataLines.push_back(owner + ".ItemData(" + std::to_string(i) + ") = "
                                        + std::to_string(ints.items[i]));
            }
            continue;
        }

        if (isBinaryProp(prop)) {
            result.skipped.push_back(ref.where + " (二进制图片, 需外部图片文件 + LoadPicture)");
            continue;
        }

        std::string text = readStringValue(ref.offset);
        if (!looksLikeText(text)) {
            result.skipped.push_back(ref.where + " (取值无法解析为文本)");
            continue;
        }
        body.push_back(owner + "." + prop + " = " + vbStringLiteral(text));
    }

    for (auto& l : itemDataLines) body.push_back(l);

    std::string frmName = std::filesystem::path(frm.frmFilePath).filename().string();
    std::string resName = frxPath.filename().string();

    std::string out;
    out += "' ============================================================\n";
    out += "' " + resName + " -> VB 代码   (C3 --extract-frx, Fix 195)\n";
    out += "' 源窗体: " + frmName + "\n";
    out += "' ------------------------------------------------------------\n";
    out += "' 一次性迁移步骤:\n";
    out += "'   1. 把下面 Form_Load 的过程体搬进 " + frmName + " 的代码段\n";
    out += "'      (窗体里已有 Form_Load 就合并进去, 不要重复定义过程)。\n";
    out += "'   2. 删掉 " + frmName + " 设计器头里下面这些引用行 —— 它们只指向资源文件:\n";
    size_t binLines = 0;
    for (const auto& ref : refs) {
        std::string prop = ref.where;
        size_t dot = prop.rfind('.');
        if (dot != std::string::npos) prop = prop.substr(dot + 1);
        if (isBinaryProp(prop)) { binLines++; continue; }   // 图片类在文末统一交代
        out += "'        " + prop + " = " + (ref.rawText.empty() ? resName : ref.rawText)
             + "   ' " + ref.where + "\n";
    }
    if (binLines) {
        out += "'      另 " + std::to_string(binLines)
             + " 行 Icon/Picture 类二进制引用, 一并删掉 (见文末\"需手工处理\")。\n";
    }
    out += "'   3. 删除 " + resName + "。之后编译不再告警 VB4004。\n";
    out += "'\n";
    out += "' 为什么可以这样做: C3 编译期本来就已把资源内容内联进生成的 C 代码,\n";
    out += "' 生成的 exe 从不读 " + resName + " —— 这一步只换承载设计期数据的载体,\n";
    out += "' 把它从「谁也看不懂的二进制」变成 .frm 里的普通赋值语句。\n";
    out += "' 本文件是 UTF-8 编码; 若 .frm 是 GBK, 请让编辑器按 GBK 保存后再粘贴。\n";
    if (!result.skipped.empty()) {
        out += "'\n";
        out += "' 以下资源无法用 VB 语句表达, 需手工处理:\n";
        for (const auto& s : result.skipped) out += "'   " + s + "\n";
    }
    out += "' ============================================================\n";
    out += "\n";
    out += "Private Sub Form_Load()\n";
    for (const auto& b : body) out += "    " + b + "\n";
    out += "End Sub\n";

    result.content = out;
    result.assignmentCount = static_cast<int>(body.size());
    return result;
}

} // namespace vb6c3
