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

#include <cctype>
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

// ============================================================
// 类虚表 (tB B08d): __cvtbl 字段 + 本类视图的表类型 + 表实例 + 调用点间接派发
//
// 一张表/一个类, 表项是"本类面上这个成员的实现" (本类的定义, 或 B07b 已发的转发桩) → 表实例
// 与桩同一条取名口径, 不会链接期找不到定义。
//
// 为什么字段是 `const void*` 而不是 typed 表指针: 链上每个类的视图槽数不同 (祖先只看到它自己
// 之上的槽), 但前缀布局要求 `__cvtbl` 在整条链上**同一个偏移**、同一个字段 → 类型只能是
// void*, 由用点按本类视图强转。于是任何 TU 里都不会出现 `vb6_cvtbl_A*`↔`vb6_cvtbl_B*` 这种
// 不兼容指针转换 (只有 void*↔自家类型), 一个警告都不会多。
// ============================================================

const ClassChainView* CCodeGen::classViewOf(const Module& module) const {
    if (!clsreg_ || clsreg_->empty()) return nullptr;
    auto it = clsreg_->find(ifaceLower(module.moduleName));
    if (it == clsreg_->end() || it->second.mod != &module) return nullptr;
    if (it->second.chainBroken) return nullptr;
    return &it->second;
}

const ClassChainView* CCodeGen::classViewByName(const std::string& cls) const {
    if (!clsreg_ || clsreg_->empty() || cls.empty()) return nullptr;
    auto it = clsreg_->find(ifaceLower(cls));
    if (it == clsreg_->end() || it->second.chainBroken || !it->second.mod) return nullptr;
    return &it->second;
}

std::string CCodeGen::cvtblImplName(const ClassChainView& v,
                                    const ClassChainView::VirtSlot& s) const {
    if (!s.impl || !v.mod) return std::string();
    // 与本类那份定义 (makeProcSignature / inheritedStubSig) 同一套拼名: 桩名也用**本类**模块名。
    return cProcName(procBaseName(*s.impl), accessOf(*s.impl), v.mod->moduleName);
}

// .h: 结构体里的 `const void* __cvtbl` (位置: __comObj/__iv_ 之后、祖先字段之前 —— 见 ai/022 D32②)
void CCodeGen::emitClassVirtField(Module& module) {
    const ClassChainView* v = classViewOf(module);
    if (!v || v->virtSlots.empty()) return;  // 无虚槽的类连字段都不加 → 零新语法逐字节不变
    h_.emitLine("    const void* __cvtbl;  /* tB Inherits B08d: &vb6_cvtbl_" +
                cIdent(v->name) + "_impl; 祖先视图按前缀索引派生表 */");
}

// .h: 本类视图的表类型 + 表实例的跨函数声明 (实例定义发在 .c 末尾, 那里所有过程体已落地)
void CCodeGen::emitClassVirtType(Module& module) {
    const ClassChainView* v = classViewOf(module);
    if (!v || v->virtSlots.empty()) return;
    const std::string tbl = "vb6_cvtbl_" + cIdent(v->name);
    const std::string meStruct = "vb6_cls_" + cIdent(v->name);
    h_.emitBlank();
    h_.emitLine("// === Inherits (tB B08d): 类虚表类型 (本类视图; 更深的类以它作前缀) ===");
    h_.emitLine("typedef struct " + tbl + " {");
    for (const auto& s : v->virtSlots) {
        std::string params = meStruct + "* me";
        if (auto* ps = paramsOf(*s.impl)) {
            for (auto& p : *ps) params += ", " + makeParamCType(p.get(), false);
            for (auto& p : *ps) {
                if (p->isOptional && !p->isParamArray) params += ", int _has_" + cIdent(p->name);
            }
        }
        h_.emitLine("    " + inheritedRetType(*s.impl) + " (*" + cIdent(s.field) + ")(" +
                    params + ");  /* slot '" + s.slotKey + "' declared by " +
                    (s.owner ? s.owner->moduleName : std::string("?")) + " */");
    }
    h_.emitLine("} " + tbl + ";");
    h_.emitLine("extern const " + tbl + " " + tbl + "_impl;");
}

// .c: _New() 装载本类的表 (每个具体类装自己那一张)
void CCodeGen::emitClassVirtNewInit(Module& module) {
    const ClassChainView* v = classViewOf(module);
    if (!v || v->virtSlots.empty()) return;
    c_.emitLine("me->__cvtbl = &vb6_cvtbl_" + cIdent(v->name) +
                "_impl;  /* tB Inherits B08d */");
}

