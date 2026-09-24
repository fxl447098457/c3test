// ai/023 S06: --pack/--unpack 实现 (格式见 driver_pack.hpp 头注)。

#include "driver/driver_pack.hpp"
#include "project/package_manifest.hpp"
#include "common/encoding.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace vb6c3 {

namespace {

namespace fs = std::filesystem;

constexpr char kMagic[6] = {'C', '3', 'P', 'K', 'G', '\x01'};
constexpr char kTrailerMagic[7] = {'C', '3', 'P', 'K', 'E', 'O', 'F'};
constexpr size_t kTrailerSize = 8 + 4 + 7;  // cdOffset + cdCount + magic

void putU16(std::string& s, uint16_t v) {
    s.push_back(char(v & 0xFF));
    s.push_back(char((v >> 8) & 0xFF));
}

void putU32(std::string& s, uint32_t v) {
    for (int i = 0; i < 4; ++i) s.push_back(char((v >> (i * 8)) & 0xFF));
}

void putU64(std::string& s, uint64_t v) {
    for (int i = 0; i < 8; ++i) s.push_back(char((v >> (i * 8)) & 0xFF));
}

uint16_t getU16(const std::string& s, size_t at) {
    return uint16_t(uint8_t(s[at])) | (uint16_t(uint8_t(s[at + 1])) << 8);
}

uint64_t getU64(const std::string& s, size_t at) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= uint64_t(uint8_t(s[at + i])) << (i * 8);
    return v;
}

std::string readAllBytes(const fs::path& p, bool& ok) {
    std::ifstream f(p, std::ios::binary);
    ok = bool(f);
    if (!ok) return {};
    std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    ok = !f.bad();
    return data;
}

bool writeAllBytes(const fs::path& p, const std::string& data) {
    std::error_code ec;
    if (!p.parent_path().empty()) fs::create_directories(p.parent_path(), ec);
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(data.data(), std::streamsize(data.size()));
    return bool(f);
}

} // namespace

int runPackMode(const std::string& dirUtf8) {
    fs::path dir = utf8ToPath(dirUtf8);
    fs::path manifestPath = dir / "package.c3d";
    std::error_code ec;
    if (!fs::exists(manifestPath, ec) || !fs::is_regular_file(manifestPath, ec)) {
        std::cerr << "C3 --pack: package.c3d not found in directory: " << dirUtf8 << std::endl;
        return 1;
    }

    bool ok = false;
    std::string content = readAllBytes(manifestPath, ok);
    if (!ok) {
        std::cerr << "C3 --pack: cannot read package.c3d" << std::endl;
        return 1;
    }
    PackageManifest m = parsePackageManifest(content);
    if (!m.ok()) {
        std::cerr << "C3 --pack: invalid package.c3d:" << std::endl;
        for (const auto& e : m.errors) std::cerr << "  " << e << std::endl;
        return 1;
    }

    // 成员顺序: package.c3d 在前, [Files] 按清单顺序随后 (路径基准 = 清单所在目录)
    std::vector<std::pair<std::string, std::string>> members;  // (name, bytes)
    members.emplace_back("package.c3d", std::move(content));
    for (const auto& fe : m.files) {
        std::string bytes = readAllBytes(dir / utf8ToPath(fe.path), ok);
        if (!ok) {
            std::cerr << "C3 --pack: cannot read packaged file: " << fe.path << std::endl;
            return 1;
        }
        members.emplace_back(fe.path, std::move(bytes));
    }

    // 顺序区
    std::string blob(kMagic, kMagic + sizeof(kMagic));
    struct CdEntry { std::string name; uint64_t offset; uint64_t size; };
    std::vector<CdEntry> cd;
    cd.reserve(members.size());
    for (const auto& [name, bytes] : members) {
        cd.push_back({name, uint64_t(blob.size()), uint64_t(bytes.size())});
        putU16(blob, uint16_t(name.size()));
        blob += name;
        putU64(blob, uint64_t(bytes.size()));
        blob += bytes;
    }

    // 尾部中央目录
    uint64_t cdOffset = uint64_t(blob.size());
    for (const auto& e : cd) {
        putU16(blob, uint16_t(e.name.size()));
        blob += e.name;
        putU64(blob, e.offset);
        putU64(blob, e.size);
    }
    putU64(blob, cdOffset);
    putU32(blob, uint32_t(cd.size()));
    blob.append(kTrailerMagic, kTrailerMagic + sizeof(kTrailerMagic));

    fs::path outPath = dir / utf8ToPath(m.name + "-" + m.version + ".c3pkg");
    if (!writeAllBytes(outPath, blob)) {
        std::cerr << "C3 --pack: cannot write " << pathToUtf8(outPath) << std::endl;
        return 1;
    }
    std::cout << "C3 --pack: " << pathToUtf8(outPath) << " (" << members.size()
              << " files, " << blob.size() << " bytes)" << std::endl;
    return 0;
}

