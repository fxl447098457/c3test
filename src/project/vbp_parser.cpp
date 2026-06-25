// vb6c3 - VB6工程文件(.vbp)解析器
// .vbp 是纯文本行导向的INI风格文件, 每行 Key=Value

#include "project/vbp_parser.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace vb6c3 {

VbpProject VbpParser::parse(const std::string& vbpFilePath) {
    VbpProject project;
    project.vbpFilePath = std::filesystem::absolute(vbpFilePath);

    std::ifstream ifs(vbpFilePath);
    if (!ifs.is_open()) {
        return project;
    }

    std::stringstream ss;
    ss << ifs.rdbuf();
    std::string content = ss.str();

    return parseString(content, vbpFilePath);
}

VbpProject VbpParser::parseString(const std::string& content, const std::string& vbpFilePath) {
    VbpProject project;
    if (!vbpFilePath.empty()) {
        project.vbpFilePath = std::filesystem::absolute(vbpFilePath);
    }

    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        // 去除行尾 \r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // 跳过空行和注释
        if (line.empty() || line[0] == '\'') continue;

        // 跳过扩展段 [Section]
        if (line[0] == '[') continue;

        // 解析 Key=Value
        auto eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = line.substr(0, eqPos);
        std::string value = line.substr(eqPos + 1);

        // 根据 key 分类处理
        VbpSourceType srcType = parseSourceType(key);
        if (srcType != VbpSourceType::Unknown ||
            key == "Form" || key == "Module" || key == "Class" ||
            key == "UserControl" || key == "PropertyPage" || key == "Designer") {

            // 确定源文件类型
            if (key == "Form") srcType = VbpSourceType::Form;
            else if (key == "Module") srcType = VbpSourceType::Module;
            else if (key == "Class") srcType = VbpSourceType::Class;
            else if (key == "UserControl") srcType = VbpSourceType::UserControl;
            else if (key == "PropertyPage") srcType = VbpSourceType::PropertyPage;
            else if (key == "Designer") srcType = VbpSourceType::Designer;

            VbpSourceEntry entry;
            entry.type = srcType;

            if (srcType == VbpSourceType::Form) {
                // Form=xxx.frm (无模块名)
                entry.filePath = value;
            } else {
                // Module=Name; xxx.bas 或 Class=Name; xxx.cls
                // 分号分隔: 可能是 "Name; File" 或 "Name;File"
                auto semiPos = value.find(';');
                if (semiPos != std::string::npos) {
                    entry.moduleName = value.substr(0, semiPos);
                    // 去除模块名和路径两端的空白
                    auto filePart = value.substr(semiPos + 1);
                    // ltrim
                    size_t start = filePart.find_first_not_of(" \t");
                    if (start != std::string::npos) {
                        entry.filePath = filePart.substr(start);
                    } else {
                        entry.filePath = filePart;
                    }
                    // rtrim
                    while (!entry.filePath.empty() &&
                           (entry.filePath.back() == ' ' || entry.filePath.back() == '\t')) {
                        entry.filePath.pop_back();
                    }
                } else {
                    entry.filePath = value;
                }
            }

            if (!entry.filePath.empty()) {
                project.sources.push_back(std::move(entry));
            }
            continue;
        }

        // Reference=*\G{GUID}#major#minor#lcid#path#desc
        if (key == "Reference") {
            VbpProject::Reference ref;
            // 格式: *\G{GUID}#major#minor#lcid#path#desc
            // 简化解析: 按 # 分割
            std::vector<std::string> parts;
            std::istringstream refStream(value);
            std::string part;
            while (std::getline(refStream, part, '#')) {
                parts.push_back(part);
            }
            if (parts.size() >= 2) {
                // parts[0] = *\G{GUID} 或 {GUID}
                std::string guidPart = parts[0];
                auto gPos = guidPart.find("{");
                if (gPos != std::string::npos) {
                    auto ePos = guidPart.find("}", gPos);
                    if (ePos != std::string::npos) {
                        ref.guid = guidPart.substr(gPos, ePos - gPos + 1);
                    }
                }
            }
            if (parts.size() >= 3) {
                try { ref.major = std::stoi(parts[1]); } catch (...) {}
                try { ref.minor = std::stoi(parts[2]); } catch (...) {}
            }
            if (parts.size() >= 6) {
                ref.path = parts[4];
                ref.description = parts[5];
            }
            project.references.push_back(std::move(ref));
            continue;
        }

        // Object={GUID}#major#minor; filename.ocx
        if (key == "Object") {
            VbpProject::ObjectRef obj;
            // 格式: {GUID}#major#minor; filename.ocx
            // 用 ; 分割最后部分
            auto semiPos = value.find(';');
            std::string guidPart = (semiPos != std::string::npos)
                ? value.substr(0, semiPos) : value;
            std::string filePart = (semiPos != std::string::npos)
                ? value.substr(semiPos + 1) : "";

            // ltrim filePart
            size_t start = filePart.find_first_not_of(" \t");
            if (start != std::string::npos) {
                obj.fileName = filePart.substr(start);
            }

            // 从 guidPart 中提取 GUID 和版本
            auto gPos = guidPart.find("{");
            if (gPos != std::string::npos) {
                auto ePos = guidPart.find("}", gPos);
                if (ePos != std::string::npos) {
                    obj.guid = guidPart.substr(gPos, ePos - gPos + 1);
                }
            }
            // 版本: #major#minor
            auto hashPos = guidPart.find('#');
            if (hashPos != std::string::npos) {
                auto rest = guidPart.substr(hashPos + 1);
                auto hashPos2 = rest.find('#');
                if (hashPos2 != std::string::npos) {
                    try { obj.major = std::stoi(rest.substr(0, hashPos2)); } catch (...) {}
                    try { obj.minor = std::stoi(rest.substr(hashPos2 + 1)); } catch (...) {}
                }
            }

            project.objects.push_back(std::move(obj));
            continue;
        }

        // 项目属性
        if (key == "Type") {
            project.projectType = parseProjectType(value);
        } else if (key == "Name") {
            project.projectName = unquote(value);
        } else if (key == "Title") {
            project.title = unquote(value);
        } else if (key == "ExeName32") {
            project.exeName = unquote(value);
        } else if (key == "Path32") {
            project.outputPath = unquote(value);
        } else if (key == "Startup") {
            project.startupObject = unquote(value);
        } else if (key == "IconForm") {
            project.iconForm = unquote(value);
        } else if (key == "HelpFile") {
            project.helpFile = unquote(value);
        } else if (key == "Command32") {
            project.commandLine = unquote(value);
        } else if (key == "ResFile32") {
            project.resFile = unquote(value);
        } else if (key == "CompilationType") {
            try { project.compilationType = std::stoi(value); } catch (...) {}
        } else if (key == "OptimizationType") {
            try { project.optimizationType = std::stoi(value); } catch (...) {}
        }
        // 其他项目属性暂不处理 (版本信息、编译选项、线程选项等)
    }

    return project;
}

VbpProjectType VbpParser::parseProjectType(const std::string& value) {
    if (value == "Exe") return VbpProjectType::StandardExe;
    if (value == "OLE Exe") return VbpProjectType::ActiveXExe;
    if (value == "DLL") return VbpProjectType::ActiveXDLL;
    if (value == "Control") return VbpProjectType::ActiveXControl;
    return VbpProjectType::Unknown;
}

VbpSourceType VbpParser::parseSourceType(const std::string& key) {
    // 已在内联处理
    return VbpSourceType::Unknown;
}

std::string VbpParser::unquote(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

} // namespace vb6c3
