// vb6c3 - Inherits 发码 (tB 扩展; ai/022 记录 D24③④/D26, 批次 B07b)
//
// 派生类的**前缀兼容布局** + **转发桩**:
//   struct vb6_cls_Derived { void* __comObj; <祖先字段, 根→叶> <自有字段>; }
//   vb6_Derived_M(vb6_cls_Derived* me, …) { return vb6_Base_M((vb6_cls_Base*)me, …); }
//
// 为什么走桩而不是在调用点裸强转 `me` (D24④): ① B08 的 Overridable 要有唯一挂钩点;
// ② B09 的 MyBase 要去虚化; ③ 调用点强转要求前缀布局逐字段一致, 而那是可以出错的假设。
// 指针转换在 C 里对**不完整类型**也合法 (成员访问才非法), 所以桩只要求祖先类型名可见。
//
// 与 stage 3.4 (Driver::mergeInheritedMembers) 同源: 这里遍历的是它回填的 inhFields/inhProcs,
// 因此"结构体字段清单"与"可调用成员清单"不可能各按一套规则走。

#include "backend/cgen.hpp"

#include "ast/ast.hpp"
#include "semantics/class_chain_registry.hpp"
#include "semantics/interface_sig.hpp"  // ifaceLower

#include <string>
#include <vector>