int runUnpackMode(const std::string& pkgPathUtf8, const std::string& outDirUtf8) {
    fs::path pkgPath = utf8ToPath(pkgPathUtf8);
    std::error_code ec;
    if (!fs::exists(pkgPath, ec)) {
        std::cerr << "C3 --unpack: file not found: " << pkgPathUtf8 << std::endl;
        return 1;
    }

    bool ok = false;
    std::string blob = readAllBytes(pkgPath, ok);
    if (!ok || blob.size() < sizeof(kMagic) + kTrailerSize) {
        std::cerr << "C3 --unpack: not a C3 package (too small)" << std::endl;
        return 1;
    }
    if (std::memcmp(blob.data(), kMagic, sizeof(kMagic)) != 0) {
        std::cerr << "C3 --unpack: bad magic (not a C3 package)" << std::endl;
        return 1;
    }
    if (std::memcmp(blob.data() + blob.size() - sizeof(kTrailerMagic), kTrailerMagic,
                    sizeof(kTrailerMagic)) != 0) {
        std::cerr << "C3 --unpack: missing trailer (truncated package?)" << std::endl;
        return 1;
    }

    // 尾部 → 中央目录
    size_t base = blob.size() - kTrailerSize;
    uint64_t cdOffset = getU64(blob, base);
    // cdCount 是 u32 (LE); getU64 读 8 字节后取低 32 位等价
    uint32_t cdCount = uint32_t(getU64(blob, base + 8) & 0xFFFFFFFFull);
    if (cdOffset < sizeof(kMagic) ||
        cdOffset + uint64_t(kTrailerSize) > blob.size()) {
        std::cerr << "C3 --unpack: corrupt central directory offset" << std::endl;
        return 1;
    }

    struct Member { std::string name; uint64_t offset; uint64_t size; };
    std::vector<Member> members;
    {
        size_t pos = size_t(cdOffset);
        for (uint32_t i = 0; i < cdCount; ++i) {
            if (pos + 2 > blob.size()) {
                std::cerr << "C3 --unpack: corrupt central directory" << std::endl;
                return 1;
            }
            uint16_t nameLen = getU16(blob, pos);
            pos += 2;
            if (pos + nameLen + 16 > blob.size()) {
                std::cerr << "C3 --unpack: corrupt central directory" << std::endl;
                return 1;
            }
            Member m;
            m.name = blob.substr(pos, nameLen);
            pos += nameLen;
            m.offset = getU64(blob, pos);
            m.size = getU64(blob, pos + 8);
            pos += 16;
            members.push_back(std::move(m));
        }
    }

    // 先取 package.c3d (决定缺省释放目录), 再统一校验 + 落盘
    std::string manifestBytes;
    for (const auto& mem : members) {
        if (mem.name == "package.c3d") {
            if (mem.offset + mem.size > blob.size()) {
                std::cerr << "C3 --unpack: package.c3d out of range" << std::endl;
                return 1;
            }
            size_t hdr = size_t(mem.offset);
            if (hdr + 2 + mem.name.size() + 8 > blob.size() ||
                getU16(blob, hdr) != mem.name.size() ||
                blob.compare(hdr + 2, mem.name.size(), mem.name) != 0 ||
                getU64(blob, hdr + 2 + mem.name.size()) != mem.size) {
                std::cerr << "C3 --unpack: member header mismatch: package.c3d" << std::endl;
                return 1;
            }
            manifestBytes = blob.substr(hdr + 2 + mem.name.size() + 8, size_t(mem.size));
        }
    }

    fs::path outRoot;
    if (!outDirUtf8.empty()) {
        outRoot = utf8ToPath(outDirUtf8);
    } else if (!manifestBytes.empty()) {
        PackageManifest m = parsePackageManifest(manifestBytes);
        if (m.ok()) {
            outRoot = pkgPath.parent_path() / utf8ToPath(m.name + "-" + m.version);
        } else {
            outRoot = pkgPath.parent_path();
        }
    } else {
        outRoot = pkgPath.parent_path();
    }

    int extracted = 0;
    for (const auto& mem : members) {
        // 防路径逃逸: 成员名不许绝对路径/'..'/反斜杠
        if (mem.name.empty() || mem.name.find('\\') != std::string::npos ||
            mem.name.find("..") != std::string::npos ||
            (mem.name.size() >= 2 && mem.name[1] == ':')) {
            std::cerr << "C3 --unpack: unsafe member name: " << mem.name << std::endl;
            return 1;
        }
        // 顺序区同名成员必须落在中央目录声明的位置 (防错位/防越界)
        size_t hdr = size_t(mem.offset);
        if (hdr + 2 + mem.name.size() + 8 > blob.size() ||
            hdr + 2 + mem.name.size() + 8 + mem.size > blob.size()) {
            std::cerr << "C3 --unpack: member out of range: " << mem.name << std::endl;
            return 1;
        }
        if (getU16(blob, hdr) != mem.name.size() ||
            blob.compare(hdr + 2, mem.name.size(), mem.name) != 0 ||
            getU64(blob, hdr + 2 + mem.name.size()) != mem.size) {
            std::cerr << "C3 --unpack: member header mismatch: " << mem.name << std::endl;
            return 1;
        }
        std::string bytes = blob.substr(hdr + 2 + mem.name.size() + 8, size_t(mem.size));
        if (!writeAllBytes(outRoot / utf8ToPath(mem.name), bytes)) {
            std::cerr << "C3 --unpack: cannot write " << pathToUtf8(outRoot / utf8ToPath(mem.name))
                      << std::endl;
            return 1;
        }
        ++extracted;
    }

    std::cout << "C3 --unpack: " << extracted << " files -> " << pathToUtf8(outRoot)
              << std::endl;
    return 0;
}

} // namespace vb6c3
