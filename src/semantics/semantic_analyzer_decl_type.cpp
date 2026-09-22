#include "semantics/semantic_analyzer.hpp"
#include <algorithm>
#include <cctype>
#include <tuple>
#include <initializer_list>
#include "semantics/semantic_analyzer_internal.h"

namespace vb6c3 {

// --- semantic_analyzer_decl_type.cpp: 类型/枚举/Declare/事件/常量/变量 声明 Visitor ---
// 由 src/semantics/semantic_analyzer_decl.cpp 拆出（2026-09-17），纯搬移、零行为改动。


void SemanticAnalyzer::visit(TypeDecl& node) {
    if (pass_ == 1) {
        auto sym = std::make_unique<Symbol>(
            SymbolKind::UserDefinedType, node.name,
            Vb6Type::UserDefinedType, node.loc, node.access
        );
        // P20-21: 注册UDT成员信息
        for (const auto& memberPtr : node.members) {
            Symbol::UdtMemberInfo mi;
            mi.name = memberPtr->name;
            if (memberPtr->type) {
                mi.type = resolveTypeRef(memberPtr->type.get());
                // 若类型是UDT/Enum等命名类型, 保存类型引用名
                if (auto* stRef = dynamic_cast<SimpleTypeRef*>(memberPtr->type.get())) {
                    if (mi.type == Vb6Type::UserDefinedType) {
                        mi.typeRefName = stRef->name;
                    }
                    // Fix 085: 对象字段 (As 项目类/Collection/COM接口, 均解析为 Object)
                    // 也保存类型引用名 — 供 cgen 区分"项目类对象字段"与"COM/Collection
                    // 对象字段", 从而把 obj.Field.Method(...) 生成到正确的类方法调用/
                    // COM dispatch 通道 (否则 UDT 字段直接 obj.Field.Method 触发 C2039/
                    // C2224). 现有 typeRefName 消费者均限定 UserDefinedType, 安全.
                    else if (mi.type == Vb6Type::Object) {
                        mi.typeRefName = stRef->name;
                    }
                }
            }
            if (memberPtr->arraySize) {
                mi.arraySize = 1;
            }
            // Fix 037: 传播动态数组标记 (`() As Type`) — UdtMemberInfo.isArrayDynamic
            if (memberPtr->isArrayDynamic) {
                mi.isArrayDynamic = true;
            }
            sym->udtMembers.push_back(std::move(mi));
        }
        symTab_.define(std::move(sym));
    }
}

void SemanticAnalyzer::visit(TypeMember& node) {
    // P20-21: UDT成员已在TypeDecl中统一处理
}

void SemanticAnalyzer::visit(EnumDecl& node) {
    if (pass_ == 1) {
        auto sym = std::make_unique<Symbol>(
            SymbolKind::EnumType, node.name,
            Vb6Type::Long, node.loc, node.access
        );
        symTab_.define(std::move(sym));

        // 注册Enum成员为常量
        int64_t nextValue = 0;
        for (auto& member : node.members) {
            auto memberSym = std::make_unique<Symbol>(
                SymbolKind::EnumMember, member->name,
                Vb6Type::Long, member->loc, node.access
            );
            memberSym->hasConstValue = true;
            memberSym->constType = Vb6Type::Long;

            // 如果有显式值
            if (member->value) {
                if (auto* lit = dynamic_cast<LiteralExpr*>(member->value.get())) {
                    if (lit->literalKind == LiteralKind::Long ||
                        lit->literalKind == LiteralKind::LongPtr)  // Fix 082: ^ 后缀
                        nextValue = lit->longValue;
                    else if (lit->literalKind == LiteralKind::Integer)
                        nextValue = lit->intValue;
                } else if (auto* unary = dynamic_cast<UnaryExpr*>(member->value.get())) {
                    // Handle negative literals: -1, -42, etc.
                    if (auto* inner = dynamic_cast<LiteralExpr*>(unary->operand.get())) {
                        int64_t v = 0;
                        if (inner->literalKind == LiteralKind::Long ||
                        inner->literalKind == LiteralKind::LongPtr) v = inner->longValue;
                        else if (inner->literalKind == LiteralKind::Integer) v = inner->intValue;
                        if (unary->op == UnaryOp::Negate) nextValue = -v;
                        else nextValue = v;
                    }
                }
            }
            memberSym->constIntValue = nextValue;
            nextValue++;

            symTab_.define(std::move(memberSym));
        }
    }
}

void SemanticAnalyzer::visit(EnumMember& node) {
    // 在EnumDecl中处理
}

void SemanticAnalyzer::visit(DeclareDecl& node) {
    if (pass_ == 1) {
        SymbolKind sk = (node.procKind == ProcKind::Sub)
                        ? SymbolKind::DeclareSub : SymbolKind::DeclareFunc;

        Vb6Type retType = Vb6Type::Void;
        if (node.procKind == ProcKind::Function) {
            retType = resolveTypeRef(node.returnType.get());
        }

        auto sym = std::make_unique<Symbol>(sk, node.name, retType, node.loc, node.access);

        for (auto& param : node.params) {
            ParameterInfo pi;
            pi.name = param->name;
            pi.type = resolveTypeOrDefault(param->name, param->asType.get());
            pi.isByVal = param->isByVal;
            pi.isOptional = param->isOptional;
            pi.isParamArray = param->isParamArray;
            // P14.1.4: 传播Optional默认值
            if (param->isOptional) {
                pi.hasDefaultValue = (param->defaultValue != nullptr);
                pi.defaultValueExpr = evalOptionalDefault(param->defaultValue.get(), pi.type);
            }
            sym->params.push_back(std::move(pi));
        }

        symTab_.define(std::move(sym));
    }
    // 外部声明无过程体
}

void SemanticAnalyzer::visit(EventDecl& node) {
    if (pass_ == 1) {
        auto sym = std::make_unique<Symbol>(
            SymbolKind::Event, node.name,
            Vb6Type::Void, node.loc, node.access
        );
        for (auto& param : node.params) {
            ParameterInfo pi;
            pi.name = param->name;
            pi.type = resolveTypeOrDefault(param->name, param->asType.get());
            pi.isByVal = param->isByVal;
            pi.isOptional = param->isOptional;
            pi.isParamArray = param->isParamArray;
            // P14.1.4: 传播Optional默认值
            if (param->isOptional) {
                pi.hasDefaultValue = (param->defaultValue != nullptr);
                pi.defaultValueExpr = evalOptionalDefault(param->defaultValue.get(), pi.type);
            }
            sym->params.push_back(std::move(pi));
        }
        symTab_.define(std::move(sym));
    }
}

void SemanticAnalyzer::visit(ConstDecl& node) {
    if (pass_ == 1) {
        registerConstant(node);
    }
}

void SemanticAnalyzer::visit(VariableDecl& node) {
    if (pass_ == 1) {
        registerVariable(node);
    }
}

void SemanticAnalyzer::visit(ParameterDecl& node) {
    // 参数在SubDecl/FunctionDecl中处理
}

} // namespace vb6c3
