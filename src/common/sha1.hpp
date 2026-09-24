#pragma once
// ai/023 S05: 纯自研 SHA-1 (清单 [Files] 逐文件哈希比对用)。
// 编译器零第三方依赖 (023 五节纪律), SHA-1 只做完整性校验、不做安全用途,
// 碰撞风险不影响"检测手改包"这一目标。
//
// 用法: sha1HexOfFile(pathUtf8) → 40 位小写 hex; 读不了文件返回空串。

#include <cstdint>
#include <string>

namespace vb6c3 {

// 对一段内存做 SHA-1, 返回 40 位小写 hex。
std::string sha1Hex(const uint8_t* data, size_t len);

// 读整个文件 (二进制) 并计算 SHA-1。失败返回空串。
std::string sha1HexOfFile(const std::string& pathUtf8);

} // namespace vb6c3
