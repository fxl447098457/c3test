#include "backend/cgen.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <functional>

namespace vb6c3 {

// --- cgen_util_com.cpp: COM 值解析 + 打包标记 + 类字段名规范化 ---



// ============================================================
// COM辅助 (P6.2)
// ============================================================

std::string CCodeGen::resolveComValue(const std::string& unpackType) {
    // P24-07: 早期绑定 — 利用签名returnType选择正确的解包函数
    if (!isComMarker_) return lastExpr_;

    std::string objExpr = std::move(comObjExpr_);
    std::string memberName = std::move(comMemberName_);

    // P24-07: 早期绑定推断 — 利用TypeLib签名的returnType决策
    if (isEarlyBoundCom_ && earlyBoundSym_) {
        isEarlyBoundCom_ = false;
        const Symbol* comSym = earlyBoundSym_;
        earlyBoundSym_ = nullptr;
        std::string memLower = memberName;
        std::transform(memLower.begin(), memLower.end(), memLower.begin(), ::tolower);
        auto it = comSym->comMethods.find(memLower);
        if (it != comSym->comMethods.end()) {
            const auto& sig = it->second;
            std::string returnType = mapType(sig.returnType);
            std::string getPropArgs = objExpr + ", L\"" + memberName + "\"";
            if (returnType == "BSTR") {
                lastExpr_ = "vb6_ComGetStringProp(" + getPropArgs + ")";
            } else if (returnType == "int32_t" || returnType == "int16_t") {
                lastExpr_ = "vb6_ComGetIntProp(" + getPropArgs + ")";
            } else if (returnType == "double" || returnType == "float") {
                lastExpr_ = "vb6_ComGetDoubleProp(" + getPropArgs + ")";
            } else if (returnType == "void*") {
                lastExpr_ = "vb6_ComGetObjectProp(" + getPropArgs + ")";
            } else {
                // P24-07: 未知返回类型(如Enum→UserDefinedType) → 按目标变量类型选择
                if (unpackType == "BSTR") {
                    lastExpr_ = "vb6_ComGetStringProp(" + getPropArgs + ")";
                } else if (unpackType == "Int" || unpackType == "Long" || unpackType == "Boolean") {
                    lastExpr_ = "vb6_ComGetIntProp(" + getPropArgs + ")";
                } else if (unpackType == "Double" || unpackType == "Single") {
                    lastExpr_ = "vb6_ComGetDoubleProp(" + getPropArgs + ")";
                } else if (unpackType == "Object") {
                    lastExpr_ = "vb6_ComGetObjectProp(" + getPropArgs + ")";
                } else {
                    lastExpr_ = "vb6_VariantFromComResult(vb6_ComCall(" + objExpr + ", L\"" + memberName + "\", NULL, 0))";
                }
            }
            isComMarker_ = false;
            return lastExpr_;
        }
    }
    isEarlyBoundCom_ = false;
    isComMarker_ = false;

    std::string getPropArgs = objExpr + ", L\"" + memberName + "\"";

    if (unpackType == "BSTR") {
        lastExpr_ = "vb6_ComGetStringProp(" + getPropArgs + ")";
    } else if (unpackType == "Int" || unpackType == "Long" || unpackType == "Boolean") {
        lastExpr_ = "vb6_ComGetIntProp(" + getPropArgs + ")";
    } else if (unpackType == "Double" || unpackType == "Single") {
        lastExpr_ = "vb6_ComGetDoubleProp(" + getPropArgs + ")";
    } else if (unpackType == "Object") {
        lastExpr_ = "vb6_ComGetObjectProp(" + getPropArgs + ")";
    } else if (unpackType == "Variant") {
        // P24-02: COM属性返回原生VARIANT(如dic.Keys/dic.Items返回SAFEARRAY)
        lastExpr_ = "vb6_VariantFromComResult(vb6_ComCall(" + objExpr + ", L\"" + memberName + "\", NULL, 0))";
    } else {
        // 默认: BSTR解封 (最通用, COM VARIANT → BSTR自动转换)
        lastExpr_ = "vb6_ComGetStringProp(" + getPropArgs + ")";
    }
    return lastExpr_;
}


// Fix 092m: 目标类字段类型 → COM 解包类型 hint (见 cgen.hpp 声明注释).
// 从当前模块作用域的 Class 符号 (跨模块 external Class 已由 driver 拷贝该表) 取
// memberFieldTypes[字段名] 并映射; 任何缺失都回退 "BSTR" (comGetStringProp 最通用,
// 与 Fix 092m 之前的默认行为一致, 不引入回归).
std::string CCodeGen::classFieldComUnpackHint(const std::string& className,
                                              const std::string& memberName) const {
    if (className.empty() || !symTab_.moduleScope()) return "BSTR";
    const std::string want = Symbol::toLower(className);
    const std::string fld = Symbol::toLower(memberName);
    for (const auto& kv : symTab_.moduleScope()->symbols()) {
        const Symbol* cs = kv.second.get();
        if (!cs || cs->kind != SymbolKind::Class) continue;
        if (Symbol::toLower(cs->name) != want) continue;
        auto it = cs->memberFieldTypes.find(fld);
        if (it == cs->memberFieldTypes.end()) return "BSTR";
        const std::string tn = Symbol::toLower(it->second);
        if (tn == "string") return "BSTR";
        if (tn == "long" || tn == "integer" || tn == "boolean" || tn == "byte") return "Long";
        if (tn == "single" || tn == "double" || tn == "date" || tn == "currency") return "Double";
        if (tn == "object") return "Object";
        if (tn == "variant" || tn == "var") return "Variant";
        // 命名类型 (项目类/UDT) → 对象解包; 未知名字回退 BSTR
        return symTab_.lookup(it->second) ? "Object" : "BSTR";
    }
    return "BSTR";
}


// Fix 092p: 类数据字段名规范化 (见 cgen.hpp 声明注释).
std::string CCodeGen::canonicalClassFieldName(const std::string& className,
                                              const std::string& memberName) const {
    if (className.empty() || !symTab_.moduleScope()) return memberName;
    const std::string want = Symbol::toLower(className);
    const std::string fld = Symbol::toLower(memberName);
    for (const auto& kv : symTab_.moduleScope()->symbols()) {
        const Symbol* cs = kv.second.get();
        if (!cs || cs->kind != SymbolKind::Class) continue;
        if (Symbol::toLower(cs->name) != want
            && Symbol::toLower(cs->sourceModule) != want) continue;
        auto it = cs->memberFieldNames.find(fld);
        return (it != cs->memberFieldNames.end()) ? it->second : memberName;
    }
    return memberName;
}


// Fix 093a: 当前类是否声明了同名成员字段 — 裸标识符赋值 (`field = value`) 时,
// 本类字段优先于从全局符号表捡到的外部同名 Property Let/Set (VB6 里同一类中
// 字段与属性不可能同名). 典型: cClientCallback.cls 的 `recvBuffer = data`
// (recvBuffer 是本类 Public 字段) 被误命中 cWinsock 的 Property Let RecvBuffer,
// 生成 vb6_cWinsock_prop_let_recvBuffer((void*)me, ...) → LNK2019.
bool CCodeGen::isOwnClassField(const std::string& memberName) const {
    if (!isClassModule_ || moduleName_.empty() || !symTab_.moduleScope()) return false;
    const std::string want = Symbol::toLower(moduleName_);
    const std::string fld = Symbol::toLower(memberName);
    for (const auto& kv : symTab_.moduleScope()->symbols()) {
        const Symbol* cs = kv.second.get();
        if (!cs || cs->kind != SymbolKind::Class) continue;
        if (Symbol::toLower(cs->name) != want
            && Symbol::toLower(cs->sourceModule) != want) continue;
        return cs->memberFieldNames.count(fld) > 0;
    }
    return false;
}


// Fix 093a: 类成员(方法/属性)名规范化 — VB6 大小写不敏感, 类内声明与调用点拼写
// 可能不同 (类里声明 `Count` 而调用点写 `count`; 或类里是 `test` 调用点写 `Test`),
// 而 C 符号大小写敏感: 生成的函数名与类定义不一致 → LNK2019. 以 Class 符号
// memberNames 中的声明拼写为准 (与 cIdent 规范化类名同理). 表中无此项时原样返回.
std::string CCodeGen::canonicalClassMemberName(const std::string& className,
                                               const std::string& memberName) const {
    if (className.empty() || !symTab_.moduleScope()) return memberName;
    const std::string want = Symbol::toLower(className);
    const std::string mem = Symbol::toLower(memberName);
    for (const auto& kv : symTab_.moduleScope()->symbols()) {
        const Symbol* cs = kv.second.get();
        if (!cs || cs->kind != SymbolKind::Class) continue;
        if (Symbol::toLower(cs->name) != want
            && Symbol::toLower(cs->sourceModule) != want) continue;
        for (const auto& mn : cs->memberNames) {
            if (Symbol::toLower(mn) == mem) return mn;
        }
        return memberName;
    }
    return memberName;
}


std::string CCodeGen::comPackExpr(Expr& expr) {
    // 根据表达式类型推断应该用的VARIANT封装函数
    Vb6Type vt = inferExprType(expr);
    switch (vt) {
        case Vb6Type::String:
            return "vb6_ComPackBSTR";  // BSTR → VARIANT
        case Vb6Type::Integer:
        case Vb6Type::Long:
            return "vb6_ComPackInt";   // int32_t → VARIANT
        case Vb6Type::Boolean:
            return "vb6_ComPackBool";  // VB6 Boolean → VARIANT VT_BOOL
        case Vb6Type::Single:
        case Vb6Type::Double:
            return "vb6_ComPackDouble"; // double → VARIANT
        case Vb6Type::Object:
            return "vb6_ComPackObject"; // void* → VARIANT
        case Vb6Type::Variant:
            return "vb6_ComPackValue";  // Fix 030: 通用打包宏 — 路由任意 C 类型实参 (inferExprType 回退 Variant 时安全)
        default:
            // Variant/未知: 尝试用BSTR封装 (运行时会处理转换)
            // 更安全的做法: 检查已知变量类型
            if (expr.kind == ASTNodeKind::IdentifierExpr) {
                auto& id = static_cast<IdentifierExpr&>(expr);
                std::string lower = id.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if (knownObjectVars_.count(lower)) return "vb6_ComPackObject";
                if (knownBstrVars_.count(lower)) return "vb6_ComPackBSTR";
                if (knownDoubleVars_.count(lower)) return "vb6_ComPackDouble";
                if (knownLongVars_.count(lower)) return "vb6_ComPackInt";
                if (knownVariantVars_.count(lower)) return "vb6_ComPackVariant";
            }
            if (expr.kind == ASTNodeKind::MemberAccessExpr) {
                auto& ma = static_cast<MemberAccessExpr&>(expr);
                if (ma.object && ma.object->kind == ASTNodeKind::IdentifierExpr) {
                    auto& objId = static_cast<IdentifierExpr&>(*ma.object);
                    std::string objLower = objId.name;
                    std::transform(objLower.begin(), objLower.end(), objLower.begin(), ::tolower);
                    if (knownVariantVars_.count(objLower)) return "vb6_ComPackVariant";
                }
            }
            return "vb6_ComPackInt";  // 默认整数封装
    }
}


// P25: 解析COM标记为类型化属性取值, 用于COM调用参数打包
// 当isComMarker_为true时, 根据packFnHint选择对应类型的COM属性取值函数
// 如果isComMarker_为false, 返回空串
std::string CCodeGen::resolveComMarkerForPack(const std::string& packFnHint) {
    // P26: vb6_ComPackVariant / Fix 030: vb6_ComPackValue 需要把 COM 调用返回的
    // VARIANT* 转成 vb6_VARIANT (即使 isComMarker_ 已被消费, lastExpr_ 仍可能是 COM 调用结果).
    // vb6_ComPackValue(vb6_VariantFromComResult(ComCall)) 经 _Generic VariantIdentity 路径
    // 等价于 vb6_ComPackVariant(vb6_VariantFromComResult(ComCall)).
    if (packFnHint == "vb6_ComPackVariant" || packFnHint == "vb6_ComPackValue") {
        if (lastExpr_.find("vb6_ComCall(") == 0 ||
            lastExpr_.find("vb6_ComGetProp(") == 0 ||
            lastExpr_.find("vb6_ComGetObjectProp(") == 0 ||
            lastExpr_.find("vb6_ComCallObject(") == 0) {
            return "vb6_VariantFromComResult(" + lastExpr_ + ")";
        }
    }
    if (!isComMarker_) return "";
    isComMarker_ = false;
    std::string objExpr = std::move(comObjExpr_);
    std::string memName = std::move(comMemberName_);

    // 前期绑定: 利用签名确定返回类型
    if (isEarlyBoundCom_ && earlyBoundSym_) {
        isEarlyBoundCom_ = false;
        const Symbol* comSym = earlyBoundSym_;
        earlyBoundSym_ = nullptr;
        std::string memLower = memName;
        std::transform(memLower.begin(), memLower.end(), memLower.begin(), ::tolower);
        auto it = comSym->comMethods.find(memLower);
        if (it != comSym->comMethods.end() && it->second.isPropertyGet) {
            const auto& sig = it->second;
            std::string returnType = mapType(sig.returnType);
            if (returnType == "int32_t" || returnType == "int16_t") {
                return "vb6_ComGetIntProp(" + objExpr + ", L\"" + memName + "\")";
            } else if (returnType == "BSTR") {
                return "vb6_ComGetStringProp(" + objExpr + ", L\"" + memName + "\")";
            } else if (returnType == "double" || returnType == "float") {
                return "vb6_ComGetDoubleProp(" + objExpr + ", L\"" + memName + "\")";
            } else if (returnType == "void*") {
                return "vb6_ComGetObjectProp(" + objExpr + ", L\"" + memName + "\")";
            }
        }
    }

    // 后期绑定: 根据packFnHint推断所需的属性取值函数
    // packFnHint由comPackExpr根据上下文确定, 代表参数期望的C类型
    if (packFnHint == "vb6_ComPackObject") {
        return "vb6_ComGetObjectProp(" + objExpr + ", L\"" + memName + "\")";
    } else if (packFnHint == "vb6_ComPackBSTR" || packFnHint.empty()) {
        return "vb6_ComGetStringProp(" + objExpr + ", L\"" + memName + "\")";
    } else if (packFnHint == "vb6_ComPackInt") {
        return "vb6_ComGetIntProp(" + objExpr + ", L\"" + memName + "\")";
    } else if (packFnHint == "vb6_ComPackDouble") {
        return "vb6_ComGetDoubleProp(" + objExpr + ", L\"" + memName + "\")";
    } else if (packFnHint == "vb6_ComPackBool") {
        return "vb6_ComGetIntProp(" + objExpr + ", L\"" + memName + "\")";
    }
    // vb6_ComPackVariant / Fix 030 vb6_ComPackValue: 需要把 COM 返回的 VARIANT* 转成 vb6_VARIANT
    if (packFnHint == "vb6_ComPackVariant" || packFnHint == "vb6_ComPackValue") {
        if (lastExpr_.find("vb6_ComCall(") == 0 ||
            lastExpr_.find("vb6_ComGetProp(") == 0 ||
            lastExpr_.find("vb6_ComGetObjectProp(") == 0 ||
            lastExpr_.find("vb6_ComCallObject(") == 0) {
            return "vb6_VariantFromComResult(" + lastExpr_ + ")";
        }
    }
    return "";
}
} // namespace vb6c3
