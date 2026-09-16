# ferock 的个人 todo

> 想法池（草稿区），约定见根目录 [todo.md](../todo.md)。决定动手的条目转仓库 Issue 后从这里删除。

## vbman 编译一致性（C3 vs 微软 VB6）

- [ ] 对照微软版 VB6，让 vbman 代码的编译结果在 C3 下与 VB6 编译结果一致，包括 VBMAN.dll 的对应执行结果
  - 基线：VB6Mini 编译 `tests\vbman\src\VBMAN.vbp` 成功（129 模块，产物 dist\DLL\VBMAN.dll 2.3MB）；C3 前端 129 模块 0 error，cl 阶段 21 错误（详见 [docs/vbman/001_first_compile.md](../docs/vbman/001_first_compile.md)）
  - 修复顺序：跨类调用实参缺失 ×15 → 事件 sink 声明 ×3 → Variant 转换散点 ×3，每修一类跑 `docs\vbman\build_vbman.ps1` 观察错误数递减
  - 编译一致后进入运行一致：VB6 与 C3 各自产出 VBMAN.dll，对比执行结果
  - 待用户后续提供 vbman 单元测试
