#pragma once
// 源文件管理 - 文件读取、编码检测、行映射

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

namespace vb6c3 {

// 文本编码类型
enum class SourceEncoding : uint8_t {
    ASCII,      // 纯ASCII
    UTF8,       // UTF-8 (无BOM)
    UTF8_BOM,   // UTF-8 with BOM
    UTF16_LE,   // UTF-16 Little Endian (Windows默认)
    UTF16_BE,   // UTF-16 Big Endian
    GBK,        // GBK (Code Page 936, VB6默认)
    Unknown,
};

// 源文件缓冲区
class SourceBuffer {
public:
    // 从文件加载
    static std::unique_ptr<SourceBuffer> fromFile(const std::string& path);

    // M22: 读取文件并转换为UTF-8 (供VbpParser/FrmParser等使用, 返回UTF-8内容)
    // 返回空string表示读取失败, 同时返回检测到的编码
    struct ReadResult {
        std::string content;   // UTF-8规范化内容 (CRLF→LF)
        SourceEncoding encoding;
    };
    static ReadResult readAndConvertToUtf8(const std::string& path);

    // 从字符串创建(测试用)
    static std::unique_ptr<SourceBuffer> fromString(
        const std::string& filename,
        const std::string& content);

    // 获取规范化后的UTF-8内容
    const std::string& content() const { return content_; }

    // 获取文件路径
    const std::string& filePath() const { return filePath_; }

    // 获取检测到的编码
    SourceEncoding detectedEncoding() const { return encoding_; }

    // 行号查询: 给定偏移量, 返回行号(1-based)和列号(1-based)
    void getLocation(uint32_t offset, uint32_t& line, uint32_t& column) const;

    // 获取指定行的内容(1-based)
    std::string_view getLine(uint32_t line) const;

    // 总行数
    uint32_t lineCount() const { return static_cast<uint32_t>(lineOffsets_.size()); }

private:
    SourceBuffer() = default;

    // 检测编码并转换为UTF-8
    SourceEncoding detectEncoding(const uint8_t* data, size_t size);
    std::string convertToUtf8(const uint8_t* data, size_t size, SourceEncoding enc);

    // 构建行偏移表
    void buildLineTable();

    std::string filePath_;
    std::string content_;           // UTF-8规范化内容
    SourceEncoding encoding_ = SourceEncoding::Unknown;
    std::vector<uint32_t> lineOffsets_; // 每行起始偏移量
};

} // namespace vb6c3
