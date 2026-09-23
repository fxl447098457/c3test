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
};

// key = 类名小写 (工程级唯一, D1)
using ClassChainRegistry = std::unordered_map<std::string, ClassChainView>;

} // namespace vb6c3
