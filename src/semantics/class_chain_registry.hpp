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
struct InheritsStmt;

// 单个工程类 (只登记"能当基类用"的模块: 类模块、非泛型模板、非接口宿主)。
struct ClassChainView {
    std::string name;                  // 类名 (原大小写, 工程级唯一)
    const Module* mod = nullptr;       // 该类的模块 (B07b 的成员合并要读它的声明)
    const InheritsStmt* clause = nullptr;  // 指向 Module::inherits[0]; 空 = 无基类
    std::string baseKey;               // 基类的小写键 (已解到 chainKey), 空 = 无基类
    std::string baseText;              // 基名书写原文 (诊断可读)
    std::vector<std::string> chain;    // 自根到叶的小写键 (根在前, 末位是自身) — B07b 的成员合并按此序取声明
    bool chainBroken = false;          // 基链有错 (未知基/环/过深), 不再级联报错
};

// key = 类名小写 (工程级唯一, D1)
using ClassChainRegistry = std::unordered_map<std::string, ClassChainView>;

} // namespace vb6c3
