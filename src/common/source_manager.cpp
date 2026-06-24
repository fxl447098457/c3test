#include "common/source_manager.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

namespace vb6c3 {

// === 编码检测 ===

SourceEncoding SourceBuffer::detectEncoding(const uint8_t* data, size_t size) {
    if (size < 2) return SourceEncoding::ASCII;

    // BOM检测
    if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        return SourceEncoding::UTF8_BOM;
    }
    if (size >= 2 && data[0] == 0xFF && data[1] == 0xFE) {
        return SourceEncoding::UTF16_LE;
    }
    if (size >= 2 && data[0] == 0xFE && data[1] == 0xFF) {
        return SourceEncoding::UTF16_BE;
    }

    // 无BOM: 尝试判断UTF-8 vs GBK
    // 启发式: 如果内容中高位字节符合UTF-8多字节序列规则, 则为UTF-8
    // 否则假设为GBK (VB6默认编码)
    bool hasHighByte = false;
    bool validUtf8 = true;
    for (size_t i = 0; i < size && validUtf8; ) {
        uint8_t c = data[i];
        if (c < 0x80) {
            i++;
        } else if ((c & 0xE0) == 0xC0) {
            hasHighByte = true;
            if (i + 1 >= size || (data[i+1] & 0xC0) != 0x80) validUtf8 = false;
            else i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            hasHighByte = true;
            if (i + 2 >= size || (data[i+1] & 0xC0) != 0x80 || (data[i+2] & 0xC0) != 0x80) validUtf8 = false;
            else i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            hasHighByte = true;
            if (i + 3 >= size || (data[i+1] & 0xC0) != 0x80 || (data[i+2] & 0xC0) != 0x80 || (data[i+3] & 0xC0) != 0x80) validUtf8 = false;
            else i += 4;
        } else {
            hasHighByte = true;
            validUtf8 = false;
            i++;
        }
    }

    if (!hasHighByte) return SourceEncoding::ASCII;
    if (validUtf8) return SourceEncoding::UTF8;
    return SourceEncoding::GBK;
}

// === 编码转换 ===

std::string SourceBuffer::convertToUtf8(const uint8_t* data, size_t size, SourceEncoding enc) {
    switch (enc) {
        case SourceEncoding::ASCII:
        case SourceEncoding::UTF8:
            return std::string(reinterpret_cast<const char*>(data), size);

        case SourceEncoding::UTF8_BOM:
            // 跳过3字节BOM
            return std::string(reinterpret_cast<const char*>(data + 3), size - 3);

        case SourceEncoding::UTF16_LE: {
            // 跳过2字节BOM
            const uint8_t* start = data + 2;
            size_t len = (size - 2) / 2;
            std::string result;
            result.reserve(len * 3); // UTF-8最多3字节/字符
            for (size_t i = 0; i < len; ) {
                uint16_t ch = start[i*2] | (start[i*2+1] << 8);
                if (ch >= 0xD800 && ch <= 0xDBFF && i + 1 < len) {
                    // 代理对
                    uint16_t lo = start[(i+1)*2] | (start[(i+1)*2+1] << 8);
                    if (lo >= 0xDC00 && lo <= 0xDFFF) {
                        uint32_t cp = 0x10000 + ((static_cast<uint32_t>(ch) - 0xD800) << 10) + (lo - 0xDC00);
                        result += static_cast<char>(0xF0 | ((cp >> 18) & 0x07));
                        result += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                        result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (cp & 0x3F));
                        i += 2;
                        continue;
                    }
                }
                if (ch < 0x80) {
                    result += static_cast<char>(ch);
                } else if (ch < 0x800) {
                    result += static_cast<char>(0xC0 | (ch >> 6));
                    result += static_cast<char>(0x80 | (ch & 0x3F));
                } else {
                    result += static_cast<char>(0xE0 | (ch >> 12));
                    result += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
                    result += static_cast<char>(0x80 | (ch & 0x3F));
                }
                i++;
            }
            return result;
        }

        case SourceEncoding::UTF16_BE: {
            const uint8_t* start = data + 2;
            size_t len = (size - 2) / 2;
            std::string result;
            result.reserve(len * 3);
            for (size_t i = 0; i < len; i++) {
                uint16_t ch = (start[i*2] << 8) | start[i*2+1];
                if (ch < 0x80) {
                    result += static_cast<char>(ch);
                } else if (ch < 0x800) {
                    result += static_cast<char>(0xC0 | (ch >> 6));
                    result += static_cast<char>(0x80 | (ch & 0x3F));
                } else {
                    result += static_cast<char>(0xE0 | (ch >> 12));
                    result += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
                    result += static_cast<char>(0x80 | (ch & 0x3F));
                }
            }
            return result;
        }

        case SourceEncoding::GBK: {
#ifdef _WIN32
            // Windows: 使用MultiByteToWideChar → WideCharToMultiByte
            int wlen = MultiByteToWideChar(936, 0, reinterpret_cast<const char*>(data), static_cast<int>(size), nullptr, 0);
            if (wlen <= 0) return std::string(reinterpret_cast<const char*>(data), size);
            std::vector<wchar_t> wide(wlen);
            MultiByteToWideChar(936, 0, reinterpret_cast<const char*>(data), static_cast<int>(size), wide.data(), wlen);
            int utf8len = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wlen, nullptr, 0, nullptr, nullptr);
            if (utf8len <= 0) return std::string(reinterpret_cast<const char*>(data), size);
            std::string result(utf8len, '\0');
            WideCharToMultiByte(CP_UTF8, 0, wide.data(), wlen, &result[0], utf8len, nullptr, nullptr);
            return result;
#else
            // Linux/macOS: 暂时直接返回原始字节
            // TODO: 使用iconv转换
            return std::string(reinterpret_cast<const char*>(data), size);
#endif
        }

        default:
            return std::string(reinterpret_cast<const char*>(data), size);
    }
}

