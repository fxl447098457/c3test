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
        {"longptr", Vb6Type::LongPtr},    // Fix 081e: LongPtr = architecture-width integer
        // Fix 084m: LongLong 走自己的枚举 —— 恒为 64 位有符号, 不随架构退化 (见 types.hpp)
        {"longlong", Vb6Type::LongLong},
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
        // C3 扩展 (ai/032): 无符号/小整型族。位宽对照 Integer/Long/LongLong,
        // 见 common/types.hpp 的 Vb6Type 注释。等宽于既有类型, 所以走同一套
        // isIntegral / typeSize / promote 阶梯即可, 不需要第二套数值口径。
        {"sbyte", Vb6Type::SByte},       // 8-bit signed
        {"uinteger", Vb6Type::UInteger}, // 16-bit unsigned
        {"ulong", Vb6Type::ULong},       // 32-bit unsigned
        {"ulonglong", Vb6Type::ULongLong},// 64-bit unsigned
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
    // Fix 161: `VB.` / `VBA.` 是 VB6 **自身类型库的限定前缀** (末段才是类型名),
    // 与下方"Vb 前缀 ⇒ 枚举 ⇒ Long"的启发式不是一回事, 但那句
    // `lower.compare(0, 2, "vb") == 0` 会把 `vb.control` 一并吞掉 → 返回 Long。
    // 本函数是两层共同上游 (codegen mapTypeRef 在返回非 Unknown 时直接 mapType
    // 短路, 不再走它自己的 Vb 前缀兜底), 所以判错会**两层一致地**降级成标量:
    //   VisualStyles.bas 229/232:
    //     SetupVisualStylesFixes(ByVal Form As VB.Form) / Dim CurrControl As VB.Control
    //   → VisualStyles.c 135/137: int32_t Form, int32_t CurrControl
    //   → Form.Controls / CurrControl.Style 是**标量的成员**, 走 M22 降级成全局名
    //     vb6_Form_Controls / vb6_CurrControl_Style           → C2065 ×4
    //   → CurrControl = <For Each 取到的 Variant>              → C2440 ×2
    //   → ProperControlName(ByVal Control As VB.Control) 同理影响 Common.c 813/815/817
    // 判 Object 后 mapType=void*, 成员访问回到 COM 属性读取路径。
    // 只收这 4 个已核实为对象类型的末段 (demo 里 VB./VBA. 限定的全部取值)。
    // **必须放在本函数最前 (仅次于 builtinTypes_)**: 下方的 Enum/Constants 后缀
    // 与 "Vb"/"OLE_" 前缀启发式都会抢先命中 `vb.xxx` (首版就因此毫无效果)。
    if (lower.size() > 3) {
        std::string leaf161;
        if (lower.compare(0, 3, "vb.") == 0) leaf161 = lower.substr(3);
        else if (lower.compare(0, 4, "vba.") == 0) leaf161 = lower.substr(4);
        if (leaf161 == "control" || leaf161 == "form" || leaf161 == "usercontrol"
            || leaf161 == "mdiform") {
            return Vb6Type::Object;
        }
    }
    // Fix 050: COM 枚举类型 (如 DataTypeEnum, CursorTypeEnum, LockTypeEnum)
    // 来自引用的 COM 类型库, 不在项目符号表中。VB6 枚举底层是 Long。
    // 以 "Enum" 结尾的类型名视为 Long, 使 resolveTypeRef 和 cgen_decl 中的
    // resolveTypeName 调用都能正确返回 Long, 避免参数被误判为 Variant。
    if (lower.size() > 4 && lower.compare(lower.size() - 4, 4, "enum") == 0) {
        return Vb6Type::Long;
    }
    // Fix 098: VB6 标准库枚举 (XxxConstants, 如 ColorConstants/ScaleModeConstants/
    // KeyCodeConstants) 的底层类型是 Long。若视为 Unknown 会退化成 Variant,
    // 使 `Optional lColor As ColorConstants = -1` 生成 Variant** 参数并把指针
    // 当作 COLORREF 传给 TTM_SETTIPBKCOLOR/TTM_SETTIPTEXTCOLOR → 控件全黑。
    if (lower.size() > 9 && lower.compare(lower.size() - 9, 9, "constants") == 0) {
        return Vb6Type::Long;
    }
    // Fix 050b: VB6 类型别名 — 这些类型在 mapTypeRef() 中被映射为 int32_t,
    // 但 resolveTypeName() 返回 Unknown, 导致 resolveTypeRef() 回退到 Variant,
    // 与代码生成器的 int32_t 不一致, 产生 vb6_VARIANT→int32_t C2440 错误。
    // LongPtr/LongLong已加入builtinTypes_ (Vb6Type::LongPtr → intptr_t)
    // OLE_ 前缀类型 (OLE_COLOR, OLE_HANDLE 等) — DWORD = Long
    if (lower.size() >= 4 && lower.compare(0, 4, "ole_") == 0) {
        return Vb6Type::Long;
    }
    // Vb 前缀枚举 (VbCompareMethod, VbTriState, VbFileAttribute 等) — Long
    if (lower.size() >= 2 && lower.compare(0, 2, "vb") == 0 &&
        lower != "boolean" && lower != "byte") {
        return Vb6Type::Long;
    }
    // Fix 097: 无 vb 前缀的 VBA 枚举别名 — CompareMethod (VbCompareMethod 的
    // 公开别名, VB6 类型库两个名字都可见). 与 vb 前缀规则同理视为 Long,
    // 否则 cgen 的 resolveArrayElemType 回落 Variant 而字段声明映射 int32_t,
    // 两处不一致 → 类工厂生成 me->m_CompareMode = vb6_VariantEmpty() → C2440
    // (Dictionary.cls:45 "Private m_CompareMode As CompareMethod").
    if (lower == "comparemethod") {
        return Vb6Type::Long;
    }
    // Fix 160-G: MSDATASRC 库限定的 String 别名。MSDATASRC.tlb (Data Binding
    // Collection) 里 DataSource / DataMember 都是 LPWSTR 的 typedef, VB6 中即
    // String。整串限定名两层都查不到 → 语义层回退 Variant (本函数调用方),
    // 而 mapTypeRef 兜底是 "未知类型 → void*" (cgen_base_type.cpp 末尾) —
    // 两侧不一致使同一字段一半按 Variant 记账、一半按 void* 声明:
    //   VBFlexGrid.ctl 2030: Private PropDataMember As MSDATASRC.DataMember
    //   → VBFlexGrid.h 1685 字段 void*, 而 prop_let 发
    //     me->PropDataMember = vb6_VariantFromValue(Value)  → C2440 VARIANT→void*
    //   → prop_get 内 DataMember = PropDataMember 经 092f 又套
    //     vb6_VariantToObjectVal(void*)                       → 反向 C2440
    // 只匹配 `msdatasrc.` 限定形式, 不收录裸 DataSource/DataMember, 以免遮蔽
    // 工程自定义同名符号 (本函数的检查发生在符号表查找之前)。
    if (lower.rfind("msdatasrc.", 0) == 0) {
        std::string leaf = lower.substr(10);
        if (leaf == "datasource" || leaf == "datamember") return Vb6Type::String;
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
        case Vb6Type::LongPtr:         return "LongPtr";
        case Vb6Type::LongLong:        return "LongLong";
        case Vb6Type::Void:            return "Void";
        case Vb6Type::Unknown:         return "Unknown";
        default:                        return "?";
    }
}

