#include "semantics/semantic_analyzer.hpp"
#include <algorithm>
#include <cctype>
#include <functional>
#include <tuple>
#include <initializer_list>
#include "semantics/semantic_analyzer_internal.h"

namespace vb6c3 {

// --- semantic_analyzer_register.cpp: 注册声明 (Pass 1): registerDecl / registerVariable / registerConstant ---


// ============================================================
// 注册声明 (Pass 1)
// ============================================================

void SemanticAnalyzer::registerDecl(Decl& decl) {
    switch (decl.kind) {
        case ASTNodeKind::SubDecl:
        case ASTNodeKind::FunctionDecl:
        case ASTNodeKind::PropertyDecl:
        case ASTNodeKind::DeclareDecl:
        case ASTNodeKind::EventDecl:
            dispatchDecl(decl, *this);
            break;
        case ASTNodeKind::TypeDecl:
        case ASTNodeKind::EnumDecl:
            // 已在 Fix 049 预扫描中注册 (VB6 允许类型声明位于使用之后)
            break;
        case ASTNodeKind::VariableDecl:
            registerVariable(static_cast<VariableDecl&>(decl));
            break;
        case ASTNodeKind::ConstDecl:
            registerConstant(static_cast<ConstDecl&>(decl));
            break;
        default:
            break;
    }
}

void SemanticAnalyzer::registerVariable(VariableDecl& decl) {
    auto sym = std::make_unique<Symbol>(
        SymbolKind::Variable, decl.name,
        resolveTypeOrDefault(decl.name, decl.asType.get()),
        decl.loc, decl.access
    );
    sym->isStatic = decl.isStatic;
    sym->isNewVar = decl.isNew;  // Fix 183: 记录 As New 标志, 供跨模块惰性实例化
    sym->isArray = !decl.dimensions.empty() || decl.isDynamicArray;
    sym->dimCount = (int32_t)decl.dimensions.size();  // P8.1: 多维数组维度数
    // P6.5: 记录WithEvents标志和源类名
    if (decl.isWithEvents) {
        sym->isWithEvents = true;
        if (decl.asType && decl.asType->kind == ASTNodeKind::SimpleTypeRef) {
            sym->withEventsSourceClass = static_cast<SimpleTypeRef*>(decl.asType.get())->name;
        }
    }
    // Fix 010r-11: 记录变量的声明类型名 (用于跨模块解析)
    // 当变量声明为 As ClassName 时，存储类名以便 consuming 模块的 cgen 能正确识别类实例变量
    if (decl.asType && decl.asType->kind == ASTNodeKind::SimpleTypeRef) {
        sym->variableTypeName = static_cast<SimpleTypeRef*>(decl.asType.get())->name;
        sym->srcTypeName = sym->variableTypeName;  // tB B08c
    }
    symTab_.define(std::move(sym));
}

void SemanticAnalyzer::registerConstant(ConstDecl& decl) {
    auto sym = std::make_unique<Symbol>(
        SymbolKind::Constant, decl.name,
        resolveTypeOrDefault(decl.name, decl.asType.get()),
        decl.loc, decl.access
    );

    // Fix 046: If no explicit As Type, reset to Unknown so value derivation
    // can infer the type from the literal value. Without this, resolveTypeOrDefault
    // returns Variant (default), and the "if (sym->type == Vb6Type::Unknown)" checks
    // in value derivation never fire, leaving all untyped constants as Variant.
    // This causes false-positive Variant detection in codegen (e.g., vb6_VariantToLong
    // wrapping integer constants like HWND_TOPMOST = -1, keybd_event keys = 18, etc.).
    if (!decl.asType && sym->type == Vb6Type::Variant) {
        sym->type = Vb6Type::Unknown;
    }

    // 如果有值表达式，推导常量类型
    if (decl.value) {
        // 简单的常量值推导 (从字面量)
        if (auto* lit = dynamic_cast<LiteralExpr*>(decl.value.get())) {
            sym->hasConstValue = true;
            switch (lit->literalKind) {
                case LiteralKind::Boolean:
                    sym->constType = Vb6Type::Boolean;
                    sym->constBoolValue = lit->boolValue;
                    if (sym->type == Vb6Type::Unknown)
                        sym->type = Vb6Type::Boolean;
                    break;
                case LiteralKind::Integer:
                    sym->constType = Vb6Type::Integer;
                    sym->constIntValue = lit->intValue;
                    if (sym->type == Vb6Type::Unknown)
                        sym->type = Vb6Type::Integer;
                    break;
                case LiteralKind::Long:
                    sym->constType = Vb6Type::Long;
                    sym->constIntValue = lit->longValue;
                    if (sym->type == Vb6Type::Unknown)
                        sym->type = Vb6Type::Long;
                    break;
                case LiteralKind::LongPtr:  // Fix 082: ^ 后缀
                    sym->constType = Vb6Type::LongPtr;
                    sym->constIntValue = lit->longValue;
                    if (sym->type == Vb6Type::Unknown)
                        sym->type = Vb6Type::LongPtr;
                    break;
                case LiteralKind::Single:
                    sym->constType = Vb6Type::Single;
                    sym->constFloatValue = lit->floatValue;
                    if (sym->type == Vb6Type::Unknown)
                        sym->type = Vb6Type::Single;
                    break;
                case LiteralKind::Double:
                    sym->constType = Vb6Type::Double;
                    sym->constFloatValue = lit->doubleValue;
                    if (sym->type == Vb6Type::Unknown)
                        sym->type = Vb6Type::Double;
                    break;
                case LiteralKind::String:
                    sym->constType = Vb6Type::String;
                    // Strip outer quotes and fold VB6 "" escape (two quotes → one)
                    {
                        std::string cv = lit->rawText;
                        if (cv.size() >= 2 && cv.front() == '"' && cv.back() == '"') {
                            cv = cv.substr(1, cv.size() - 2);
                        }
                        std::string cvFolded;
                        cvFolded.reserve(cv.size());
                        for (size_t ci = 0; ci < cv.size(); ci++) {
                            if (cv[ci] == '"' && ci + 1 < cv.size() && cv[ci + 1] == '"') {
                                cvFolded += '"';
                                ci++;
                            } else {
                                cvFolded += cv[ci];
                            }
                        }
                        sym->constStringValue = cvFolded;
                    }
                    if (sym->type == Vb6Type::Unknown)
                        sym->type = Vb6Type::String;
                    break;
                default:
                    sym->constType = Vb6Type::Variant;
                    if (sym->type == Vb6Type::Unknown)
                        sym->type = Vb6Type::Variant;
                    break;
            }
        } else if (auto* ident = dynamic_cast<IdentifierExpr*>(decl.value.get())) {
            // Const值引用另一个符号 - Pass2处理
        } else if (auto* unary = dynamic_cast<UnaryExpr*>(decl.value.get())) {
            // 负数: -42 -> Long
            if (unary->op == UnaryOp::Negate) {
                if (auto* innerLit = dynamic_cast<LiteralExpr*>(unary->operand.get())) {
                    sym->hasConstValue = true;
                    if (innerLit->literalKind == LiteralKind::Long ||
                        innerLit->literalKind == LiteralKind::LongPtr) {
                        sym->constType = Vb6Type::Long;
                        sym->constIntValue = -innerLit->longValue;
                        if (sym->type == Vb6Type::Unknown)
                            sym->type = Vb6Type::Long;
                    } else if (innerLit->literalKind == LiteralKind::Integer) {
                        sym->constType = Vb6Type::Integer;
                        sym->constIntValue = -innerLit->intValue;
                        if (sym->type == Vb6Type::Unknown)
                            sym->type = Vb6Type::Integer;
                    }
                }
            }
        } else if (auto* bin43 = dynamic_cast<BinaryExpr*>(decl.value.get())) {
            // Task #38: 整型字面量的位运算常量 (GHND = GMEM_MOVEABLE Or GMEM_ZEROINIT)。
            // 此前 BinaryExpr 不推导 → sym->type 停留 Unknown → 兜底 Variant;
            // 而 cgen 把它折叠成 #define GHND (66) (int), 使用点按 Variant 解包 →
            // vb6_VariantToLong(66) C2440 (cDlg.cls GlobalAlloc/ChooseFont hGlobal)。
            // 对两侧可求值 (字面量/一元负号) 的 Or/And/Xor 直接算出结果, 推导 Long —
            // 与 cgen 的常量折叠同一口径, 使用点不再包 Variant 转换。
            std::function<bool(Expr*, int64_t*)> fold43 =
                [&](Expr* e, int64_t* out) -> bool {
                if (auto* lit = dynamic_cast<LiteralExpr*>(e)) {
                    if (lit->literalKind == LiteralKind::Integer) {
                        *out = lit->intValue; return true;
                    }
                    if (lit->literalKind == LiteralKind::Long
                        || lit->literalKind == LiteralKind::LongPtr) {
                        *out = lit->longValue; return true;
                    }
                    return false;
                }
                if (auto* un = dynamic_cast<UnaryExpr*>(e)) {
                    int64_t v = 0;
                    if (un->op == UnaryOp::Negate && fold43(un->operand.get(), &v)) {
                        *out = -v; return true;
                    }
                    return false;
                }
                // 常量引用: GHND = (GMEM_MOVEABLE Or GMEM_ZEROINIT) 的操作数是
                // 本模块已注册的 Const (registerConstant 顺序执行, GMEM_* 在前,
                // constIntValue 已就绪)。仅接受整型常量。
                if (auto* id = dynamic_cast<IdentifierExpr*>(e)) {
                    Symbol* cs = symTab_.lookup(id->name);
                    if (cs && cs->kind == SymbolKind::Constant && cs->hasConstValue
                        && (cs->constType == Vb6Type::Integer
                            || cs->constType == Vb6Type::Long
                            || cs->constType == Vb6Type::LongPtr
                            || cs->constType == Vb6Type::Byte
                            || cs->constType == Vb6Type::Boolean)) {
                        *out = cs->constIntValue; return true;
                    }
                    return false;
                }
                return false;
            };
            if (bin43->op == BinaryOp::Or || bin43->op == BinaryOp::And
                || bin43->op == BinaryOp::Xor) {
                int64_t l43 = 0, r43 = 0;
                if (fold43(bin43->left.get(), &l43)
                    && fold43(bin43->right.get(), &r43)) {
                    int64_t v43 = 0;
                    switch (bin43->op) {
                        case BinaryOp::Or:  v43 = l43 | r43; break;
                        case BinaryOp::And: v43 = l43 & r43; break;
                        default:            v43 = l43 ^ r43; break;
                    }
                    sym->hasConstValue = true;
                    sym->constType = Vb6Type::Long;
                    sym->constIntValue = v43;
                    if (sym->type == Vb6Type::Unknown)
                        sym->type = Vb6Type::Long;
                }
            }
        }
    }

    // 没有As Type部分且未推导出类型，默认Variant
    if (sym->type == Vb6Type::Unknown || sym->type == Vb6Type::Empty) {
        sym->type = Vb6Type::Variant;
    }

    symTab_.define(std::move(sym));
}
} // namespace vb6c3
