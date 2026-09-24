// ai/023 S01: package.c3d 清单解析器实现
// 文案纪律: errors 里的字符串一律 ASCII (022 D12 同族), 供 Test-SyntaxFail 类断言。

#include "project/package_manifest.hpp"
#include "project/vbp_parser.hpp"
#include "common/diagnostics.hpp"
#include "common/encoding.hpp"
#include "common/source_manager.hpp"
#include "common/sha1.hpp"
#include <cctype>
#include <filesystem>

namespace vb6c3 {

namespace {

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t");
    return s.substr(a, b - a + 1);
}

bool hasNonAscii(const std::string& s) {
    for (unsigned char c : s) {
        if (c >= 0x80) return true;
    }
    return false;
}

std::string lowerAscii(const std::string& s) {
    std::string r = s;
    for (char& c : r) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    return r;
}

} // namespace

bool isValidPackageName(const std::string& name) {
    if (name.empty()) return false;
    for (unsigned char c : name) {
        if (!(std::isalnum(c) || c == '_')) return false;
    }
    return true;
}

bool isValidPackageVersion(const std::string& version) {
    if (version.empty() || version.front() == '.') return false;
    for (unsigned char c : version) {
        if (!(std::isalnum(c) || c == '.' || c == '_' || c == '-')) return false;
    }
    return true;
}

