// vb6c3 - Interface 契约登记 (tB 扩展; ai/022 D2/D4, 批次 B02)
//
// stage 2.7: 在所有 Interface 块解析完、语义分析开始**之前**, 把工程级接口契约建成
// 只读登记表 IfaceRegistry (名字 → 展平槽表)。必须早于 stage 3: 跨模块引用接口名是
// 常态, 而每个模块各有自己的符号表, 因此登记表挂在 Driver 上而不是任何符号表里。
//
// 本批只做"编译期契约"侧: 建表 + Extends 链求解 + 槽序展平 + 冲突诊断。
// Implements 的严格比对在语义层 (semantic_analyzer_iface.cpp), 发码在 B04。

#include "driver/driver.hpp"

#include "ast/ast.hpp"
#include "semantics/interface_sig.hpp"
#include "semantics/interfaces_registry.hpp"
#include "semantics/coclass_identity.hpp"   // CoClass 身份 (tB, B11/C02)

#include <iostream>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace vb6c3 {

bool Driver::runInterfacePrepass() {
    ifaces_.clear();
    ifaceOrder_.clear();

    std::set<std::string> moduleKeys;
    for (auto& mod : modules_) moduleKeys.insert(ifaceLower(mod->moduleName));

    // --- Pass A: 登记接口名 (工程级唯一) ---
    for (auto& mod : modules_) {
        // v1 边界 (D11): 泛型模板类内的 Interface 块会随特化克隆被登记两次
        // (模板模块本体 + 特化副本都在 modules_ 里), 表现为莫名其妙的重名错.
        // 与其让使用者困惑, 不如在这里明确拒绝.
        if (!mod->classTypeParams.empty() && !mod->interfaces.empty()) {
            diag_->error(DiagnosticID::SemInterfaceNotSupported, mod->interfaces.front()->loc,
                "Interface declarations are not allowed inside a generic class template (" +
                mod->moduleName + ")");
            continue;
        }
        // 头行宿主识别 (B03, D17): VB6 "一文件一接口" 写成 `IFoo.cls` + 体内唯一的
        // `Interface IFoo … End Interface`. 模块名要到 driver_frontend 才定得下来,
        // 所以识别放在这里而不是 parser 里.
        if (mod->isClassModule && mod->interfaces.size() == 1 && mod->interfaces.front() &&
            ifaceLower(mod->interfaces.front()->name) == ifaceLower(mod->moduleName)) {
            mod->isInterfaceModule = true;
            for (const auto& d : mod->declarations) {
                if (!d) continue;
                diag_->error(DiagnosticID::SemInterfaceNotSupported, d->loc,
                    "Interface host module '" + mod->moduleName +
                    "' may contain only the Interface block (declaration here is not allowed)");
                break;  // 一条宿主违规只报一次
            }
        }
        for (auto& d : mod->interfaces) {
            if (!d) continue;
            const std::string key = ifaceLower(d->name);
            if (key.empty()) continue;  // 无名 = parse 阶段已报错, 不再级联
            // 头行宿主的接口名天然等于它自己的模块名, 这不是撞车 (B03)
            const bool hostOwnName =
                mod->isInterfaceModule && d.get() == mod->interfaces.front().get();
            if (moduleKeys.count(key) && !hostOwnName) {
                diag_->error(DiagnosticID::SemDuplicateDeclaration, d->loc,
                    "Interface name '" + d->name +
                    "' collides with a module of the same name (module names and "
                    "interface names share one project-wide namespace)");
                continue;
            }
            auto res = ifaces_.emplace(key, IfaceView{});
            if (!res.second) {
                diag_->error(DiagnosticID::SemDuplicateDeclaration, d->loc,
                    "Duplicate interface name '" + d->name +
                    "' (interface names are project-unique)");
                continue;
            }
            IfaceView& v = res.first->second;
            v.name = d->name;
            v.decl = d.get();
            v.extendsKey = ifaceLower(d->extendsName);
            for (const auto& a : d->attributes) {
                if (ifaceLower(a.name) == "interfaceid" && a.hasStr) v.guid = a.strValue;
            }
            ifaceOrder_.push_back(key);
        }
    }

    // --- Pass B: Extends 链求解 (未知父 / 环) ---
    for (const std::string& key : ifaceOrder_) {
        IfaceView& v = ifaces_[key];
        if (v.extendsKey.empty()) continue;
        if (!ifaces_.count(v.extendsKey)) {
            diag_->error(DiagnosticID::SemInterfaceUnknownParent, v.decl->loc,
                "Interface '" + v.name + "' extends unknown interface '" + v.decl->extendsName + "'");
            v.chainBroken = true;
            continue;
        }
        std::set<std::string> seen;
        seen.insert(key);
        std::string chain = v.name;
        std::string cur = v.extendsKey;
        while (!cur.empty()) {
            auto it = ifaces_.find(cur);
            if (it == ifaces_.end()) break;
            chain += " -> " + it->second.name;
            if (seen.count(cur)) {
                diag_->error(DiagnosticID::SemCircularDependency, v.decl->loc,
                    "Circular Extends chain on interface '" + v.name + "': " + chain);
                v.chainBroken = true;
                break;
            }
            seen.insert(cur);
            cur = it->second.extendsKey;
        }
    }

    // --- Pass C: 槽表展平 (父先己后, 同层声明序) + 槽名冲突 ---
    for (const std::string& key : ifaceOrder_) {
        IfaceView& v = ifaces_[key];
        if (v.chainBroken) continue;

        // 自根到叶的接口序列 (根在前)
        std::vector<const IfaceView*> chain;
        for (std::string cur = key; !cur.empty();) {
            auto it = ifaces_.find(cur);
            if (it == ifaces_.end()) break;
            if (it->second.chainBroken) { v.chainBroken = true; break; }
            chain.insert(chain.begin(), &it->second);
            cur = it->second.extendsKey;
        }
        if (v.chainBroken) continue;

        std::set<std::string> used;      // 已占用的槽键
        std::set<std::string> ownNames;  // 本接口内的成员名 (禁重载)
        for (const IfaceView* iv : chain) {
            const bool own = (iv == &v);
            for (const auto& m : iv->decl->members) {
                if (!m.decl) continue;
                IfaceProcSig sig;
                if (!ifaceSigFromDecl(*m.decl, sig)) continue;
                if (own) {
                    if (!ownNames.insert(sig.slotKey).second) {
                        diag_->error(DiagnosticID::SemInterfaceSlotConflict, m.decl->loc,
                            "Interface '" + v.name + "' declares member '" + sig.memberName +
                            "' more than once (interface members cannot be overloaded)");
                        continue;
                    }
                }
                if (!used.insert(sig.slotKey).second) {
                    std::string holder;
                    for (const auto& s : v.slots) {
                        if (s.key == sig.slotKey) { holder = s.ownerIface; break; }
                    }
                    diag_->error(DiagnosticID::SemInterfaceSlotConflict, m.decl->loc,
                        "Interface '" + iv->name + "' slot '" + sig.slotKey +
                        "' conflicts with the inherited slot from '" + holder +
                        "' (COM vtables have no shadowing)");
                    continue;
                }
                IfaceSlotView slot;
                slot.key = sig.slotKey;
                slot.memberName = sig.memberName;
                slot.ownerIface = iv->name;
                slot.sig = m.decl.get();
                slot.index = static_cast<int32_t>(v.slots.size());
                v.slots.push_back(std::move(slot));
            }
        }
    }

    // --- Pass D: 委托式实现 `Implements I Via m_holder` (ai/022 D42/D43, 批次 B10) ---
    //
    // 为什么在这里、不在语义层: 判定要看"字段类型那个类实现了接口没有", 而那些类的
    // 符号要到 stage 3.5 才注入本模块作用域 —— 只有此刻整工程的模块表看得见。
    // 裁决结果 vias_ 有两个消费方: 语义层据此免掉逐槽 VB3012 (契约由被委托对象满足),
    // 发码层据此给没有自家实现的槽转调持有对象的接口槽 (cgen_iface_vtbl.cpp)。
    vias_.clear();
    {
        std::unordered_map<std::string, Module*> byName;
        for (auto& mod : modules_) byName[ifaceLower(mod->moduleName)] = mod.get();

        // 与语义层 lookupWrittenIface 同一口径: 先按写的全名, 再按点号末段 (Fix 083)
        auto resolveIface = [this](const std::string& written) -> const IfaceView* {
            auto it = ifaces_.find(ifaceLower(written));
            if (it == ifaces_.end()) {
                size_t dot = written.rfind('.');
                if (dot != std::string::npos)
                    it = ifaces_.find(ifaceLower(written.substr(dot + 1)));
            }
            return it == ifaces_.end() ? nullptr : &it->second;
        };
        auto findField = [](Module& m, const std::string& name) -> VariableDecl* {
            for (const auto& d : m.declarations) {
                if (!d || d->kind != ASTNodeKind::VariableDecl) continue;
                auto& v = static_cast<VariableDecl&>(*d);
                if (ifaceLower(v.name) == ifaceLower(name)) return &v;
            }
            return nullptr;
        };

        for (auto& mod : modules_) {
            for (const auto& impl : mod->implements) {
                if (!impl || impl->viaField.empty()) continue;  // 非委托式: 本 Pass 不管
                const std::string written = impl->interfaceName + " Via " + impl->viaField;
                auto reject = [&](DiagnosticID id, const std::string& msg) {
                    diag_->error(id, impl->loc, "'" + written + "': " + msg);
                };
                if (!mod->isClassModule) {
                    reject(DiagnosticID::SemViaTargetUnknown,
                           "Via is only allowed in a class module");
                    continue;
                }
                const IfaceView* v = resolveIface(impl->interfaceName);
                if (!v) {
                    reject(DiagnosticID::SemViaTargetUnknown,
                           "the delegated name is not an Interface block (Via is only "
                           "defined for interfaces with a checked contract)");
                    continue;
                }
                VariableDecl* fld = findField(*mod, impl->viaField);
                auto* ref = fld && fld->asType
                    ? dynamic_cast<SimpleTypeRef*>(fld->asType.get()) : nullptr;
                if (!ref) {
                    reject(DiagnosticID::SemViaTargetUnknown,
                           "holder '" + impl->viaField + "' is not a module-level field "
                           "declared As <Class> in this module");
                    continue;
                }
                Module* holder = nullptr;
                auto hit = byName.find(ifaceLower(ref->name));
                if (hit != byName.end()) holder = hit->second;
                if (!holder || !holder->isClassModule || holder->isInterfaceModule) {
                    reject(DiagnosticID::SemViaTargetUnknown,
                           "field '" + impl->viaField + "' is not of a project class type ("
                           "'" + ref->name + "' is not a class module)");
                    continue;
                }
                // v1 边界 (D43-4): 持有类必须**自己**实现同一个接口。它自己又是委托
                // (A Via f, f:B; B Via g, g:A) 的话运行期能构成无限回环, 这里直接拒。
                bool holderImplements = false;
                bool chained = false;
                for (const auto& hi : holder->implements) {
                    if (!hi || resolveIface(hi->interfaceName) != v) continue;
                    if (!hi->viaField.empty()) {
                        reject(DiagnosticID::SemViaHolderNotImplemented,
                               "class '" + holder->moduleName + "' delegates interface '" +
                               v->name + "' too (chained Via is not supported)");
                        chained = true;
                    } else {
                        holderImplements = true;
                    }
                    break;
                }
                if (!holderImplements && !chained) {
                    reject(DiagnosticID::SemViaHolderNotImplemented,
                           "class '" + holder->moduleName + "' (type of field '" +
                           impl->viaField + "') does not implement interface '" + v->name + "'");
                    continue;
                }
                ViaView vv;
                vv.ifaceKey = ifaceLower(v->name);
                vv.fieldName = fld->name;
                vv.holderModule = holder->moduleName;
                vias_[ifaceLower(mod->moduleName)].push_back(std::move(vv));
            }
        }
    }

    // --- Pass E: CoClass 身份求解 (tB 扩展, ai/026 三节 / ai/022 D46, 批次 B11/C02) ---
    // 求解本身在 src/semantics/coclass_identity.cpp 这个唯一入口里；这里只给上下文、缓存结果。
    // 报告走 stderr 的 "C3: ..." 信息行（driver_compile.cpp 的 "C3: 加载工程" 是同族先例）：
    // 诊断通道今天只在**阶段失败**时才整体打印，note 级在成功的编译里根本看不见；而 --emit-c
    // 的 stdout 是 C 文本，不能混。D44/D45 那条"每层的可观测面不一样"的第三次应验。
    coclassIds_.clear();
    {
        CoClassEnv env;
        // <Proj>: vbp 的 Name= > 工程基名 > 兜底字面量（第三条沿用 com_entry 那侧已有的兜序）
        env.project = !vbpProjectName_.empty() ? vbpProjectName_
                    : (!projectBaseName_.empty() ? projectBaseName_ : std::string("VB6EXE"));
        env.vbpClsids = &classClsidMap_;
        env.ifaces = &ifaces_;
        for (auto& mod : modules_) {
            for (auto& cc : mod->coclasses) {
                if (!cc || cc->name.empty()) continue;  // 无名块: parse 期已报错, 不再级联
                CoClassIdentity id = resolveCoClassIdentity(*cc, env);
                std::cerr << "C3: CoClass '" << id.name << "' identity: CLSID=" << id.clsid
                          << " (" << identitySourceName(id.clsidSource) << ")"
                          << " IID=" << (id.iid.empty() ? std::string("-") : id.iid)
                          << " (" << identitySourceName(id.iidSource) << ")"
                          << " ProgID=" << id.progId
                          << " (" << identitySourceName(id.progIdSource) << ")"
                          << " impl='" << id.implName << "'"
                          << " comCreatable=" << (id.comCreatable ? "True" : "False") << std::endl;
                // 同名两个块: 首值胜。重复名/引用是否存在这类校验按 D44 整片归 C03。
                coclassIds_.emplace(ifaceLower(id.name), std::move(id));
            }
        }
    }

    return !diag_->hasErrors();
}

} // namespace vb6c3
