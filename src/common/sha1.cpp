// ai/023 S05: SHA-1 实现 (FIPS 180-1; 纯完整性校验用, 见 sha1.hpp 头注)。

#include "common/sha1.hpp"
#include "common/encoding.hpp"

#include <cstdio>
#include <vector>

namespace vb6c3 {

namespace {

struct Sha1Ctx {
    uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
    uint64_t totalBits = 0;
    uint8_t buf[64] = {};
    size_t bufLen = 0;
};

inline uint32_t rol(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

void sha1Block(Sha1Ctx& c, const uint8_t* p) {
    uint32_t w[80];
    for (int i = 0; i < 16; ++i) {
        w[i] = (uint32_t(p[i * 4]) << 24) | (uint32_t(p[i * 4 + 1]) << 16) |
               (uint32_t(p[i * 4 + 2]) << 8) | uint32_t(p[i * 4 + 3]);
    }
    for (int i = 16; i < 80; ++i) {
        w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }
    uint32_t a = c.h[0], b = c.h[1], d = c.h[3], e = c.h[4];
    uint32_t cc = c.h[2];
    for (int i = 0; i < 80; ++i) {
        uint32_t f, k;
        if (i < 20) {
            f = (b & cc) | ((~b) & d);
            k = 0x5A827999u;
        } else if (i < 40) {
            f = b ^ cc ^ d;
            k = 0x6ED9EBA1u;
        } else if (i < 60) {
            f = (b & cc) | (b & d) | (cc & d);
            k = 0x8F1BBCDCu;
        } else {
            f = b ^ cc ^ d;
            k = 0xCA62C1D6u;
        }
        uint32_t t = rol(a, 5) + f + e + k + w[i];
        e = d;
        d = cc;
        cc = rol(b, 30);
        b = a;
        a = t;
    }
    c.h[0] += a;
    c.h[1] += b;
    c.h[2] += cc;
    c.h[3] += d;
    c.h[4] += e;
}

void sha1Update(Sha1Ctx& c, const uint8_t* data, size_t len) {
    c.totalBits += uint64_t(len) * 8;
    while (len > 0) {
        size_t take = 64 - c.bufLen;
        if (take > len) take = len;
        for (size_t i = 0; i < take; ++i) c.buf[c.bufLen + i] = data[i];
        c.bufLen += take;
        data += take;
        len -= take;
        if (c.bufLen == 64) {
            sha1Block(c, c.buf);
            c.bufLen = 0;
        }
    }
}

std::string sha1Final(Sha1Ctx& c) {
    uint64_t bits = c.totalBits;
    uint8_t pad = 0x80;
    sha1Update(c, &pad, 1);
    uint8_t zero = 0;
    while (c.bufLen != 56) sha1Update(c, &zero, 1);
    uint8_t lenBytes[8];
    for (int i = 0; i < 8; ++i) lenBytes[i] = uint8_t(bits >> (56 - i * 8));
    // 直接手动 append (sha1Update 会再累计 totalBits, 但已不再使用)
    for (int i = 0; i < 8; ++i) c.buf[c.bufLen + i] = lenBytes[i];
    sha1Block(c, c.buf);

    char out[41];
    for (int i = 0; i < 5; ++i) {
        std::snprintf(out + i * 8, 9, "%08x", c.h[i]);
    }
    out[40] = '\0';
    return std::string(out);
}

} // namespace

std::string sha1Hex(const uint8_t* data, size_t len) {
    Sha1Ctx c;
    sha1Update(c, data, len);
    return sha1Final(c);
}

std::string sha1HexOfFile(const std::string& pathUtf8) {
    std::FILE* f = nullptr;
    auto p = utf8ToPath(pathUtf8);
    // _wfopen 走宽字符, 中文路径不经过 ACP
#ifdef _WIN32
    f = _wfopen(p.c_str(), L"rb");
#else
    f = std::fopen(p.string().c_str(), "rb");
#endif
    if (!f) return {};
    Sha1Ctx c;
    std::vector<uint8_t> buf(65536);
    size_t n;
    while ((n = std::fread(buf.data(), 1, buf.size(), f)) > 0) {
        sha1Update(c, buf.data(), n);
    }
    std::fclose(f);
    return sha1Final(c);
}

} // namespace vb6c3
