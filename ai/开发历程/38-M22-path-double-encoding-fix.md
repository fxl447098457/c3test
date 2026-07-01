# M22: filesystem::path双重编码全面修复

> 日期: 2026-07-01
> Commit: ed91e05
> 回归: 70/74 (4个前M22遗留UDT/Implements失败)

## 背景

上一轮(62e0df5)修复了Issue B中`project.exeName`的双重编码问题，但审计发现
`filesystem::path(const char*)`在Windows上按ACP解释字节的隐患遍布整个代码库。

## 根因分析

Windows上`std::filesystem::path(const char*)`构造函数通过CRT将`char*`转为内部UTF-16，
CRT使用当前locale/ACP(codepage)解释字节。VBP解析器输出的字符串是UTF-8编码，
传入`path()`后被当作ACP重新解码——UTF-8字节"工程"(E5 B7 A5 E7 A8 8B)被GBK解码为"宸ョ▼"。

**影响范围**: 所有从VBP解析器输出的字符串（exeName、outputPath、sources[].filePath、
moduleName等）直接传入`filesystem::path()`、`filesystem::absolute()`、`filesystem::exists()`
等函数时，都可能产生双重编码。调用`.string()`则从错误编码的path对象中提取ACP字符串，
造成二级损伤。

## 修复方案

### 1. common/encoding.hpp — 统一编码工具

将`pathToUtf8`、`utf8ToWide`、`utf8ToPath`三个函数从`driver.cpp`的`static`局部函数
提升为`inline`全局函数，放在`common/encoding.hpp`中供所有模块共享：

```cpp
inline std::string pathToUtf8(const std::filesystem::path& p);      // path → UTF-8
inline std::wstring utf8ToWide(const std::string& utf8);            // UTF-8 → wstring
inline std::filesystem::path utf8ToPath(const std::string& utf8);   // UTF-8 → path (安全构造)
```

`utf8ToPath()`是核心：Windows上先UTF-8→UTF-16(MultiByteToWideChar)→filesystem::path(wstring)，
Linux/macOS直接path(utf8)（原生UTF-8文件系统）。

### 2. driver.cpp — 全面替换

- 删除本地`static pathToUtf8`和`utf8ToWide`，改用`#include "common/encoding.hpp"`
- `filesystem::path(UTF-8字符串)` → `utf8ToPath(UTF-8字符串)`
- `path_obj.string()` → `pathToUtf8(path_obj)`
- `filesystem::absolute(UTF-8字符串)` → `filesystem::absolute(utf8ToPath(UTF-8字符串))`
- 同理exists/create_directories等自由函数

### 3. vbp_parser.hpp resolvePath()

VBP源文件路径(`entry.filePath`)是UTF-8，`resolvePath()`中`path(relativePath)`和
`parent_path() / relativePath`都产生双重编码。改为`utf8ToPath(relativePath)`。

### 4. frm_parser.cpp

`absolute(frmFilePath)` → `absolute(utf8ToPath(frmFilePath))`。

### 5. msvc_driver.cpp

`path(options.outputFile)` → `utf8ToPath(options.outputFile)`，
`.parent_path().string()` → `pathToUtf8(.parent_path())`。

## 审计统计

| 风险等级 | 数量 | 说明 |
|---------|------|------|
| 高 | 14个 | 直接path(UTF-8字符串)构造 |
| 高 | 3个 | .string()提取ACP导致二级乱码 |
| 中 | 5个 | 混合路径或条件触发 |
| 低 | 5个 | 纯ASCII路径不受影响 |

## 修改文件

| 文件 | 修改内容 |
|------|---------|
| src/common/encoding.hpp | 新建：pathToUtf8/utf8ToWide/utf8ToPath inline函数 |
| src/driver/driver.cpp | 删除static函数，全面用utf8ToPath/pathToUtf8替换 |
| src/project/vbp_parser.hpp | resolvePath()用utf8ToPath |
| src/project/frm_parser.cpp | absolute()用utf8ToPath |
| src/backend/msvc_driver.cpp | path()/.string()用utf8ToPath/pathToUtf8 |

## 验证

- 增量构建：零错误
- 两个demo(form + realVBP)：编译成功，EXE中文文件名正确
- 回归测试：70/74通过

## 待观察

- `argv`传入的路径是ACP编码，但经过`pathToUtf8(absolute(argv_path))`后转为UTF-8，
  后续`utf8ToPath()`能正确还原（UTF-8→UTF-16→path），不会产生问题
- Linux/macOS上`utf8ToPath`直接传递UTF-8字符串，与原生行为一致
