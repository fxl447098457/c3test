#pragma once
#include <cstdint>
#include <string>

namespace vb6c3::rtl {

// BSTR字符串 - VB6兼容BSTR (length-prefix + wchar_t* + null)
// MVP: Windows上使用SysAllocString系列
// 将来: Core层自实现，不依赖OLE32

class BString {
public:
    BString();
    explicit BString(const wchar_t* str);
    explicit BString(const char* utf8Str);
    BString(const BString& other);
    BString(BString&& other) noexcept;
    ~BString();

    BString& operator=(const BString& other);
    BString& operator=(BString&& other) noexcept;

    const wchar_t* c_str() const { return data_; }
    uint32_t length() const { return len_; }
    bool empty() const { return len_ == 0; }

    // VB6字符串操作
    static BString concat(const BString& a, const BString& b);
    int compare(const BString& other, bool caseSensitive = false) const;
    BString left(uint32_t count) const;
    BString right(uint32_t count) const;
    BString mid(uint32_t start, uint32_t count) const;
    uint32_t inStr(const BString& search, uint32_t start = 1) const;
    BString upper() const;
    BString lower() const;
    BString trim() const;
    BString ltrim() const;
    BString rtrim() const;

    // UTF-8转换
    std::string toUtf8() const;
    static BString fromUtf8(const std::string& utf8);

private:
    wchar_t* data_ = nullptr;
    uint32_t len_ = 0;

    void alloc(uint32_t len);
    void free();
};

} // namespace vb6c3::rtl
