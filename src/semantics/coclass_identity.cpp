// vb6c3 - CoClass 身份求解实现 (ai/026 三节 / ai/022 D46, 批次 B11/C02)
//
// 纯函数、零副作用：C02 不发码，唯一消费者是 stage 2.7 的 Pass E（求解 + 缓存 + 一条 note
// 诊断）。可复现性由"入参相同 ⇒ 输出逐字节相同"保证，全链路没有随机数、没有时间戳。

#include "semantics/coclass_identity.hpp"

#include "ast/ast.hpp"
#include "semantics/interface_sig.hpp"
#include "semantics/interfaces_registry.hpp"

#include <cstdio>
#include <vector>

namespace vb6c3 {

const char* identitySourceName(IdentitySource s) {
    switch (s) {
        case IdentitySource::Explicit: return "explicit";
        case IdentitySource::Vbp:      return "vbp";
        case IdentitySource::Minted:   return "minted";
        default:                       return "missing";
    }
}

// 四条独立 FNV-1a 链填满 GUID 的 16 字节；版本号位与变体位按 RFC 4122 钉成 v4/variant-1
// 形状（内容仍是确定性的，不是随机 UUID）。
std::string mintGuid(const std::string& seed,
                     uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4) {
    const uint32_t kPrime = 0x01000193u;
    for (char c : seed) {
        s1 ^= static_cast<uint32_t>(static_cast<unsigned char>(c)); s1 *= kPrime;
        s2 ^= static_cast<uint32_t>(static_cast<unsigned char>(c)); s2 *= kPrime;
        s3 ^= static_cast<uint32_t>(static_cast<unsigned char>(c)); s3 *= kPrime;
        s4 ^= static_cast<uint32_t>(static_cast<unsigned char>(c)); s4 *= kPrime;
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "{%08X-%04X-%04X-%04X-%04X%08X}",
                  s1,
                  static_cast<unsigned>(s2 >> 16) & 0xFFFF,
                  static_cast<unsigned>((s2 & 0xFFFF) | 0x4000),
                  static_cast<unsigned>(((s3 >> 16) & 0xFFFF) | 0x8000),
                  static_cast<unsigned>(s3 & 0xFFFF),
                  s4);
    return std::string(buf);
}

// 常量组与 legacy 的 generateClsid/generateIid **不同**（D46-2）：两套 mint 并存期间
// 不许出现同一个 GUID，否则 B13/B16 分叉时无从判断是谁 mint 的。
static const uint32_t kCoclassSeeds[4] = {0x5a1b2c3du, 0x9e4f5061u, 0x2b3c4d5eu, 0x718293a4u};
static const uint32_t kIfaceSeeds[4]   = {0xc3d4e5f6u, 0x4a5b6c7du, 0x8e9f0a1bu, 0xdec1d2e3u};

std::string coclassSeed(const std::string& project, const std::string& coclassName) {
    return "coc:" + ifaceLower(project) + "." + ifaceLower(coclassName);
}
std::string ifaceSeed(const std::string& project, const std::string& ifaceName) {
    return "itf:" + ifaceLower(project) + "." + ifaceLower(ifaceName);
}

std::string ifaceIidFromName(const std::string& project, const std::string& ifaceName) {
    return mintGuid(ifaceSeed(project, ifaceName),
                    kIfaceSeeds[0], kIfaceSeeds[1], kIfaceSeeds[2], kIfaceSeeds[3]);
}

std::string resolveIfaceIid(const std::string& project, const IfaceView& v) {
    if (!v.guid.empty()) return v.guid;  // [InterfaceId("...")] 是最高优先档
    return ifaceIidFromName(project, v.name);
}

IfaceIdMap buildIfaceIdMap(const std::string& project, const IfaceRegistry& ifaces) {
    IfaceIdMap out;
    for (const auto& kv : ifaces) {
        const IfaceView& v = kv.second;
        if (v.name.empty() || v.chainBroken) continue;  // 父链已错的接口不再往外发身份
        out.emplace(ifaceLower(v.name), resolveIfaceIid(project, v));
    }
    return out;
}

namespace {

// 块级属性行按名字取字符串实参（大小写不敏感；写了但实参不是字符串则算没命中）
const InterfaceAttr* findStrAttr(const std::vector<InterfaceAttr>& attrs,
                                 const std::string& loweredName, std::string& out) {
    for (const auto& a : attrs) {
        if (ifaceLower(a.name) != loweredName) continue;
        if (!a.hasStr) continue;
        out = a.strValue;
        return &a;
    }
    return nullptr;
}

bool findBoolAttr(const std::vector<InterfaceAttr>& attrs, const std::string& loweredName) {
    for (const auto& a : attrs) {
        if (ifaceLower(a.name) != loweredName) continue;
        if (a.hasNum) return a.numValue != 0;
        return true;  // 只写 `[ComCreatable]` 视为 True
    }
    return false;
}

} // namespace

CoClassIdentity resolveCoClassIdentity(const CoClassDecl& block, const CoClassEnv& env) {
    CoClassIdentity id;
    id.name = block.name;
    id.legacyFolded = block.foldedFromAttributes();
    id.comCreatable = findBoolAttr(block.attributes, "comcreatable");

    std::string s;
    if (findStrAttr(block.attributes, "implementation", s)) id.implName = s;

    for (const auto& ref : block.ifaces) {
        if (ref.isDefault) { id.defaultIface = ref.ifaceName; break; }
    }

    // --- CLSID: 显式 > vbp 三段式 > 确定性 mint ---
    if (findStrAttr(block.attributes, "coclassid", s)) {
        id.clsid = s;
        id.clsidSource = IdentitySource::Explicit;
    } else {
        // vbp 的 CLSID 挂在**类模块**上：先按 [Implementation] 指的模块查，查不到再按
        // CoClass 块名查（VB6 工程里两者常常同名）。
        const std::string* hit = nullptr;
        if (env.vbpClsids) {
            auto byImpl = env.vbpClsids->find(ifaceLower(id.implName.empty() ? block.name : id.implName));
            if (byImpl != env.vbpClsids->end()) hit = &byImpl->second;
            if (!hit) {
                auto bySelf = env.vbpClsids->find(ifaceLower(block.name));
                if (bySelf != env.vbpClsids->end()) hit = &bySelf->second;
            }
        }
        if (hit) {
            id.clsid = *hit;
            id.clsidSource = IdentitySource::Vbp;
        } else {
            id.clsid = mintGuid(coclassSeed(env.project, block.name),
                                kCoclassSeeds[0], kCoclassSeeds[1], kCoclassSeeds[2], kCoclassSeeds[3]);
            id.clsidSource = IdentitySource::Minted;
        }
    }

    // --- IID: 默认接口的显式 [InterfaceId] > 确定性 mint；没有默认接口就没有 IID ---
    if (!id.defaultIface.empty()) {
        const IfaceView* v = nullptr;
        if (env.ifaces) {
            auto it = env.ifaces->find(ifaceLower(id.defaultIface));
            if (it != env.ifaces->end()) v = &it->second;
        }
        // 登记不到同名接口（块写 `[Default] IX` 而 IX 是 legacy 类模块）时按**名字**mint，
        // 与 B13b 之前逐字节同值 —— 这一档不是新式接口，`ifaceIds_` 里也不会有它。
        id.iid = v ? resolveIfaceIid(env.project, *v) : ifaceIidFromName(env.project, id.defaultIface);
        id.iidSource = (v && !v->guid.empty()) ? IdentitySource::Explicit : IdentitySource::Minted;
    } else {
        id.iidSource = IdentitySource::Missing;
    }

    // --- ProgID: 显式 > `<Proj>.<CoClass名>`（VB6 的默认拼法）---
    if (findStrAttr(block.attributes, "progid", s)) {
        id.progId = s;
        id.progIdSource = IdentitySource::Explicit;
    } else {
        id.progId = env.project + "." + block.name;
        id.progIdSource = IdentitySource::Minted;
    }
    return id;
}

} // namespace vb6c3
