// vb6c3 - 新式 Interface 契约比对 (tB 扩展; ai/022 D5, 批次 B02)
//
// 与 legacy VB6 Implements 的关系 (D5 分叉): 同一个 `Implements <Name>` 语句, 名字
// 命中 stage 2.7 登记表 = 新式接口 → 走这里的 **error 级**严格比对; 否则原样交给
// semantic_analyzer.cpp 的旧路径 (warn 级 + IFace_M 命名约定), 一行不改。
//
// 实现侧取名与接口侧共用 interface_sig.hpp 的规范函数, 因此"接口怎么写、实现就得
// 怎么写"只有一份定义。比对口径 = 源码签名 (见该文件头注释)。

#include "semantics/semantic_analyzer.hpp"

#include "semantics/interface_sig.hpp"
#include "semantics/interfaces_registry.hpp"

#include <map>
#include <string>

namespace vb6c3 {
namespace {

// 槽键还原成可读成员名 (属性三槽去掉 get_/put_/putref_ 前缀)
std::string ifaceBareName(const std::string& slotKey) {
    for (const char* p : {"putref_", "get_", "put_"}) {
        std::string k(p);
        if (slotKey.size() > k.size() && slotKey.compare(0, k.size(), k) == 0) {
            return slotKey.substr(k.size());
        }
    }
    return slotKey;
}

} // namespace

void SemanticAnalyzer::checkNewStyleInterface(const Module& module, const IfaceView& view,
                                              const std::string& writtenName,
                                              const SourceLocation& loc) {
    // D11 v1 边界: 泛型类不得实现接口 (泛型器已拒 Implements, 这里兜住新式路径)
    if (!module.classTypeParams.empty()) {
        diag_.error(DiagnosticID::SemInterfaceNotImplemented, loc,
            "Generic class '" + module.moduleName +
            "' cannot implement interface '" + writtenName + "' (not supported yet)");
        return;
    }
    if (view.chainBroken) return;  // 建表阶段 (stage 2.7) 已就该接口报过错, 不再级联

    // 实现侧成员表: 槽键 → 签名 (同名多成员映射到同一槽 = 契约歧义, 报错)
    std::map<std::string, IfaceProcSig> impl;
    for (const auto& d : module.declarations) {
        if (!d) continue;
        IfaceProcSig sig;
        if (!ifaceSigFromDecl(*d, sig)) continue;
        const std::string key = sig.slotKey;  // emplace 会移空 sig, 诊断文本先留一份
        auto res = impl.emplace(key, std::move(sig));
        if (!res.second) {
            diag_.error(DiagnosticID::SemInterfaceSignatureMismatch, d->loc,
                "Class '" + module.moduleName + "' has two members bound to interface '" +
                view.name + "' slot '" + key + "'");
        }
    }

    for (const auto& slot : view.slots) {
        IfaceProcSig want;
        if (slot.sig) ifaceSigFromDecl(*slot.sig, want);
        const std::string member = slot.ownerIface + "." +
            (slot.memberName.empty() ? ifaceBareName(slot.key) : slot.memberName);

        auto it = impl.find(slot.key);
        if (it == impl.end()) {
            diag_.error(DiagnosticID::SemInterfaceNotImplemented, loc,
                "Implements " + writtenName + ": member '" + member + "' (" + want.text +
                ") is not implemented by class '" + module.moduleName + "'");
            continue;
        }
        if (!ifaceSigEqual(want, it->second)) {
            diag_.error(DiagnosticID::SemInterfaceSignatureMismatch, it->second.decl
                            ? it->second.decl->loc
                            : loc,
                "Implements " + writtenName + ": member '" + member +
                "' signature mismatch (interface: " + want.text + ", class: " +
                it->second.text + ")");
        }
    }
}

} // namespace vb6c3
