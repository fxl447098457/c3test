#pragma once
// coff_archive.hpp — COFF 归档(.lib)/目标文件(.obj) 符号表只读解析 (ai/024, 批次 T04, L2)
//
// 定位 (§五 L2): 不是优化, 是 L0/L1 算错时的唯一恢复路径 —— 库是 `_f@6` 而
// 声明算出 `_f@8` 时, 只有归一化匹配能告诉用户"库里其实有 _f@6"。
//
// 自研只读解析 (约 200 行), 不 shell 调 dumpbin —— `rc.exe` 探测那 50 行已经
// 证明每加一个外部工具都在放大 CI 与 MinGW 分支的麻烦 (§三末行)。
//
// SR4: 只读、版本化魔数校验、任何解析失败退化成"原样交链接器"
// (L2 是诊断不是前提) —— 解析失败只影响诊断的有无, 绝不产生编译错误。

#include <string>
#include <vector>

namespace vb6c3 {

struct CoffSymbols {
    bool ok = false;                 // 解析成功
    std::string error;               // !ok 时的人类可读原因 (ASCII)
    std::vector<std::string> names;  // 归档符号索引里的符号名, 原样 (含 `_`/`@N` 修饰)
};

// 解析一个 .lib (MSVC 归档, 取 linker member 的符号索引) 或 .obj (COFF 符号表,
// 只取"已定义的外部符号": storageClass=2 且 sectionNumber!=0)。
// 任何不符合预期布局的地方 → ok=false, error 给出原因; 绝不抛异常、绝不越界。
CoffSymbols readCoffSymbols(const std::string& path);

// 归一化: 忽略前导 `_`、忽略尾随 `@N` (stdcall 参数字节数)、大小写不敏感。
// 例: "_sl_add@8"→"sl_add"  "main"→"main"  "_Foo"→"foo"
// 仅用于匹配; 展示给用户的一律用原始名。
std::string normalizeCoffSymbol(const std::string& name);

} // namespace vb6c3
