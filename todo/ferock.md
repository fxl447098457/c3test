# ferock 的个人 todo

> 想法池（草稿区），约定见根目录 [todo.md](../todo.md)。决定动手的条目转仓库 Issue 后从这里删除。

## vbman 编译一致性（C3 vs 微软 VB6）

- [ ] 对照微软版 VB6，让 vbman 代码的编译结果在 C3 下与 VB6 编译结果一致，包括 VBMAN.dll 的对应执行结果
  - 基线：VB6Mini 编译 `tests\vbman\src\VBMAN.vbp` 成功（129 模块，产物 dist\DLL\VBMAN.dll 2.3MB）；C3 前端 129 模块 0 error，cl 阶段 21 错误（详见 [docs/vbman/001_first_compile.md](../docs/vbman/001_first_compile.md)）
  - 修复顺序：跨类调用实参缺失 ×15 → 事件 sink 声明 ×3 → Variant 转换散点 ×3，每修一类跑 `docs\vbman\build_vbman.ps1` 观察错误数递减
  - 编译一致后进入运行一致：VB6 与 C3 各自产出 VBMAN.dll，对比执行结果
  - 待用户后续提供 vbman 单元测试

## changelog
1. Fix 095（driver.cpp）：撞名时标准模块过程优先于类成员——mWebSocketUtils.bas 与 cWebSocketUtils.cls 双定义，裸调该解析到模块版（无 me 参数）。-13，一处修复清掉 WebSocket 全家
2. Fix 096（cgen_com.cpp）：WithEvents 未实现的事件生成空 stub，vtable 不再悬空引用。-3
3. Fix 097（type_system.cpp + cgen_expr.cpp）：CompareMethod（VBA 枚举的无 vb 前缀别名）两处统一映射 Long。-1
4. Fix 098（cgen_stmt.cpp）：Set 左值是函数调用表达式 → 求值后丢弃（VB6 语义 no-op）。-2
5. Fix 099（parser_expr.cpp）：归因过程中最大反转——诊断打印 0 命中证明问题根本不在 cgen，而在 parser：With 块内 Users.Decode .Rs 被误解析成链式成员访问而非“实参 .Rs”。Fix 077 的空格检测扩展到 obj.Method .Field 形态。-2
