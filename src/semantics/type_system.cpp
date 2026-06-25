#include "semantics/type_system.hpp"
#include <algorithm>
#include <cctype>

namespace vb6c3 {

TypeSystem::TypeSystem() {
    // VB6内置类型名映射 (小写)
    builtinTypes_ = {
        {"boolean", Vb6Type::Boolean},
        {"byte", Vb6Type::Byte},
        {"integer", Vb6Type::Integer},
        {"int", Vb6Type::Integer},       // 缩写
        {"long", Vb6Type::Long},
        {"lng", Vb6Type::Long},
        {"single", Vb6Type::Single},
        {"sng", Vb6Type::Single},
        {"double", Vb6Type::Double},
        {"dbl", Vb6Type::Double},
        {"currency", Vb6Type::Currency},
        {"cur", Vb6Type::Currency},
        {"decimal", Vb6Type::Decimal},
        {"date", Vb6Type::Date},
        {"string", Vb6Type::String},
        {"str", Vb6Type::String},
        {"object", Vb6Type::Object},
        {"obj", Vb6Type::Object},
        {"variant", Vb6Type::Variant},
        {"var", Vb6Type::Variant},
        {"any", Vb6Type::Unknown},       // Declare中的Any
    };
}

Vb6Type TypeSystem::resolveTypeName(const std::string& name) const {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    auto it = builtinTypes_.find(lower);
    if (it != builtinTypes_.end()) {
        return it->second;
    }
    // 用户自定义类型 (Type/Enum)
    return Vb6Type::Unknown;
}

const char* TypeSystem::typeToString(Vb6Type t) {
    switch (t) {
        case Vb6Type::Empty:           return "Empty";
        case Vb6Type::Null:            return "Null";
        case Vb6Type::Integer:         return "Integer";
        case Vb6Type::Long:            return "Long";
        case Vb6Type::Single:          return "Single";
        case Vb6Type::Double:          return "Double";
        case Vb6Type::Currency:        return "Currency";
        case Vb6Type::Date:            return "Date";
        case Vb6Type::String:          return "String";
        case Vb6Type::Object:          return "Object";
        case Vb6Type::Error:           return "Error";
        case Vb6Type::Boolean:         return "Boolean";
        case Vb6Type::Variant:         return "Variant";
        case Vb6Type::Decimal:         return "Decimal";
        case Vb6Type::Byte:            return "Byte";
        case Vb6Type::UserDefinedType: return "UserDefinedType";
        case Vb6Type::Void:            return "Void";
        case Vb6Type::Unknown:         return "Unknown";
        default:                        return "?";
    }
}

bool TypeSystem::isNumeric(Vb6Type t) {
    return isIntegral(t) || isFloat(t) || t == Vb6Type::Currency;
}

bool TypeSystem::isIntegral(Vb6Type t) {
    return t == Vb6Type::Byte || t == Vb6Type::Integer ||
           t == Vb6Type::Long || t == Vb6Type::Boolean;
}

bool TypeSystem::isFloat(Vb6Type t) {
    return t == Vb6Type::Single || t == Vb6Type::Double;
}

bool TypeSystem::isString(Vb6Type t) {
    return t == Vb6Type::String;
}

bool TypeSystem::isObject(Vb6Type t) {
    return t == Vb6Type::Object;
}

bool TypeSystem::canImplicitConvert(Vb6Type from, Vb6Type to) {
    // Variant可以接受任何类型
    if (to == Vb6Type::Variant) return true;
    // 任何类型可以存入Variant
    if (from == Vb6Type::Variant) return true;

    // 相同类型
    if (from == to) return true;

    // Empty可以转为任何类型
    if (from == Vb6Type::Empty) return true;

    // 数值类型之间的转换 (VB6允许, 可能丢失精度)
    if (isNumeric(from) && isNumeric(to)) return true;

    // 数值→String
    if (isNumeric(from) && to == Vb6Type::String) return true;

    // String→数值 (VB6允许, 运行时可能出错)
    if (from == Vb6Type::String && isNumeric(to)) return true;

    // Boolean↔数值
    if (from == Vb6Type::Boolean && isNumeric(to)) return true;
    if (isNumeric(from) && to == Vb6Type::Boolean) return true;

    // Date↔Double
    if (from == Vb6Type::Date && to == Vb6Type::Double) return true;
    if (from == Vb6Type::Double && to == Vb6Type::Date) return true;

    // Object→任何引用类型
    if (from == Vb6Type::Object) return true;

    return false;
}

Vb6Type TypeSystem::promote(Vb6Type a, Vb6Type b) {
    // Variant: 结果总是Variant
    if (a == Vb6Type::Variant || b == Vb6Type::Variant)
        return Vb6Type::Variant;

    // 相同类型
    if (a == b) return a;

    // String连接 (&): 结果为String
    if (a == Vb6Type::String && b == Vb6Type::String)
        return Vb6Type::String;

    // 数值提升 (按大小: Byte < Integer < Long < Single < Double < Currency)
    // 简化: 使用类型大小决定
    auto rank = [](Vb6Type t) -> int {
        switch (t) {
            case Vb6Type::Byte:    return 1;
            case Vb6Type::Integer: return 2;
            case Vb6Type::Boolean: return 2;
            case Vb6Type::Long:    return 3;
            case Vb6Type::Single:  return 4;
            case Vb6Type::Double:  return 5;
            case Vb6Type::Currency:return 6;
            case Vb6Type::Date:    return 5;  // Date内部是Double
            default:               return 0;
        }
    };

    if (isNumeric(a) && isNumeric(b)) {
        return rank(a) >= rank(b) ? a : b;
    }

    // 混合Numeric和String → Variant
    if ((isNumeric(a) && isString(b)) || (isString(a) && isNumeric(b)))
        return Vb6Type::Variant;

    return Vb6Type::Variant;
}

int TypeSystem::typeSize(Vb6Type t) {
    switch (t) {
        case Vb6Type::Byte:     return 1;
        case Vb6Type::Boolean:  return 2;
        case Vb6Type::Integer:  return 2;
        case Vb6Type::Long:     return 4;
        case Vb6Type::Single:   return 4;
        case Vb6Type::Double:   return 8;
        case Vb6Type::Currency: return 8;
        case Vb6Type::Date:     return 8;
        case Vb6Type::String:   return 4;  // BSTR指针
        case Vb6Type::Object:   return 4;  // IDispatch指针
        case Vb6Type::Variant:  return 16; // VARIANT
        default:                return 0;
    }
}

Vb6Type TypeSystem::defaultIntType(int64_t val) {
    if (val >= -32768 && val <= 32767) return Vb6Type::Integer;
    if (val >= -2147483648LL && val <= 2147483647LL) return Vb6Type::Long;
    return Vb6Type::Long;  // VB6没有LongLong, 超出范围仍为Long
}

} // namespace vb6c3