bool TypeSystem::isNumeric(Vb6Type t) {
    return isIntegral(t) || isFloat(t) || t == Vb6Type::Currency || t == Vb6Type::Decimal;
}

bool TypeSystem::isIntegral(Vb6Type t) {
    return t == Vb6Type::Byte || t == Vb6Type::Integer ||
           t == Vb6Type::Long || t == Vb6Type::Boolean ||
           t == Vb6Type::LongLong || t == Vb6Type::LongPtr ||    // Fix 084m
           // C3 扩展 (ai/032): 4 个新整型。ULong **必须在这里** —— 它此前只在
           // mapType/typeSize 有映射而没进 isIntegral, 于是 `ULong + Long` 这类
           // 算不出来 (promote 拿 isNumeric 卡住 → 一路退化成 Variant)。
           t == Vb6Type::SByte || t == Vb6Type::UInteger ||
           t == Vb6Type::ULong || t == Vb6Type::ULongLong;
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
            // C3 扩展 (ai/032): 新增整型按位宽取与既有同宽类型相同的档位, 有符号/无符号
            // 不另分档 —— VB6 的算术结果本来就只看宽度 (VB6 无无符号类型, 也就没有
            // "有符号+无符号怎么提升"的先例可循, 取等宽即最高位宽胜出的最简口径)。
            case Vb6Type::Byte:    return 1;
            case Vb6Type::SByte:   return 1;
            case Vb6Type::Integer: return 2;
            case Vb6Type::UInteger:return 2;
            case Vb6Type::Boolean: return 2;
            case Vb6Type::Long:    return 3;
            case Vb6Type::ULong:   return 3;
            case Vb6Type::Single:  return 4;
            case Vb6Type::Double:  return 5;
            case Vb6Type::Currency:return 6;
            case Vb6Type::Decimal:  return 7;  // P20-07: Decimal wider than Currency
            case Vb6Type::Date:    return 5;  // Date内部是Double
            // 64 位整数档 (C3 扩展 ai/032 顺带补齐): LongLong 以前不在本表里, 于是
            // 落到 default 的 0 档 —— `LongLong + Long` 会被判成 Long (32 位), 高位
            // 静默丢失。ULongLong 与之同档。这是本次的既有行为改动, 由
            // tests/run_tests.ps1 全量门禁背书。
            case Vb6Type::LongLong:return 6;
            case Vb6Type::ULongLong: return 6;
            // LongPtr **刻意不登记**: 它的宽度是目标相关的 (x86 4 字节 / x64 8 字节),
            // 而 TypeSystem 这一层拿不到目标架构 (只有后端的 targetArch_, 且
            // typeSize(LongPtr) 本身也还在用宿主 sizeof(void*), 属既有隐患 Fix 081e)。
            // 给它写死任何一档都会错: 写 6 会让 `LongPtr + Double` 被判成 LongPtr,
            // CStr 走整型入口把小数截掉 —— 负控实测 (登记 6 档后编
            // tests/test_vbnet_ext.bas): CStr(p + 1.5) 出 4、CStr(p * 2.5) 出 7,
            // 且 x86 与 x64 两个目标**都是** 4/7 (档位是宿主侧算的, 与目标无关)。
            // 不登记则落 default 0 档 = 由对侧胜出, 与改动前完全一致 —— 目标相关的
            // 修法 (按 targetArch_ 取档) 需要先把架构信息贯通到语义层, 不在本次范围。
            default:               return 0;
        }
    };

    if (isNumeric(a) && isNumeric(b)) {
        // 浮点/货币/Decimal 压过整型 —— 与位宽无关。
        // 光靠下面那句 rank 比较表达不出这条: 64 位整型按位宽排在 Single(4) /
        // Double(5) 之上, 于是 `x + 1.5` 会被判成整型, 上层 CStr 若按整型入口发码
        // 就会把小数截掉。负控实测 (把 LongLong/ULongLong/LongPtr 一律登记 6 档、
        // 且去掉本规则后编 tests/test_vbnet_ext.bas):
        //     CStr(u3 + 1.5)  = 5      ← 截断 (vb6_CStrULongLong)
        //     CStr(u3 + CCur(1.5)) = 5 ← 截断
        //     CStr(p + 1.5)  = 4      ← 截断 (p As LongPtr, 登记 6 档)
        //     CStr(ll + 1.5) = 5.5    ← **没**截断: LongLong 在 CStr 那侧没有
        //                              整型分支, 走的是 Variant 路 (这条正好说明
        //                              "rank 答错" 会不会真丢值取决于下游发码,
        //                              所以判据必须落在真算出来的值上)
        // VB6/VB.NET 里 `Long + Double` 是 Double, 64 位整型不该例外。
        // 对改动前的类型表这是**恒等变换**: 浮点/货币/Decimal 档 (4~7) 本来就
        // 高于所有整型档 (1~3), 走到下面那句也同样是它们胜 —— 只对本次新加的
        // 64 位整型 (LongLong/ULongLong) 生效; LongPtr 根本没登记档位 (见上),
        // 它靠本规则与 rank 的 0 档一起维持改动前行为。
        const bool aWide = isFloat(a) || a == Vb6Type::Currency || a == Vb6Type::Decimal;
        const bool bWide = isFloat(b) || b == Vb6Type::Currency || b == Vb6Type::Decimal;
        if (aWide != bWide) return aWide ? a : b;
        return rank(a) >= rank(b) ? a : b;
    }

    // 混合Numeric和String → Variant
    if ((isNumeric(a) && isString(b)) || (isString(a) && isNumeric(b)))
        return Vb6Type::Variant;

    return Vb6Type::Variant;
}

