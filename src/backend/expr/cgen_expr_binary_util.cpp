#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>
#include <cstdio>

namespace vb6c3 {

// --- cgen_expr_binary_util.cpp: BSTR 包装 + 二元运算符映射 ---
// 由 src/backend/expr/cgen_expr_binary.cpp 拆出（2026-09-17），纯搬移、零行为改动。

std::string CCodeGen::wrapToBSTR(const std::string& expr, Expr& node) {
    // P24-01: 后期绑定COM调用返回VARIANT*, 需解包为BSTR(必须在vb6_BSTR检查之前)
    // P24-02: Variant数组索引返回vb6_VARIANT, 需转BSTR
    if (expr.find("vb6_VariantArrayGet") == 0) {
        return "vb6_VariantToString(" + expr + ")";
    }
    if (expr.find("vb6_ComCall(") == 0) {
        return "vb6_VariantToString(vb6_VariantFromComResult(" + expr + "))";
    }
    // Fix 092w: 链式默认属性访问结果 (vb6_VariantFromComResult(vb6_ComCall(...))) 是
    // vb6_VARIANT — BSTR 上下文需提取为字符串, 否则 C2440 (Demo_Database 506)。
    // 必须放在 vb6_BSTR 子串检查之前 (实参中可能含 vb6_BSTR_FromStr)。
    if (expr.find("vb6_VariantFromComResult(") == 0) {
        return "vb6_VariantToString(" + expr + ")";
    }
    // P24-01: COM属性返回int/double, 需转BSTR
    if (expr.find("vb6_ComGetIntProp(") != std::string::npos ||
        expr.find("vb6_ComVtableGetInt(") != std::string::npos) {
        return "vb6_CStrLong(" + expr + ")";
    }
    if (expr.find("vb6_ComGetDoubleProp(") != std::string::npos ||
        expr.find("vb6_ComVtableGetDouble(") != std::string::npos) {
        return "vb6_CStrDbl(" + expr + ")";
    }
    if (expr.find("vb6_BSTR") != std::string::npos) return expr;
    if (expr.find("vb6_CStr") != std::string::npos) return expr;
    if (expr.find("vb6_GetControlText") != std::string::npos) return expr;
    if (expr.find("vb6_GetControlCaption") != std::string::npos) return expr;
    // Fix 049a: VB6_SA_AT(BSTR, arr, idx) returns BSTR — no wrapping needed
    if (expr.find("VB6_SA_AT(BSTR,") != std::string::npos) return expr;
    // vb6_Now() returns double (Date), NOT BSTR — removed early return
    // vb6_Now will fall through to inferExprType → Vb6Type::Date → vb6_CStrDate()
    if (expr.find("vb6_Left") != std::string::npos) return expr;
    if (expr.find("vb6_Right") != std::string::npos) return expr;
    if (expr.find("vb6_Mid") != std::string::npos) return expr;
    if (expr.find("vb6_Format") != std::string::npos) return expr;
    if (expr.find("vb6_Str") != std::string::npos) return expr;
    if (expr.find("vb6_Chr") != std::string::npos) return expr;
    if (expr.find("vb6_Replace") != std::string::npos) return expr;
    if (expr.find("vb6_Space") != std::string::npos) return expr;
    if (expr.find("vb6_InputBox") != std::string::npos) return expr;
    if (expr.find("vb6_Dir") != std::string::npos) return expr;
    if (expr.find("vb6_Command") != std::string::npos) return expr;
    if (expr.find("vb6_Environ") != std::string::npos) return expr;
    if (expr.find("vb6_CurDir") != std::string::npos) return expr;
    if (expr.find("vb6_App_Path") != std::string::npos) return expr;
    if (expr.find("vb6_App_EXEName") != std::string::npos) return expr;
    if (expr.find("vb6_App_HelpFile") != std::string::npos) return expr;
    // BSTR变量: 已知BSTR变量或者vb6_Module1_xxx 格式的BSTR
    // 简化: 如果以vb6_开头且非数值函数, 假定是BSTR
    if (expr.find("vb6_") == 0) {
        // 数值/日期函数需要包装为BSTR
        // Date类: vb6_Now/vb6_Date/vb6_Time 返回double(Date)
        if (expr.find("vb6_Now") != std::string::npos ||
            expr.find("vb6_Date") != std::string::npos ||
            expr.find("vb6_Time") != std::string::npos) {
            return "vb6_CStrDate(" + expr + ")";
        }
        // 数值函数: 返回int/double等
        if (expr.find("vb6_CLng") != std::string::npos ||
            expr.find("vb6_CInt") != std::string::npos ||
            expr.find("vb6_CDbl") != std::string::npos ||
            expr.find("vb6_CSng") != std::string::npos ||
            expr.find("vb6_CBool") != std::string::npos ||
            expr.find("vb6_CByte") != std::string::npos ||
            expr.find("vb6_Abs") != std::string::npos ||
            expr.find("vb6_Len") != std::string::npos ||
            expr.find("vb6_LenB") != std::string::npos ||
            expr.find("vb6_InStr") != std::string::npos ||
            expr.find("vb6_InStrRev") != std::string::npos ||
            expr.find("vb6_Timer") != std::string::npos ||
            expr.find("vb6_Rnd") != std::string::npos ||
            expr.find("vb6_Sqr") != std::string::npos ||
            expr.find("vb6_Sgn") != std::string::npos ||
            expr.find("vb6_Fix") != std::string::npos ||
            expr.find("vb6_Int") != std::string::npos ||
            expr.find("vb6_Val") != std::string::npos ||
            expr.find("vb6_VarType") != std::string::npos) {
            return "vb6_CStrLong(" + expr + ")";
        }
        // Fix 091i: 项目内返回 Variant 的函数 (driver 预扫描 variantReturnFuncs_,
        // 含 vb6_<cls>_prop_get_<name>) 不能按"其他 vb6_ 函数假定为 BSTR"直通 —
        // 需 vb6_VariantToString 提取. 此前字符串拼接 / BSTR 形参处生成
        // vb6_BSTR_Concat(L"...", vb6_cDataBase_LastInsertId(...)) C2440
        // (Demo_Database.c 310/506/678).
        if (variantReturnFuncs_) {
            size_t p091i = expr.find('(');
            if (p091i != std::string::npos
                && variantReturnFuncs_->count(expr.substr(0, p091i))) {
                return "vb6_VariantToString(" + expr + ")";
            }
        }
        // Fix 118j: 其余 vb6_ 前缀的自定义函数不能一律假定返回 BSTR —
        // 项目内函数 vb6_Triple1(m) 返回 int16_t, 直通后生成
        //   vb6_BSTR_Concat(L"val=", vb6_Triple1(m))
        // 把 int16_t 当作 BSTR 指针解引用 → 0xC0000005 (Debug.Print "val=" & F(x)).
        // 用 inferExprType 查符号表拿函数返回类型, 数值/Date/Boolean/Byte/LongPtr
        // 按类型转换; String / Variant / 未知 保持原直通行为.
        switch (inferExprType(node)) {
            case Vb6Type::Integer:
            case Vb6Type::Long:    return "vb6_CStrLong(" + expr + ")";
            case Vb6Type::Single:  return "vb6_CStrSingle(" + expr + ")";
            case Vb6Type::Double:  return "vb6_CStrDbl(" + expr + ")";
            case Vb6Type::Boolean: return "vb6_CStrBool(" + expr + ")";
            case Vb6Type::Byte:    return "vb6_CStrByte(" + expr + ")";
            case Vb6Type::Date:    return "vb6_CStrDate(" + expr + ")";
            // LongPtr (intptr_t): 无 vb6_CStrLongPtr, 走 Variant 通用路径
            // (Fix 084 同思路: _Generic 自动包装 → vb6_CStr 统一转 BSTR)
            case Vb6Type::LongPtr:
            case Vb6Type::ULong:   return "vb6_CStr(vb6_VariantFromValue(" + expr + "))";
            default:               return expr;  // String / Variant / 未知 → 假定BSTR
        }
    }
    // string literal L"..."
    if (expr.find("vb6_BSTR_FromStr(") != std::string::npos) return expr;
    // 推断类型
    Vb6Type t = inferExprType(node);
    switch (t) {
        case Vb6Type::String: return expr;
        case Vb6Type::Integer:
        case Vb6Type::Long:   return "vb6_CStrLong(" + expr + ")";
        case Vb6Type::Single: return "vb6_CStrSingle(" + expr + ")";   // Fix 117c: VT_R4
        case Vb6Type::Double: return "vb6_CStrDbl(" + expr + ")";
        case Vb6Type::Boolean: return "vb6_CStrBool(" + expr + ")";
        case Vb6Type::Byte:   return "vb6_CStrByte(" + expr + ")";
        case Vb6Type::Date:   return "vb6_CStrDate(" + expr + ")";
        case Vb6Type::Variant: {
            // Fix 049a: inferExprType falls back to Variant for unknown symbols.
            // If the AST node is a known BSTR variable, return as-is to avoid
            // generating vb6_CStr(BSTR_expr) which causes C2440 (BSTR→VARIANT).
            // For all other cases (including actual Variant variables), keep
            // vb6_CStr(expr) which correctly converts VARIANT→BSTR.
            if (node.kind == ASTNodeKind::IdentifierExpr) {
                auto& id = static_cast<IdentifierExpr&>(node);
                std::string lower = id.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if (knownBstrVars_.count(lower)) return expr;
            }
            // Fix 084: inferExprType 回退 Variant 的表达式可能是标量(int32_t等)、
            // BSTR 或已是 vb6_VARIANT. 直接 vb6_CStr(expr) 会因实参类型不匹配触发
            // C2440 (如 vb6_CStr((int32_t)GetCurrentThreadId())). 用 vb6_VariantFromValue
            // 按 C 实参类型 _Generic 自动包装: 标量→VariantLong, BSTR→VariantString,
            // 已是 vb6_VARIANT→identity 直通, 再交给 vb6_CStr 统一转 BSTR.
            return "vb6_CStr(vb6_VariantFromValue(" + expr + "))";
        }
        default: return "vb6_CStrLong(" + expr + ")";  // fallback
    }
}


std::string CCodeGen::mapBinaryOp(BinaryOp op) const {
    switch (op) {
        case BinaryOp::Or:     return "|";    // VB6 Or = 位或
        case BinaryOp::Xor:    return "^";    // VB6 Xor = 位异或
        case BinaryOp::And:    return "&";    // VB6 And = 位与
        case BinaryOp::Eq:     return "==";
        case BinaryOp::Neq:    return "!=";
        case BinaryOp::Lt:     return "<";
        case BinaryOp::Gt:     return ">";
        case BinaryOp::Le:     return "<=";
        case BinaryOp::Ge:     return ">=";
        case BinaryOp::Add:    return "+";
        case BinaryOp::Sub:    return "-";
        case BinaryOp::Mod:    return "%";
        case BinaryOp::Mul:    return "*";
        case BinaryOp::Div:    return "/";
        case BinaryOp::Is:     return "==";   // 对象引用比较
        // 以下运算符在visit(BinaryExpr&)中已特殊处理, 此处不应到达
        case BinaryOp::Concat: return "/* CONCAT */";
        case BinaryOp::IntDiv: return "/* INTDIV */";
        case BinaryOp::Pow:    return "/* POW */";
        case BinaryOp::Eqv:    return "/* EQV */";
        case BinaryOp::Imp:    return "/* IMP */";
        case BinaryOp::Like:   return "/* LIKE */";
        default:               return "/* unhandled BinaryOp */";
    }
}

} // namespace vb6c3
