#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>
#include <cstdio>

namespace vb6c3 {

// --- cgen_expr_array.cpp: SafeArray / 数组元素类型辅助方法 ---

std::string CCodeGen::mapSaElemType(Vb6Type type) const {
    switch (type) {
        case Vb6Type::Boolean:  return "vb6_sa_bool";
        case Vb6Type::Byte:     return "vb6_sa_byte";
        case Vb6Type::Integer:  return "vb6_sa_int";
        case Vb6Type::Long:     return "vb6_sa_long";
        case Vb6Type::Single:   return "vb6_sa_single";
        case Vb6Type::Double:   return "vb6_sa_double";
        case Vb6Type::Date:     return "vb6_sa_double";
        case Vb6Type::Currency: return "vb6_sa_currency";  // P1-9修复: 8-byte Currency
        case Vb6Type::String:   return "vb6_sa_bstr";
        case Vb6Type::Variant:  return "vb6_sa_variant";
        case Vb6Type::Object:   return "vb6_sa_ptr";
        default:                return "vb6_sa_variant";
    }
}

std::string CCodeGen::mapSaElemCType(Vb6Type type) const {
    switch (type) {
        case Vb6Type::Boolean:  return "int16_t";
        case Vb6Type::Byte:     return "uint8_t";
        case Vb6Type::Integer:  return "int16_t";
        case Vb6Type::Long:     return "int32_t";
        case Vb6Type::Single:   return "float";
        case Vb6Type::Double:   return "double";
        case Vb6Type::Date:     return "double";
        case Vb6Type::String:   return "BSTR";
        case Vb6Type::Variant:  return "vb6_VARIANT";
        case Vb6Type::Currency: return "int64_t";
        case Vb6Type::Object:   return "void*";
        default:                return "vb6_VARIANT";
    }
}

Vb6Type CCodeGen::resolveArrayElemType(ASTNode* typeRef) const {
    if (!typeRef) return Vb6Type::Variant;
    if (typeRef->kind == ASTNodeKind::ArrayTypeRef) {
        auto& arrType = static_cast<ArrayTypeRef&>(*typeRef);
        return resolveArrayElemType(arrType.elementType.get());
    }
    if (typeRef->kind == ASTNodeKind::SimpleTypeRef) {
        auto& simple = static_cast<SimpleTypeRef&>(*typeRef);
        Vb6Type t = typeSys_.resolveTypeName(simple.name);
        if (t != Vb6Type::Unknown) return t;
        // Fix 090by: VB6 内置对象类型 (VBA 标准对象, 无 typelib 符号) —
        // Collection/Forms/ErrObject/App/Screen/Printer/Clipboard.
        // 与 semantic_analyzer.cpp vb6BuiltinObjTypes 一致, 处理为 Object.
        // 若不识别, resolveTypeName/lookup 均 miss → 回落 Variant → 类字段
        // (Private x As Collection) 被误注册 classVariantMembers_ → 类内
        // m_Col(i) 生成 vb6_VariantArrayGet(&me->m_Col, i) (C2172 实参不是
        // 指针), 而非 void* COM 字段的 vb6_ComCall(me->m_Col, L"Item", ...).
        {
            std::string nm0 = simple.name;
            std::string nmLower0;
            nmLower0.resize(nm0.size());
            std::transform(nm0.begin(), nm0.end(), nmLower0.begin(), ::tolower);
            if (nmLower0.size() > 4 && nmLower0.compare(0, 4, "vba.") == 0) {
                nmLower0 = nmLower0.substr(4);
            }
            static const std::unordered_set<std::string> vb6BuiltinObjTypes = {
                "collection", "forms", "errobject", "app", "screen", "printer", "clipboard",
                "control", "form"
            };
            if (vb6BuiltinObjTypes.count(nmLower0)) return Vb6Type::Object;
        }
        // Fix 049b: 项目内 UDT/Enum/类名 需查符号表 (typeSys_ 只含内置类型)。
        // 否则 Dim x() As 某UDT (且该 UDT 声明位于 Dim 之后, Fix 049 预扫描已注册)
        // 的元素类型退回 Variant → arrayElemTypes_ 记录 Variant → 元素访问生成
        // VB6_SA_AT(vb6_VARIANT, ...).字段 (C2039/C2223) → With 对象类型也变 Variant,
        // 成员解析退化为跨模块类查找 (如 .Pos 误解析到 cToast.Pos, C2198)。
        // 与 resolveArrayUdtElemCType (Fix 055b) 的符号表回退保持一致。
        // Fix 107: 用 lookupTypeSymbol — 类型与过程同名时 (Fix 103 把类型存到
        // <name>$ty), 通用 lookup 命中过程符号 → 枚举/UDT 字段回落 Variant,
        // 与 mapTypeRef 的 int32_t/vb6_type_x 不一致 → C2440.
        if (auto* sym = lookupTypeSymbol(simple.name)) {
            if (sym->kind == SymbolKind::UserDefinedType) return Vb6Type::UserDefinedType;
            if (sym->kind == SymbolKind::EnumType) return Vb6Type::Long;
            if (sym->kind == SymbolKind::Class || sym->kind == SymbolKind::ComClass ||
                sym->kind == SymbolKind::ComInterface || sym->kind == SymbolKind::ComModule ||
                sym->kind == SymbolKind::ComGlobalNs) {
                return Vb6Type::Object;
            }
        }
        // Fix 097: 无 vb 前缀的 VBA 枚举别名 — CompareMethod 是 Long
        // (与 type_system.cpp resolveTypeName 的 Fix 097 对齐, 否则字段声明
        // 映射 int32_t 而此处回落 Variant, 类工厂初始化生成
        // me->m_CompareMode = vb6_VariantEmpty() → C2440, Dictionary.cls:45).
        {
            std::string nm097 = simple.name;
            std::transform(nm097.begin(), nm097.end(), nm097.begin(), ::tolower);
            if (nm097 == "comparemethod") return Vb6Type::Long;
        }
        return Vb6Type::Variant;
    }
    return Vb6Type::Variant;
}

std::string CCodeGen::resolveArrayUdtElemCType(ASTNode* typeRef) const {
    if (!typeRef) return "";
    if (typeRef->kind == ASTNodeKind::ArrayTypeRef) {
        auto& arrType = static_cast<ArrayTypeRef&>(*typeRef);
        return resolveArrayUdtElemCType(arrType.elementType.get());
    }
    if (typeRef->kind == ASTNodeKind::SimpleTypeRef) {
        auto& simple = static_cast<SimpleTypeRef&>(*typeRef);
        // 检查类型系统
        Vb6Type vtype = typeSys_.resolveTypeName(simple.name);
        if (vtype == Vb6Type::UserDefinedType) {
            return "vb6_type_" + cIdent(simple.name);
        }
        // Fix 055b: resolveTypeName 对项目内 UDT 返回 Unknown,
        // 需要在符号表中查找是否为 UserDefinedType 符号
        auto* sym = symTab_.lookup(simple.name);
        if (sym && (sym->type == Vb6Type::UserDefinedType || sym->kind == SymbolKind::UserDefinedType)) {
            return "vb6_type_" + cIdent(simple.name);
        }
        sym = symTab_.lookupModule(simple.name);
        if (sym && (sym->type == Vb6Type::UserDefinedType || sym->kind == SymbolKind::UserDefinedType)) {
            return "vb6_type_" + cIdent(simple.name);
        }
    }
    return "";
}

} // namespace vb6c3