Vb6Type TypeSystem::bitwiseResult(Vb6Type a, Vb6Type b) {
    // VB6 的 And/Or/Xor/Eqv/Imp 是**位运算**，不是短路逻辑运算：只有两侧本身都是
    // Boolean 时结果才是 Boolean，否则是两侧提升后的数值型。发码层 (cgen_expr_binary
    // Fix 039) 早就把两侧转 Long 再 & | ^，所以以前只有「类型被问错」——
    // 于是 `CStr(a Or b)`、`"x=" & (a And b)`、`vb6_ComPack*(hDC Or 0)` 打成 True/False
    // 或装箱成 VT_BOOL。规则只在这里写一份，语义层与发码层都调它 (账 #216)。
    if (a == Vb6Type::Boolean && b == Vb6Type::Boolean) return Vb6Type::Boolean;
    if (isNumeric(a) && isNumeric(b)) return promote(a, b);
    return Vb6Type::Variant;   // 字符串/对象/未知: 交给运行期 (VB6 是 Type Mismatch)
}

Vb6Type TypeSystem::logicalNotResult(Vb6Type t) {
    // 同上：`Not` 对数值是按位取反 (发码层出 `~`)，只有操作数是 Boolean 才是布尔非。
    // Byte/Integer 一起答 Integer —— VB6 把这两档先按 Integer 处理，答成 Byte 会让
    // `b = Not b2` 绕过溢出检查、把 -1 静默 wrap 成 255。
    if (t == Vb6Type::Boolean || t == Vb6Type::Variant) return t;
    if (t == Vb6Type::Byte || t == Vb6Type::Integer) return Vb6Type::Integer;
    // C3 扩展 (ai/032): 8/16 位新整型与 Byte/Integer 同口径收进 Integer (同上面的
    // 理由: 窄类型的按位取反结果可能超出原宽度, 答宽一档才不绕过溢出检查);
    // 32/64 位各自保留。
    if (t == Vb6Type::SByte || t == Vb6Type::UInteger) return Vb6Type::Integer;
    if (t == Vb6Type::Long || t == Vb6Type::LongPtr || t == Vb6Type::LongLong ||
        t == Vb6Type::ULong || t == Vb6Type::ULongLong) return t;
    if (isFloat(t) || t == Vb6Type::Currency || t == Vb6Type::Decimal) return Vb6Type::Long;
    return Vb6Type::Variant;
}

