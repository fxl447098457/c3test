#include "semantics/semantic_analyzer.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <tuple>
#include <initializer_list>
#include "semantics/semantic_analyzer_internal.h"

namespace vb6c3 {

// --- semantic_analyzer_util.cpp: 辅助方法 (赋值/实参检查、引用标记、未引用符号) + Optional 参数默认值求值 ---


// ============================================================
// 辅助方法
// ============================================================

void SemanticAnalyzer::checkAssignment(Vb6Type targetType, Vb6Type valueType,
                                        const SourceLocation& loc,
                                        const std::string& context) {
    if (!TypeSystem::canImplicitConvert(valueType, targetType)) {
        diag_.warn(DiagnosticID::SemTypeMismatch, loc,
            context + ": 无法将 " + TypeSystem::typeToString(valueType) +
            " 隐式转换为 " + TypeSystem::typeToString(targetType));
    }
}

void SemanticAnalyzer::checkCallArgs(Symbol* procSym, IndexOrCallExpr& callNode) {
    if (!procSym) return;

    // 内置函数未注册参数信息, 跳过参数校验
    if (procSym->isBuiltin) return;

    size_t expectedParams = 0;
    size_t optionalParams = 0;
    bool hasParamArray = false;

    for (auto& p : procSym->params) {
        if (p.isParamArray) {
            hasParamArray = true;
            continue;
        }
        if (p.isOptional) {
            optionalParams++;
        }
        expectedParams++;
    }

    size_t providedArgs = callNode.positional.size() + callNode.named.size();

    if (hasParamArray) {
        // ParamArray: 至少需要 required 个参数
        size_t required = expectedParams - optionalParams;
        if (providedArgs < required) {
            diag_.error(DiagnosticID::SemWrongNumberOfArguments, callNode.loc,
                "参数数量错误: '" + procSym->name + "' 至少需要 " +
                std::to_string(required) + " 个参数, 实际提供 " +
                std::to_string(providedArgs));
        }
    } else {
        size_t required = expectedParams - optionalParams;
        if (providedArgs < required || providedArgs > expectedParams) {
            diag_.error(DiagnosticID::SemWrongNumberOfArguments, callNode.loc,
                "参数数量错误: '" + procSym->name + "' 需要 " +
                std::to_string(required) + "-" + std::to_string(expectedParams) +
                " 个参数, 实际提供 " + std::to_string(providedArgs));
        }
    }

    // 命名参数检查
    for (auto& namedArg : callNode.named) {
        bool found = false;
        std::string lowerName = Symbol::toLower(namedArg.name);
        for (auto& p : procSym->params) {
            if (Symbol::toLower(p.name) == lowerName) {
                found = true;
                break;
            }
        }
        if (!found) {
            diag_.error(DiagnosticID::SemNamedArgNotFound, callNode.loc,
                "命名参数未找到: '" + namedArg.name + "' (过程 '" + procSym->name + "')");
        }
    }

    // 重复命名参数检查
    std::vector<std::string> seenNamedArgs;
    for (auto& namedArg : callNode.named) {
        std::string lowerName = Symbol::toLower(namedArg.name);
        for (auto& seen : seenNamedArgs) {
            if (Symbol::toLower(seen) == lowerName) {
                diag_.error(DiagnosticID::SemDuplicateNamedArg, callNode.loc,
                    "重复的命名参数: '" + namedArg.name + "'");
                break;
            }
        }
        seenNamedArgs.push_back(namedArg.name);
    }
}

void SemanticAnalyzer::markReferenced(const std::string& name) {
    auto* sym = symTab_.lookup(name);
    if (sym) {
        sym->isReferenced = true;
    }
}

std::string SemanticAnalyzer::makeInternalName(const std::string& prefix,
                                                const std::string& name) {
    return prefix + "_" + name;
}

void SemanticAnalyzer::checkUnreferencedSymbols() {
    // 遍历模块级符号, 检查未引用的变量/常量
    auto* modScope = symTab_.moduleScope();
    for (auto& [key, sym] : modScope->symbols()) {
        if (!sym->isReferenced &&
            (sym->kind == SymbolKind::Variable || sym->kind == SymbolKind::Constant)) {
            // 仅在 verbose 模式下警告, VB6默认允许未使用变量
            diag_.warn(DiagnosticID::SemUndeclaredIdentifier, sym->location,
                "未使用的" + std::string(sym->kindName()) + ": '" + sym->name + "'");
        }
    }
}


// ============================================================
// P14.1.4: Optional参数默认值求值
// ============================================================
std::string SemanticAnalyzer::evalOptionalDefault(ASTNode* defaultValue, Vb6Type paramType) {
    // 无显式默认值 -> 返回空字符串, cgen将使用类型零值
    if (!defaultValue) return "";
    
    // 将AST字面量表达式转换为C表达式字符串
    auto* lit = dynamic_cast<LiteralExpr*>(defaultValue);
    if (lit) {
        switch (lit->literalKind) {
            case LiteralKind::Integer:
            case LiteralKind::Long:
                return lit->rawText;  // "10", "-1" 等
            case LiteralKind::LongPtr: {
                // Fix 082: rawText 带 VB 后缀 ("&H80000000^"), 原样返回会写进生成 C →
                // C2059. 与 cgen_expr.cpp 的 LongPtr 分支同规则: LongPtr 是平台相关宽度
                // (32/64 位机分别 4/8 字节) → intptr_t; 十六进制无符号形式避免十进制
                // INT64_MIN 字面量溢出.
                std::string t = lit->rawText;
                if (!t.empty() && t.back() == '^') t.pop_back();
                if (t.size() >= 2 && t[0] == '&' && (t[1] == 'H' || t[1] == 'h')) {
                    return "((intptr_t)0x" + t.substr(2) + "ULL)";
                }
                if (t.size() >= 2 && t[0] == '&' && (t[1] == 'O' || t[1] == 'o')) {
                    return "((intptr_t)0" + t.substr(2) + "ULL)";
                }
                if (t.size() >= 2 && t[0] == '&' && (t[1] == 'B' || t[1] == 'b')) {
                    return "((intptr_t)0b" + t.substr(2) + "ULL)";
                }
                return "((intptr_t)" + t + "ULL)";
            }
            case LiteralKind::Single: {
                // Fix 133z: 单精度默认值 `1!` → C 浮点字面量 `1.0000000f`.
                // 原样返回 rawText ("1!") 会写进生成 C → 语法错误
                // (czUI.ctl: `Optional ByVal penWidth As Single = 1!` →
                // `if (!_has_penWidth) penWidth = 1!;` → C2059).
                float fv = lit->floatValue;
                std::ostringstream oss133z;
                char buf133z[64];
                snprintf(buf133z, sizeof(buf133z), "%.9g", (double)fv);
                oss133z << buf133z;
                if (strchr(buf133z, '.') == nullptr && strchr(buf133z, 'e') == nullptr
                    && strchr(buf133z, 'E') == nullptr)
                    oss133z << ".0";
                oss133z << "f";
                return oss133z.str();
            }
            case LiteralKind::Double: {
                // Fix 133z: parser 无 Single 字面量kind, `1!`/`0!` 以 Double 存储,
                // rawText 带 VB 后缀 ("1!"). 原样返回会写进生成 C → C2059
                // (czUI.ctl: `Optional ByVal penWidth As Single = 1!` →
                // `if (!_has_penWidth) penWidth = 1!;`). 剥掉 `!`/`#` 尾缀并
                // 保证是合法 C 浮点字面量 (整数形态补 `.0`).
                std::string dt = lit->rawText;
                while (!dt.empty() && (dt.back() == '!' || dt.back() == '#'))
                    dt.pop_back();
                if (dt.find('.') == std::string::npos
                    && dt.find('e') == std::string::npos
                    && dt.find('E') == std::string::npos)
                    dt += ".0";
                return dt;
            }
            case LiteralKind::String:
                // VB6 "hello" -> C vb6_BSTR_FromStr(L"hello")
                {
                    std::string sInner = lit->rawText;
                    if (sInner.size() >= 2 && sInner.front() == '"' && sInner.back() == '"')
                        sInner = sInner.substr(1, sInner.size() - 2);
                    return "vb6_BSTR_FromStr(L\"" + sInner + "\")";
                }
            case LiteralKind::Boolean:
                // VB6 True = -1, False = 0
                return (lit->rawText == "True" || lit->rawText == "-1") ? "-1" : "0";
            case LiteralKind::Nothing:
                return "NULL";
            case LiteralKind::Empty:
                return "vb6_VariantEmpty()";
            case LiteralKind::Null:
                return "vb6_VariantNull()";
            default:
                break;
        }
    }
    
    // UnaryExpr: 递归求值操作数, 加前缀 (P20-20)
    auto* unary = dynamic_cast<UnaryExpr*>(defaultValue);
    if (unary) {
        std::string inner = evalOptionalDefault(unary->operand.get(), paramType);
        if (inner.empty()) return "";
        switch (unary->op) {
            case UnaryOp::Negate:
                // 数值型: "(-1)" "(-3.14)"
                if (inner.find("vb6_") == 0) return "";  // 非数值C表达式, 暂不处理
                return "(-" + inner + ")";
            case UnaryOp::Not:
                // Not表达式: 暂不常见做默认值, 返回空
                return "";
        }
    }

    // IdentifierExpr: 解析VB6内建常量 (P20-20)
    auto* ident = dynamic_cast<IdentifierExpr*>(defaultValue);
    if (ident) {
        const std::string& n = ident->name;
        // 转小写比较
        std::string nLower = n;
        for (auto& c : nLower) c = (char)tolower((unsigned char)c);

        // 字符串常量
        if (nLower == "vbcrlf" || nLower == "vbnewline")
            return "vb6_BSTR_FromStr(L\"\\r\\n\")";
        if (nLower == "vbcr")
            return "vb6_BSTR_FromStr(L\"\\r\")";
        if (nLower == "vblf")
            return "vb6_BSTR_FromStr(L\"\\n\")";
        if (nLower == "vbtab")
            return "vb6_BSTR_FromStr(L\"\\t\")";
        if (nLower == "vbnullstring")
            return "vb6_BSTR_FromStr(L\"\")";
        if (nLower == "vbback")
            return "vb6_BSTR_FromStr(L\"\\b\")";
        if (nLower == "vbformfeed")
            return "vb6_BSTR_FromStr(L\"\\f\")";
        if (nLower == "vbverticaltab")
            return "vb6_BSTR_FromStr(L\"\\v\")";

        // 数值/枚举常量
        if (nLower == "vbtrue")  return "-1";
        if (nLower == "vbfalse") return "0";
        if (nLower == "vbyes")   return "6";
        if (nLower == "vbno")    return "7";
        if (nLower == "vbok")    return "1";
        if (nLower == "vbcancel") return "2";
        if (nLower == "vbabort")  return "3";
        if (nLower == "vbretry")  return "4";
        if (nLower == "vbignore") return "5";

        // 特殊值
        if (nLower == "vbempty")    return "vb6_VariantEmpty()";
        if (nLower == "vbnull")     return "vb6_VariantNull()";
        if (nLower == "vbnothing")  return "NULL";

        // 项目级Const/EnumMember: 通过符号表查找Constant或EnumMember符号 (P20-20)
        // Fix 081a: 也查找 EnumMember，使 Optional ByVal Ecl As QRCodegenEcc = QRCodegenEcc_LOW
        // 等枚举常量默认值能正确解析为整数值
        {
            Symbol* sym = symTab_.lookup(n);
            if (sym && sym->hasConstValue) {
                if (sym->kind == SymbolKind::Constant || sym->kind == SymbolKind::EnumMember) {
                    switch (sym->constType) {
                    case Vb6Type::Long:
                    case Vb6Type::Integer: { return std::to_string(sym->constIntValue); }
                    case Vb6Type::Single:
                    case Vb6Type::Double: { return std::to_string(sym->constFloatValue); }
                    case Vb6Type::String: {
                        // C-escape constStringValue before embedding in C string literal
                        std::string cEsc;
                        cEsc.reserve(sym->constStringValue.size() + 16);
                        for (char ec : sym->constStringValue) {
                            switch (ec) {
                                case '\\': cEsc += "\\\\"; break;
                                case '"':  cEsc += "\\\""; break;
                                case '\n': cEsc += "\\n"; break;
                                case '\r': cEsc += "\\r"; break;
                                case '\t': cEsc += "\\t"; break;
                                default:   cEsc += ec; break;
                            }
                        }
                        return std::string("vb6_BSTR_FromStr(L\"") + cEsc + "\")";
                    }
                    case Vb6Type::Boolean: { return sym->constBoolValue ? "-1" : "0"; }
                    default: break;
                }
            }
        }
        }

        // 未知标识符, 暂不处理
        return "";
    }
    
    // 其他非字面量表达式 -> 暂不支持, 返回空让cgen用类型零值
    return "";
}

} // namespace vb6c3
