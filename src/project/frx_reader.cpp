// ============================================================
// .frx 二进制资源文件读取器实现
// ============================================================
#include "project/frx_reader.hpp"
#include <fstream>
#include <cstring>
#include <algorithm>

// Windows MultiByteToWideChar for GBK->UTF-8 conversion
#ifdef _WIN32
#include <windows.h>
#endif

namespace vb6c3 {

// 静态成员初始化
std::vector<uint8_t> FrxReader::fileData_;
std::string FrxReader::lastError_;

// ============================================================
// 加载 .frx 文件
// ============================================================
bool FrxReader::load(const std::filesystem::path& frxPath) {
    std::ifstream f(frxPath, std::ios::binary);
    if (!f.is_open()) {
        lastError_ = "Cannot open .frx file: " + frxPath.string();
        return false;
    }
    f.seekg(0, std::ios::end);
    size_t size = (size_t)f.tellg();
    f.seekg(0, std::ios::beg);
    fileData_.resize(size);
    f.read((char*)fileData_.data(), size);
    f.close();
    lastError_.clear();
    return true;
}

// ============================================================
// 读取 little-endian 值
// ============================================================
uint32_t FrxReader::readLE32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

uint16_t FrxReader::readLE16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

// ============================================================
// 检测图片格式
// ============================================================
FrxImageFormat FrxReader::detectFormat(const uint8_t* data, size_t size) {
    if (size < 4) return FrxImageFormat::Unknown;
    // JPEG: FF D8 FF
    if (data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF)
        return FrxImageFormat::JPEG;
    // ICO: 00 00 01 00
    if (data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01 && data[3] == 0x00)
        return FrxImageFormat::ICO;
    // BMP: "BM"
    if (data[0] == 'B' && data[1] == 'M')
        return FrxImageFormat::BMP;
    // PNG: 89 50 4E 47
    if (data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47)
        return FrxImageFormat::PNG;
    // GIF: "GIF8"
    if (data[0] == 'G' && data[1] == 'I' && data[2] == 'F' && data[3] == '8')
        return FrxImageFormat::GIF;
    // WMF: D7 CD C6 9A
    if (data[0] == 0xD7 && data[1] == 0xCD && data[2] == 0xC6 && data[3] == 0x9A)
        return FrxImageFormat::WMF;
    // EMF: 01 00 00 00 (first DWORD of EMR_HEADER)
    if (data[0] == 0x01 && data[1] == 0x00 && data[2] == 0x00 && data[3] == 0x00 && size >= 6)
        return FrxImageFormat::EMF;
    return FrxImageFormat::Unknown;
}

// ============================================================
// 读取图片数据
// ============================================================
// .frx Picture 属性格式:
//   普通控件: [4B totalSize] [4B magic "lt\0\0"] [4B imgSize] [imgSize bytes imageData]
//   ImageList: [4B totalSize] [16B GUID] [4B magic "lt\0\0"] [4B imgSize] [imgSize bytes imageData]
FrxPictureData FrxReader::readPicture(size_t offset, bool isImageList) {
    FrxPictureData result;
    if (fileData_.empty()) {
        lastError_ = ".frx file not loaded";
        return result;
    }
    if (offset + 12 > fileData_.size()) {
        lastError_ = "Offset out of range";
        return result;
    }

    const uint8_t* base = fileData_.data() + offset;
    size_t remaining = fileData_.size() - offset;

    // 跳过头部
    size_t headerSize;
    if (isImageList) {
        // ImageList: 4B totalSize + 16B GUID + 4B magic + 4B imgSize = 28 bytes header
        headerSize = 28;
    } else {
        // Normal: 4B totalSize + 4B magic + 4B imgSize = 12 bytes header
        headerSize = 12;
    }

    if (offset + headerSize > fileData_.size()) {
        lastError_ = "Picture header exceeds file bounds";
        return result;
    }

    // 读取图片数据大小
    uint32_t imgSize;
    if (isImageList) {
        // [4B totalSize] [16B GUID] [4B magic] [4B imgSize] [data]
        imgSize = readLE32(base + 24);
    } else {
        // [4B totalSize] [4B magic] [4B imgSize] [data]
        imgSize = readLE32(base + 8);
    }

    if (imgSize == 0 || imgSize > remaining - headerSize) {
        lastError_ = "Invalid image size in .frx";
        return result;
    }

    // 提取图片数据
    const uint8_t* imgData = base + headerSize;
    result.data.assign(imgData, imgData + imgSize);
    result.format = detectFormat(imgData, imgSize);

    return result;
}

// ============================================================
// 读取多行文本 (TextBox.Text)
// ============================================================
// 格式: [1B length] [length bytes GBK text]
// 注意: 长度可能超过255, 实际格式可能是 [4B length] [data]
// 经分析 frxParse 的 Text1.Text: offset 0x27C4
//   27 27 00 00 "Text1\r\n大师的\r\n佛挡杀佛\r\n"
//   -> 4字节长度(0x27=39), 后跟39字节GBK文本
FrxTextData FrxReader::readText(size_t offset) {
    FrxTextData result;
    if (fileData_.empty()) {
        lastError_ = ".frx file not loaded";
        return result;
    }
    if (offset + 4 > fileData_.size()) {
        lastError_ = "Text offset out of range";
        return result;
    }

    const uint8_t* base = fileData_.data() + offset;
    // 尝试4字节长度前缀 (VB6 .frx 中 Text 属性使用 DWORD 长度)
    uint32_t textLen = readLE32(base);
    if (textLen == 0 || offset + 4 + textLen > fileData_.size()) {
        // 回退: 尝试1字节长度前缀
        textLen = base[0];
        if (textLen == 0 || offset + 1 + textLen > fileData_.size()) {
            lastError_ = "Invalid text length in .frx";
            return result;
        }
        result.text = gbkToUtf8(std::string((const char*)(base + 1), textLen));
    } else {
        result.text = gbkToUtf8(std::string((const char*)(base + 4), textLen));
    }
    return result;
}

// ============================================================
// 读取字符串列表 (ListBox.List)
// ============================================================
// 格式: [2B count] [per item: 2B length + length bytes GBK text]
FrxListData FrxReader::readStringList(size_t offset) {
    FrxListData result;
    if (fileData_.empty()) {
        lastError_ = ".frx file not loaded";
        return result;
    }
    if (offset + 2 > fileData_.size()) {
        lastError_ = "String list offset out of range";
        return result;
    }

    const uint8_t* base = fileData_.data() + offset;
    // VB6 .frx List format: [2B count] [2B prefix] [per item: 2B len + GBK text]
    // The prefix value equals count; skip both
    uint16_t count = readLE16(base);
    size_t pos = 4;  // skip count(2B) + prefix(2B)

    for (uint16_t i = 0; i < count; i++) {
        if (offset + pos + 2 > fileData_.size()) break;
        uint16_t strLen = readLE16(base + pos);
        pos += 2;
        if (offset + pos + strLen > fileData_.size()) break;
        std::string gbkStr((const char*)(base + pos), strLen);
        result.items.push_back(gbkToUtf8(gbkStr));
        pos += strLen;
    }
    return result;
}

// ============================================================
// 读取整数列表 (ListBox.ItemData)
// ============================================================
// 格式: [2B count] [per item: 2B integer value (WORD)]
// 注意: VB6 ItemData 实际是 Long (4 bytes), 需要验证
FrxIntListData FrxReader::readIntList(size_t offset) {
    FrxIntListData result;
    if (fileData_.empty()) {
        lastError_ = ".frx file not loaded";
        return result;
    }
    if (offset + 2 > fileData_.size()) {
        lastError_ = "Int list offset out of range";
        return result;
    }

    const uint8_t* base = fileData_.data() + offset;
    // VB6 .frx ItemData format: [2B count] [2B prefix] [per item: 2B Integer]
    // The prefix value equals count; skip both
    uint16_t count = readLE16(base);
    size_t pos = 4;  // skip count(2B) + prefix(2B)

    if (count > 10000) {
        lastError_ = "Invalid int list count in .frx";
        return result;
    }

    for (uint16_t i = 0; i < count; i++) {
        if (offset + pos + 2 > fileData_.size()) break;
        int16_t val = (int16_t)readLE16(base + pos);
        result.items.push_back(val);
        pos += 2;
    }
    return result;
}

// ============================================================
// GBK -> UTF-8 转换
// ============================================================
std::string FrxReader::gbkToUtf8(const std::string& gbk) {
#ifdef _WIN32
    if (gbk.empty()) return "";
    // GBK -> UTF-16
    int wlen = MultiByteToWideChar(936, 0, gbk.c_str(), (int)gbk.size(), NULL, 0);
    if (wlen <= 0) return gbk;  // fallback
    std::wstring wstr(wlen, 0);
    MultiByteToWideChar(936, 0, gbk.c_str(), (int)gbk.size(), &wstr[0], wlen);
    // UTF-16 -> UTF-8
    int ulen = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), wlen, NULL, 0, NULL, NULL);
    if (ulen <= 0) return gbk;
    std::string utf8(ulen, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), wlen, &utf8[0], ulen, NULL, NULL);
    return utf8;
#else
    return gbk;  // non-Windows: no conversion
#endif
}

} // namespace vb6c3
