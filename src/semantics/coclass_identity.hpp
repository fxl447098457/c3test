#pragma once
// vb6c3 - CoClass 身份求解 (tB 扩展; 设计依据 ai/026 三节 + ai/022 D46, 批次 B11/C02)
//
// **CLSID / IID / ProgID 三者只能从这里求出来**（026 三节"不可让步"）。后续任何消费者
// —— C05 的 `New`/`CreateObject` 编译期改写、B13 的 dllentry coclass 表、B15 的类型库、
// 注册器 —— 一律调 `resolveCoClassIdentity`，不得各自再读一遍属性行或 vbp 表。
//
// 与 legacy 的关系（D46-2）：`cgen_util_dllentry_prelude.inc` 里那两枚局部 lambda
// （generateClsid/generateIid）是同族但**不同种子**的另一套 mint，今天只跑在 ActiveX DLL
// 路径上、没有任何用例覆盖，所以 C02 不动它们。两套 mint 的常量组刻意不同 → 不可能撞车；
// 把它们并成一处的动作按 026 七-1 / D15-8 归 B13/B16 的分叉。

#include <cstdint>
#include <string>
#include <unordered_map>

namespace vb6c3 {

class CoClassDecl;
struct IfaceView;
using IfaceRegistry = std::unordered_map<std::string, IfaceView>;

// 三档来源（026 三节的优先级表）
enum class IdentitySource : uint8_t {
    Explicit,   // 块里写了属性行
    Vbp,        // vbp 三段式 `Class=Name; x.cls; {CLSID}`
    Minted,     // FNV-1a 确定性派生
    Missing,    // 求不出来（如块没有 [Default] 接口 → 没有 IID 可求）
};

const char* identitySourceName(IdentitySource s);

struct CoClassIdentity {
    std::string name;          // CoClass 块名（源码原样大小写）
    std::string implName;      // [Implementation("...")] 实参，可空
    std::string defaultIface;  // [Default] 条目的接口名，可空
    std::string clsid;         // "{...}"
    std::string iid;           // 默认接口的 IID；无默认接口则空
    std::string progId;
    IdentitySource clsidSource = IdentitySource::Minted;
    IdentitySource iidSource = IdentitySource::Missing;
    IdentitySource progIdSource = IdentitySource::Minted;
    bool comCreatable = false;  // [ComCreatable(True)]
    // 这条身份来自**折算**而不是手写块 (ai/026 六节 C04 / ai/022 D52, 批次 B11/C04)。
    // 唯一消费者是 stage 2.8 的 VB3020 文案：折算记录的名字恒等于一个类模块名，
    // 说"那是个 CoClass 块、没有成员表可继承"对一个从没写过块的存量工程是假理由。
    bool legacyFolded = false;
};

// 求解要看的"块外面的世界"：工程名、vbp 的 CLSID 表、接口登记表（取 [InterfaceId]）。
struct CoClassEnv {
    std::string project;  // <Proj>：vbp `Name=` > 工程基名 > 既有兜底字面量
    // key = **类模块名**小写（vbp 三段式挂在模块上，不挂在 CoClass 名上）
    const std::unordered_map<std::string, std::string>* vbpClsids = nullptr;
    const IfaceRegistry* ifaces = nullptr;
};

// FNV-1a × 四条种子链 → GUID 文本。形状与 legacy 那两枚 lambda 一致，常量组不同。
std::string mintGuid(const std::string& seed,
                     uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4);

// 唯一入口。纯函数：不写诊断、不碰全局，结果的可复现性只由入参决定。
CoClassIdentity resolveCoClassIdentity(const CoClassDecl& block, const CoClassEnv& env);

// seed 串的规范（写出来是为了让用例能用另一套实现独立复算）
std::string coclassSeed(const std::string& project, const std::string& coclassName);
std::string ifaceSeed(const std::string& project, const std::string& ifaceName);

} // namespace vb6c3
