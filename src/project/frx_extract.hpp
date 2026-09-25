#pragma once
// ============================================================
// Fix 195: .frx 设计期取值 → VB 代码 导出器
// ============================================================
// 背景: VB6 的行式设计器格式存不下多行文本/二进制图片, 于是把它们甩进同名
// .frx, .frm 里只留字节偏移引用。结果是"工程能不能编对"取决于一个谁也看不懂
// 的二进制文件在不在 —— 对只把 C3 当编译器用的用户纯属负担。
//
// 出口: 把 .frx 里能用 VB 语句表达的设计期取值 (文本 / 列表 / 列表项数据)
// 导出成普通赋值语句, 搬进 .frm 的 Form_Load 之后即可删除 .frx, 编译不再依赖它。
// 编译期本来就已把 .frx 内容内联进生成的 C 代码 —— 生成的 exe 从不读 .frx,
// 所以这一步只影响"谁来承载设计期数据", 不影响运行期行为。

#include <string>
#include "project/frm_parser.hpp"

namespace vb6c3 {

// 导出结果: 一段可直接贴进 .frm 代码段的 VB6 源码 (UTF-8)。
// 无任何可导出内容时, content 里只有说明性注释。
struct FrxExportResult {
    std::string content;                        // VB6 源码 (UTF-8)
    int assignmentCount = 0;                    // 导出的赋值语句条数
    std::vector<std::string> skipped;           // 无法用 VB 语句表达的资源 (图片等)
    std::string error;                          // 非空表示失败
};

// frm: 已解析的 .frm; frxPath: 已定位的资源文件路径 (可为空)
FrxExportResult frxExtractToVb(const FrmFile& frm, const std::filesystem::path& frxPath);

} // namespace vb6c3
