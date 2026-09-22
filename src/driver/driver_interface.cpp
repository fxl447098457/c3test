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

#include <set>
#include <string>
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
        for (auto& d : mod->interfaces) {
            if (!d) continue;
            const std::string key = ifaceLower(d->name);
            if (key.empty()) continue;  // 无名 = parse 阶段已报错, 不再级联
            if (moduleKeys.count(key)) {
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

    return !diag_->hasErrors();
}

} // namespace vb6c3
