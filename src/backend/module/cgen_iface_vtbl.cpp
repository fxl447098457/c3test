// vb6c3 - tB 式 Interface 契约发码 (ai/022 D2/D3, 批次 B04)
//
// 本文件只负责"新式 `Interface ... End Interface`"这条路径，与 legacy VB6 的
// `IFoo_M` 前缀扫描 + 胖对 `vb6_iface_<I>{vtbl,obj}` (cgen_com.cpp) 完全隔离：
//
//   类型（工程级，只发一次，#ifndef 守卫）
//     typedef struct vb6_ivref_I { ... } vb6_ivref_I;   // 薄指针的指向类型（单词）
//     typedef struct vb6_ivtbl_I { QI, AddRef, Release, <自有槽…> } vb6_ivtbl_I;
//   实现类
//     vb6_cls_C { void* __comObj; vb6_ivref_I __iv_I; … }   // __comObj 必须仍是第 0 字段 (D19)
//     static <R> vb6_iimpl_C_I_<slot>(vb6_ivref_I* self, …) // container_of 后直调成员
//     static const vb6_ivtbl_I vb6_ivtbl_I_for_C = { … };
//     vb6_cls_C_New(): me->__iv_I.vt = &vb6_ivtbl_I_for_C;
//   接口值 = &obj->__iv_I（D3 的薄指针口径），派发见 cgen_expr_call_ivref 分支.
//
// IUnknown 三件套：B04 是**占位**（E_NOTIMPL / 常量计数），槽号因此从 B04 起永久固定。
// B05 把 AddRef/Release 换成真计数（引用计数头 `__refcount` 只落在实现新式接口的类上，
// 无新语法的工程一个字节都不变），QueryInterface 仍占位到 B06（TypeOf/转换同批）.

#include "backend/cgen.hpp"

#include "semantics/interface_sig.hpp"
#include "semantics/interfaces_registry.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

