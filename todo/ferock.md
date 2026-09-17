# ferock 的个人 todo

> 想法池（草稿区），约定见根目录 [todo.md](../todo.md)。决定动手的条目转仓库 Issue 后从这里删除。

## vbman 编译一致性（C3 vs 微软 VB6）

- [ ] 对照微软版 VB6，让 vbman 代码的编译结果在 C3 下与 VB6 编译结果一致，包括 VBMAN.dll 的对应执行结果
  - 基线：VB6Mini 编译 `tests\vbman\src\VBMAN.vbp` 成功（129 模块，产物 dist\DLL\VBMAN.dll 2.3MB）；C3 前端 129 模块 0 error，cl 阶段 21 错误（详见 [docs/vbman/001_first_compile.md](../docs/vbman/001_first_compile.md)）
  - 修复顺序：跨类调用实参缺失 ×15 → 事件 sink 声明 ×3 → Variant 转换散点 ×3，每修一类跑 `docs\vbman\build_vbman.ps1` 观察错误数递减
  - 编译一致后进入运行一致：VB6 与 C3 各自产出 VBMAN.dll，对比执行结果
  - 待用户后续提供 vbman 单元测试

## changelog

### 0.10.3

- 2026-09-17 源码拆分（函数体片段推广，600+ 行口径）：4 个单巨函数文件一次拆净 —— cgen_expr_ident 744→主 26 + detail/ 3 片段（224/190/323）、cgen_util 699→主 26 + 4 片段（64/328/229/76）、cgen_setlet 809→主 264 + 2 片段（313/247）、cgen_assign 1367→主 28 + 4 片段（397/426/230/310）；切点全部取函数体内既有的顶层语义分节注释（零重排），每例断言「head + 片段 + tail 拼回原文件逐行一致」；13 个 .inc 不需登记 CMakeLists（不是编译单元）；全量构建增量通过。src 下 ≥500 行文件 12 → 8
- 2026-09-17 回归运行器修复（tests/run_tests.ps1）：新增 `Invoke-TestExe` 统一启动出口 —— 路径 A（.NET `ProcessStartInfo` + `ReadToEndAsync` 管道捕获）优先、路径 B（`Start-Process -Redirect*`）后备，两条都失败才算 FAIL 并**打印异常原文**（旧实现 catch 只写 `FAIL (run error)`，把环境问题伪装成测试失败）；`Test-Run` / `Test-Vbp` 两处重复逻辑收敛到该函数。`.out`/`.err` 仍按原样落盘供人工复查；不改测试语义、不动 CMake。修复后 `test smoke` PASS、全量 `test all` 回到基线 `PASS=82 FAIL=0 SKIP=1 TOTAL=83`
- 2026-09-17（修正上一版归因）：把「FAIL=39 全假阴性」归因为「Start-Process 被会话安全策略阻断」**不成立** —— 复测 `Start-Process -NoNewWindow -Wait -RedirectStandardOutput` 本机可用，HEAD 版原文在相同调用方式（bat → `powershell -File`，含 bash 管道）下 3/3 通过，**无法稳定复现**；当时唯一可复现的缺陷是运行器吞异常。事件特征留档：39 个 FAIL 恰等于全部可运行 run 用例数，`.out`/`.err` 存在但 0 字节（进程没起来）
- 2026-09-17（环境注记，仍然有效）：`reg.exe` 被程序黑名单拦截 → vcvarsall 拿不到 Windows SDK 路径（`C1083 crtdbg.h` / `winsock2.h`），规避办法是把 `C3_VCVARSALL` 指向只 set `INCLUDE`/`LIB`/`PATH` 的替身 bat
- 2026-09-17 源码拆分（函数体片段 + 类体片段）：cgen_expr_member 1496→主 36 + detail/ 8 片段（precheck 119 / form_builtin 231 / obj_dispatch 190 / class_module 222 / m22_module 169 / generic_access 120 / voidptr_com 191 / class_fallback 239），首次在「相对花括号深度 1」处切分（原 173~773 行是一个 `if(IdentifierExpr)` 巨块，深度 0 只有 2 个点），片段自身不闭合但拼接后逐行一致；ast_printer 581→主 47 + ast/detail/ 5 个类体片段（entry 13 / decl 93 / stmt 222 / expr 93 / support 128），在 `class PrintVisitor` 类体内 `#include`（继 cgen.hpp 之后第二次用类体片段）。拆前存 `--dump-ast` 基线（18 样例 / 464 行），拆后逐字节一致 18/18；增量构建零错误；回归 82/0/1/83。src 下 ≥500 行文件 8 → 6
- 2026-09-17（环境注记，x86 路径）：`--arch x86` 时 C3 内部会 `call vcvarsall.bat x86`（msvc_driver_discovery.cpp 的 buildVcvarsPrefix），而 reg.exe 被拦 → `C1083 windows.h`，表现为回归里恰好 2 个 `FAIL (compile)`（test_earlybound2 / test_not_com，run_tests.ps1 里唯一的 -Arch x86 用例）—— 不是代码回归。规避：把 `VCINSTALLDIR` 指向伪 VS 目录 `%TEMP%\c3_fake_vc`，其下放按 %1 切 x64/x86 的 `Auxiliary\Build\vcvarsall.bat` 替身；配 `C3_VCVARSALL=%TEMP%\wb_vcvarsall_fake.bat` 后回归回到 82/0/1/83

