#pragma once
// vb6c3 - Interface 契约登记表 (tB 扩展; 设计依据 ai/022 记录 D2/D4, 批次 B02)
//
// stage 2.7 (Driver::runInterfacePrepass) 一次性建表, 之后只读:
// 语义层 (Implements 契约比对) 与后续发码层 (B04 槽表) 共用同一份视图。
// 与泛型的 GenRegistry 同族: plain 结构 + 指针引用 AST 节点, 不克隆、不拥有。

#include <string>
#include <unordered_map>
#include <vector>

namespace vb6c3 {

class Decl;
class InterfaceDecl;

// 单个接口槽 = COM vtable 的一席。
// 槽键规范 (D2, 属性拆三槽): Sub/Function -> 成员名; Property Get/Let/Set ->
// get_<名> / put_<名> / putref_<名>。全部小写。
struct IfaceSlotView {
    std::string key;          // 槽键 (小写, 已含 get_/put_/putref_ 前缀)
    std::string memberName;   // 声明原样成员名 (诊断可读)
    std::string ownerIface;   // 声明该槽的接口名 (原大小写)
    const Decl* sig = nullptr;  // SubDecl/FunctionDecl/PropertyDecl, body 恒空
    int32_t index = 0;        // 展平槽序 (父先己后, 同层声明序; 0 基)
};

struct IfaceView {
    std::string name;                     // 接口名 (原大小写)
    const InterfaceDecl* decl = nullptr;
    std::string extendsKey;               // 父接口的小写键, 空 = 无父
    std::vector<IfaceSlotView> slots;     // 展平后: 继承来的在前
    std::string guid;                     // [InterfaceId("...")] 实参, 可空
    bool chainBroken = false;             // 父链有错 (未知父/环), 不再级联报错
};

// key = 接口名小写 (工程级唯一, D1)
using IfaceRegistry = std::unordered_map<std::string, IfaceView>;

// ============================================================
// ai/022 B10: 委托式实现 `Implements I Via m_holder` 的裁决结果
// ============================================================
// 同样在 stage 2.7 一次建成、之后只读: 判定要跨模块看"持有字段的类型那个类实现了
// 接口没有", 而那时别的类的符号还没进本模块符号表 (stage 3.5 才注入) —— 只有这里
// 能把整工程的模块表看全。
struct ViaView {
    std::string ifaceKey;      // 被委托接口的小写键 (登记表里的规范名)
    std::string fieldName;     // 持有字段名 (源码原样大小写)
    std::string holderModule;  // 字段类型对应的类模块名 (原样大小写)
};

// key = 实现类模块名小写; 一个类可以委托多个接口, 按 `Implements` 书写序
using ViaRegistry = std::unordered_map<std::string, std::vector<ViaView>>;

} // namespace vb6c3
