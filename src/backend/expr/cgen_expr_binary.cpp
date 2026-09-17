#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>
#include <cstdio>

namespace vb6c3 {

// --- cgen_expr_binary.cpp: 二元表达式求值 + BSTR 包装 + 二元运算符映射 ---

// M22: 将非BSTR表达式包装为BSTR (用于字符串连接 & 运算符)
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
        return expr;  // 其他vb6_函数假定为BSTR
    }
    // string literal L"..."
    if (expr.find("vb6_BSTR_FromStr(") != std::string::npos) return expr;
    // 推断类型
    Vb6Type t = inferExprType(node);
    switch (t) {
        case Vb6Type::String: return expr;
        case Vb6Type::Integer:
        case Vb6Type::Long:   return "vb6_CStrLong(" + expr + ")";
        case Vb6Type::Single:
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
            lastExpr_ = "(-(vb6_StrCmp(" + left + ", " + right + ") " + cmpOp + "))";
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
            // Fix 092v: LongPtr 关系比较同样产生 C 的 0/1 → 取负为 -1/0 (VB6 Boolean)
            lastExpr_ = "(-(" + left + " " + op + " " + right + "))";
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
    // Fix 092v: VB6 关系比较 (=,<>,<,>,<=,>=,Is) 结果为 Boolean(-1/0),
    // 而 C 原生比较为 0/1. 取负转 -1/0, 使上层 Not(位反)/And/Xor/算术
    // 与 VB6 一致. 只有关系运算符需要转; 算术(+-*/等)保持原样.
    static const std::unordered_set<std::string> relOps092v = {
        "==", "!=", "<", ">", "<=", ">="
    };
    if (relOps092v.count(op)) {
        lastExpr_ = "(-(" + left + " " + op + " " + right + "))";
    } else {
        lastExpr_ = "(" + left + " " + op + " " + right + ")";
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
