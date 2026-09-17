#pragma once
// cgen_emitter.hpp - 代码输出辅助: 缩进管理、格式化
// 2026-09-17 从 src/backend/cgen.hpp 拆出（原第 21~60 行，逐行未改）。
// 使用者: cgen.hpp（CCodeGen 的 h_ / c_ 成员）、cgen_util.cpp（局部 entry）。
// 注意: 本文件自带 namespace vb6c3，须在全局作用域 include（伞头已如此处理）。

#include <string>
#include <sstream>
#include <vector>

namespace vb6c3 {

// ============================================================
// 代码输出辅助: 缩进管理、格式化
// ============================================================

class CodeEmitter {
public:
    // 输出一行（自动缩进+换行）
    void emitLine(const std::string& line = "");

    // 输出内容（不带换行）
    void emit(const std::string& text);

    // 输出空行
    void emitBlank();

    // Fix 090q: 注册"下一行输出前须先落地的声明行"(如 As Any ByRef 实参的
    // UDT 临时变量声明). 表达式拼接期无法在行中插语句, 故挂到下一个 emitLine
    // 之前按当前缩进输出 — 表达式文本与该行同次 flush, 声明恒先于引用.
    void addPending(const std::string& line) { pendingLines_.push_back(line); }

    // 缩进控制
    void indent() { indentLevel_++; }
    void dedent() { if (indentLevel_ > 0) indentLevel_--; }

    // 获取生成的代码
    std::string str() const { return oss_.str(); }

    // 清空
    void clear() { oss_.str(""); oss_.clear(); indentLevel_ = 0; }

private:
    std::ostringstream oss_;
    int indentLevel_ = 0;
    // Fix 090q: 待"下一行前"输出的声明行(见 addPending 注释)
    std::vector<std::string> pendingLines_;

    // Fix 090q: 在下一行输出前先落地 pendingLines_ (flushPending 由 emitLine
    // 调用; 在 flushPending 中把调用权交还 oss_ 直接写入, 避免递归)
    void flushPending();
};

} // namespace vb6c3