namespace vb6c3 {
namespace {

// 限定名末段：`Project.IFoo` → `IFoo`（与 parseImplements 的点号拼接对称, Fix 083）
std::string ivLastSegment(const std::string& s) {
    size_t p = s.rfind('.');
    return p == std::string::npos ? s : s.substr(p + 1);
}

bool ivNameMatches(const std::string& written, const std::string& name) {
    const std::string lower = ifaceLower(name);
    return ifaceLower(written) == lower || ifaceLower(ivLastSegment(written)) == lower;
}

std::vector<std::unique_ptr<ParameterDecl>>* ivParamsOf(Decl& d) {
    switch (d.kind) {
        case ASTNodeKind::SubDecl:      return &static_cast<SubDecl&>(d).params;
        case ASTNodeKind::FunctionDecl: return &static_cast<FunctionDecl&>(d).params;
        case ASTNodeKind::PropertyDecl: return &static_cast<PropertyDecl&>(d).params;
        default:                        return nullptr;
    }
}

// B06a: 接口 IID —— 16 字节，按真实 GUID 内存序摆（Data1/2/3 小端、Data4 原序），
// 这样 P6 把它交给真 COM 时不必再翻字节。取值优先级：源码里的 [InterfaceId("...")]
// （IfaceView.guid，B01 起只写不读，本批开始有消费者）→ 否则按 4 词 FNV-1a 从接口
// 小写名确定性派生（与 cgen_util_dllentry_prelude.inc 的 generateIid 同族；D8 禁止随机）。
uint32_t ivFnv1a(const std::string& s, uint32_t seed) {
    uint32_t h = seed;
    for (unsigned char c : s) { h ^= c; h *= 0x01000193u; }
    return h;
}

bool ivParseGuidText(const std::string& text, unsigned char out[16]) {
    auto digitVal = [](char c) -> int { return c <= '9' ? c - '0' : (c - 'a' + 10); };
    std::string hex;
    for (char ch : text) {
        char lo = (char)tolower((unsigned char)ch);
        if (isdigit((unsigned char)lo) || (lo >= 'a' && lo <= 'f')) hex.push_back(lo);
    }
    if (hex.size() != 32) return false;
    unsigned char b[16];
    for (int i = 0; i < 16; i++) {
        b[i] = (unsigned char)(digitVal(hex[(size_t)i * 2]) * 16 + digitVal(hex[(size_t)i * 2 + 1]));
    }
    // 可读序: Data1=b[0..3] Data2=b[4..5] Data3=b[6..7] Data4=b[8..15]
    out[0] = b[3]; out[1] = b[2]; out[2] = b[1]; out[3] = b[0];
    out[4] = b[5]; out[5] = b[4];
    out[6] = b[7]; out[7] = b[6];
    for (int i = 8; i < 16; i++) out[i] = b[i];
    return true;
}

void ivDeriveIid(const IfaceView& v, unsigned char out[16]) {
    if (!v.guid.empty() && ivParseGuidText(v.guid, out)) return;
    const std::string key = "iviface:" + ifaceLower(v.name);
    const uint32_t h1 = ivFnv1a(key, 0xa1b2c3d4u);
    const uint32_t h2 = ivFnv1a(key, 0xe5f60718u);
    const uint32_t h3 = ivFnv1a(key, 0x9a0b1c2du);
    const uint32_t h4 = ivFnv1a(key, 0x3e4f5061u);
    const uint32_t d1 = h1;
    const uint32_t d2 = (h2 >> 16) & 0xFFFFu;
    const uint32_t d3 = ((h2 & 0xFFFFu) | 0x4000u) & 0xFFFFu;   // version 4
    const uint32_t d4a = ((((h3 >> 16) & 0xFFFFu) | 0x8000u)) & 0xFFFFu;  // variant 1
    const uint32_t d4b = (h3 & 0xFFFFu);
    out[0] = (unsigned char)(d1 >> 24); out[1] = (unsigned char)(d1 >> 16);
    out[2] = (unsigned char)(d1 >> 8);  out[3] = (unsigned char)d1;
    out[4] = (unsigned char)(d2 >> 8);  out[5] = (unsigned char)d2;
    out[6] = (unsigned char)(d3 >> 8);  out[7] = (unsigned char)d3;
    out[8] = (unsigned char)(d4a >> 8); out[9] = (unsigned char)d4a;
    out[10] = (unsigned char)(d4b >> 8); out[11] = (unsigned char)d4b;
    out[12] = (unsigned char)(h4 >> 24); out[13] = (unsigned char)(h4 >> 16);
    out[14] = (unsigned char)(h4 >> 8);  out[15] = (unsigned char)h4;
}

std::string ivIidInitializer(const unsigned char b[16]) {
    static const char* hex = "0123456789ABCDEF";
    std::string out = "{ ";
    for (int i = 0; i < 16; i++) {
        if (i) out += ",";
        out += std::string("0x") + hex[(b[i] >> 4) & 0xF] + hex[b[i] & 0xF];
    }
    out += " }";
    return out;
}

} // namespace

// ============================================================
// 查找
// ============================================================

const IfaceView* CCodeGen::ivLookupIface(const std::string& written) {
    if (!ivreg_) return nullptr;
    auto it = ivreg_->find(ifaceLower(written));
    if (it == ivreg_->end()) it = ivreg_->find(ifaceLower(ivLastSegment(written)));
    if (it == ivreg_->end()) return nullptr;
    if (it->second.chainBroken || it->second.slots.empty()) return nullptr;
    return &it->second;
}

std::string CCodeGen::ivrefCType(const std::string& typeName) {
    const IfaceView* v = ivLookupIface(typeName);
    return v ? "vb6_ivref_" + cIdent(v->name) + "*" : std::string();
}

// 本模块（类）实现了哪些新式接口：按 `Implements` 书写序，同名去重
std::vector<const IfaceView*> CCodeGen::ivImplementedIfaces(Module& module) {
    std::vector<const IfaceView*> out;
    std::vector<std::string> seen;
    for (const auto& impl : module.implements) {
        if (!impl) continue;
        const IfaceView* v = ivLookupIface(impl->interfaceName);
        if (!v) continue;
        const std::string key = ifaceLower(v->name);
        if (std::find(seen.begin(), seen.end(), key) != seen.end()) continue;
        seen.push_back(key);
        out.push_back(v);
    }
    return out;
}

// 槽的实现成员：显式子句优先，其次同名隐式匹配（与语义层同一套口径, D16-2）
Decl* CCodeGen::ivFindImplMember(Module& module, const IfaceView& v, const IfaceSlotView& slot) {
    for (auto& d : module.declarations) {
        if (!d) continue;
        const std::vector<ImplementsClause>* clauses = ifaceProcClauses(*d);
        if (!clauses || clauses->empty()) continue;
        for (const auto& c : *clauses) {
            if (!ivNameMatches(c.ifaceName, slot.ownerIface) &&
                !ivNameMatches(c.ifaceName, v.name)) {
                continue;
            }
            // 允许写成员名 (`I.Name`) 或直接写槽名 (`I.get_Name`)
            if (ifaceClauseSlotKey(*d, c.memberName) == slot.key ||
                ifaceLower(c.memberName) == slot.key) {
                return d.get();
            }
        }
    }
    for (auto& d : module.declarations) {
        if (!d) continue;
        const std::vector<ImplementsClause>* clauses = ifaceProcClauses(*d);
        if (clauses && !clauses->empty()) continue;  // 写了子句 = 只按子句入座
        IfaceProcSig sig;
        if (!ifaceSigFromDecl(*d, sig)) continue;
        if (sig.slotKey == slot.key) return d.get();
    }
    return nullptr;
}

// ============================================================
// 签名文本
// ============================================================

// 调用点成员名 -> 槽键：先按 Sub/Function 的裸名匹配，再按属性三槽前缀回退
// （`s.Name` 读属性 → get_name；写属性由赋值路径自己带前缀，B04b 之后补）
std::string CCodeGen::ivSlotKeyForMember(const IfaceView& v, const std::string& member) {
    const std::string bare = ifaceLower(member);
    for (const IfaceSlotView& s : v.slots) {
        if (s.key == bare) return s.key;
    }
    for (const char* p : {"get_", "put_", "putref_"}) {
        const std::string prefixed = std::string(p) + bare;
        for (const IfaceSlotView& s : v.slots) {
            if (s.key == prefixed) return s.key;
        }
    }
    return std::string();
}

std::string CCodeGen::ivSlotRetType(const Decl* sig) {
    if (!sig) return "void";
    Decl& d = *const_cast<Decl*>(sig);
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

// 形参声明文本（不含 self），与 makeParamList 同口径：ByRef→指针，Optional 追加 _has_ 尾标记
std::string CCodeGen::ivParamDecls(Decl& d) {
    std::string out;
    std::vector<std::unique_ptr<ParameterDecl>>* ps = ivParamsOf(d);
    if (!ps) return out;
    for (auto& p : *ps) out += ", " + makeParamCType(p.get(), false);
    for (auto& p : *ps) {
        if (p->isOptional && !p->isParamArray) out += ", int _has_" + cIdent(p->name);
    }
    return out;
}

// 适配器转调实参（不含 me），顺序必须与 ivParamDecls 严格一致
std::string CCodeGen::ivForwardArgs(Decl& d) {
    std::string out;
    std::vector<std::unique_ptr<ParameterDecl>>* ps = ivParamsOf(d);
    if (!ps) return out;
    for (auto& p : *ps) out += ", " + cIdent(p->name);
    for (auto& p : *ps) {
        if (p->isOptional && !p->isParamArray) out += ", _has_" + cIdent(p->name);
    }
    return out;
}

// 实现成员对应的 C 过程名（与 cgen_com.cpp 的 legacy 取名口径一致）
std::string CCodeGen::ivImplCName(Decl& d) {
    const std::string mod = isClassModule_ ? moduleName_ : std::string();
    if (d.kind == ASTNodeKind::SubDecl) {
        SubDecl& n = static_cast<SubDecl&>(d);
        return cProcName(n.name, n.access, mod);
    }
    if (d.kind == ASTNodeKind::FunctionDecl) {
        FunctionDecl& n = static_cast<FunctionDecl&>(d);
        return cProcName(n.name, n.access, mod);
    }
    if (d.kind == ASTNodeKind::PropertyDecl) {
        PropertyDecl& n = static_cast<PropertyDecl&>(d);
        std::string prefix = "prop_let_";
        if (n.propKind == ProcKind::PropertyGet) prefix = "prop_get_";
        else if (n.propKind == ProcKind::PropertySet) prefix = "prop_set_";
        return cProcName(prefix + n.name, n.access, mod);
    }
    return std::string();
}

// ============================================================
// 头文件：工程级接口类型（#ifndef 守卫，任何模块用到都自足）
// ============================================================

void CCodeGen::emitIfaceContractTypedefs() {
    if (!ivreg_ || ivreg_->empty()) return;  // 无新式接口的工程：零输出（护栏）

    std::vector<const IfaceView*> views;
    for (const auto& kv : *ivreg_) views.push_back(&kv.second);
    // unordered_map 迭代顺序不稳定 → 按小写名排序，保证生成物可复现
    std::sort(views.begin(), views.end(), [](const IfaceView* a, const IfaceView* b) {
        return ifaceLower(a->name) < ifaceLower(b->name);
    });

    bool headerDone = false;
    bool iidBaseDone = false;
    for (const IfaceView* v : views) {
        if (v->chainBroken || v->slots.empty()) continue;
        if (!headerDone) {
            h_.emitLine("// === tB-style Interface contract slot tables (ai/022 B04) ===");
            headerDone = true;
        }
        const std::string id = cIdent(v->name);
        const std::string tbl = "vb6_ivtbl_" + id;
        const std::string ref = "vb6_ivref_" + id;
        std::string guard = "VB6_IVTBL_" + id;
        for (char& ch : guard) {
            if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
        }
        // IUnknown 的 IID 全工程唯一一份（同样每 TU 一份 static 常量 → 只能按值比）
        if (!iidBaseDone) {
            iidBaseDone = true;
            h_.emitLine("#ifndef VB6_IV_IID_IUNKNOWN");
            h_.emitLine("#define VB6_IV_IID_IUNKNOWN");
            h_.emitLine("/* IID of IUnknown {00000000-0000-0000-C000-000000000046}, GUID memory order */");
            h_.emitLine("static const unsigned char vb6_iv_iid_IUnknown[16] = "
                        "{ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46 };");
            h_.emitLine("#endif");
            h_.emitBlank();
        }
        h_.emitLine("#ifndef " + guard);
        h_.emitLine("#define " + guard);
        h_.emitLine("typedef struct " + ref + " " + ref + ";");
        h_.emitLine("typedef struct " + tbl + " {");
        h_.indent();
        h_.emitLine("/* IUnknown prefix slots: AddRef/Release real in B05, QueryInterface in B06a */");
        h_.emitLine("long (*QueryInterface)(void* self, const void* riid, void** ppv);");
        h_.emitLine("unsigned long (*AddRef)(void* self);");
        h_.emitLine("unsigned long (*Release)(void* self);");
        for (const IfaceSlotView& slot : v->slots) {
            h_.emitLine(ivSlotRetType(slot.sig) + " (*" + cIdent(slot.key) + ")(" +
                        ref + "* self" + ivParamDeclsRef(slot.sig) + ");");
        }
        h_.dedent();
        h_.emitLine("} " + tbl + ";");
        h_.emitLine("struct " + ref + " { const " + tbl + "* vt; };");
        // B06a: 本接口的 IID（QueryInterface 按值比这个 16 字节块）
        {
            unsigned char iid[16];
            ivDeriveIid(*v, iid);
            h_.emitLine("static const unsigned char vb6_iv_iid_" + id + "[16] = " +
                        ivIidInitializer(iid) + ";  /* " +
                        (v->guid.empty() ? "derived from name" : "from InterfaceId attribute") + " */");
        }
        h_.emitLine("#endif");
        h_.emitBlank();
    }
}

// 头文件槽声明用的形参文本（接口侧签名可能是 const 节点）
std::string CCodeGen::ivParamDeclsRef(const Decl* sig) {
    if (!sig) return std::string();
    return ivParamDecls(*const_cast<Decl*>(sig));
}

// ============================================================
// 类结构体字段 + _New 初始化
// ============================================================

void CCodeGen::emitIfaceClassFields(Module& module) {
    const std::vector<const IfaceView*> ifaces = ivImplementedIfaces(module);
    if (ifaces.empty()) return;
    // B05: 引用计数头. 只落在"实现新式接口"的类上 —— 无新语法的工程结构体逐字节不变.
    // 位置在 __comObj (第 0 字段, D19 硬约束) 之后、__iv_<I> 槽字段之前.
    h_.emitLine("    int32_t __refcount;  /* tB Interface B05: 1 at New, +1 per owning ref */");
    for (const IfaceView* v : ifaces) {
        const std::string id = cIdent(v->name);
        h_.emitLine("    vb6_ivref_" + id + " __iv_" + id +
                    ";  /* tB Interface " + v->name + " (B04): iface ptr = &me->__iv_" + id + " */");
    }
}

void CCodeGen::emitIfaceNewInit(Module& module) {
    const std::string clsId = cIdent(moduleName_);
    const std::vector<const IfaceView*> ifaces = ivImplementedIfaces(module);
    if (ifaces.empty()) return;
    // B05: _New 交出的那一次引用由"创建者"持有: 类变量永不 Release (现状),
    // 而 `Set <接口变量> = New <类>` 走引用移交 (不 AddRef) → 该接口变量就是唯一主人.
    c_.emitLine("me->__refcount = 1;  /* tB Interface B05 */");
    for (const IfaceView* v : ifaces) {
        const std::string id = cIdent(v->name);
        c_.emitLine("me->__iv_" + id + ".vt = &vb6_ivtbl_" + id + "_for_" + clsId + ";");
    }
}

// ============================================================
// 类侧：适配器 + 槽表实例
// ============================================================

void CCodeGen::emitIfaceImplTables(Module& module) {
    const std::vector<const IfaceView*> ifaces = ivImplementedIfaces(module);
    if (ifaces.empty()) return;

    const std::string clsId = cIdent(moduleName_);
    const std::string clsStruct = "vb6_cls_" + clsId;

    c_.emitBlank();
    c_.emitLine("// === tB Interface contract impl: " + module.moduleName + " (ai/022 B04) ===");

    // IUnknown 三件套一律按 (类, 接口) 各一份：`self` 是某个 `__iv_<I>` 的地址，
    // 回推实例、以及 QI 命中"本类的另一个接口"时要减/加的偏移都依赖具体接口
    // （B05 已为 AddRef/Release 这么做了，QI 同理）。先给本类全部 AddRef 发前向
    // 声明，QI 才能在任意发射顺序下调到兄弟接口的 AddRef。
    for (const IfaceView* v : ifaces) {
        c_.emitLine("static unsigned long vb6_iunk_" + clsId + "_" + cIdent(v->name) +
                    "_AddRef(void* self);");
    }

    for (const IfaceView* v : ifaces) {
        const std::string id = cIdent(v->name);
        const std::string ref = "vb6_ivref_" + id;

        // B06a: 真 QueryInterface —— 认 IUnknown、本接口、以及本类实现的其它接口
        const std::string walkQI = "    " + clsStruct + "* me = (" + clsStruct + "*)((char*)self - offsetof(" +
                                   clsStruct + ", __iv_" + id + "));";
        c_.emitBlank();
        c_.emitLine("static long vb6_iunk_" + clsId + "_" + id + "_QueryInterface(void* self, const void* riid, void** ppv) {");
        c_.indent();
        c_.emitLine(walkQI);
        c_.emitLine("(void)me;  /* 无兄弟接口时上面的回推用不到 */");
        c_.emitLine("if (!ppv) return 0x80070057L;  /* E_POINTER */");
        c_.emitLine("*ppv = NULL;");
        c_.emitLine("if (!riid) return 0x80070057L;  /* E_POINTER */");
        c_.emitLine("if (vb6_IidEqual(riid, vb6_iv_iid_IUnknown) || vb6_IidEqual(riid, vb6_iv_iid_" + id + ")) {");
        c_.indent();
        c_.emitLine("*ppv = self;");
        c_.emitLine("vb6_iunk_" + clsId + "_" + id + "_AddRef(self);");
        c_.emitLine("return 0L;  /* S_OK */");
        c_.dedent();
        c_.emitLine("}");
        for (const IfaceView* s : ifaces) {
            if (s == v) continue;
            const std::string sid = cIdent(s->name);
            c_.emitLine("if (vb6_IidEqual(riid, vb6_iv_iid_" + sid + ")) {");
            c_.indent();
            c_.emitLine("*ppv = &me->__iv_" + sid + ";");
            c_.emitLine("vb6_iunk_" + clsId + "_" + sid + "_AddRef(*ppv);");
            c_.emitLine("return 0L;  /* S_OK */");
            c_.dedent();
            c_.emitLine("}");
        }
        c_.emitLine("return 0x80004002L;  /* E_NOINTERFACE: not implemented by this class */");
        c_.dedent();
        c_.emitLine("}");

        // B05: 真引用计数 (self = &me->__iv_<I>)
        const std::string walk = "    " + clsStruct + "* me = (" + clsStruct + "*)((char*)self - offsetof(" +
                                 clsStruct + ", __iv_" + id + "));";
        c_.emitBlank();
        c_.emitLine("static unsigned long vb6_iunk_" + clsId + "_" + id + "_AddRef(void* self) {");
        c_.indent();
        c_.emitLine(walk);
        c_.emitLine("me->__refcount += 1;");
        c_.emitLine("return (unsigned long)me->__refcount;");
        c_.dedent();
        c_.emitLine("}");
        c_.emitLine("static unsigned long vb6_iunk_" + clsId + "_" + id + "_Release(void* self) {");
        c_.indent();
        c_.emitLine(walk);
        c_.emitLine("me->__refcount -= 1;");
        c_.emitLine("if (me->__refcount > 0) return (unsigned long)me->__refcount;");
        // 实例已被 COM 包装器接管时 (__comObj 非空) 销毁权在包装器 (P6/B13 统一两套计数)
        c_.emitLine("if (me->__comObj != NULL) return 0UL;  /* wrapper owns teardown */");
        c_.emitLine(clsStruct + "_Destroy(me);  /* 0 引用: Class_Terminate + 释放 */");
        c_.emitLine("return 0UL;");
        c_.dedent();
        c_.emitLine("}");

        std::vector<std::string> slotFns;
        for (const IfaceSlotView& slot : v->slots) {
            Decl* impl = ivFindImplMember(module, *v, slot);
            if (!impl) {
                // 契约缺失在语义层已经报过错（B02/B02b），这里只留 NULL 占位避免级联编译错误
                slotFns.push_back("NULL");
                continue;
            }
            const std::string fn = "vb6_iimpl_" + clsId + "_" + id + "_" + cIdent(slot.key);
            slotFns.push_back(fn);

            c_.emitBlank();
            c_.emitLine("static " + ivSlotRetType(slot.sig) + " " + fn + "(" + ref + "* self" +
                        ivParamDecls(*impl) + ") {");
            c_.indent();
            c_.emitLine(clsStruct + "* me = (" + clsStruct + "*)((char*)self - offsetof(" +
                        clsStruct + ", __iv_" + id + "));");
            const std::string call = ivImplCName(*impl) + "(me" + ivForwardArgs(*impl) + ")";
            if (ivSlotRetType(slot.sig) == "void") {
                c_.emitLine(call + ";");
            } else {
                c_.emitLine("return " + call + ";");
            }
            c_.dedent();
            c_.emitLine("}");
        }

        c_.emitBlank();
        c_.emitLine("static const vb6_ivtbl_" + id + " vb6_ivtbl_" + id + "_for_" + clsId + " = {");
        c_.indent();
        c_.emitLine("vb6_iunk_" + clsId + "_" + id + "_QueryInterface,");
        c_.emitLine("vb6_iunk_" + clsId + "_" + id + "_AddRef,");
        c_.emitLine("vb6_iunk_" + clsId + "_" + id + "_Release,");
        for (size_t i = 0; i < slotFns.size(); i++) {
            c_.emitLine(slotFns[i] + (i + 1 < slotFns.size() ? "," : ""));
        }
        c_.dedent();
        c_.emitLine("};");
    }
}

// ============================================================
// B05: 接口变量的作用域末尾释放
// ============================================================

void CCodeGen::trackIvrefLocalForRelease(const std::string& cName) {
    for (const std::string& s : ivrefLocalsToRelease_) {
        if (s == cName) return;
    }
    ivrefLocalsToRelease_.push_back(cName);
}

// 与 ansiTempsToFree_ 在同一位置调用（过程正常出口）。Exit Sub/Function 走裸
// return，与 ANSI 临时变量同样漏清理——现状与本批边界一并记进总表。
void CCodeGen::emitIvrefScopeRelease() {
    for (const std::string& s : ivrefLocalsToRelease_) {
        c_.emitLine("if (" + s + ") " + s + "->vt->Release(" + s +
                    ");  /* tB Interface B05: scope-exit release */");
    }
    ivrefLocalsToRelease_.clear();
}

} // namespace vb6c3
