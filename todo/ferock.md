# ferock 的个人 todo

> 想法池（草稿区），约定见根目录 [todo.md](../todo.md)。决定动手的条目转仓库 Issue 后从这里删除。

## vbman 编译一致性（C3 vs 微软 VB6）

- [ ] 对照微软版 VB6，让 vbman 代码的编译结果在 C3 下与 VB6 编译结果一致，包括 VBMAN.dll 的对应执行结果
  - 基线：VB6Mini 编译 `tests\vbman\src\VBMAN.vbp` 成功（129 模块，产物 dist\DLL\VBMAN.dll 2.3MB）；C3 前端 129 模块 0 error，cl 阶段 21 错误（详见 [docs/vbman/001_first_compile.md](../docs/vbman/001_first_compile.md)）
  - 修复顺序：跨类调用实参缺失 ×15 → 事件 sink 声明 ×3 → Variant 转换散点 ×3，每修一类跑 `docs\vbman\build_vbman.ps1` 观察错误数递减
  - 编译一致后进入运行一致：VB6 与 C3 各自产出 VBMAN.dll，对比执行结果
  - 待用户后续提供 vbman 单元测试

## changelog
- 2026-09-17 cgen 头拆分：cgen.hpp（996 行单类头）拆为伞头 54 行 + cgen_emitter.hpp（CodeEmitter 独立成头）+ detail/ 3 个类体片段（api 203 / state 342 / helpers 382），类内 #include 片段、逐行零重排，36 处引用路径不变；回归 82/0/1/83 零变化
- 2026-09-17 AST 头拆分：ast.hpp（1459 行）拆为伞头 18 行 + src/ast/detail/ 8 子头（enums/fwd/base/expr/stmt/stmt_io/decl/util，最大 366 行），按类边界切、零重排，6 处引用路径不变；回归 82/0/1/83 零变化
- 2026-09-17（ai/003-开发计划）：补「实现说明」——P4~P8 的 rtl/*.hpp/cpp 规划名是早期 C++ 路线，实际走 C 侧 RTL，相关文件名已不存在；原计划文本不改写
- 2026-09-17（src/rtl/core）：41 个平铺文件按家族分 4 个子目录（vb6rtl 9 / vb6com 8 / vb6comserver 7 / vb6forms 13），根下只留 2 个 DI 桩 + bstring；同步 rc / CMake 路径，CMake 嵌入依赖由 17 项补齐为 39 项
- 2026-09-17 RTL 头拆分：vb6rtl.h（1007 行）拆为伞头 16 行 + 7 子头（base/bstr/variant/builtin/array/class_com/runtime），同步 4 处嵌入管线，回归 82/0/1/83 零变化
- 2026-09-17 清理：删 20 个纯占位空文件；同步 ai 文档旧路径；更新外壳 README / CLAUDE
- 2026-09-16 源码拆分专题：巨型文件按家族拆到约 500 行/文件（RTL 3 个 + C++ 6 个，另含 backend 目录重组）
- 2026-09-16 Fix 099（parser_expr.cpp）：归因过程中最大反转——诊断打印 0 命中证明问题根本不在 cgen，而在 parser：With 块内 Users.Decode .Rs 被误解析成链式成员访问而非“实参 .Rs”。Fix 077 的空格检测扩展到 obj.Method .Field 形态。-2
- 2026-09-16 Fix 098（cgen_stmt.cpp）：Set 左值是函数调用表达式 → 求值后丢弃（VB6 语义 no-op）。-2
- 2026-09-16 Fix 097（type_system.cpp + cgen_expr.cpp）：CompareMethod（VBA 枚举的无 vb 前缀别名）两处统一映射 Long。-1
- 2026-09-16 Fix 096（cgen_com.cpp）：WithEvents 未实现的事件生成空 stub，vtable 不再悬空引用。-3
- 2026-09-16 Fix 095（driver.cpp）：撞名时标准模块过程优先于类成员——mWebSocketUtils.bas 与 cWebSocketUtils.cls 双定义，裸调该解析到模块版（无 me 参数）。-13，一处修复清掉 WebSocket 全家
- 2026-09-14~15 工程化：脚本 vswhere + 环境变量可移植；顶层目录英文化；README / CONTRIBUTING / ENV；冒烟测试；VERSION
- 2026-09-11 M31-M33：链接期 LNK1104 / LNK2019 / LNK2005 全部清零，VBMAN.dll 可注册（tag v0.1.0-m33-vbman-dll）
- 2026-09-02 编译提速：/MP 并行 + --no-warn + --incremental + --trim-includes
- 2026-09-01~10 vbman 编译期错误清零：777 → 0，全程 bisect 子集跟踪
- 2026-08-31 x64：LongPtr / LongLong 类型 + 指针安全运行时
- 2026-07-18 M29/M30：COM DLL DISPID 不一致（Calc.Add 找不到成员）；x86 交叉编译路径拼接
- 2026-07-14 M27/M28：第三方 COM 事件 SourceIID；VB6 字符串语义（vbCrLf / 双引号转义）+ Format 约 600 行重写
- 2026-07-06~10 P24/P25：COM 后期绑定 + 集合枚举 + 服务质量；frxParse 端到端跑通
- 2026-07-02~05 M12/M13：FRX 二进制资源读取；双架构 x64/x86；COM ProgID / Variant 数组索引 / 定长字符串系列修复
- 2026-07-01 M22：首个真实 VB6 工程（realVBP + 窗体 demo）编译链接成功，9 项运行时修复
- 2026-06-30 P17-P21：兼容性填平（Batch A-F）+ 74/74 回归零失败基线，M14
- 2026-06-29 P14-P16：兼容性深化 + WithEvents 控件事件 + ActiveX DLL 能力验证，M11/M12
- 2026-06-27 P8-P10：TypeLib 内建生成替代 MIDL；RTL 改以 RC 资源内嵌（零配置编译），M9/M10
- 2026-06-26 P5-P7：多模块工程 / COM 对象模型 / 窗体控件三项综合验证，M6-M8
- 2026-06-25 P2-P4：两遍语义 + 120+ 内置符号；C 代码生成 + MSVC 驱动，编译出 hello.exe，M3-M5
- 2026-06-24 P0-P1：词法器（LL(2)+Unicode）+ 解析器（约 2900 行）跑通，M1/M2
