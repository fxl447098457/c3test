# 025-Lambda 计划书

> 日期：2026-09-23
> 状态：**提案，待拍板**（锚点为本轮实测；开工前须复核漂移）
> 一句话结论：**v1 只做「表达式 lambda + 无捕获」**，语法 `Function(a As Long, b As Long) As Long => a < b`，值表示**复用已发货的 Delegate thunk**（不新增第二种函数指针形态）。两条不可让步的硬规则在第四节。

---

## 一、为什么"无捕获"是设计前提而不是省事

`src/backend/module/cgen_delegate.cpp:8-12` 的已发货口径：

> 委托值 = 生成调用桩（thunk）的地址，**与 LongPtr 位兼容**；thunk 对外签名 = 委托约定，体内纯转发转调 cdecl 目标，约定差异被桩吸收，零 ABI/参数漂移。x64 下 MSVC 接受并忽略 `__stdcall`，两架构同一份生成码。

一旦允许捕获，委托值必须变成「thunk + 环境指针」胖对，于是当场撕裂三样东西：

1. **LongPtr 位兼容**——已发货特性（Delegate）的对外契约，也是它当初的验收硬要求；
2. **传给 C API 的能力**——`Declare` 的函数指针形参、`AddressOf` 那条链，拿到的必须是真函数指针；
3. **被捕获变量的生命周期**——VB6 局部在栈上，捕获 `ByRef` 局部即悬垂；捕获对象引用又要接上刚做的 `__refcount`（022 B05）→ 闭包对象与接口计数撞在同一块地上。

所以：**无捕获不是 v1 过渡，是"要么这样、要么另立一个胖对设计"的分岔**。将来若真要闭包，那是新特性（新值表示 + 新 ABI 论证），不许以"给现有委托值加个字段"的形式溜进来。

## 二、语法

```vb
Delegate Function CompareFn(a As Long, b As Long) As Long      ' 已发货

Dim cmp As CompareFn = Function(a As Long, b As Long) As Long => a < b
Call Sort(arr, Function(a As Long, b As Long) As Long => a < b)
Call ForEach(list, Function(x As Long) As Long => Print(x))     ' 返回值可弃
```

- **`=>` 是新 token**（实测：`src/lexer/lexer.cpp` 今天对 `=>` **零命中**，需加 `TokenKind::FatArrow` + `kindToString`）。它属于本项目最擅长的那类**"错误→可解析"安全新增**：`=` 后紧跟 `>` 今天不成词、必然落报错分支（同 022 B01 的属性行手法）。
- **必须没有 `End Function`**。带 `End` 的多语句体塞进表达式位，会与 `:`（行内语句分隔符）及后置 `)` 的归并产生真歧义——那是词法/语法层要付一大笔债的地方，单表达式 + `=>` 把债直接抹掉。
- **参数类型可省略，当且仅当期望类型已知**（赋给 `As <Delegate>` 变量、或实参位对应形参是某 Delegate 类型）：`Function(a, b) => a < b` 合法。C3 **没有** `Dim x = …` 局部推断，无期望类型时写不全就报错——不要为了"现代"去发明类型推断层。
- 诊断文本一律 ASCII（022 D12 硬约束）；用例文件 GBK+CRLF。

## 三、明确不做（写死，避免下轮复议）

| 不做 | 理由 |
|---|---|
| 捕获（闭包） | 第一节 |
| 多语句 lambda（带 `End Function`） | 表达式位定界歧义 |
| `Sub` 型 lambda（无返回值） | 没有表达式可绑，收益极低 |
| `x => expr`（连 `Function` 都省） | `x =>` 需与"表达式后跟 `>=`/比较"重算优先级，省一个词不值得动表达式解析器 |
| 泛型形参被 lambda 实参填充 | 等 Generics 侧稳定；且与泛型克隆路径叠加 |
| async / iterator lambda | 本项目无 async、无 `yield`（关键字层实测零命中），不在此特性范围 |

## 四、两条不可让步的硬规则

### R1 · lambda 与重载决议的互锁（必须先定，不能留给实现时随机决定）

Overload 体系按**实参类型**打分（`$ov$` 键 + `_ov` 指纹 mangling + 五档打分，先例 `ea0591b`），而无类型 lambda 的参数类型要等**期望类型**定、期望类型要等**候选集**定 → 循环。

> **规则**：省略参数类型的 lambda 实参**不参与**重载打分；若某次调用因此仍有 ≥2 个候选匹配，直接报
> `Ambiguous overload: specify lambda parameter types`。
> 写全类型时，该实参按其 **Delegate 类型**参加打分（等同传委托值/`LongPtr`），**绝不**试图按 lambda 体推断参与。

