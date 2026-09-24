#pragma once
// ai/023 S01: package.c3d 清单解析器 (包/工程引用)
//
// 清单格式 (023 五节草案): 小节式 Key=Value, 与 vbp 同族, 纯 ASCII。
//   [C3Package]  Format=1 / Name= / Version= / Desc=
//   [Files]      相对路径=<sha1>,<size>
//   [Export]     Module=Name / Class=Name / Friend=False
//   [Needs]      需求布尔位 (如 Comctl6=True)
//
// 纪律 (023 五节):
//   - Format=1; 未知段/未知键一律报错, 不许静默忽略 (格式演进的唯一保险)
//   - 所有路径相对清单自身所在目录 (唯一基准, 与 resolvePath() 同族)
//   - 纯 ASCII; 见到非 ASCII 字节即报错
//
// S01 范围: 只解析 + 报错, 不加载源码 (那是 S02), 不做哈希校验 (S05)。

#include <filesystem>
#include <string>
#include <vector>

namespace vb6c3 {

struct PackageManifest {
    int format = 0;
    std::string name;
    std::string version;
    std::string desc;

    struct FileEntry {
        std::string path;   // 相对清单所在目录
        std::string sha1;   // 40 hex; S05 才比对
        std::string size;   // 十进制字节数 (原样保留)
    };
    std::vector<FileEntry> files;

    struct ExportEntry {
        std::string kind;   // "Module" 或 "Class"
        std::string name;
    };
    std::vector<ExportEntry> exports;

    // [Export] Friend=True 时, 包内 Friend 成员对宿主也可见 (默认 False:
    // Friend 只在包内可见, 023 七节导出边界)
    bool friendVisible = false;

    // 需求布尔位 (023 八-2: 包只允许声明需求, 不允许携带产物)
    std::vector<std::pair<std::string, std::string>> needs;

    // 清单本身的硬错误 (未知段/未知键/格式错/非 ASCII/缺必填)
    std::vector<std::string> errors;
    bool ok() const { return errors.empty(); }
};

// 解析 package.c3d 内容。永不抛异常; 错误进 manifest.errors (ASCII 文案)。
PackageManifest parsePackageManifest(const std::string& content);

// 包名/版本合法性 (023 六节寻址的唯一防逃逸闸门):
//   Name:   [A-Za-z0-9_]+
//   Version:[A-Za-z0-9._\-]+ 且不以 '.' 开头
// 两者都禁止路径分隔符与 '..' —— 目录名由 root/name-version 拼接而来,
// 名字不合法即视为"解析结果跳出搜索根"(硬校验第一条)。
bool isValidPackageName(const std::string& name);
bool isValidPackageVersion(const std::string& version);

class Diagnostics;
struct VbpProject;

// S02: 解析成功后的一个包 (目录 + 清单)。driver 拿它做源码级加载。
struct ResolvedPackage {
    std::string name;                 // 包名 (vbp 引用侧, 与清单 Name 已核对一致)
    std::string version;
    std::filesystem::path dir;        // 包根目录 (含 package.c3d)
    PackageManifest manifest;
};

// 从 VB6 源码文本提取 `Attribute VB_Name = "Name"` 的模块名 (找不到返回空)。
// 包源码的模块名 = 导出边界 (S03) 与模块名冲突检查 (S02) 的依据。
std::string extractVbModuleName(const std::string& content);

// ai/023 S03: 模块级过程声明轻量扫描 (仅用于包导出屏蔽诊断, 不参与发码)。
// 识别 [Public|Private|Friend|Static] (Sub|Function|Property Get|Property Let|Property Set)
// Name(...) 行, 返回 (访问级别小写, 过程名)。行导向 + 忽略注释; 不处理续行 —
// 包内过程名给诊断用, 漏扫的后果只是诊断降级为 3001 警告, 不影响正确性。
std::vector<std::pair<std::string, std::string>> extractProcDecls(const std::string& content);

// ai/023 S01: 包引用解析 + 三条硬校验 (S02 起附带源码级加载所需的解析结果)。
//   1. 解析结果跳出搜索根 → 报错 (以"包名/版本字符集"为防逃逸闸门, 见上)
//   2. 清单缺文件 → 警告 (D7 校验不拒收; 哈希比对是 S05)
//   3. 包名与宿主工程名冲突 → 报错 (模块名冲突在 S02 加载层查, 见下)
// cliRoots = CLI --package-root 各项 (相对路径以工程目录为基准, E2)。
// 缺省根 <vbp目录>/packages 排最后: 显式总赢过隐式 (与 024 E2 同序约定)。
// resolved 非空时 (S02): 接收解析成功的包; driver 据此把包内 .bas/.cls 追加进编译集。
void checkPackages(const VbpProject& project,
                   const std::vector<std::string>& cliRoots,
                   Diagnostics& diag,
                   std::vector<ResolvedPackage>* resolved = nullptr);

} // namespace vb6c3
