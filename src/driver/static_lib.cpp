// static_lib.cpp — 静态库引用识别与寻址 (ai/024, 批次 T01)
// 纯逻辑, 不依赖 AST / 诊断 / codegen — 便于单测与复用 (T04 的 COFF 解析器会接在
// 同一个归一化口径上, 见 E4/T04 的 "忽略前导 _、忽略 @N" 约定)。

#include "driver/static_lib.hpp"
#include "common/encoding.hpp"

#include <algorithm>
#include <filesystem>
#include <system_error>

namespace vb6c3 {

namespace {

void toLowerAscii(std::string& s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
}

// 存在性判断一律走 error_code 重载: 路径含非法字符/超长时返回 false 而不是抛异常
// (SR4 的口径: 任何寻址失败都必须是"没找到", 不能把编译器自身掀翻)
bool pathExists(const std::filesystem::path& p) {
    std::error_code ec;
    return std::filesystem::exists(p, ec) && !ec;
}

bool hasDirSeparator(const std::string& s) {
    return s.find('/') != std::string::npos || s.find('\\') != std::string::npos;
}

} // namespace

const char* libKindName(LibKind k) {
    switch (k) {
        case LibKind::StaticMsvc:  return "static (MSVC archive)";
        case LibKind::StaticMinGW: return "static (MinGW archive)";
        case LibKind::Dynamic:     break;
    }
    return "dynamic";
}

std::string libStripQuotes(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

std::string libLowerExt(const std::string& libStr) {
    const std::string t = libStripQuotes(libStr);

    const auto dot = t.find_last_of('.');
    if (dot == std::string::npos) return {};

    // 点号必须落在**最后一段**里才算扩展名: "3rd.party\\mylib" 的点在目录段, 不算。
    const auto sep = t.find_last_of("/\\");
    if (sep != std::string::npos && dot < sep) return {};

    // 点号后为空 (如 "foo.") → 不算扩展名
    if (dot + 1 >= t.size()) return {};

    std::string ext = t.substr(dot);
    toLowerAscii(ext);
    return ext;
}

LibKind classifyLib(const std::string& libStr) {
    const std::string ext = libLowerExt(libStr);
    if (ext == ".lib" || ext == ".obj") return LibKind::StaticMsvc;
    if (ext == ".a"   || ext == ".o")   return LibKind::StaticMinGW;
    return LibKind::Dynamic;
}

void LibSearchPaths::addRoot(const std::string& dir) {
    if (dir.empty()) return;

    std::filesystem::path p = utf8ToPath(dir);
    if (p.is_relative()) {
        if (baseDir_.empty()) return;   // 无基准 → 无法解析, 静默丢弃 (调用方保证先 setBaseDir)
        p = utf8ToPath(baseDir_) / p;
    }
    // 归一化 (去掉 ./ 与多余分隔符), 不要求文件存在
    std::error_code ec;
    auto canon = std::filesystem::weakly_canonical(p, ec);
    roots_.push_back(pathToUtf8(ec ? p : canon));
}

void LibSearchPaths::addDefaultRoot() {
    if (defaultRootAdded_ || baseDir_.empty()) return;
    defaultRootAdded_ = true;
    roots_.push_back(pathToUtf8(utf8ToPath(baseDir_) / "Lib"));
}

LibResolution LibSearchPaths::resolve(const std::string& libStr, LibBackend backend) const {
    LibResolution r;

    const std::string name = libStripQuotes(libStr);
    if (name.empty()) return r;

    const LibKind kind = classifyLib(name);

    // === 后端 × 格式匹配 (§六-1): 先诊断拒收, 别丢给链接器 ===
    if (backend == LibBackend::Msvc && kind == LibKind::StaticMinGW) {
        r.rejected = true;
        r.reason = "MinGW archive (" + libLowerExt(name) +
                   ") cannot be linked by the MSVC backend; use .lib/.obj";
        return r;
    }
    if (backend == LibBackend::MinGW && kind == LibKind::StaticMsvc) {
        r.rejected = true;
        r.reason = "MSVC archive (" + libLowerExt(name) +
                   ") cannot be linked by the MinGW backend; use .a/.o";
        return r;
    }

    const std::filesystem::path rel = utf8ToPath(name);

    // === 1) 绝对路径: 直接用, 不搜索 ===
    if (rel.is_absolute()) {
        if (pathExists(rel)) {
            r.ok = true;
            r.absPath = pathToUtf8(rel);
        }
        return r;
    }

    // === 2) 含分隔符的相对路径: 相对工程目录, 单一候选, 不做搜索 ===
    if (hasDirSeparator(name)) {
        const std::filesystem::path full = utf8ToPath(baseDir_) / rel;
        if (pathExists(full)) {
            r.ok = true;
            r.absPath = pathToUtf8(full);
        } else {
            // 报错时把"唯一候选"作为搜索清单给出, 便于用户直接看到 C3 找的是哪
            r.searchedRoots.push_back(pathToUtf8(full));
        }
        return r;
    }

    // === 3) 裸文件名: 按搜索根顺序 ===
    for (const auto& root : roots_) {
        const std::filesystem::path full = utf8ToPath(root) / rel;
        if (pathExists(full)) {
            r.ok = true;
            r.absPath = pathToUtf8(full);
            return r;
        }
    }

    r.searchedRoots = roots_;
    return r;
}

} // namespace vb6c3
