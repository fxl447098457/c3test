#pragma once
// ai/023 S06: --pack/--unpack 分发外壳。
//
// 容器 = 无压缩顺序文件 + 尾部中央目录 (zip 同款思想, 但零压缩零依赖):
//   偏移 0     : "C3PKG\x01"                       6 字节 magic + 格式版本
//   顺序区     : 每个成员  u16 nameLen(LE) + name(UTF-8) + u64 size(LE) + 原始字节
//   中央目录   : 每个成员  u16 nameLen(LE) + name + u64 offset(LE) + u64 size(LE)
//   尾部 19 字节: u64 cdOffset(LE) + u32 cdCount(LE) + "C3PKEOF"
//
// 纪律 (023 D5): **编译器永不读容器** —— pack/unpack 只是目录形态 ⇄ 单文件
// 形态的外壳, 将来可整体替换 (换 zip / git 地址) 而编译器零改动。
//
// 成员集 = package.c3d 本身 + 清单 [Files] 逐项 (按清单顺序)。
// unpack 校验每个成员的 offset/size 与中央目录一致后才落盘。

#include <string>

namespace vb6c3 {

// --pack <目录>: 目录须含 package.c3d。产物 <目录>/<Name>-<Version>.c3pkg。
// 返回进程退出码 (0 = 成功), 文案一律 ASCII 到 stdout/stderr。
int runPackMode(const std::string& dirUtf8);

// --unpack <容器> [--output-dir <目录>]: 缺省释放到容器同目录下 <Name>-<Version>/。
int runUnpackMode(const std::string& pkgPathUtf8, const std::string& outDirUtf8);

} // namespace vb6c3
