#pragma once
// vb6c3 - Interface 槽键与签名比对 (tB 扩展; ai/022 D2, 批次 B02)
//
// 同一套规范函数同时服务两侧: driver 的 stage 2.7 建槽表、语义层的 Implements 契约
// 比对。两侧走同一函数 = "接口怎么写、实现就得怎么写" 只有一个定义, 不会漂移。
//
// 比对口径 = **源码签名** (类型引用原文小写 + 参数个数 + ByVal/Optional/ParamArray
// + 返回类型), 不做 Vb6Type 归一: 跨模块的 Enum/UDT 名在 stage 3 早期尚未注入,
// 归一后比反而会把"其实正确的实现"误判成不符 (Fix 047 同源问题)。

#include "ast/ast.hpp"
#include <string>
#include <vector>

namespace vb6c3 {

inline std::string ifaceLower(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) out += (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
    return out;
}

// 类型引用的规范文本 (nullptr = 未写 As = Variant)
inline std::string ifaceTypeText(const ASTNode* t) {
    if (!t) return "variant";
    switch (t->kind) {
        case ASTNodeKind::SimpleTypeRef:
            return ifaceLower(static_cast<const SimpleTypeRef*>(t)->name);
        case ASTNodeKind::ArrayTypeRef:
            return ifaceTypeText(static_cast<const ArrayTypeRef*>(t)->elementType.get()) + "()";
        case ASTNodeKind::FixedStringTypeRef:
            return "fixedstring";
        default:
            return "?";
    }
}

// COM 惯例槽键 (D2): 属性拆三槽, Sub/Function 用原名
inline std::string ifaceSlotKey(const Decl& d) {
    std::string n = ifaceLower(
        d.kind == ASTNodeKind::SubDecl      ? static_cast<const SubDecl&>(d).name :
        d.kind == ASTNodeKind::FunctionDecl ? static_cast<const FunctionDecl&>(d).name :
        d.kind == ASTNodeKind::PropertyDecl ? static_cast<const PropertyDecl&>(d).name : std::string());
    if (d.kind == ASTNodeKind::PropertyDecl) {
        switch (static_cast<const PropertyDecl&>(d).propKind) {
            case ProcKind::PropertyGet: return "get_" + n;
            case ProcKind::PropertyLet: return "put_" + n;
            case ProcKind::PropertySet: return "putref_" + n;
            default: break;
        }
    }
    return n;
}

struct IfaceParamSig {
    std::string type;
    bool isByVal = false;
    bool isOptional = false;
    bool isParamArray = false;
};

struct IfaceProcSig {
    std::string slotKey;
    std::string memberName;            // 声明原样名 (诊断可读)
    std::string procKind;              // "Sub" / "Function" / "Property Get" / ...
    std::string retType;               // 无返回值 (Sub/Let/Set) = ""
    std::vector<IfaceParamSig> params;
    std::string text;                  // 诊断用可读签名
    const Decl* decl = nullptr;        // 出处节点 (诊断定位)
};

inline std::string ifaceDeclName(const Decl& d) {
    return d.kind == ASTNodeKind::SubDecl      ? static_cast<const SubDecl&>(d).name :
           d.kind == ASTNodeKind::FunctionDecl ? static_cast<const FunctionDecl&>(d).name :
           d.kind == ASTNodeKind::PropertyDecl ? static_cast<const PropertyDecl&>(d).name
                                               : std::string();
}

// 仅 Sub/Function/Property 是接口成员/实现成员; 其余声明返回 false
inline bool ifaceSigFromDecl(const Decl& d, IfaceProcSig& out) {
    out = IfaceProcSig();
    out.decl = &d;
    out.memberName = ifaceDeclName(d);
    const std::vector<std::unique_ptr<ParameterDecl>>* params = nullptr;
    const ASTNode* ret = nullptr;
    switch (d.kind) {
        case ASTNodeKind::SubDecl: {
            const auto& n = static_cast<const SubDecl&>(d);
            out.procKind = "Sub"; out.slotKey = ifaceSlotKey(d); params = &n.params;
            break;
        }
        case ASTNodeKind::FunctionDecl: {
            const auto& n = static_cast<const FunctionDecl&>(d);
            out.procKind = "Function"; out.slotKey = ifaceSlotKey(d);
            params = &n.params; ret = n.returnType.get(); out.retType = ifaceTypeText(ret);
            break;
        }
        case ASTNodeKind::PropertyDecl: {
            const auto& n = static_cast<const PropertyDecl&>(d);
            out.slotKey = ifaceSlotKey(d); params = &n.params;
            if (n.propKind == ProcKind::PropertyGet) {
                out.procKind = "Property Get";
                out.retType = ifaceTypeText(n.returnType.get());
            } else {
                out.procKind = n.propKind == ProcKind::PropertyLet ? "Property Let"
                                                                     : "Property Set";
            }
            break;
        }
        default:
            return false;
    }
    out.text = out.procKind + " " + out.memberName + "(";
    for (const auto& p : *params) {
        IfaceParamSig ps;
        ps.type = ifaceTypeText(p->asType.get());
        ps.isByVal = p->isByVal;
        ps.isOptional = p->isOptional;
        ps.isParamArray = p->isParamArray;
        out.text += (out.params.empty() ? "" : ", ");
        out.text += (ps.isParamArray ? "ParamArray " : (ps.isByVal ? "ByVal " : "ByRef "));
        if (ps.isOptional) out.text += "Optional ";
        out.text += ps.type;
        out.params.push_back(std::move(ps));
    }
    out.text += ")";
    if (!out.retType.empty()) out.text += " As " + out.retType;
    return true;
}

inline bool ifaceSigEqual(const IfaceProcSig& a, const IfaceProcSig& b) {
    if (a.procKind != b.procKind || a.retType != b.retType) return false;
    if (a.params.size() != b.params.size()) return false;
    for (size_t i = 0; i < a.params.size(); i++) {
        if (a.params[i].type != b.params[i].type) return false;
        if (a.params[i].isByVal != b.params[i].isByVal) return false;
        if (a.params[i].isOptional != b.params[i].isOptional) return false;
        if (a.params[i].isParamArray != b.params[i].isParamArray) return false;
    }
    return true;
}

} // namespace vb6c3
