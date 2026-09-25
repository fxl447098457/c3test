#pragma once
// 编码工具 — Windows路径UTF-8/UTF-16/ACP互转
// VB6编译器内部统一使用UTF-8字符串, 但Windows filesystem::path按ACP解释char*,
// 导致VBP解析器输出的UTF-8字符串被双重编码。本模块提供安全转换函数。

#include <string>
#include <filesystem>
#include <fstream>
#include <cstdio>

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

// wide string → UTF-8 string
// Windows: 用于把 GetCommandLineW() 得到的 Unicode 命令行参数转成内部统一的 UTF-8.
inline std::string wideToUtf8(const std::wstring& wide) {
#ifdef _WIN32
    if (wide.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string utf8(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &utf8[0], len, nullptr, nullptr);
    while (!utf8.empty() && utf8.back() == '\0') utf8.pop_back();
    return utf8;
#else
    return std::string(wide.begin(), wide.end());
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

// ============================================================
// Fix 196: 文件流的 UTF-8 安全包装
// ------------------------------------------------------------
// std::ofstream/std::ifstream/std::fopen 的**窄字符串**重载在 Windows 上把 char*
// 当**系统 ANSI 代码页 (936/GBK)** 解释, 而 C3 内部路径一律 UTF-8。中文/韩文/俄文
// 目录下这些调用会打开乱码路径 —— 轻则「无法写入文件」直接失败, 重则写到一个
// **同名的乱码文件**里, 静默丢产物。下面包装统一经 utf8ToPath() 走宽字符重载。
//
// 实测 (非 ASCII %TEMP%):
//   std::ofstream ofs("C:\\Windows\\Temp\\新建temp\\C3C\\..\\FrxData.h")  -> 失败
//   ofstreamUtf8(同一字符串)                                            -> 成功
//
// ASCII 路径下与原行为逐字节一致 (多一次 UTF-8→UTF-16 转换), 无副作用。
// ============================================================

inline std::ofstream ofstreamUtf8(const std::string& utf8Path,
                                  std::ios::openmode mode = std::ios::out) {
    return std::ofstream(utf8ToPath(utf8Path), mode);
}

inline std::ifstream ifstreamUtf8(const std::string& utf8Path,
                                  std::ios::openmode mode = std::ios::in) {
    return std::ifstream(utf8ToPath(utf8Path), mode);
}

inline std::FILE* fopenUtf8(const std::string& utf8Path, const char* mode) {
#ifdef _WIN32
    return _wfopen(utf8ToPath(utf8Path).c_str(), utf8ToWide(mode).c_str());
#else
    return std::fopen(utf8Path.c_str(), mode);
#endif
}

// 窄串版存在性判断 (收 UTF-8 路径; 失败一律当"不存在", 不抛异常)
inline bool existsUtf8(const std::string& utf8Path) {
    std::error_code ec;
    return std::filesystem::exists(utf8ToPath(utf8Path), ec) && !ec;
}

} // namespace vb6c3