配套：`ParamArray` 位置不接受 lambda（照 022 D11"接口成员含 ParamArray 先警告"的同族手法给一条诊断）。

### R2 · 捕获检测必须是显式诊断，且有负例

lambda 体内引用宿主过程的局部变量、参数或 `Me` → 报 `Lambda cannot capture locals`（并说明：请提为模块级 `Private`，或改用具名过程 + `Delegate`）。这条不是"顺手加的报错"，它是**第一节的边界在编译器里的唯一执行点**；没有它，捕获会从"看起来能用其实指向栈"的门溜进来，而那类 bug 是本项目历史上最难查的一族。

## 五、落地接缝

| 层 | 落点 | 动作 |
|---|---|---|
| 词法 | `src/lexer/token.hpp`、`token.cpp`、`lexer_keywords.cpp`、`lexer.cpp` | 加 `FatArrow`；**不进** `isStatementStart`（022 D12 软关键字边界） |
| AST | `src/ast/detail/ast_decl.hpp` 或 expr 侧 | `LambdaExpr{ params, allTypesInferred, retType, Expr body }`；**`src/ast/ast_clone.cpp` 必须补拷贝**（022 B02b 踩过：漏拷会让泛型特化副本静默丢信息） |
| 语义 | `src/semantics/semantic_analyzer*.cpp` | 期望类型查 Delegate 登记表（现成）；实现 R1 + R2 |
| 发码 | `src/backend/module/cgen_delegate.cpp` 通道 | 在宿主模块发 `static` 匿名过程 `vb6_lam_<mod>_<n>`，再走现成 `delegateThunkName`（`:26`）生成 thunk——**不新增第二种函数指针表示** |

## 六、批次（门 = `scripts/dev.ps1 -SkipTest` + `tests/run_tests.ps1 -Category all` 零新增失败）

| 批 | 内容 | 状态 | 验收 |
|---|---|---|---|
| L01 | `=>` token + `LambdaExpr`（含克隆路径）+ 语法（全类型形式），**不发码** | ☐ | `test_lambda.bas` 语法正例；缺 `=>`/缺 `As` 的负例 |
| L02 | 期望类型推断（省略参数类型）+ R1 歧义规则 | ☐ | 正例：`Dim d As CompareFn = Function(a, b) => a < b`；负例：重载两可 → 必须报 ASCII 歧义文本 |
| L03 | R2 捕获检测 + 发码（匿名过程 + 复用 delegate thunk） | ☐ | 端到端：`Call Sort(..., Function(a As Long, b As Long) As Long => a < b)` 真排序断言 `LM1..LM4` |
| L04 | x86 + x64 双跑 + 把 lambda 值传给 `Declare` 的 C API 回调 | ☐ | API 回调用例（证明位兼容仍在）；负控**跑修复前二进制**（022 教训） |
| L05 | 逐字节护栏 + 手册页 `docs/vb6-manual/…`（Delegate 页补 lambda 一节） | ☐ | 8 文件 `--emit-c` 对基线全同 |

L03/L04 改到 Delegate 共同发码路径 → 逐字节护栏必做；`tests/run_tests.ps1` 改前自核字节头（本轮实测它带 UTF-8 BOM，与 022 D16-6 记录不符）。

## 七、风险

| # | 风险 | 缓解 |
|---|---|---|
| LR1 | 有人以"加个环境指针"实现捕获，撕裂 LongPtr 位兼容 | 第一节 + R2 诊断都在编译器里有执行点；本表是反悔成本的记录 |
| LR2 | R1 未先定 → 实现期随机决定，日后无法改 | 写成诊断 + 用例锁死（L02 的负例就是契约） |
| LR3 | 匿名过程命名 `vb6_lam_<mod>_<n>` 与用户过程重名 | 前缀 `vb6_lam_` 走 `cIdent` 保留区；加一条负例（用户过程叫 `lam_1` 不应相撞） |
| LR4 | lambda 体里再出现 `AddressOf`/`Set` 等语句性构造 | v1 限制体为纯表达式，解析层即拒（错误→可解析，零回归面） |

## 八、待拍板

- **O1** 是否允许 lambda 直接出现在 `Declare` 实参位并隐式转成委托（倾向：只允许形参类型是 Delegate 类型的位；`LongPtr` 位要显式，避免"看起来能用其实约定不符"）。
- **O2** `Print(x)` 这类返回值的过程在 lambda 体末尾被当表达式用时的处理（VB6 里 `Print` 是语句，不是函数）——v1 建议：体必须是表达式，语句一律拒。
- **O3** 是否支持 `=> `右侧接 `AddressOf`（无意义，建议拒）。
- **O4** 匿名过程的 C 符号是否进 `.map`/调试信息，与将来 DAP 的对齐方式（先记，不定）。
