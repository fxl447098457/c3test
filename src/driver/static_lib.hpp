#pragma once
// static_lib.hpp — 静态库引用识别与寻址 (ai/024, 批次 T01)
//
// === 怎么引用库 (E1: 一个取值，不是一条语句) ===
// `Declare` 的 `Lib` 串**以归档后缀结尾**即走静态链接期解析；否则动态路径行为一字不变:
//
//   Lib "sqlite3.obj"      → 静态 (MSVC 归档)
//   Lib "mylib.lib"        → 静态 (MSVC 归档)
//   Lib "libfoo.a"         → 静态 (MinGW 归档)
//   Lib "foo.o"            → 静态 (MinGW 归档)
//   Lib "user32"           → 动态 (不变)
//   Lib "msvbvm60.dll"     → 动态 (不变)
//
// 不新增语句: 没有 tB 的 `Import Library`, 没有 `As NAMESPACE`, 没有 `Link "dep"`。
// 命名空间本来就用 `Lib` 串表达 (与 VB6 已有心智一致)。
//
// === 怎么查找库目录 (E2: 基准 = 工程目录，绝不引入 cwd) ===
// 解析四级，先到先得:
//   1) 绝对路径            → 直接用
//   2) 含分隔符的相对路径  → 相对【工程目录】(vbp 所在目录 / 单文件模式=源文件目录)
//   3) 裸文件名            → 按搜索根顺序扫:
//        a. vbp 的 `LibDir=` 各项 (按书写顺序)
//        b. CLI 的 `--libdir` 各项 (按书写顺序)
//        c. `<工程目录>/Lib`  ← 默认根, 排在最后 (显式总赢过隐式)
//       三者中的**相对路径一律以工程目录为基准** —— 全process只有这一个基准,
//       连命令行给的也一样。理由: 构建结果不该随"你在哪个目录敲的 C3"而变
//       (E2 的"绝不引入 cwd 基准"; 与 ComLib=/ResFile= 同族)。绝对路径原样使用。
//   4) 全落空 → 编译期报错, 并**列出搜过的每一个根**
//
// 后端 × 格式必须先诊断拒收 (§六-1), 别丢给 link.exe 吐一堆 LNK1104。

#include <string>
#include <vector>

namespace vb6c3 {

// Lib 串的形态
enum class LibKind {
    Dynamic,      // 裸名 / .dll → 动态加载 (今天行为)
    StaticMsvc,   // .lib / .obj → MSVC 归档
    StaticMinGW,  // .a / .o     → MinGW 归档
};

// 目标后端。v1 只有 MSVC 走通 (T06 才做 MinGW .a)。
enum class LibBackend {
    Msvc,
    MinGW,
};

const char* libKindName(LibKind k);

// 去掉 VB6 字符串字面量的首尾引号 (parser 保留原样, 见 DeclareDecl::libName)
std::string libStripQuotes(const std::string& s);

// 取小写扩展名 (含前导点)。无扩展名 / 点号出现在目录段 → 返回空串。
// 例: "sqlite3.obj"→".obj"  "3rd.party\\mylib"→""  "user32"→""
std::string libLowerExt(const std::string& libStr);

// 按后缀判定形态。无后缀 → Dynamic (即"今天的行为")。
LibKind classifyLib(const std::string& libStr);

// 一次寻址的结果 (同时携带诊断素材, 避免调用方二次猜测)
struct LibResolution {
    bool ok = false;         // 找到文件
    bool rejected = false;   // 后端不匹配 → 拒收 (区别于"没找到")
    std::string absPath;     // ok 时的绝对路径 (UTF-8)
    std::string reason;      // rejected 时的人类可读原因 (ASCII)
    std::vector<std::string> searchedRoots;  // 未找到时: 搜过的根, 用于报错
};

// 搜索根表。构造后 setBaseDir() 必须给绝对路径。
class LibSearchPaths {
public:
    // 工程目录 (vbp 所在目录; 单文件模式 = 源文件所在目录)。必须已绝对化。
    void setBaseDir(std::string absBaseDir) { baseDir_ = std::move(absBaseDir); }
    const std::string& baseDir() const { return baseDir_; }

    // 按调用顺序追加一个搜索根。相对路径按 baseDir_ 解析后存入。
    // 空串忽略 (便于直接喂 vbp 值)。
    void addRoot(const std::string& dir);
    // 追加默认根 <baseDir_>/Lib (只加一次, 重复调用无副作用)
    void addDefaultRoot();

    const std::vector<std::string>& roots() const { return roots_; }
    bool empty() const { return roots_.empty(); }

    // 解析一个静态 Lib 串。Dynamic 形态调用本函数无意义 (返回 rejected=false, ok=false)。
    LibResolution resolve(const std::string& libStr, LibBackend backend) const;

private:
    std::string baseDir_;
    std::vector<std::string> roots_;
    bool defaultRootAdded_ = false;
};

} // namespace vb6c3
