#pragma once
// vb6c3 - 类继承链登记表 (tB 扩展; 设计依据 ai/022 记录 D6/D24, 批次 B07)
//
// stage 2.8 (Driver::runClassChainPrepass) 一次性建表, 之后只读。与 IfaceRegistry /
// GenRegistry 同族: plain 结构 + 指针引用 AST 节点, 不克隆、不拥有。
//
// 为什么不挂符号表 (D2 同源理由): 每个模块各一张 SymbolTable, 而类名是工程级唯一的;
// 派生链要跨模块解 (Derived.cls 的基类在另一个 .cls 里), 挂在 Driver 上才只有一份真相。
//
// 本批 (B07a) 只做"编译期"侧: 基名解析 + 链求解 (未知基/环/深度) + v1 边界拒绝。
// B07b 消费本表做成员合并与遮蔽 —— `chain` 的父先己后顺序就是合并顺序。

#include <string>
#include <unordered_map>
#include <vector>

namespace vb6c3 {

class Module;
class Decl;
struct InheritsStmt;

// 单个工程类 (只登记"能当基类用"的模块: 类模块、非泛型模板、非接口宿主)。
struct ClassChainView {
    std::string name;                  // 类名 (原大小写, 工程级唯一)
    Module* mod = nullptr;             // 该类的模块 (成员合并与发码都要读它的声明; 非 const 是因为
                                     // 发码层的 makeProcSignature/mapTypeRef 吃非 const 引用)
    const InheritsStmt* clause = nullptr;  // 指向 Module::inherits[0]; 空 = 无基类
    std::string baseKey;               // 基类的小写键 (已解到 chainKey), 空 = 无基类
    std::string baseText;              // 基名书写原文 (诊断可读)
    std::vector<std::string> chain;    // 自根到叶的小写键 (根在前, 末位是自身) — B07b 的成员合并按此序取声明
    bool chainBroken = false;          // 基链有错 (未知基/环/过深), 不再级联报错

    // --- B07b: stage 3.4 (mergeInheritedMembers) 回填, 之后只读 ---
    // 为什么回填而不是让发码层重算: 合并的"祖先 own 声明 + 遮蔽裁决"必须与符号表里那 8 张
    // 成员表**同源**, 否则 struct 字段与可调用成员会各按一套规则走 (静默错字段)。
    std::vector<Decl*> inhFields;  // 祖先数据字段 (自根到叶, 只算各祖先自己的声明)
    // 需要转发桩的祖先过程 (自根到叶, 已去掉被遮蔽与 Private 的)。owner = 声明它的那个祖先模块,
    // 发码时要拿它拼 `vb6_<owner>_<M>((vb6_cls_<owner>*)me, …)`。
    struct InheritedProc {
        Decl* decl = nullptr;
        Module* owner = nullptr;
    };
    std::vector<InheritedProc> inhProcs;

    // --- B08b: 虚成员 (Overridable / Overrides) 的裁决结果, stage 2.8 Pass E 回填, 之后只读 ---
    // dynamicKeys = "本类体内调用这个名字必须走虚槽"的成员键集合: 本类 (或它的某个后代)
    // 声明的 Overridable 成员, 且链上**有更深的类**写了 Overrides。B08d 之后这条线真的发虚表
    // (见下面的 virtSlots), 语义层只留"裸名调用"那一处拒绝; 属性方向要等槽键, 所以本表存
    // **成员名**小写、发码层另用 virtSlots (D32④)。
    // 键用 interface_sig.hpp::ifaceSlotKey 的槽键 (属性按 get_/put_/putref_ 分向)。
    std::vector<std::string> dynamicKeys;
    bool hasVirtualMods = false;  // 本类自己声明过任一虚修饰符 (早退判据与诊断定位用)

    // --- B08d: 类虚表 (stage 3.4b buildVirtualSlotTables 回填, 之后只读) ---
    // 一个槽 = "链上有人用 Overrides 覆盖过"的那个可覆盖成员。B08b 的 dynamicKeys 只有
    // **成员名** (分析器拿不到属性方向), 发码要的是有序 + 带方向的槽清单, 所以另起这张表。
    //
    // 定序规则 (前缀布局成立的前提, 别改): 按**首次声明位置** 根→叶 排序, 每槽键一份,
    // 且筛选集合取链**根**的 dynamicKeys (K 对链上所有类相同) → 于是任一祖先的槽表都是
    // 更深层类槽表的前缀, `me->__cvtbl` 的字段偏移在整条链上一致。
    struct VirtSlot {
        std::string slotKey;   // ifaceSlotKey: 属性自带 get_/put_/putref_ 方向
        std::string nameKey;   // 成员名小写 (与分析器 dynamicKeys 同键, 供其快路径比对)
        std::string field;     // 虚表里的 C 字段名 (小写, prop_get_ 前缀口径) — 三处共用唯一出处
        Decl* decl = nullptr;  // 槽的**首个 Overridable 声明处** (诊断可读)
        Module* owner = nullptr;
        Decl* impl = nullptr;  // **本类**该槽的入口声明: 本类自己的声明, 或 stage 3.4 判定要发
                               // 转发桩的那份祖先声明。C 名一律按本类模块名拼 (与两份定义同源)。
    };
    std::vector<VirtSlot> virtSlots;  // 空 = 本类不带 __cvtbl 字段 (零新语法时逐字节不变的护栏)
};

// key = 类名小写 (工程级唯一, D1)
using ClassChainRegistry = std::unordered_map<std::string, ClassChainView>;

} // namespace vb6c3
