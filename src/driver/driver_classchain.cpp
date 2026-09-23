// vb6c3 - 类继承链登记 (tB 扩展; ai/022 D6/D24, 批次 B07a)
//
// stage 2.8: 所有模块解析完、语义分析开始**之前**, 把工程内的类与其 `Inherits` 子句建成
// 只读登记表 ClassChainRegistry (小写类名 → 链视图)。必须早于 stage 3: 派生类的成员合并
// 要读基类的声明, 而每个模块各有一张符号表 (登记表挂 Driver 的理由见 class_chain_registry.hpp)。
//
// 本批只做"编译期"侧: 基名解析 + 链求解 (未知基 / 环 / 深度) + v1 边界拒绝。
// 成员合并、遮蔽裁决、发码在 B07b (消费 chain 的父先己后顺序)。

#include "driver/driver.hpp"

#include "ast/ast.hpp"
#include "semantics/class_chain_registry.hpp"
#include "semantics/interface_sig.hpp"  // ifaceLower: tB 这一条线共用的小写键函数

#include <set>
#include <string>
#include <vector>

namespace vb6c3 {

// v1 深度上限: 链上类数 (含自身)。VB6/tB 都没规定上限, 但 B07b 的转发桩数与链长成
// 正比, 且环检测之外的病态输入要有个兜底 → 取一个远超真实代码的值。
static const size_t kMaxInheritsDepth = 16;

bool Driver::runClassChainPrepass() {
    classes_.clear();
    classOrder_.clear();

    // 早退: 工程里没有一条 Inherits → 不建表、不发诊断, 生成物与 B07a 之前逐字节相同
    // (零回归护栏的按 feature 门禁口径, D24 末两条)。
    bool anyClause = false;
    for (auto& mod : modules_) {
        if (mod && !mod->inherits.empty()) { anyClause = true; break; }
    }
    if (!anyClause) return true;

    // --- Pass A: 登记可继承的工程类 + v1 边界拒绝 ---
    for (auto& mod : modules_) {
        if (!mod) continue;
        const std::string key = ifaceLower(mod->moduleName);
        // 子句侧的边界先查 (与"这个类能否当基类"无关, 写了就要给说法):
        //   泛型模板类内的 Inherits 会随特化克隆被复制一份, 与 IfaceRegistry 当年同一坑;
        //   非类模块 (.bas/.frm) 没有类实例语义。
        if (!mod->inherits.empty() && key.empty()) {
            diag_->error(DiagnosticID::SemInheritsNotSupported, mod->inherits[0].loc,
                "Inherits clause has no host class name to attach to");
        } else if (!mod->inherits.empty()) {
            if (!mod->classTypeParams.empty()) {
                diag_->error(DiagnosticID::SemInheritsNotSupported, mod->inherits[0].loc,
                    "Inherits is not allowed inside a generic class template (" +
                    mod->moduleName + ")");
            } else if (!mod->isClassModule) {
                diag_->error(DiagnosticID::SemInheritsNotSupported, mod->inherits[0].loc,
                    "Inherits is only allowed in a class module (.cls); '" + mod->moduleName +
                    "' is not a class module");
            }
        }
        if (!mod->isClassModule || !mod->classTypeParams.empty() || key.empty()) continue;
        // 接口宿主 (.cls 里那一个同名 Interface 块) 没有实例, 不能当基类, 也不登记.
        if (mod->isInterfaceModule) continue;

        auto res = classes_.emplace(key, ClassChainView{});
        if (!res.second) {
            diag_->error(DiagnosticID::SemDuplicateDeclaration, mod->loc,
                "Duplicate class name '" + mod->moduleName + "' in the project");
            continue;
        }
        ClassChainView& v = res.first->second;
        v.name = mod->moduleName;
        v.mod = mod.get();
        classOrder_.push_back(key);

        if (mod->inherits.size() > 1) {
            // v1 = 单继承: 一条子句里写 `Inherits A, B` 在 parse 期就撞 VB2003, 这里是
            // "分两行各写一条" 的形态。取第一条继续, 免得基名解析全线级联。
            diag_->error(DiagnosticID::SemInheritsNotSupported, mod->inherits[1].loc,
                "Class '" + mod->moduleName + "' has more than one Inherits clause ("
                "single inheritance only; VB6/tB have no multiple class inheritance)");
        }
        if (!mod->inherits.empty() && !mod->inherits[0].baseName.empty()) {
            v.clause = &mod->inherits[0];
            v.baseText = mod->inherits[0].baseName;
        }
    }

    // --- Pass B: 基名解析 (未知基) + 环检测 ---
    for (const std::string& key : classOrder_) {
        auto self = classes_.find(key);
        if (self == classes_.end() || !self->second.clause) continue;
        ClassChainView& v = self->second;

        std::string bk = ifaceLower(v.baseText);
        if (classes_.find(bk) == classes_.end()) {
            // 点号限定名 (`Project.Base`) 的末段兜底, 与发码侧 ivLastSegment 同思路。
            size_t dot = bk.rfind('.');
            if (dot != std::string::npos) {
                std::string tail = bk.substr(dot + 1);
                if (classes_.find(tail) != classes_.end()) bk = tail;
            }
        }
        if (classes_.find(bk) == classes_.end()) {
            diag_->error(DiagnosticID::SemInheritsUnknownBase, v.clause->loc,
                "Class '" + v.name + "' inherits unknown base class '" + v.baseText +
                "' (the target must be a class module in this project)");
            v.chainBroken = true;
            continue;
        }
        v.baseKey = bk;

        std::set<std::string> seen;
        seen.insert(key);
        std::string chain = v.name;
        for (std::string cur = bk; !cur.empty();) {
            auto it = classes_.find(cur);
            if (it == classes_.end()) break;
            chain += " -> " + it->second.name;
            if (seen.count(cur)) {
                diag_->error(DiagnosticID::SemCircularDependency, v.clause->loc,
                    "Circular Inherits chain on class '" + v.name + "': " + chain);
                v.chainBroken = true;
                break;
            }
            seen.insert(cur);
            cur = it->second.baseKey;
        }
    }

    // --- Pass C: 链展开 (父先己后) + 深度上限 ---
    for (const std::string& key : classOrder_) {
        auto self = classes_.find(key);
        if (self == classes_.end()) continue;
        ClassChainView& v = self->second;
        if (v.chainBroken) continue;

        std::set<std::string> guard;  // Pass B 已查过环, 这里只兜底防无限循环
        for (std::string cur = key; !cur.empty();) {
            auto it = classes_.find(cur);
            if (it == classes_.end()) break;
            if (it->second.chainBroken) { v.chainBroken = true; break; }
            if (!guard.insert(cur).second) break;
            v.chain.insert(v.chain.begin(), cur);  // 小写键; B07b 的成员合并按此序取声明
            cur = it->second.baseKey;
        }
        if (v.chainBroken) { v.chain.clear(); continue; }
        if (v.chain.size() > kMaxInheritsDepth) {
            std::string text;
            for (const std::string& k : v.chain) {
                auto it = classes_.find(k);
                text += (text.empty() ? "" : " -> ") + (it == classes_.end() ? k : it->second.name);
            }
            diag_->error(DiagnosticID::SemInheritsTooDeep, v.clause->loc,
                "Class '" + v.name + "' Inherits chain is deeper than the supported limit (" +
                std::to_string(kMaxInheritsDepth) + " classes): " + text);
            v.chainBroken = true;
            v.chain.clear();
        }
    }

    return !diag_->hasErrors();
}

} // namespace vb6c3
