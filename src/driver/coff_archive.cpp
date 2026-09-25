// coff_archive.cpp — 见 coff_archive.hpp (ai/024, 批次 T04, L2)
//
// 支持两种输入:
//   1) MSVC 归档 (.lib): "!<arch>\n" 魔数。符号索引在名为 "/" 的第一个成员里
//      (linker member #1): 4 字节大端符号数 + N×4 字节大端成员偏移 + N 个
//      NUL 结尾的符号名。这是 link.exe 实际消费的索引, 最权威。
//   2) COFF 目标文件 (.obj): 文件头第 2 字节起为机器类型 (x86=0x014C 小端,
//      x64=0x8664)。符号表位置在头里 (pointerToSymbolTable/numberOfSymbols),
//      紧随其后是字符串表。只收"已定义的外部符号" (storageClass=2 且
//      sectionNumber!=0), 未定义引用与节内静态符号都不要。

#include "driver/coff_archive.hpp"
#include "common/encoding.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace vb6c3 {

namespace {

bool readFile(const std::string& path, std::string* out) {
    // Fix 196: 走 _wfopen —— 用户 .lib 可能躺在中文目录下, fopen 会把 UTF-8 当 ACP
    std::FILE* f = fopenUtf8(path, "rb");
    if (!f) return false;
    char buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out->append(buf, n);
    const bool ok = (std::ferror(f) == 0);
    std::fclose(f);
    return ok;
}

// —— 归档 (.lib) ——

struct ArchiveMemberHeader {
    std::string name;   // 16 字节, 去尾空格
    uint32_t size = 0;  // 10 字节十进制
    bool valid = false;
};

ArchiveMemberHeader parseMemberHeader(const std::string& d, size_t off) {
    ArchiveMemberHeader h;
    if (off + 60 > d.size()) return h;
    auto field = [&](size_t rel, size_t len) {
        return d.substr(off + rel, len);
    };
    // 尾标必须是 "`\n" (0x60 0x0A), 这是布局校验的核心 (SR4)
    if (field(58, 2) != "`\n") return h;
    h.name = field(0, 16);
    while (!h.name.empty() && h.name.back() == ' ') h.name.pop_back();
    const std::string sizeStr = field(48, 10);
    h.size = static_cast<uint32_t>(std::strtoul(sizeStr.c_str(), nullptr, 10));
    h.valid = true;
    return h;
}

bool parseArchive(const std::string& d, CoffSymbols* out) {
    // 魔数 "!<arch>\n"
    if (d.size() < 8 || d.compare(0, 8, "!<arch>\n") != 0) {
        out->error = "not a COFF archive";
        return false;
    }
    size_t off = 8;
    while (off + 60 <= d.size()) {
        const ArchiveMemberHeader h = parseMemberHeader(d, off);
        if (!h.valid) {
            out->error = "corrupt member header";
            return false;
        }
        const size_t dataOff = off + 60;
        if (dataOff + h.size > d.size()) {
            out->error = "member overruns file";
            return false;
        }
        // linker member #1: 名字恰为 "/". (第二个索引成员名是 "/<之前字节数>",
        // 内容等价, 不必重复解析。)
        if (h.name == "/") {
            const std::string& m = d;  // 数据就在文件里
            size_t p = dataOff;
            if (h.size < 4) {
                out->error = "linker member too small";
                return false;
            }
            const uint32_t count =
                static_cast<uint32_t>((static_cast<uint8_t>(m[p]) << 24) |
                                      (static_cast<uint8_t>(m[p+1]) << 16) |
                                      (static_cast<uint8_t>(m[p+2]) << 8) |
                                       static_cast<uint8_t>(m[p+3]));
            p += 4;
            const size_t need = 4ull * count;
            if (p + need > dataOff + h.size) {
                out->error = "symbol offset table overruns member";
                return false;
            }
            p += need;
            // 之后是 count 个 NUL 结尾的名字 (成员内剩余部分可能还有对齐尾随)
            out->names.reserve(count);
            for (uint32_t i = 0; i < count; ++i) {
                if (p >= dataOff + h.size) break;  // 容错: 索引数与名字数不一致时截断
                size_t end = m.find('\0', p);
                if (end == std::string::npos || end > dataOff + h.size) end = dataOff + h.size;
                out->names.emplace_back(m, p, end - p);
                p = end + 1;
            }
            out->ok = true;
            return true;
        }
        // 成员数据补齐到偶数边界 (补丁是 0x0A)
        off = dataOff + h.size + (h.size & 1);
    }
    out->error = "no linker symbol member found";
    return false;
}

// —— 单个 COFF 目标文件 (.obj) ——

bool parseObject(const std::string& d, CoffSymbols* out) {
    if (d.size() < 20) {
        out->error = "truncated COFF header";
        return false;
    }
    // 机器类型 (小端): 0x014C=I386, 0x8664=AMD64 (ARM64=0xAA64 也不拒)
    const uint16_t machine = static_cast<uint16_t>(static_cast<uint8_t>(d[0])) |
                             (static_cast<uint16_t>(static_cast<uint8_t>(d[1])) << 8);
    (void)machine;  // 只读诊断, 不校验机器 (架构混用由 LNK4272 那条遗留管)
    auto rd32 = [&](size_t off) {
        return static_cast<uint32_t>(static_cast<uint8_t>(d[off])) |
               (static_cast<uint32_t>(static_cast<uint8_t>(d[off+1])) << 8) |
               (static_cast<uint32_t>(static_cast<uint8_t>(d[off+2])) << 16) |
               (static_cast<uint32_t>(static_cast<uint8_t>(d[off+3])) << 24);
    };
    const uint32_t symPtr = rd32(12);
    const uint32_t symCount = rd32(16);
    if (symPtr == 0) {
        out->error = "object has no symbol table";
        return false;
    }
    const size_t strOff = static_cast<size_t>(symPtr) + 18ull * symCount;
    if (strOff + 4 > d.size()) {
        out->error = "symbol table overruns file";
        return false;
    }
    const uint32_t strSize = rd32(strOff);
    for (uint32_t i = 0; i < symCount; ++i) {
        const size_t e = static_cast<size_t>(symPtr) + 18ull * i;
        if (e + 18 > d.size()) break;
        const uint8_t sc = static_cast<uint8_t>(d[e + 16]);   // storage class
        const int16_t sect = static_cast<int16_t>(
            static_cast<uint8_t>(d[e + 12]) |
            (static_cast<uint16_t>(static_cast<uint8_t>(d[e + 13])) << 8));
        if (sc != 2 || sect == 0) {   // 只要"已定义的外部符号"
            i += d[e + 17];           // 跳过 aux 记录
            continue;
        }
        std::string name;
        if (d[e] == 0 && d[e+1] == 0 && d[e+2] == 0 && d[e+3] == 0) {
            const uint32_t off = rd32(e + 4);
            if (off < strSize && strOff + off < d.size()) {
                size_t end = d.find('\0', strOff + off);
                if (end == std::string::npos) end = d.size();
                name.assign(d, strOff + off, end - (strOff + off));
            }
        } else {
            name.assign(d, e, 8);
            // 短名不足 8 字节时会被 NUL 填充, 截到第一个 NUL
            const size_t z = name.find('\0');
            if (z != std::string::npos) name.resize(z);
        }
        if (!name.empty()) out->names.push_back(std::move(name));
        i += d[e + 17];  // 跳过 aux 记录
    }
    out->ok = true;
    return true;
}

} // namespace

CoffSymbols readCoffSymbols(const std::string& path) {
    CoffSymbols out;
    std::string d;
    if (!readFile(path, &d)) {
        out.error = "cannot read file";
        return out;
    }
    if (d.size() >= 8 && d.compare(0, 8, "!<arch>\n") == 0) {
        parseArchive(d, &out);
    } else {
        parseObject(d, &out);
    }
    return out;
}

std::string normalizeCoffSymbol(const std::string& name) {
    size_t b = 0;
    if (!name.empty() && name[0] == '_') b = 1;
    size_t e = name.size();
    // 尾随 @N (stdcall): 全是数字才剥
    if (e > b) {
        size_t p = name.find('@', b);
        if (p != std::string::npos) {
            bool allDigits = p + 1 < e;
            for (size_t i = p + 1; i < e; ++i) {
                if (name[i] < '0' || name[i] > '9') { allDigits = false; break; }
            }
            if (allDigits) e = p;
        }
    }
    std::string r = name.substr(b, e - b);
    for (char& c : r) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return r;
}

} // namespace vb6c3
