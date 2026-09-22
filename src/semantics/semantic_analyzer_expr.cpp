#include "semantics/semantic_analyzer.hpp"
#include <algorithm>
#include <cctype>
#include <tuple>
#include <initializer_list>
#include "semantics/semantic_analyzer_internal.h"

namespace vb6c3 {

// --- semantic_analyzer_expr.cpp: 表达式 Visitor + 类型引用 Visitor ---


// ============================================================
// 表达式 Visitor
// ============================================================

Vb6Type SemanticAnalyzer::analyzeExpr(Expr& expr) {
    lastExprType_ = Vb6Type::Unknown;
    if (verbose_) std::cerr << "[Sem]       expr: " << expr.kindName() << std::endl;
    dispatchExpr(expr, *this);
    return lastExprType_;
}

void SemanticAnalyzer::visit(BinaryExpr& node) {
    Vb6Type leftType = analyzeExpr(*node.left);
    Vb6Type rightType = analyzeExpr(*node.right);

    // 根据运算符推导结果类型
    switch (node.op) {
        case BinaryOp::Concat:
            // & 运算符: 字符串连接
            lastExprType_ = Vb6Type::String;
            break;
        case BinaryOp::Eq: case BinaryOp::Neq:
        case BinaryOp::Lt: case BinaryOp::Gt:
        case BinaryOp::Le: case BinaryOp::Ge:
        case BinaryOp::Like: case BinaryOp::Is:
            // 比较运算符 → Boolean
            lastExprType_ = Vb6Type::Boolean;
            break;
        case BinaryOp::And: case BinaryOp::Or: case BinaryOp::Xor:
        case BinaryOp::Eqv: case BinaryOp::Imp:
            // 逻辑运算符: 如果两边都是Boolean → Boolean, 否则 → 数值提升
            if (leftType == Vb6Type::Boolean && rightType == Vb6Type::Boolean) {
                lastExprType_ = Vb6Type::Boolean;
            } else if (TypeSystem::isNumeric(leftType) && TypeSystem::isNumeric(rightType)) {
                lastExprType_ = TypeSystem::promote(leftType, rightType);
            } else {
                lastExprType_ = Vb6Type::Variant;
            }
            break;
        case BinaryOp::Add: case BinaryOp::Sub:
        case BinaryOp::Mul: case BinaryOp::Div:
        case BinaryOp::IntDiv: case BinaryOp::Mod: case BinaryOp::Pow:
            // 算术运算符 → 数值提升
            if (TypeSystem::isNumeric(leftType) && TypeSystem::isNumeric(rightType)) {
                lastExprType_ = TypeSystem::promote(leftType, rightType);
                // 整除和取模 → Long
                if (node.op == BinaryOp::IntDiv || node.op == BinaryOp::Mod) {
                    lastExprType_ = Vb6Type::Long;
                }
            } else if (leftType == Vb6Type::String && rightType == Vb6Type::String && node.op == BinaryOp::Add) {
                // 字符串 + 字符串 → String (VB6允许但推荐用&)
                lastExprType_ = Vb6Type::String;
            } else {
                lastExprType_ = Vb6Type::Variant;
            }
            break;
    }
}

void SemanticAnalyzer::visit(UnaryExpr& node) {
    Vb6Type operandType = analyzeExpr(*node.operand);

    switch (node.op) {
        case UnaryOp::Negate:
            // -x: 数值类型不变, Boolean → Integer
            if (operandType == Vb6Type::Boolean) {
                lastExprType_ = Vb6Type::Integer;
            } else if (TypeSystem::isNumeric(operandType)) {
                lastExprType_ = operandType;
            } else {
                lastExprType_ = Vb6Type::Variant;
            }
            break;
        case UnaryOp::Not:
            // Not x: Boolean → Boolean, 数值 → 数值(按位取反)
            if (operandType == Vb6Type::Boolean) {
                lastExprType_ = Vb6Type::Boolean;
            } else if (TypeSystem::isIntegral(operandType)) {
                lastExprType_ = operandType;
            } else {
                lastExprType_ = Vb6Type::Variant;
            }
            break;
    }
}

void SemanticAnalyzer::visit(LiteralExpr& node) {
    switch (node.literalKind) {
        case LiteralKind::Integer:  lastExprType_ = Vb6Type::Integer; break;
        case LiteralKind::Long:     lastExprType_ = Vb6Type::Long; break;
        case LiteralKind::LongPtr:  lastExprType_ = Vb6Type::LongPtr; break;
        case LiteralKind::Single:   lastExprType_ = Vb6Type::Single; break;
        case LiteralKind::Double:   lastExprType_ = Vb6Type::Double; break;
        case LiteralKind::Currency: lastExprType_ = Vb6Type::Currency; break;
        case LiteralKind::Decimal:  lastExprType_ = Vb6Type::Decimal; break;
        case LiteralKind::String:   lastExprType_ = Vb6Type::String; break;
        case LiteralKind::Date:     lastExprType_ = Vb6Type::Date; break;
        case LiteralKind::Boolean:  lastExprType_ = Vb6Type::Boolean; break;
        case LiteralKind::Nothing:  lastExprType_ = Vb6Type::Object; break;
        case LiteralKind::Empty:    lastExprType_ = Vb6Type::Empty; break;
        case LiteralKind::Null:     lastExprType_ = Vb6Type::Null; break;
    }
}

void SemanticAnalyzer::visit(IdentifierExpr& node) {
    std::string lower = Symbol::toLower(node.name);

    // 在符号表中查找
    auto* sym = symTab_.lookup(node.name);
    if (sym) {
        sym->isReferenced = true;
        lastExprType_ = sym->type;
    } else {
        // 未找到标识符
        if (optionExplicit_ && pass_ == 2) {
            diag_.warn(DiagnosticID::SemUndeclaredIdentifier, node.loc,
                "未声明的标识符: '" + node.name + "' (可能来自其他模块)");
        }
        lastExprType_ = Vb6Type::Variant;  // 宽松模式: 推导为Variant
    }
}

void SemanticAnalyzer::visit(MemberAccessExpr& node) {
    Vb6Type objType = analyzeExpr(*node.object);
    // P20-21: 如果object是UDT, 查找成员类型
    if (objType == Vb6Type::UserDefinedType) {
        // 查找UDT符号获取成员类型
        if (auto* ident = dynamic_cast<IdentifierExpr*>(node.object.get())) {
            auto* udtSym = symTab_.lookup(ident->name);
            if (udtSym && udtSym->kind == SymbolKind::UserDefinedType) {
                std::string memberLower = node.memberName;
                std::transform(memberLower.begin(), memberLower.end(), memberLower.begin(), ::tolower);
                for (const auto& mi : udtSym->udtMembers) {
                    std::string miLower = mi.name;
                    std::transform(miLower.begin(), miLower.end(), miLower.begin(), ::tolower);
                    if (miLower == memberLower) {
                        lastExprType_ = mi.type;
                        return;
                    }
                }
            }
        }
    }
    // P24-04: ComModule全局函数访问 (VBMAN.Version)
    if (auto* ident = dynamic_cast<IdentifierExpr*>(node.object.get())) {
        auto* modSym = symTab_.lookup(ident->name);
        if (modSym && modSym->kind == SymbolKind::ComModule) {
            std::string memberLower = node.memberName;
            std::transform(memberLower.begin(), memberLower.end(), memberLower.begin(), ::tolower);
            auto it = modSym->comModuleFunctions.find(memberLower);
            if (it != modSym->comModuleFunctions.end()) {
                lastExprType_ = it->second.returnType;
                return;
            }
        }
    }
    // P24-04: ComGlobalNs — VB_GlobalNameSpace promoted函数访问 (如 VBMAN.Version)
    // VBMAN是提升到全局的函数, VBMAN()返回cVBMAN COM对象, .Version是cVBMAN的方法
    // 所以 VBMAN.Version = VBMAN().Version, 即先调用 promoted函数获取对象, 再访问成员
    if (auto* ident = dynamic_cast<IdentifierExpr*>(node.object.get())) {
        auto* gnsSym = symTab_.lookup(ident->name);
        if (gnsSym && gnsSym->kind == SymbolKind::ComGlobalNs) {
            // promoted函数返回Object (COM对象), 成员访问走晚绑定
            lastExprType_ = Vb6Type::Variant;
            return;
        }
    }
    // 简化: 非UDT或未找到成员, 推导为Variant
    lastExprType_ = Vb6Type::Variant;
}

void SemanticAnalyzer::visit(DictionaryAccessExpr& node) {
    Vb6Type objType = analyzeExpr(*node.object);
    lastExprType_ = Vb6Type::Variant;
}

void SemanticAnalyzer::visit(IndexOrCallExpr& node) {
    // 分析被调用者
    Vb6Type calleeType = Vb6Type::Unknown;

    // 判断是函数调用还是数组索引
    if (auto* ident = dynamic_cast<IdentifierExpr*>(node.callee.get())) {
        auto* sym = symTab_.lookup(ident->name);
        if (sym) {
            sym->isReferenced = true;

            if (sym->kind == SymbolKind::Function ||
                sym->kind == SymbolKind::DeclareFunc) {
                // 函数调用: 返回类型即为表达式类型
                calleeType = sym->type;
                checkCallArgs(sym, node);
            } else if (sym->kind == SymbolKind::Sub ||
                       sym->kind == SymbolKind::DeclareSub) {
                // Sub作为表达式调用 (VB6允许, 返回Variant)
                calleeType = Vb6Type::Variant;
                checkCallArgs(sym, node);
            } else if (sym->isArray) {
                // 数组索引: 元素类型
                uint16_t baseType = static_cast<uint16_t>(sym->type) &
                                    ~static_cast<uint16_t>(Vb6Type::Array);
                calleeType = static_cast<Vb6Type>(baseType);
            } else {
                calleeType = sym->type;
            }
        } else {
            // 可能是内置函数 (MsgBox, Len, etc.) 或未声明
            // 简化: 推导为Variant
            calleeType = Vb6Type::Variant;

            // 检查Option Explicit
            if (optionExplicit_ && pass_ == 2 &&
                node.positional.empty()) {
                // 非数组索引、无参数 → 可能是未声明变量
                // 但无法确定, 这里不做报错
            }
        }
    } else {
        // 成员调用 (obj.method args)
        analyzeExpr(*node.callee);
        calleeType = Vb6Type::Variant;
    }

    // 分析参数
    for (auto& arg : node.positional) {
        analyzeExpr(*arg);
    }
    for (auto& namedArg : node.named) {
        analyzeExpr(*namedArg.value);
    }

    lastExprType_ = calleeType;
}

void SemanticAnalyzer::visit(NewExpr& node) {
    // 查找类名是否在符号表中注册(类符号或作为对象类型引用)
    auto* sym = symTab_.lookup(node.className);
    if (sym && sym->kind == SymbolKind::Class) {
        node.className = sym->name;  // 规范化大小写
    }
    // 即使未找到类符号，也不报错(可能是COM对象或外部类，运行时解析)
    lastExprType_ = Vb6Type::Object;
}

void SemanticAnalyzer::visit(TypeOfExpr& node) {
    analyzeExpr(*node.object);
    lastExprType_ = Vb6Type::Boolean;
}

void SemanticAnalyzer::visit(AddressOfExpr& node) {
    markReferenced(node.funcName);
    lastExprType_ = Vb6Type::Long;  // 函数指针
}

void SemanticAnalyzer::visit(MeExpr& node) {
    // Me代表当前对象实例 (类模块或窗体模块)
    if (currentModule_ && (currentModule_->isClassModule || currentModule_->isFormModule)) {
        lastExprType_ = Vb6Type::Object;
    } else {
        diag_.error(DiagnosticID::SemTypeMismatch, node.loc,
            "'Me' can only be used in class/form modules");
        lastExprType_ = Vb6Type::Object;
    }
}

void SemanticAnalyzer::visit(WithMemberExpr& node) {
    // .member → 访问With对象的成员
    // 简化: 推导为Variant
    lastExprType_ = Vb6Type::Variant;
}

// ============================================================
// 类型引用 Visitor
// ============================================================

void SemanticAnalyzer::visit(SimpleTypeRef& node) {
    // 类型引用不会产生表达式类型
}

void SemanticAnalyzer::visit(ArrayTypeRef& node) {
    // 分析数组维度表达式
    for (auto& dim : node.dimensions) {
        if (dim.lower) analyzeExpr(*dim.lower);
        if (dim.upper) analyzeExpr(*dim.upper);
    }
}

void SemanticAnalyzer::visit(FixedStringTypeRef& node) {
    if (node.length) analyzeExpr(*node.length);
}

// ---- 文件I/O语句 ----

void SemanticAnalyzer::visit(OpenStmt& node) {
    analyzeExpr(*node.pathName);
    if (node.recordLength) analyzeExpr(*node.recordLength);
    analyzeExpr(*node.fileNumber);
}

void SemanticAnalyzer::visit(GetStmt& node) {
    analyzeExpr(*node.fileNumber);
    if (node.recordNumber) analyzeExpr(*node.recordNumber);
    analyzeExpr(*node.varName);
}

void SemanticAnalyzer::visit(PutStmt& node) {
    analyzeExpr(*node.fileNumber);
    if (node.recordNumber) analyzeExpr(*node.recordNumber);
    analyzeExpr(*node.varName);
}
} // namespace vb6c3