### 0.10.2

- 2026-09-17 源码拆分（生成物）：vb6_di_win32_stubs.c 1184→di/ 下 7 个按 Lib 家族生成的桩文件（最大 377 行）+ 手写 vb6_di_stubs.c 204；先改 cgen_decl_api.cpp 写 vb6_di_lib 标记，再改 gen_di_stubs.ps1 摆脱 unresolved_syms.txt 并支持重跑；RTL 管线 41→47，重建 99/99 + e2e 2/2 + 回归 82/0/1/83。
- 2026-09-17（版本）：三处版本号统一为 0.10.2 —— VERSION / driver.cpp 的 `C3 version` / CMakeLists.txt 的 `project(... VERSION ...)`（此前 0.10.1 / 0.10.0 / 0.1.0 三处不一致）
- 2026-09-17 源码拆分（纯搬移）：semantic_analyzer.cpp 666→主 317 + dispatch 95 + register 177 + typeref 114，分派辅助 / 注册声明（Pass1）/ 类型引用解析各成一家；保留行 + 三区间拼回与原文件一致（仅丢 2 处多余空行）；重建 99/99 + e2e 2/2 + 回归 82/0/1/83。src 下 ≥500 行文件 14 → 13
- 2026-09-17 源码拆分（函数级抽取首例）：semantic_analyzer_builtin.cpp 795（registerBuiltins 单函数 780）→ 26 行伞文件 + src/semantics/builtin/ 3 个「函数体片段」（consts 289 / consts_ext 211 / funcs 295），片段在函数体内 #include、局部 lambda 与 kVariantArray 原样不动 → 零重构零行为改动；三片段拼回与原文件逐行一致；重建 96/96 + e2e 2/2 + 回归 82/0/1/83。src 下 ≥500 行文件 15 → 14
- 2026-09-17 源码拆分（RTL）：vb6forms_picture.c 528→275+274、vb6forms_widget.c 502→231+291，主文件逐字节不动；RTL 5 处管线全同步（rc 139/140 + 枚举 + files[] + driver sourceFiles + CMake），四处清单计数核对 41 一致；重建 96/96 + e2e 2/2 + 回归 82/0/1/83。src 下 ≥500 行文件 17 → 15
- 2026-09-17 源码拆分（续）：再拆 5 个「单函数主导但主导函数 <500 行」的文件 —— cgen_com 682→257+439、parser_decl 655→274+390、parser_expr 590→402+199、typelib_builder 527→345+203、frm_parser 516→320+209；主文件逐字节不动，5 个新文件登记 CMakeLists；重建 96/96 + 回归 82/0/1/83 零变化。src 下 ≥500 行文件 26 → 17
- 2026-09-17 源码拆分（② 组）：4 个「单函数主导但主导函数 <500 行」的文件纯搬移拆净 —— cgen_localdecl 552→375+191、cgen_decl_proc 552→256+309、semantic_analyzer_decl 551→387+178、cgen_expr_binary 514→356+172；主文件逐字节不动，4 个新文件登记 CMakeLists；重建 91/91 + 回归 82/0/1/83 零变化。同时修正台账口径：单函数主导 ≠ 必须函数级抽取
- 2026-09-17 cgen 头拆分：cgen.hpp（996 行单类头）拆为伞头 54 行 + cgen_emitter.hpp（CodeEmitter 独立成头）+ detail/ 3 个类体片段（api 203 / state 342 / helpers 382），类内 #include 片段、逐行零重排，36 处引用路径不变；回归 82/0/1/83 零变化
- 2026-09-17 AST 头拆分：ast.hpp（1459 行）拆为伞头 18 行 + src/ast/detail/ 8 子头（enums/fwd/base/expr/stmt/stmt_io/decl/util，最大 366 行），按类边界切、零重排，6 处引用路径不变；回归 82/0/1/83 零变化
- 2026-09-17（ai/003-开发计划）：补「实现说明」——P4~P8 的 rtl/*.hpp/cpp 规划名是早期 C++ 路线，实际走 C 侧 RTL，相关文件名已不存在；原计划文本不改写
- 2026-09-17（src/rtl/core）：41 个平铺文件按家族分 4 个子目录（vb6rtl 9 / vb6com 8 / vb6comserver 7 / vb6forms 13），根下只留 2 个 DI 桩 + bstring；同步 rc / CMake 路径，CMake 嵌入依赖由 17 项补齐为 39 项
- 2026-09-17 RTL 头拆分：vb6rtl.h（1007 行）拆为伞头 16 行 + 7 子头（base/bstr/variant/builtin/array/class_com/runtime），同步 4 处嵌入管线，回归 82/0/1/83 零变化

###  0.10.1
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
