#include "rtl/core/bstring.hpp"

#ifdef _WIN32
#include <windows.h>
#include <oleauto.h>
#endif

#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace vb6c3::rtl {

BString::BString() : data_(nullptr), len_(0) {}

BString::BString(const wchar_t* str) {
    if (str) {
        len_ = static_cast<uint32_t>(wcslen(str));
        alloc(len_);
        wmemcpy(data_, str, len_ + 1);
    } else {
        data_ = nullptr;
        len_ = 0;
    }
}

BString::BString(const char* utf8Str) {
    auto ws = fromUtf8(utf8Str ? std::string(utf8Str) : std::string());
    data_ = ws.data_;
    len_ = ws.len_;
    ws.data_ = nullptr;
    ws.len_ = 0;
}

BString::BString(const BString& other) : len_(other.len_) {
    if (len_ > 0) {
        alloc(len_);
        wmemcpy(data_, other.data_, len_ + 1);
    } else {
        data_ = nullptr;
    }
}

BString::BString(BString&& other) noexcept : data_(other.data_), len_(other.len_) {
    other.data_ = nullptr;
    other.len_ = 0;
}

BString::~BString() { free(); }

BString& BString::operator=(const BString& other) {
    if (this != &other) {
        free();
        len_ = other.len_;
        if (len_ > 0) {
            alloc(len_);
            wmemcpy(data_, other.data_, len_ + 1);
        }
    }
    return *this;
}

BString& BString::operator=(BString&& other) noexcept {
    if (this != &other) {
        free();
        data_ = other.data_;
        len_ = other.len_;
        other.data_ = nullptr;
        other.len_ = 0;
    }
    return *this;
}

BString BString::concat(const BString& a, const BString& b) {
    BString result;
    result.len_ = a.len_ + b.len_;
    if (result.len_ > 0) {
        result.alloc(result.len_);
        if (a.len_ > 0) wmemcpy(result.data_, a.data_, a.len_);
        if (b.len_ > 0) wmemcpy(result.data_ + a.len_, b.data_, b.len_);
        result.data_[result.len_] = L'\0';
    }
    return result;
}

int BString::compare(const BString& other, bool caseSensitive) const {
    if (caseSensitive) {
        return wcscmp(data_ ? data_ : L"", other.data_ ? other.data_ : L"");
    }
#ifdef _WIN32
    return CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
        data_ ? data_ : L"", len_,
        other.data_ ? other.data_ : L"", other.len_) - CSTR_EQUAL;
#else
    // TODO: POSIX实现
    return wcscmp(data_ ? data_ : L"", other.data_ ? other.data_ : L"");
#endif
}

BString BString::left(uint32_t count) const {
    if (count >= len_) return *this;
    BString result;
    result.len_ = count;
    result.alloc(count);
    wmemcpy(result.data_, data_, count);
    result.data_[count] = L'\0';
    return result;
}

BString BString::right(uint32_t count) const {
    if (count >= len_) return *this;
    BString result;
    result.len_ = count;
    result.alloc(count);
    wmemcpy(result.data_, data_ + len_ - count, count);
    result.data_[count] = L'\0';
    return result;
}

BString BString::mid(uint32_t start, uint32_t count) const {
    // VB6 Mid: 1-based start
    if (start < 1 || start > len_) return BString();
    uint32_t actualCount = std::min(count, len_ - start + 1);
    BString result;
    result.len_ = actualCount;
    result.alloc(actualCount);
    wmemcpy(result.data_, data_ + start - 1, actualCount);
    result.data_[actualCount] = L'\0';
    return result;
}

uint32_t BString::inStr(const BString& search, uint32_t start) const {
    if (start < 1) start = 1;
    if (search.len_ == 0 || len_ < search.len_) return 0;
    for (uint32_t i = start - 1; i <= len_ - search.len_; i++) {
        if (wmemcmp(data_ + i, search.data_, search.len_) == 0) {
            return i + 1; // 1-based
        }
    }
    return 0;
}

BString BString::upper() const {
    BString result(*this);
#ifdef _WIN32
    CharUpperW(result.data_);
#else
    // TODO: POSIX towupper
#endif
    return result;
}

BString BString::lower() const {
    BString result(*this);
#ifdef _WIN32
    CharLowerW(result.data_);
#else
    // TODO: POSIX towlower
#endif
    return result;
}

BString BString::trim() const {
    uint32_t start = 0, end = len_;
    while (start < end && iswspace(data_[start])) start++;
    while (end > start && iswspace(data_[end - 1])) end--;
    if (start == 0 && end == len_) return *this;
    BString result;
    result.len_ = end - start;
    result.alloc(result.len_);
    wmemcpy(result.data_, data_ + start, result.len_);
    result.data_[result.len_] = L'\0';
    return result;
}

BString BString::ltrim() const {
    uint32_t start = 0;
    while (start < len_ && iswspace(data_[start])) start++;
    if (start == 0) return *this;
    return mid(start + 1, len_ - start);
}

BString BString::rtrim() const {
    uint32_t end = len_;
    while (end > 0 && iswspace(data_[end - 1])) end--;
    if (end == len_) return *this;
    return left(end);
}

std::string BString::toUtf8() const {
    if (!data_ || len_ == 0) return "";
#ifdef _WIN32
    int utf8len = WideCharToMultiByte(CP_UTF8, 0, data_, static_cast<int>(len_),
        nullptr, 0, nullptr, nullptr);
    if (utf8len <= 0) return "";
    std::string result(utf8len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, data_, static_cast<int>(len_),
        &result[0], utf8len, nullptr, nullptr);
    return result;
#else
    // TODO: POSIX iconv
    return "";
#endif
}

BString BString::fromUtf8(const std::string& utf8) {
#ifdef _WIN32
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
        static_cast<int>(utf8.size()), nullptr, 0);
    if (wlen <= 0) return BString();
    BString result;
    result.len_ = static_cast<uint32_t>(wlen);
    result.alloc(result.len_);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
        static_cast<int>(utf8.size()), result.data_, wlen);
    result.data_[result.len_] = L'\0';
    return result;
#else
    // TODO: POSIX iconv
    return BString();
#endif
}

void BString::alloc(uint32_t len) {
    data_ = new wchar_t[len + 1]();
}

void BString::free() {
    delete[] data_;
    data_ = nullptr;
    len_ = 0;
}

} // namespace vb6c3::rtl
