#pragma once
// vb6c3 - 泛型登记表共享类型 (tB 扩展, G3)
// Parser(使用侧扁平化)、Driver(物化器)、SemanticAnalyzer(调用点推断) 三方
// 共用的扁平名编码与模板只读视图. 单独成头避免 layering 反向依赖.

#include "ast/ast.hpp"
#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <vector>

namespace vb6c3 {

// 扁名编码: Base + "_G" + <arity> + "_" + join(args, "__")  (纯下划线, 见
// parser_decl_var.cpp G2 结论). 实参自身可为嵌套扁名.
inline std::string genMakeFlat(const std::string& base,
                                       const std::vector<std::string>& args) {
    std::string out = base + "_G" + std::to_string(args.size()) + "_";
    for (size_t i = 0; i < args.size(); i++) {
        if (i) out += "__";
        out += args[i];
    }
    return out;
}

// 小写扁名: VB 名字大小写不敏感而 C 名敏感 — 全链路统一小写防"同一实例化
// 特化成两个类型" (Box_G1_Long vs box_g1_long 的 C2079 实测教训).
inline std::string genLower(const std::string& s) {
    std::string o = s;
    std::transform(o.begin(), o.end(), o.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return o;
}
inline std::string genMakeFlatLower(const std::string& base,
                                        const std::vector<std::string>& args) {
    return genLower(genMakeFlat(base, args));
}

// 模板只读视图 (analyzer 侧绑定推断用; key = lower(模板名))
struct GenTemplateView {
    Decl* decl = nullptr;                 // SubDecl/FunctionDecl/PropertyDecl
    std::vector<std::string> typeParams;  // 声明序类型参数名
};
using GenRegistry = std::unordered_map<std::string, GenTemplateView>;

} // namespace vb6c3