PackageManifest parsePackageManifest(const std::string& content) {
    PackageManifest m;

    if (hasNonAscii(content)) {
        m.errors.push_back("package manifest must be pure ASCII");
        return m;
    }

    std::string section; // 当前小节; 空串 = 头部小节之前
    size_t pos = 0;
    while (pos <= content.size()) {
        size_t eol = content.find('\n', pos);
        std::string line = (eol == std::string::npos)
            ? content.substr(pos) : content.substr(pos, eol - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        pos = (eol == std::string::npos) ? content.size() + 1 : eol + 1;

        // 整行注释 / 空行
        std::string t = trim(line);
        if (t.empty() || t[0] == '\'') continue;

        // 小节头
        if (t[0] == '[') {
            if (t.back() != ']') {
                m.errors.push_back("malformed section header: " + t);
                continue;
            }
            section = t.substr(1, t.size() - 2);
            if (section != "C3Package" && section != "Files" &&
                section != "Export" && section != "Needs") {
                m.errors.push_back("unknown section: [" + section + "]");
            }
            continue;
        }

        // Key=Value
        auto eq = t.find('=');
        if (eq == std::string::npos) {
            m.errors.push_back("expected Key=Value line: " + t);
            continue;
        }
        std::string key = trim(t.substr(0, eq));
        std::string value = trim(t.substr(eq + 1));
        if (key.empty()) {
            m.errors.push_back("empty key in line: " + t);
            continue;
        }

        if (section == "C3Package" || section.empty()) {
            if (key == "Format") {
                try { m.format = std::stoi(value); }
                catch (...) { m.errors.push_back("Format is not a number: " + value); }
            } else if (key == "Name") {
                m.name = value;
            } else if (key == "Version") {
                m.version = value;
            } else if (key == "Desc") {
                m.desc = value;
            } else {
                m.errors.push_back("unknown key in [C3Package]: " + key);
            }
        } else if (section == "Files") {
            // 路径=sha1,size  (size 允许省略)
            PackageManifest::FileEntry fe;
            fe.path = key;
            auto comma = value.find(',');
            fe.sha1 = trim(comma == std::string::npos ? value : value.substr(0, comma));
            if (comma != std::string::npos) fe.size = trim(value.substr(comma + 1));
            if (fe.sha1.empty()) {
                m.errors.push_back("missing hash for file: " + fe.path);
            }
            m.files.push_back(std::move(fe));
        } else if (section == "Export") {
            if (key == "Module" || key == "Class") {
                // 值形如 "Name; Public" — 名字取 ';' 前段 (可见性标记由
                // getPublicSymbols/access 决定, 清单侧只记名字, S03)
                std::string name = value;
                auto semi = name.find(';');
                if (semi != std::string::npos) name = trim(name.substr(0, semi));
                m.exports.push_back({key, name});
            } else if (key == "Friend") {
                // v1 语义: Friend=True 时包内 Friend 成员对宿主可见 (默认不可见)
                if (value != "True" && value != "False") {
                    m.errors.push_back("Friend must be True or False: " + value);
                } else {
                    m.friendVisible = (value == "True");
                }
            } else {
                m.errors.push_back("unknown key in [Export]: " + key);
            }
        } else if (section == "Needs") {
            if (value != "True" && value != "False") {
                m.errors.push_back("[Needs] values must be True or False: " + key);
            } else {
                m.needs.push_back({key, value});
            }
        } else {
            // 未知小节已报过错; 这里避免重复刷屏, 直接跳过
        }
    }

    // 必填项与格式版本
    if (m.format == 0) {
        m.errors.push_back("missing Format=1");
    } else if (m.format != 1) {
        m.errors.push_back("unsupported Format version: " + std::to_string(m.format));
    }
    if (m.name.empty()) {
        m.errors.push_back("missing Name");
    } else if (!isValidPackageName(m.name)) {
        m.errors.push_back("invalid Name (A-Za-z0-9_ only): " + m.name);
    }
    if (m.version.empty()) {
        m.errors.push_back("missing Version");
    } else if (!isValidPackageVersion(m.version)) {
        m.errors.push_back("invalid Version: " + m.version);
    }

    return m;
}

std::string extractVbModuleName(const std::string& content) {
    // 找 'Attribute VB_Name = "Name"' 行 (VB6 源码头几行; C3 生成的源码也是这个形态)。
    // 引号内可能有转义双引号 "" — v1 只取到下一个引号, 模块名不含 " 是合理约束。
    size_t pos = 0;
    while (pos <= content.size()) {
        size_t eol = content.find('\n', pos);
        std::string line = (eol == std::string::npos)
            ? content.substr(pos) : content.substr(pos, eol - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();

        const std::string marker = "Attribute VB_Name";
        if (line.compare(0, marker.size(), marker) == 0) {
            auto eq = line.find('=');
            if (eq != std::string::npos) {
                std::string v = trim(line.substr(eq + 1));
                if (v.size() >= 2 && v.front() == '"' && v.back() == '"') {
                    return v.substr(1, v.size() - 2);
                }
                return v;
            }
        }
        if (eol == std::string::npos) break;
        pos = eol + 1;
    }
    return "";
}

std::vector<std::pair<std::string, std::string>> extractProcDecls(const std::string& content) {
    std::vector<std::pair<std::string, std::string>> out;
    size_t pos = 0;
    while (pos <= content.size()) {
        size_t eol = content.find('\n', pos);
        std::string line = (eol == std::string::npos)
            ? content.substr(pos) : content.substr(pos, eol - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();

        std::string t = trim(line);
        // 先处理, 再推进 (最后一行可能无换行)
        bool last = (eol == std::string::npos);
        if (!last) pos = eol + 1;
        if (t.empty() || t[0] == '\'') {
            if (last) break;
            continue;
        }
        // 大小写不敏感的前缀消费
        auto consume = [&](const std::string& kw) {
            if (t.size() > kw.size() &&
                std::equal(kw.begin(), kw.end(), t.begin(),
                           [](char a, char b) { return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); }) &&
                (t[kw.size()] == ' ' || t[kw.size()] == '\t')) {
                t = trim(t.substr(kw.size()));
                return true;
            }
            return false;
        };

        std::string access = "public";
        for (;;) {
            if (consume("Public")) access = "public";
            else if (consume("Private")) access = "private";
            else if (consume("Friend")) access = "friend";
            else if (consume("Static")) {}           // Static Sub: 访问级别不变
            else break;
        }

        std::string name;
        if (consume("Sub") || consume("Function")) {
            // Name 之后可有 '(' — 取到 '(' 或空白为止
            size_t end = t.find_first_of(" \t(");
            name = (end == std::string::npos) ? t : t.substr(0, end);
        } else if (consume("Property")) {
            std::string kind;
            if (consume("Get")) kind = "Get";
            else if (consume("Let")) kind = "Let";
            else if (consume("Set")) kind = "Set";
            if (!kind.empty()) {
                size_t end = t.find_first_of(" \t(");
                name = (end == std::string::npos) ? t : t.substr(0, end);
            }
        }
        if (!name.empty() && isValidPackageName(name)) {
            out.push_back({access, name});
        }
    }
    return out;
}

void checkPackages(const VbpProject& project,
                   const std::vector<std::string>& cliRoots,
                   Diagnostics& diag,
                   std::vector<ResolvedPackage>* resolved) {
    if (resolved) resolved->clear();
    if (project.packageRefs.empty()) return;

    // 搜索根: CLI --package-root 各项在前 (显式), 缺省根 <vbp>/packages 在后。
    std::vector<std::filesystem::path> roots;
    for (const auto& r : cliRoots) {
        roots.push_back(std::filesystem::absolute(utf8ToPath(r)));
    }
    roots.push_back(project.vbpFilePath.parent_path() / utf8ToPath("packages"));

    // 硬校验第三条 (v1 口径): 包名与宿主工程名冲突 (大小写不敏感, Windows 同款)。
    auto lower = [](std::string s) {
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };
    const std::string projLower = lower(project.projectName);

    for (const auto& ref : project.packageRefs) {
        // 硬校验第一条: 名字/版本字符集 = 防逃逸闸门。目录名由 root/name-version
        // 拼接而来, 名字里没有 '/' '\\' ':' '..' 就不存在跳出搜索根的路径。
        if (!isValidPackageName(ref.name)) {
            diag.error(DiagnosticID::VbpPackageEscape, SourceLocation{},
                       "package name contains path characters: " + ref.name);
            continue;
        }
        if (!isValidPackageVersion(ref.version)) {
            diag.error(DiagnosticID::VbpPackageEscape, SourceLocation{},
                       "package version contains path characters: " + ref.name + "-" + ref.version);
            continue;
        }
        if (!projLower.empty() && lower(ref.name) == projLower) {
            diag.error(DiagnosticID::VbpPackageConflict, SourceLocation{},
                       "package name conflicts with project name: " + ref.name);
            continue;
        }

        // 寻址 (023 六节): <搜索根>/<Name>-<Version>/package.c3d
        const std::string dirName = ref.name + "-" + ref.version;
        std::filesystem::path pkgDir;
        bool found = false;
        for (const auto& root : roots) {
            auto cand = root / utf8ToPath(dirName);
            std::error_code ec;
            if (std::filesystem::is_directory(cand, ec)) {
                pkgDir = cand;
                found = true;
                break;
            }
        }
        if (!found) {
            diag.error(DiagnosticID::VbpPackageNotFound, SourceLocation{},
                       "package not found in any search root: " + dirName +
                       " (searched " + std::to_string(roots.size()) + " roots)");
            continue;
        }

        auto manifestPath = pkgDir / utf8ToPath("package.c3d");
        std::error_code ec;
        if (!std::filesystem::exists(manifestPath, ec)) {
            diag.error(DiagnosticID::VbpPackageNotFound, SourceLocation{},
                       "package manifest missing: " + pathToUtf8(manifestPath));
            continue;
        }

        auto read = SourceBuffer::readAndConvertToUtf8(pathToUtf8(manifestPath));
        auto m = parsePackageManifest(read.content);
        for (const auto& e : m.errors) {
            diag.error(DiagnosticID::VbpPackageFormat, SourceLocation{},
                       pathToUtf8(manifestPath) + ": " + e);
        }
        if (!m.ok()) continue;

        // vbp 写的包名与清单 Name 不一致 → 视为清单不合法 (引用寻址以 vbp 名为准)
        if (m.name != ref.name) {
            diag.error(DiagnosticID::VbpPackageFormat, SourceLocation{},
                       pathToUtf8(manifestPath) + ": manifest Name mismatch: " +
                       m.name + " != " + ref.name);
            continue;
        }

        // 硬校验第二条: 缺文件 → 警告不拒收 (D7: 就地改包内源码调试是真实工作流)。
        // S05: 在场文件再比对 sha1/size —— 不符也只是警告 (D7), 构建 continues。
        // 这是目录形态下"有人手改过包"的唯一检测手段 (023 五节: 逐文件哈希是强制项)。
        for (const auto& fe : m.files) {
            auto fp = pkgDir / utf8ToPath(fe.path);
            if (!std::filesystem::exists(fp, ec)) {
                diag.warn(DiagnosticID::VbpPackageFileMissing, SourceLocation{},
                          "package file missing: " + ref.name + "/" + fe.path);
                continue;
            }
            std::string actual = sha1HexOfFile(pathToUtf8(fp));
            if (actual.empty()) {
                diag.warn(DiagnosticID::VbpPackageHashMismatch, SourceLocation{},
                          "package file unreadable: " + ref.name + "/" + fe.path);
                continue;
            }
            if (!fe.sha1.empty() && lowerAscii(fe.sha1) != actual) {
                diag.warn(DiagnosticID::VbpPackageHashMismatch, SourceLocation{},
                          "package file hash mismatch: " + ref.name + "/" + fe.path +
                          " (manifest " + fe.sha1 + ", actual " + actual + ")");
            }
            if (!fe.size.empty()) {
                auto sz = std::filesystem::file_size(fp, ec);
                if (!ec && std::to_string(sz) != fe.size) {
                    diag.warn(DiagnosticID::VbpPackageHashMismatch, SourceLocation{},
                              "package file size mismatch: " + ref.name + "/" + fe.path +
                              " (manifest " + fe.size + ", actual " + std::to_string(sz) + ")");
                }
            }
        }

        // S02: 把解析成功的包交给 driver 做源码级加载
        if (resolved) {
            ResolvedPackage rp;
            rp.name = ref.name;
            rp.version = ref.version;
            rp.dir = pkgDir;
            rp.manifest = std::move(m);
            resolved->push_back(std::move(rp));
        }
    }
}

} // namespace vb6c3