namespace vb6c3 {

namespace {

// 与 cgen_iface_vtbl.cpp 的 ivParamsOf/ivImplCName 同一口径。那边是文件内匿名 namespace 的
// 自由函数, 跨 TU 取不到 → 这里各写一份 (5 行, 不改 B04 的热点文件为准)。
std::vector<std::unique_ptr<ParameterDecl>>* paramsOf(Decl& d) {
    switch (d.kind) {
        case ASTNodeKind::SubDecl:      return &static_cast<SubDecl&>(d).params;
        case ASTNodeKind::FunctionDecl: return &static_cast<FunctionDecl&>(d).params;
        case ASTNodeKind::PropertyDecl: return &static_cast<PropertyDecl&>(d).params;
        default:                        return nullptr;
    }
}

// 成员的方向前缀: 属性在 C 名里带 prop_get_/prop_let_/prop_set_, 与 makePropertySignature、
// resolveClassMemberCall 的取名一致 —— 桩必须用同一套, 否则链接期找不到定义。
std::string procBaseName(Decl& d) {
    switch (d.kind) {
        case ASTNodeKind::SubDecl:      return static_cast<SubDecl&>(d).name;
        case ASTNodeKind::FunctionDecl: return static_cast<FunctionDecl&>(d).name;
        case ASTNodeKind::PropertyDecl: {
            PropertyDecl& p = static_cast<PropertyDecl&>(d);
            const char* prefix = p.propKind == ProcKind::PropertyGet ? "prop_get_"
                               : p.propKind == ProcKind::PropertySet ? "prop_set_"
                                                                     : "prop_let_";
            return std::string(prefix) + p.name;
        }
        default: return std::string();
    }
}

AccessLevel accessOf(const Decl& d) {
    switch (d.kind) {
        case ASTNodeKind::SubDecl:      return static_cast<const SubDecl&>(d).access;
        case ASTNodeKind::FunctionDecl: return static_cast<const FunctionDecl&>(d).access;
        case ASTNodeKind::PropertyDecl: return static_cast<const PropertyDecl&>(d).access;
        default: return AccessLevel::Private;
    }
}

} // namespace

const ClassChainView* CCodeGen::classChainOf(const Module& module) const {
    if (!clsreg_ || clsreg_->empty() || !isClassModule_) return nullptr;
    auto it = clsreg_->find(ifaceLower(module.moduleName));
    if (it == clsreg_->end() || it->second.mod != &module) return nullptr;
    if (it->second.chainBroken || it->second.chain.size() < 2) return nullptr;
    return &it->second;
}

std::vector<VariableDecl*> CCodeGen::structFieldDecls(Module& module) {
    std::vector<VariableDecl*> out;
    // 继承字段在前 (根→叶), 自有字段在后 —— 派生实例的前缀必须与祖先自己的 struct 一字不差。
    // 无继承时这里只有自有字段, 输出与 B07b 之前逐字节相同 (零回归护栏)。
    if (const ClassChainView* v = classChainOf(module)) {
        for (Decl* d : v->inhFields) {
            if (d && d->kind == ASTNodeKind::VariableDecl) out.push_back(static_cast<VariableDecl*>(d));
        }
    }
    for (auto& decl : module.declarations) {
        if (decl && decl->kind == ASTNodeKind::VariableDecl) {
            out.push_back(&static_cast<VariableDecl&>(*decl));
        }
    }
    return out;
}

// 桩的返回类型文本: 与 makeProcSignature / makePropertySignature 同一套 mapTypeRef 口径。
std::string CCodeGen::inheritedRetType(Decl& d) {
    if (d.kind == ASTNodeKind::FunctionDecl) {
        return mapTypeRef(static_cast<FunctionDecl&>(d).returnType.get());
    }
    if (d.kind == ASTNodeKind::PropertyDecl) {
        PropertyDecl& p = static_cast<PropertyDecl&>(d);
        return p.propKind == ProcKind::PropertyGet ? mapTypeRef(p.returnType.get())
                                                   : std::string("void");
    }
    return "void";
}

// 桩签名: 返回类型与形参表都取自**祖先的声明节点**, 但类名前缀与 me 形参取当前 (派生) 模块。
std::string CCodeGen::inheritedStubSig(Decl& d) {
    std::string name = cProcName(procBaseName(d), accessOf(d), moduleName_);
    std::string params = classMeParam();
    if (auto* ps = paramsOf(d)) {
        for (auto& p : *ps) params += ", " + makeParamCType(p.get(), false);
        for (auto& p : *ps) {
            if (p->isOptional && !p->isParamArray) params += ", int _has_" + cIdent(p->name);
        }
    }
    return inheritedRetType(d) + " " + name + "(" + params + ")";
}

// .h: 桩的前向声明 (祖先的 Private 成员在 3.4 就被挡在可见面外, 到这里只剩 Public/Friend)
void CCodeGen::emitInheritedProcDecls(Module& module) {
    const ClassChainView* v = classChainOf(module);
    if (!v || v->inhProcs.empty()) return;
    h_.emitBlank();
    h_.emitLine("// === Inherits (tB B07b): 继承成员的转发桩声明 ===");
    for (const auto& ip : v->inhProcs) {
        if (!ip.decl) continue;
        h_.emitLine(inheritedStubSig(*ip.decl) + ";");
    }
}

// .c: 桩定义。祖先过程名按**祖先模块**取, me 先转成祖先指针再转调。
void CCodeGen::emitInheritedProcDefs(Module& module) {
    const ClassChainView* v = classChainOf(module);
    if (!v || v->inhProcs.empty()) return;
    c_.emitBlank();
    c_.emitLine("// === Inherits (tB B07b): 继承成员的转发桩 ===");
    for (const auto& ip : v->inhProcs) {
        if (!ip.decl || !ip.owner) continue;
        Decl& d = *ip.decl;
        const std::string sig = inheritedStubSig(d);
        const std::string target = cProcName(procBaseName(d), accessOf(d), ip.owner->moduleName);
        const std::string baseStruct = "vb6_cls_" + cIdent(ip.owner->moduleName);
        // 祖先过程的 me 形参是它自己的指针类型 → 转成祖先指针再转调。括号是**两层**: 外层是
        // 调用, 内层是强转 (少了外层括号就是 C2059, 这条在最小工程里实测过)。
        std::string call = "((" + baseStruct + "*)me";
        if (auto* ps = paramsOf(d)) {
            for (auto& p : *ps) call += ", " + cIdent(p->name);
            for (auto& p : *ps) {
                if (p->isOptional && !p->isParamArray) call += ", _has_" + cIdent(p->name);
            }
        }
        call += ")";
        const bool isVoid = inheritedRetType(d) == "void";
        c_.emitLine(sig + " { " + (isVoid ? "" : "return ") + target + call + "; }");
    }
    c_.emitBlank();
}

} // namespace vb6c3