// === 行偏移表 ===

void SourceBuffer::buildLineTable() {
    lineOffsets_.clear();
    lineOffsets_.push_back(0); // 第1行从0开始
    for (size_t i = 0; i < content_.size(); i++) {
        if (content_[i] == '\n') {
            lineOffsets_.push_back(static_cast<uint32_t>(i + 1));
        }
    }
}

// === 公开接口 ===

std::unique_ptr<SourceBuffer> SourceBuffer::fromFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return nullptr;

    file.seekg(0, std::ios::end);
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> raw(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(raw.data()), size);
    if (!file) return nullptr;

    auto buf = std::unique_ptr<SourceBuffer>(new SourceBuffer());
    buf->filePath_ = path;
    buf->encoding_ = buf->detectEncoding(raw.data(), raw.size());
    buf->content_ = buf->convertToUtf8(raw.data(), raw.size(), buf->encoding_);

    // 统一换行符为LF
    std::string normalized;
    normalized.reserve(buf->content_.size());
    for (size_t i = 0; i < buf->content_.size(); i++) {
        if (buf->content_[i] == '\r') {
            if (i + 1 < buf->content_.size() && buf->content_[i+1] == '\n') {
                continue; // 跳过\r, 保留\n
            }
            normalized += '\n'; // 孤立\r转为\n
        } else {
            normalized += buf->content_[i];
        }
    }
    buf->content_ = std::move(normalized);

    buf->buildLineTable();
    return buf;
}

std::unique_ptr<SourceBuffer> SourceBuffer::fromString(
    const std::string& filename,
    const std::string& content)
{
    auto buf = std::unique_ptr<SourceBuffer>(new SourceBuffer());
    buf->filePath_ = filename;
    buf->encoding_ = SourceEncoding::UTF8;
    buf->content_ = content;
    buf->buildLineTable();
    return buf;
}

void SourceBuffer::getLocation(uint32_t offset, uint32_t& line, uint32_t& column) const {
    // 二分查找所在行
    auto it = std::upper_bound(lineOffsets_.begin(), lineOffsets_.end(), offset);
    if (it == lineOffsets_.begin()) {
        line = 1;
        column = offset + 1;
        return;
    }
    --it;
    line = static_cast<uint32_t>(it - lineOffsets_.begin()) + 1;
    column = offset - *it + 1;
}

std::string_view SourceBuffer::getLine(uint32_t line) const {
    if (line < 1 || line > lineOffsets_.size()) return {};
    uint32_t start = lineOffsets_[line - 1];
    uint32_t end = (line < lineOffsets_.size()) ? lineOffsets_[line] : static_cast<uint32_t>(content_.size());
    return std::string_view(content_).substr(start, end - start);
}

} // namespace vb6c3
