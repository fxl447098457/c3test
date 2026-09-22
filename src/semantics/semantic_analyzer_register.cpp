#include "semantics/semantic_analyzer.hpp"
#include <algorithm>
#include <cctype>
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
        }
    }

    // 没有As Type部分且未推导出类型，默认Variant
    if (sym->type == Vb6Type::Unknown || sym->type == Vb6Type::Empty) {
        sym->type = Vb6Type::Variant;
    }

    symTab_.define(std::move(sym));
}
} // namespace vb6c3
