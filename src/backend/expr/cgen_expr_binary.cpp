#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>
#include <cstdio>

namespace vb6c3 {

// --- cgen_expr_binary.cpp: 二元表达式求值 + BSTR 包装 + 二元运算符映射 ---

// M22: 将非BSTR表达式包装为BSTR (用于字符串连接 & 运算符)
void CCodeGen::visit(BinaryExpr& node) {
    emitExpr(*node.left);
    // COM标记解析: 如果左操作数是COM属性, 解析为值
    // P24-02: 算术运算默认Long解包
    if (isComMarker_) {
        if (node.op == BinaryOp::Concat) resolveComValue();
        else resolveComValue("Long");
    }
    std::string left = std::move(lastExpr_);
    emitExpr(*node.right);
    // COM标记解析: 如果右操作数是COM属性, 解析为值
    if (isComMarker_) {
        if (node.op == BinaryOp::Concat) resolveComValue();
        else resolveComValue("Long");
    }
    std::string right = std::move(lastExpr_);

    // 字符串连接运算: VB6 & → vb6_BSTR_Concat / vb6_BSTR_ConcatFree
    // 嵌套Concat时用ConcatFree释放中间临时BSTR，避免内存泄漏
    // M22: 非BSTR操作数自动包装为BSTR (int→vb6_CStr(vb6_CLng(x)), double→vb6_CStr(vb6_CDbl(x)))
    if (node.op == BinaryOp::Concat) {
        left = wrapToBSTR(left, *node.left);
        right = wrapToBSTR(right, *node.right);
        bool leftIsConcat = (left.find("vb6_BSTR_Concat") != std::string::npos);
        if (leftIsConcat) {
            lastExpr_ = "vb6_BSTR_ConcatFree(" + left + ", " + right + ")";
        } else {
            lastExpr_ = "vb6_BSTR_Concat(" + left + ", " + right + ")";
        }
        return;
    }

    // P14.1.1: VB6 + 运算符 — 两端String时等同&拼接
    // VB6允许 "a" + "b" 作为字符串连接，语义与 & 相同
    // Fix 092i: 右操作数是返回 BSTR 的内建调用 (Chr$/Mid/Trim/...) 时,
    // inferExprType 无法把整个 Add 推断为 String → 退化为算术 + :
    //   cToolsHttp 286 (源码 GB_UrlDecode + Chr$(d), GB_UrlDecode As String)
    //   → 生成 (GB_UrlDecode + vb6_Chr(d)) 触发 C2110 指针相加 +
    //     vb6_BSTR_Assign 参数太少; 而同处 288 的 GB_UrlDecode + c (c As String)
    //     正常走 Concat. 左操作数为 String 且右侧是 BSTR 返回调用时按拼接处理.
    bool addIsConcat092i = (inferExprType(node) == Vb6Type::String);
    if (!addIsConcat092i && inferExprType(*node.left) == Vb6Type::String) {
        static const char* bstrReturnCalls092i[] = {
            "vb6_Chr(", "vb6_ChrW(", "vb6_Mid(", "vb6_Left(", "vb6_Right(",
            "vb6_Trim(", "vb6_LTrim(", "vb6_RTrim(", "vb6_UCase(", "vb6_LCase(",
            "vb6_Replace(", "vb6_String(", "vb6_Space(", "vb6_StrConv(",
            "vb6_Format(", "vb6_VariantToString(", "vb6_BSTR_"};
        for (auto* bcp092i : bstrReturnCalls092i) {
            if (right.compare(0, strlen(bcp092i), bcp092i) == 0) {
                addIsConcat092i = true;
                break;
            }
        }
    }
    if (node.op == BinaryOp::Add && addIsConcat092i) {
        left = wrapToBSTR(left, *node.left);
        right = wrapToBSTR(right, *node.right);
        bool leftIsConcat = (left.find("vb6_BSTR_Concat") != std::string::npos);
        if (leftIsConcat) {
            lastExpr_ = "vb6_BSTR_ConcatFree(" + left + ", " + right + ")";
        } else {
            lastExpr_ = "vb6_BSTR_Concat(" + left + ", " + right + ")";
        }
        return;
    }

    // 幂运算: VB6 ^ → pow()
    if (node.op == BinaryOp::Pow) {
        lastExpr_ = "vb6_Pow(" + left + ", " + right + ")";
        return;
    }

    // 整除: VB6 \ → vb6_IntDiv (确保整数截断)
    // Fix 084o: 操作数为 Variant 时需显式转 Long (vb6_IntDiv 形参是 int32_t),
    // 否则 cToolsHttp 等文件中 "nAsc \ 2^6" (nAsc As Variant) 产生 C2440.
    if (node.op == BinaryOp::IntDiv) {
        lastExpr_ = "vb6_IntDiv(" + toLongIfVariant(left, node.left.get())
                  + ", " + toLongIfVariant(right, node.right.get()) + ")";
        return;
    }

    // 浮点除法: VB6 / → (double)left / (double)right
    if (node.op == BinaryOp::Div) {
        lastExpr_ = "((double)(" + left + ") / (double)(" + right + "))";
        return;
    }

    // Fix 039: VB6 Eqv → ~(a^b), cast to int32_t for non-integer operands
    // (double from vb6_Pow, pointer from BSTR/void*/SafeArray*)
    // Fix 039b: For Variant operands, use vb6_VariantToLong() instead of (int32_t)() cast.
    if (node.op == BinaryOp::Eqv) {
        auto castBitwise = [&](const std::string& cExpr, const Expr* astExpr) -> std::string {
            if (cExprIsVariant(cExpr)) return "vb6_VariantToLong(" + cExpr + ")";
            if (astExpr && astExpr->kind == ASTNodeKind::IdentifierExpr) {
                auto& ident = static_cast<IdentifierExpr&>(const_cast<Expr&>(*astExpr));
                std::string lower = ident.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if (knownVariantVars_.count(lower)) return "vb6_VariantToLong(" + cExpr + ")";
            }
            return "(int32_t)(" + cExpr + ")";
        };
        lastExpr_ = "(~(" + castBitwise(left, node.left.get()) + " ^ " + castBitwise(right, node.right.get()) + "))";
        return;
    }

    // Fix 039: VB6 Imp → (~a | b), cast to int32_t for non-integer operands
    if (node.op == BinaryOp::Imp) {
        auto castBitwise = [&](const std::string& cExpr, const Expr* astExpr) -> std::string {
            if (cExprIsVariant(cExpr)) return "vb6_VariantToLong(" + cExpr + ")";
            if (astExpr && astExpr->kind == ASTNodeKind::IdentifierExpr) {
                auto& ident = static_cast<IdentifierExpr&>(const_cast<Expr&>(*astExpr));
                std::string lower = ident.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if (knownVariantVars_.count(lower)) return "vb6_VariantToLong(" + cExpr + ")";
            }
            return "(int32_t)(" + cExpr + ")";
        };
        lastExpr_ = "((~" + castBitwise(left, node.left.get()) + ") | " + castBitwise(right, node.right.get()) + ")";
        return;
    }

    // Like运算: VB6 Like → vb6_Like
    if (node.op == BinaryOp::Like) {
        lastExpr_ = "vb6_Like(" + left + ", " + right + ")";
        return;
    }

    // P24-07: 字符串比较运算 — BSTR不能用==做指针比较, 需用vb6_StrCmp
    if (node.op == BinaryOp::Eq || node.op == BinaryOp::Neq ||
        node.op == BinaryOp::Lt || node.op == BinaryOp::Gt ||
        node.op == BinaryOp::Le || node.op == BinaryOp::Ge) {
        Vb6Type lt = inferExprType(*node.left);
        Vb6Type rt = inferExprType(*node.right);
        if (lt == Vb6Type::String || rt == Vb6Type::String) {
            // 两侧都需要是BSTR: 非BSTR端用wrapToBSTR转换
            if (lt != Vb6Type::String) left = wrapToBSTR(left, *node.left);
            if (rt != Vb6Type::String) right = wrapToBSTR(right, *node.right);
            std::string cmpOp;
            switch (node.op) {
                case BinaryOp::Eq:       cmpOp = "== 0"; break;
                case BinaryOp::Neq:    cmpOp = "!= 0"; break;
                case BinaryOp::Lt:        cmpOp = "< 0";  break;
                case BinaryOp::Gt:     cmpOp = "> 0";  break;
                case BinaryOp::Le:   cmpOp = "<= 0"; break;
                case BinaryOp::Ge:cmpOp = ">= 0"; break;
                default: cmpOp = "== 0"; break;
            }
            lastExpr_ = "(vb6_StrCmp(" + left + ", " + right + ") " + cmpOp + ")";
            return;
        }
    }

    // P24-Bug2: Variant比较运算 — vb6_VARIANT不能用C内置比较运算符
    if (node.op == BinaryOp::Eq || node.op == BinaryOp::Neq ||
        node.op == BinaryOp::Lt || node.op == BinaryOp::Gt ||
        node.op == BinaryOp::Le || node.op == BinaryOp::Ge) {
        Vb6Type lt = inferExprType(*node.left);
        Vb6Type rt = inferExprType(*node.right);
        // P25: inferExprType对后期绑定COM属性返回Variant, 但实际代码生成的是类型化getter
        // 修正类型以避免对rvalue取地址或选择错误的比较函数
        if (lt == Vb6Type::Variant) {
            if (left.find("vb6_ComGetIntProp") == 0 || left.find("vb6_ComVtableGetInt") == 0) lt = Vb6Type::Long;
            else if (left.find("vb6_ComGetDoubleProp") == 0 || left.find("vb6_ComVtableGetDouble") == 0) lt = Vb6Type::Double;
            else if (left.find("vb6_ComGetStringProp") == 0) lt = Vb6Type::String;
            else if (left.find("vb6_ComGetObjectProp") == 0) lt = Vb6Type::Object;
        }
        if (rt == Vb6Type::Variant) {
            if (right.find("vb6_ComGetIntProp") == 0 || right.find("vb6_ComVtableGetInt") == 0) rt = Vb6Type::Long;
            else if (right.find("vb6_ComGetDoubleProp") == 0 || right.find("vb6_ComVtableGetDouble") == 0) rt = Vb6Type::Double;
            else if (right.find("vb6_ComGetStringProp") == 0) rt = Vb6Type::String;
            else if (right.find("vb6_ComGetObjectProp") == 0) rt = Vb6Type::Object;
        }
        // Bug #2 fix: also check knownLongVars_/knownLongPtrVars_ for simple variable names
        // because inferExprType may return Variant for optional params or out-of-scope variables
        auto isSimpleIdent = [](const std::string& s) -> bool {
            if (s.empty()) return false;
            if (!(std::isalpha(static_cast<unsigned char>(s[0])) || s[0] == '_')) return false;
            for (char c : s) { if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false; }
            return true;
        };
        if (lt == Vb6Type::Variant && isSimpleIdent(left)) {
            std::string lower = left;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (knownLongPtrVars_.count(lower)) lt = Vb6Type::LongPtr;
            else if (knownLongVars_.count(lower)) lt = Vb6Type::Long;
        }
        if (rt == Vb6Type::Variant && isSimpleIdent(right)) {
            std::string lower = right;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (knownLongPtrVars_.count(lower)) rt = Vb6Type::LongPtr;
            else if (knownLongVars_.count(lower)) rt = Vb6Type::Long;
        }
        // Bug #2 fix: LongPtr (intptr_t) comparisons should use direct C operators
        // instead of VarCmpLong which treats the operand as vb6_VARIANT*
        if (lt == Vb6Type::LongPtr || rt == Vb6Type::LongPtr) {
            std::string op;
            switch (node.op) {
                case BinaryOp::Eq:  op = "=="; break;
                case BinaryOp::Neq: op = "!="; break;
                case BinaryOp::Lt:  op = "<";  break;
                case BinaryOp::Gt:  op = ">";  break;
                case BinaryOp::Le:  op = "<="; break;
                case BinaryOp::Ge:  op = ">="; break;
                default: op = "=="; break;
            }
            lastExpr_ = "(" + left + " " + op + " " + right + ")";
            return;
        }
        if (lt == Vb6Type::Variant || rt == Vb6Type::Variant) {
            // 确定比较函数后缀
            std::string cmpFn;
            switch (node.op) {
                case BinaryOp::Eq:  cmpFn = "Eq";  break;
                case BinaryOp::Neq: cmpFn = "Ne";  break;
                case BinaryOp::Lt:  cmpFn = "Lt";  break;
                case BinaryOp::Gt:  cmpFn = "Gt";  break;
                case BinaryOp::Le:  cmpFn = "Le";  break;
                case BinaryOp::Ge:  cmpFn = "Ge";  break;
                default: cmpFn = "Eq"; break;
            }
            // Variant vs NonVariant: 使用VarCmpLong快捷函数
            if (lt == Vb6Type::Variant && rt != Vb6Type::Variant) {
                Vb6Type rActual = rt;
                if (rActual == Vb6Type::Long || rActual == Vb6Type::Integer || rActual == Vb6Type::Boolean) {
                    // P25: left可能是VARIANT rvalue(vb6_VariantFromComResult), 需要临时变量
                    // Fix 084aa: 常量宏 (#define) 不可取址 → 视为非左值走临时变量
                    bool leftIsLvalue = !left.empty() && (std::isalpha(static_cast<unsigned char>(left[0])) || left[0] == '_') && !isConstIdent(left);
                    if (leftIsLvalue) { for (char c : left) { if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') { leftIsLvalue = false; break; } } }
                    if (leftIsLvalue) {
                        lastExpr_ = "(vb6_VarCmpLong" + cmpFn + "(&" + left + ", " + right + "))";
                    } else {
                        std::string tmp = "_vcmp_" + std::to_string(vcmpCounter_++);
                        // Fix 024: left 被 inferExprType 误判为 Variant, 但实际标量 (LenB/Asc/int 等).
                        // 用 vb6_VariantFromValue 在编译期按实类型选择 variant 构造函数, 消除 C2440.
                        c_.emitLine("vb6_VARIANT " + tmp + " = vb6_VariantFromValue(" + left + ");");
                        lastExpr_ = "(vb6_VarCmpLong" + cmpFn + "(&" + tmp + ", " + right + "))";
                    }
                    return;
                }
            }
            if (rt == Vb6Type::Variant && lt != Vb6Type::Variant) {
                Vb6Type lActual = lt;
                if (lActual == Vb6Type::Long || lActual == Vb6Type::Integer || lActual == Vb6Type::Boolean) {
                    // 反转比较方向: Long op Variant → Variant reverseOp Long
                    std::string revCmpFn;
                    switch (node.op) {
                        case BinaryOp::Lt: revCmpFn = "Gt"; break;
                        case BinaryOp::Gt: revCmpFn = "Lt"; break;
                        case BinaryOp::Le: revCmpFn = "Ge"; break;
                        case BinaryOp::Ge: revCmpFn = "Le"; break;
                        default: revCmpFn = cmpFn; break;  // Eq/Ne是对称的
                    }
                    // Fix 084aa: 常量宏不可取址 → 视为非左值
                    bool rightIsLvalue = !right.empty() && (std::isalpha(static_cast<unsigned char>(right[0])) || right[0] == '_') && !isConstIdent(right);
                    if (rightIsLvalue) { for (char c : right) { if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') { rightIsLvalue = false; break; } } }
                    if (rightIsLvalue) {
                        lastExpr_ = "(vb6_VarCmpLong" + revCmpFn + "(&" + right + ", " + left + "))";
                    } else {
                        std::string tmp = "_vcmp_" + std::to_string(vcmpCounter_++);
                        // Fix 024: right 被 inferExprType 误判为 Variant, 但实际标量. 用 FromValue 包装.
                        c_.emitLine("vb6_VARIANT " + tmp + " = vb6_VariantFromValue(" + right + ");");
                        lastExpr_ = "(vb6_VarCmpLong" + revCmpFn + "(&" + tmp + ", " + left + "))";
                    }
                    return;
                }
            }
            // Variant vs Variant: 使用VarCmp函数
            // P25: 检测rvalue, 非左值需要存临时变量
            // Fix 084aa: 常量宏 (#define) 不可取址 → 视为非左值走临时变量
            auto isLvalue = [this](const std::string& s) -> bool {
                if (s.empty()) return false;
                if (isConstIdent(s)) return false;
                if (!(std::isalpha(static_cast<unsigned char>(s[0])) || s[0] == '_')) return false;
                for (char c : s) { if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false; }
                return true;
            };
            std::string leftAddr = isLvalue(left) ? ("&" + left) : ([&]{ std::string tmp = "_vcmp_" + std::to_string(vcmpCounter_++); c_.emitLine("vb6_VARIANT " + tmp + " = vb6_VariantFromValue(" + left + ");"); return "&" + tmp; }());
            std::string rightAddr = isLvalue(right) ? ("&" + right) : ([&]{ std::string tmp = "_vcmp_" + std::to_string(vcmpCounter_++); c_.emitLine("vb6_VARIANT " + tmp + " = vb6_VariantFromValue(" + right + ");"); return "&" + tmp; }());
            lastExpr_ = "(vb6_VarCmp" + cmpFn + "(" + leftAddr + ", " + rightAddr + "))";
            return;
        }
    }

    std::string op = mapBinaryOp(node.op);

    // Fix 039: VB6 And/Or/Xor are bitwise operators that require integer operands.
    // Cast to int32_t to handle double (from vb6_Pow) and pointer (BSTR, void*, SafeArray*)
    // operands. VB6 semantics: And/Or/Xor convert operands to Long before bitwise op.
    // Fix 039b: For Variant operands (knownVariantVars_ or cExprIsVariant), use
    // vb6_VariantToLong() instead of (int32_t)() cast, since VARIANT can't be cast to int.
    if (node.op == BinaryOp::And || node.op == BinaryOp::Or || node.op == BinaryOp::Xor) {
        auto castBitwise = [&](const std::string& cExpr, const Expr* astExpr) -> std::string {
            if (cExprIsVariant(cExpr)) return "vb6_VariantToLong(" + cExpr + ")";
            if (astExpr && astExpr->kind == ASTNodeKind::IdentifierExpr) {
                auto& ident = static_cast<IdentifierExpr&>(const_cast<Expr&>(*astExpr));
                std::string lower = ident.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if (knownVariantVars_.count(lower)) return "vb6_VariantToLong(" + cExpr + ")";
            }
            return "(int32_t)(" + cExpr + ")";
        };
        lastExpr_ = "(" + castBitwise(left, node.left.get()) + " " + op + " " + castBitwise(right, node.right.get()) + ")";
        return;
    }

    // Fix 086: 算术运算 (+,-,*) 的 Variant 操作数提取 — VARIANT 结构体不能直接
    // 参与 C 算术运算 (C2088). 用 vb6_VariantToLong 提取 (覆盖 ByRef Variant
    // 参数解引用 (*maxLen) 与 vb6_VariantArrayGet 等返回 Variant 的表达式).
    if (node.op == BinaryOp::Add || node.op == BinaryOp::Sub || node.op == BinaryOp::Mul) {
        auto extractVar86 = [&](std::string& cExpr, const Expr* astExpr) {
            bool isVar86 = cExprIsVariant(cExpr);
            if (!isVar86 && astExpr && astExpr->kind == ASTNodeKind::IdentifierExpr) {
                auto& id86 = static_cast<IdentifierExpr&>(const_cast<Expr&>(*astExpr));
                if (knownVariantVars_.count(Symbol::toLower(id86.name))) isVar86 = true;
            }
            if (!isVar86 && cExpr.size() > 4 && cExpr[0] == '(' && cExpr[1] == '*'
                && cExpr.back() == ')' && cExpr.find('(') == std::string::npos) {
                // (*name) 解引用形式 — ByRef Variant 参数
                std::string inner86 = cExpr.substr(2, cExpr.size() - 3);
                std::string innerLower86 = Symbol::toLower(inner86);
                if (knownVariantVars_.count(innerLower86)) {
                    isVar86 = true;
                } else if (currentProc_) {
                    for (const auto& p : currentProc_->params) {
                        if (Symbol::toLower(p.name) == innerLower86
                            && (p.type == Vb6Type::Variant || p.type == Vb6Type::Empty)) {
                            isVar86 = true;
                            break;
                        }
                    }
                }
            }
            if (isVar86) cExpr = "vb6_VariantToLong(" + cExpr + ")";
        };
        extractVar86(left, node.left.get());
        extractVar86(right, node.right.get());
    }

    // VB6的And/Or/Not是逻辑运算也是位运算（取决于操作数类型）
    // 简化处理: 直接映射为C位运算, VB6语义兼容
    lastExpr_ = "(" + left + " " + op + " " + right + ")";
}

} // namespace vb6c3
