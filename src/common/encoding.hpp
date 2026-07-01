#pragma once
// 编码工具 — Windows路径UTF-8/UTF-16/ACP互转
// VB6编译器内部统一使用UTF-8字符串, 但Windows filesystem::path按ACP解释char*,
// 导致VBP解析器输出的UTF-8字符串被双重编码。本模块提供安全转换函数。

#include <string>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

namespace vb6c3 {

// filesystem::path → UTF-8 string
// On Windows, path::string() returns ACP-encoded string which is wrong for internal UTF-8 usage.
// Path internally stores UTF-16 on Windows, so use wstring() + conversion for correct results.
inline std::string pathToUtf8(const std::filesystem::path& p) {
#ifdef _WIN32
    std::wstring wide = p.wstring();
    if (wide.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return p.string();  // fallback
    std::string utf8(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &utf8[0], len, nullptr, nullptr);
    while (!utf8.empty() && utf8.back() == '\0') utf8.pop_back();
    return utf8;
#else
    return p.string();
#endif
}

// UTF-8 string → wide string
// When a string is already UTF-8 (e.g. from VBP parser GBK→UTF-8 conversion),
// we must NOT pass it to filesystem::path(const char*) which interprets as ACP on Windows.
inline std::wstring utf8ToWide(const std::string& utf8) {
#ifdef _WIN32
    if (utf8.empty()) return L"";
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return L"";
    std::wstring wide(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wide[0], wlen);
    while (!wide.empty() && wide.back() == L'\0') wide.pop_back();
    return wide;
#else
    return std::wstring(utf8.begin(), utf8.end());
#endif
}

// UTF-8 string → filesystem::path (safe construction)
// On Windows, uses wstring to avoid ACP reinterpretation of UTF-8 bytes.
// On Linux/macOS, passes through directly (native UTF-8 filesystem).
inline std::filesystem::path utf8ToPath(const std::string& utf8) {
#ifdef _WIN32
    return std::filesystem::path(utf8ToWide(utf8));
#else
    return std::filesystem::path(utf8);
#endif
}

} // namespace vb6c3
