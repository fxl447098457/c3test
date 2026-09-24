#pragma once
// vb6c3 - CoClass 组内名字激活 (tB 扩展; ai/026 五-3/4/5 + ai/022 D54, 批次 B11/C05)
//
// stage 2.7 的最后一格 (Pass G)。做的只有一件事: 把源码里写在**类型位置**上的 CoClass
// 块名就地换成该块 `[Implementation]` 绑的类名, 并把 `Set <var> = CreateObject("<本工程
// ProgID>")` 就地换成 `New <同一个类>`。改完之后管线里不再有"把块名当类型"这回事 ——
// 语义层与发码层看到的是一个普通工程类, 走的正是实测通的那条路 (`vb6_cls_X*` 变量 +
// `vb6_cls_X_New()` 工厂 + `resolveClassMemberCall` 的静态/虚表派发)。
//
// 为什么改 AST 而不是给语义层和 cgen 各加一个别名入口: 类名在这套管线里被读的地方不止
// `resolveTypeRef`/`mapTypeRef`, 还有 `memberFieldTypes`/`srcTypeName`/COM 解包表这些按
// **名字串**比对的位点 (D54-①)。别名要一处不漏, 就得每个消费者都记得问一遍; 就地改名让
// 所有消费者看到同一个名字。同一条选型先例 = 泛型单态化 (`src/ast/ast_clone.hpp` 头注
// "语义/cgen 对泛型零感知")。

#include <memory>
#include <string>
#include <vector>

namespace vb6c3 {

class Diagnostics;
class Module;

// 一条"可以当类型用"的 CoClass 块: 手写过、带 `[Implementation]`、块名没有被同名模块占掉。
// 三样都是从 stage 2.7 Pass E 已经求好的身份记录里挑出来的, 本函数不再求解身份。
struct CoClassActivation {
    std::string blockName;  // 源码里的块名 (原样大小写, 用于信息行与文案)
    std::string implName;   // 绑定的类模块名 = 改写目标
    std::string progId;     // 可空 = 不认 CreateObject 那条路
};

// implLessBlocks = 手写过、但**没有** `[Implementation]` 的块名。这类名字出现在类型位置上
// 时当场判死 (VB3039): 今天它是静默地当 Variant/DISPID 晚绑定 (D54 实测 M1+M5), 而"块没有
// 实现类"这件事编译器已经知道, 没有理由让用户去猜。
void activateCoClassNames(std::vector<std::unique_ptr<Module>>& modules,
                          const std::vector<CoClassActivation>& acts,
                          const std::vector<std::string>& implLessBlocks,
                          Diagnostics& diag);

}  // namespace vb6c3
