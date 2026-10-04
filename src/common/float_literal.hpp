#pragma once
// 浮点字面量的形状 —— 全编译器只有一个出口 (账 #188)
//
// 为什么要单开一个头: C 里 `12f` 是**非法 token** (MSVC: "bad suffix on number", C2059),
// 整数值的 Single 必须写成 `12.0f`。这条规则以前在三个地方各抄一遍, 其中「设计期字体块」那处
// 抄漏了补小数点那一步 (`snprintf("%.4g")` + "f") ⇒ 凡设计期字号是整数 (12 / 14 / 16…) 的工程
// 直接编不过。VB6 的默认字号 8.25 恰好带小数点, 所以这条一直在门禁里隐身。
//
// 用法: 要 `f` 后缀就调 floatSingleLiteral(); 不要后缀就调 floatingLiteralText()。
// 两个函数都保证「后缀前一定有 '.' 或指数」—— 别在调用点再自己拼 "f"。

#include <iomanip>
#include <locale>
#include <sstream>
#include <string>

namespace vb6c3 {

// 定点文本, 经典 locale (小数点必须是 '.'), 保证带 '.' 或指数。
inline std::string floatingLiteralText(double value, int precision) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(precision) << value;
    std::string text = out.str();
    if (text.find_first_of(".eE") == std::string::npos) text += ".0";
    return text;
}

// VB Single 的 C 字面量。默认 9 位 = float 的 max_digits10 (能原样往返 Single)。
inline std::string floatSingleLiteral(double value, int precision = 9) {
    return floatingLiteralText(value, precision) + "f";
}

} // namespace vb6c3