int TypeSystem::typeSize(Vb6Type t) {
    switch (t) {
        case Vb6Type::Byte:     return 1;
        case Vb6Type::SByte:    return 1;  // C3 扩展 (ai/032)
        case Vb6Type::Boolean:  return 2;
        case Vb6Type::Integer:  return 2;
        case Vb6Type::UInteger: return 2;  // C3 扩展 (ai/032)
        case Vb6Type::Long:     return 4;
        case Vb6Type::Single:   return 4;
        case Vb6Type::Double:   return 8;
        case Vb6Type::Currency: return 8;
        case Vb6Type::Date:     return 8;
        case Vb6Type::String:   return 4;  // BSTR指针
        case Vb6Type::Object:   return 4;  // IDispatch指针
        case Vb6Type::Variant:  return 16; // VARIANT
        case Vb6Type::Decimal:  return 16;  // P20-07: DECIMAL is 14 bytes but aligned to 16
        case Vb6Type::ULong:    return 4;
        case Vb6Type::LongPtr:  return static_cast<int>(sizeof(void*));  // Fix 081e: 架构宽度
        case Vb6Type::LongLong: return 8;   // Fix 084m: 恒 64 位有符号
        case Vb6Type::ULongLong: return 8;  // C3 扩展 (ai/032): 恒 64 位无符号
        default:                return 0;
    }
}

Vb6Type TypeSystem::defaultIntType(int64_t val) {
    if (val >= -32768 && val <= 32767) return Vb6Type::Integer;
    if (val >= -2147483648LL && val <= 2147483647LL) return Vb6Type::Long;
    return Vb6Type::Long;  // VB6没有LongLong, 超出范围仍为Long
}

} // namespace vb6c3