// .c 末尾: 表实例。发在**所有过程体之后**是因为本类的 Private 实现是 static 定义, 之前没有原型。
void CCodeGen::emitClassVirtImpl(Module& module) {
    const ClassChainView* v = classViewOf(module);
    if (!v || v->virtSlots.empty()) return;
    const std::string tbl = "vb6_cvtbl_" + cIdent(v->name);
    c_.emitBlank();
    c_.emitLine("// === Inherits (tB B08d): 类虚表实例 ===");
    c_.emitLine("const " + tbl + " " + tbl + "_impl = {");
    for (const auto& s : v->virtSlots) {
        const std::string fn = cvtblImplName(*v, s);
        if (fn.empty()) continue;  // 3.4b 已判死"没有入口"的槽; 这里只是不发明一个空名字
        c_.emitLine("    " + fn + ",  /* " + s.slotKey + " */");
    }
    c_.emitLine("};");
}

// 对象表达式能否**安全地用两次** (取表一次、me 实参一次)。间接调用会把调用点的对象文本复制
// 进两个位置, 所以只有**不含函数调用**的表达式可以: 标识符、`(*c)` (ByRef 类形参)、
// `((vb6_cls_X*)b)` (Fix 090 的强转)、`me->m_oSocket` (取字段) 都是纯读 → 可以;
// `vb6_cls_X_Default()`、`vb6_X_prop_get_pvSocket(me)` 这类带调用的 → 不行 (求值两次就是副作用)。
// 判据就是"有没有一个左括号紧跟在标识符/下标/右括号之后" —— 强转的左括号前面是运算符, 不算。
static bool cvtblObjIsPure(const std::string& t) {
    auto isTailOfCall = [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == ')' ||
               c == ']' || c == '"' || c == '\'';
    };
    if (t.empty()) return false;
    for (size_t i = 1; i < t.size(); i++) {
        if (t[i] == '(' && isTailOfCall(t[i - 1])) return false;
    }
    return true;
}

// 调用点改写: `obj.M(...)` / `Me.M(...)` 命中本类视图的槽 → 换成按 __cvtbl 索引。
// 返回空 = 不改写 (该类无表 / 该成员不是槽 / 对象形状不支持且不要求必须派发)。
// mustDispatch=true 时, "是槽但对象形状不支持" 会**报错**而不是退回静态绑定 —— 那正是
// D27-13 判例要避免的"编得过但跑错"。默认实例 (vb6_cls_X_Default()) 那类调用点传 false:
// 它的动态类型恒等于静态类型, 直调本来就正确。
std::string CCodeGen::virtDispatchCallee(const std::string& cls, const std::string& member,
                                         const std::string& objExpr, bool mustDispatch,
                                         const SourceLocation& loc) {
    const ClassChainView* v = classViewByName(cls);
    if (!v || v->virtSlots.empty()) return std::string();
    const std::string lk = ifaceLower(member);
    // 读上下文的优先级与 resolveClassMemberCall 一致: Get > Sub/Function (这里不会出现 Let/Set,
    // 3.4b 已把它们判死) → 同名同时有 Property Get 和 Sub 时不能各挑一头。
    const ClassChainView::VirtSlot* getHit = nullptr;
    const ClassChainView::VirtSlot* plainHit = nullptr;
    for (const auto& s : v->virtSlots) {
        if (s.nameKey != lk) continue;
        if (s.slotKey == "get_" + lk) { if (!getHit) getHit = &s; }
        else if (s.slotKey == lk)     { if (!plainHit) plainHit = &s; }
    }
    const ClassChainView::VirtSlot* hit = getHit ? getHit : plainHit;
    if (!hit) return std::string();
    const std::string clsId = "vb6_cls_" + cIdent(v->name);
    if (!cvtblObjIsPure(objExpr)) {
        if (!mustDispatch) return std::string();  // 调用点接受直调 (默认实例等静态即正确形状)
        diag_.error(DiagnosticID::SemVirtualNotSupported, loc,
            "Call to overridable member '" + member + "' through object expression '" + objExpr +
            "' cannot be dispatched in this build (only a plain variable / Me / (*var) receiver"
            " is supported; ai/022 B08d)");
        return std::string();
    }
    // 拼出来的形状: ((const vb6_cvtbl_A*)((vb6_cls_A*)me)->__cvtbl)->slot
    // 三层括号: 最外裹住整个 cast (否则 -> 会先结合到 me 上), 里层裹 me 的强转。
    // `*)` 与 `)*` 的分别就是"指针强转"与"解引用一个不存在的类型", 别手滑换回去。
    return "((const vb6_cvtbl_" + cIdent(v->name) + "*)"
           "((" + clsId + "*)" + objExpr + ")->__cvtbl)->" + cIdent(hit->field);
}

} // namespace vb6c3
