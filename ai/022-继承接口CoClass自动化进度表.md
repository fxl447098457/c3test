# 022-继承接口CoClass 自动化进度总表

> 本文件是每小时自动化任务（"实现继承、接口与CoClass"）的**唯一状态源**。
> 每次运行开始先读本文件，结束前必须更新本文件（状态头 + 批次清单 + 运行日志）。
> 规范输入: `ai/讨论记录/018-接口继承与CoClass设计思路.md`（含 tB 文档要点与分阶段设计思路全文）。

## 状态头（机器可读，每次运行维护）

```
STATUS: IDLE               # NOT_STARTED | DESIGN | BUSY | IDLE | ALL_DONE
LAST_RUN: 2026-09-23T04:32:00+08:00   # 本轮 B02 主体过门并提交。开工核查（03:07）：HEAD=3add1ce（B01 已真实入库、
               # 工作树 clean）、无源文件比当时 C3.exe(01:18, md5 5e9eb1cf) 新、无 C3/cl/link/ninja 进程 → 树内容
               # 与 02:30–02:59 那次 111/0/1/112 的过门态同源，按 D14「无他人改动直接量基线」未重复跑基线回归。
               # 03:30 首门 118/0/1/119 → 复核源码时补两处护栏（见 D15-6 与运行日志）→ 04:02 终门，exe md5 全程一致。
LAST_COMMIT: 本轮 `Interface:` 提交（B02 主体，哈希见 git log；总表与代码同提交故此处不重复记哈希）；此前 B01 = 3add1ce
CURRENT_BATCH: **B02b**（B02 未完项，先于 B03）：① 成员级 `Implements I.M[, I.N]` 尾子句 ——
               `parser_decl.cpp` 签名解析完成后、`expectEndOfStatement` 前加可选逗号列表，存进
               Sub/Function/Property Decl 新字段，`checkNewStyleInterface` 里显式绑定优先于同名隐式
               匹配（用例照旧走单 `.cls` + `Test-SyntaxFail`，见 D15-5）；② 泛型模板内 Interface 的
               拒绝分支目前无用例（需 `Class X(Of T)` 工程），补一条；③ legacy 交互债：新式接口名仍
               进 `classSym->implementsNames`，ActiveX DLL 的 `cgen_util_dllentry_tables.inc:88-130`
               会按旧口径 mint IID（EXE 工程无影响，B13/B16 接线时必须分叉，见 D15-8）。
               之后才是 B03（`.cls` 头行 `Interface IFoo … End Interface` + `Module::isInterfaceModule`
               子标记 + 语法手册索引）。开工前照例先核 `git log`/`git status`/`.build/C3.exe` 的
               mtime+md5，判断要不要重量基线（上一轮曾出现"总表已 IDLE 但 commit 尚未落盘"的 4 分钟窗口）。
GATE_BASELINE: Results: PASS=118 FAIL=0 SKIP=1 TOTAL=119   # exe md5 13e99638…（.build/gate_B02F.log，04:02–04:24
               全程无插队重建，跑前后 md5 一致；SKIP=已知 test_vbman 环境项）。上一基线 111/0/1/112（B01）
               → 本轮新增 7 条用例（itf_n08..n13 + itf_p01）全绿、零新增失败。
```

> 重入保护：若运行开始时 STATUS=BUSY 且 LAST_RUN 距今不足 55 分钟，说明上一次运行可能仍在进行——本次**立即结束，不做任何修改**。

## 范围与验收硬边界（用户已确认，2026-09-23）

1. 深度：**含完整 COM 兼容**（P6 必做：IUnknown/IDispatch、类型库导出、DllGetClassObject/DllRegisterServer、CoCreateInstance/CreateObject 外部激活）。
2. 提交：每个批次过全量回归门后，仅暂存本批相关文件，commit 到当前分支（fan/dev），**永不 push**。
3. 收尾：全部批次完成后 STATUS=ALL_DONE；此后每次运行只复跑全量回归做只读验证并记一行日志，不改代码。

## 现状盘点（建表时已核实）

- VB6 式 `Implements` 已有：parser 语句收集（parser_module.cpp:119）、语义验证接口成员覆盖（semantic_analyzer.cpp:232-267）、`implementsNames`/`isInterface` 符号字段。
- COM 后端基础设施已存在：`src/backend/module/cgen_com.cpp`、`src/typelib/typelib_builder.*`、`src/com/typelib_parser_*`。
- `Inherits` / 显式 `Interface...End Interface` / `CoClass...End CoClass` 语句：**未实现**（grep 无语言层命中）。
- 语言风格先例：Delegate(a15c40b)、Overload(ea0591b)、Generics(7e2f9ed) 均为"tB式适配到本项目"，含 mangle 方案与护栏测试。

## 阶段计划（P1-P7，批次数可拆分但范围不得越界）

- **P0 设计细化**：结合 018 文档 + 现状，产出本项目语言映射设计并写入本文件"设计记录"节：Interface/CoClass 语句落在哪种模块宿主、成员 mangle 键（参照 $ov$/_g_ 先例）、vtable 内存布局与现有 cgen_com 的关系、GUID 来源（[InterfaceId]/[CoClassId] 或自动生成规则）、与既有 VB6 Implements 的共存策略、x86/x64 约束。设计完成后直接开工，不等人工批准（用户已授权自动化）。
- **P1 Interface 语句 + 编译期契约**：`Interface ... End Interface`（含 Extends 接口链、Sub/Function/Property Get/Let/Set 成员、无实现体检查）；Implements 完整性检查接入接口链；裸接口成员调用编译期解析。
- **P2 接口多态与生命周期**：接口变量 `Set`/`New`、按 vtable/表指针派发、引用计数与释放、接口↔类转换（TypeOf/隐式契约校验）。
- **P3 Inherits 类继承**：单继承、成员继承与遮蔽、Overridable/Overrides 虚钩子、Protected 可见性、MyBase 式显式基调用。
- **P4 Implements Via**：委托式实现（免手写转发），生成转调桩。
- **P5 CoClass 语句**：`CoClass ... End CoClass` + `[Default] Interface`、`[InterfaceId]/[CoClassId]`、组内 `New CoClass`/`CreateObject(ProgID)` 激活、默认接口派发。
- **P6 COM 兼容**：IUnknown 三件套运行时、IDispatch（GetIDsOfNames/Invoke）、类型库导出接入现有 typelib_builder、DllGetClassObject/DllRegisterServer/DllUnregisterServer/DllCanUnloadNow、外部（VB6/VBA/脚本）激活冒烟验收。
- **P7 终验收尾**：端到端示例工程 + 全量回归 + 文档（018 附录或新 0xx）+ STATUS=ALL_DONE。

每阶段要求：新增对应测试类别/用例；门 = `scripts/build.bat`（或 dev.ps1）构建 + `tests/run_tests.ps1 -Category all` 零新增失败；护栏参照先例（如零重载逐字节一致）。

## 批次清单（P0 细化后逐批登记，完成打勾）

| 批次 | 阶段 | 内容 | 状态 | Commit | Gate |
|---|---|---|---|---|---|
| B00 | P0 | 设计细化并写入本文件 | ☑ | 74ae1e7 | 纯文档批次，未构建（理由见运行日志） |
| B01 | P1 | 词法/AST/语法：`Interface…End Interface` 块 + `Extends` + 成员签名 + 无实现体检查 + 方括号属性行 | ☑ | 3add1ce | `Results: PASS=111 FAIL=0 SKIP=1 TOTAL=112`（gate_B01.log 全量）+ 7 条负例全绿 + x64/x86 双跑 + 8 文件（6 .bas/2 .vbp）emit-c 对无 B01 基线 exe 逐字节全同 |
| B02 | P1 | 语义：接口符号注册 + Extends 链 prepass(2.7) + 槽位表 + Implements 契约完整性/签名比对 + 成员级 `Implements I.M` 子句 | ◐ | 本轮 `Interface:` 提交（见 git log） | `Results: PASS=118 FAIL=0 SKIP=1 TOTAL=119`（gate_B02F.log；exe md5 13e99638 跑前后一致）+ 新增 7 条用例全绿 + legacy `test_implements` 仍 PASS。**已交付**：stage 2.7 `runInterfacePrepass`/`IfaceRegistry`/Extends 链与槽表/五类诊断 + `checkNewStyleInterface` 严格契约比对（D15 记 1/2/3/4/5/6）。**未完（→ B02b）**：成员级 `Implements I.M` 子句；`SymbolKind::Interface` 按 D15-2 推迟到 B04 与消费点同批 |
| B03 | P1 | `.cls` 头行宿主形式 `Interface IFoo … End Interface`（1 文件 1 接口）+ 语法手册索引 | ☐ | | |
| B04 | P2 | 接口值代码生成：`vb6_ivtbl_<I>` COM 形态槽表 + 类侧实例 + 薄指针表示 + `As <Iface>` 变量登记 + 派发 | ☐ | | |
| B05 | P2 | 生命周期：实现类结构前置 vtbl 指针数组 + refcount 头 + AddRef/Release + Set/Nothing/作用域释放 | ☐ | | |
| B06 | P2 | 转换与判定：接口↔类、多接口对象、`TypeOf … Is <接口>`、上/下行转换契约校验 | ☐ | | |
| B07 | P3 | `Inherits` 语法 + 类链检测（单继承/环/深度）+ 继承成员合并与遮蔽 + 派生域 | ☐ | | |
| B08 | P3 | `Protected` 可见性 + `Overridable/Overrides/NotOverridable` + 类级虚表 `vb6_cvtbl_<Cls>` 与多态派发 | ☐ | | |
| B09 | P3 | `MyBase.M(…)` 显式基调用（去虚化）+ 构造链顺序 + 无新语法逐字节护栏 | ☐ | | |
| B10 | P4 | `Implements IFace Via <holderVar>` 委托式实现：持有字段 + 自动转调桩 + 签名检查 | ☐ | | |
| B11 | P5 | `CoClass…End CoClass` 语法 + `[CoClassId]/[Default] Interface/[ComCreatable]/[CoClassCustomConstructor]` + 契约聚合校验 | ☐ | | |
| B12 | P5 | 组内激活：`New <CoClass>` / `CreateObject("ProgID")` 编译期映射 + 默认接口派发 | ☐ | | |
| B13 | P6 | IUnknown 三件套真实实现（IID 表 QI / 原子 AddRef-Release）+ 对象布局 COM 化收尾 | ☐ | | |
| B14 | P6 | IDispatch 四件套接入新式接口（GetTypeInfo/GetIDsOfNames/Invoke + DispId 表） | ☐ | | |
| B15 | P6 | 类型库导出：typelib_builder 从 dispinterface 扩到 TKIND_INTERFACE/dual + GUID 来源接线 | ☐ | | |
| B16 | P6 | DllGetClassObject/DllRegisterServer/DllUnregisterServer/DllCanUnloadNow 对新式 CoClass/类工厂接线 + x86/x64 双验 | ☐ | | |
| B17 | P6 | 外部激活冒烟验收（CoCreateInstance 早绑定 + CreateObject/IDispatch 晚绑定 双路） | ☐ | | |
| B18 | P7 | 端到端示例工程 + 全量回归 + 设计文档归档（018 附录或新 023）+ STATUS=ALL_DONE | ☐ | | |

> 批次可按实施中发现的耦合度合并/拆分，但**阶段范围不得越界**；每次运行只推进能各自独立过门的批。

## 设计记录（P0 / B00 产出，2026-09-23）

> 所有代码位置均为本次核实的现状坐标；实施时若已漂移以实际为准。

### D1 语法宿主（tB "twin 模块" 的本项目映射）

- 本项目**无 twin 模块**：模块种类只由扩展名在 `src/driver/driver_frontend.cpp:150-164` 决定，`.cls|.ctl|.pag → isClassModule`、`.frm → isFormModule`，`Module`（`src/ast/detail/ast_decl.hpp:275-306`）上没有 ModuleKind 枚举。
- **决策**：`Interface`/`CoClass` 作为**模块级声明块**，允许出现在 `.bas` 与 `.cls` 中（现有 `Enum`/`Type`/`Declare`/`Attribute` 在所有模块种类都无门控，故新增块不需门控改造）；名字**工程级唯一**，等价于 tB 的"工程级可见"。`.frm`/`.ctl` 不接受（头部分支有专用扫描逻辑）。
- 两种宿主形式：① **块形式** `Interface IShape … End Interface`，可与其他声明共存于 `.bas`（一个文件多个接口）；② **头行形式**（B03）`.cls` 首行 `Interface IFoo` + 尾行 `End Interface`，完全镜像既有 `Class Name(Of T)` 头行识别（`src/parser/parser_module.cpp:89-110`）与 `End Class` 消费（:105-110），使"一文件一接口"的 VB6 习惯成立。头行形式需要 `Module` 上新增 `isInterfaceModule/isCoClassModule` 子标记（该信息今天连 `.ctl/.pag` 都在 driver_frontend 局部丢失，正是同一处接缝）。
- 插入接缝：模块级主循环 `parser_module.cpp:82-165`，在 `Implements`(:120) 与 `Attribute`(:137) 分支之间加 `Interface`/`CoClass` 分支；块体解析模板取 `Enum…End Enum`（`parser_decl.cpp:260-290`，纯签名无体），成员含过程声明时取 `Property…End Property` 的 `parseBlockUntil` 式。
- **关键字策略（重要，零误伤）**：`Interface/CoClass/Inherits/Extends/Overridable/NotOverridable/Overrides/Protected/MyBase/Via` 全部按 Delegate(a15c40b) 的 4 处登记：`src/lexer/token.hpp` 枚举 + `src/lexer/token.cpp`（isKeyword/isStatementStart/kindToString）+ `src/lexer/lexer_keywords.cpp` 表 —— **并且同时登记进软关键字表 `src/parser/parser_helpers.cpp:14-63`**（先例：`Access`/`Default`/`Name` 等）。软 = 仍可在 `canBeName()` 位置当标识符用，块头部分支按 `kind` 精确匹配，两者不冲突。语料核查：`Interface` 一词在 tests/archive/publish 的 .bas/.cls/.frm 中仅出现于字符串与注释（6 文件，含 VBFlexGrid 的 `Attribute *.VB_Description`），其余 9 个词零命中；软登记是二道保险。
- **属性行 `[InterfaceId("{…}")]`**：lexer 现状是 `[` 直接扫到配对 `]` 产出**一个 Identifier token（文本含方括号）**（`src/lexer/lexer.cpp:231-244`），因为 `[My Type]` 名称引用语法依赖它。故**不改 lexer**：在模块级/声明前位置，若 Identifier token 文本以 `[` 开头且以 `]` 结尾，则用一个小解析器拆成 `属性名(+可选字符串实参)` 并挂到"下一条声明"上。今天这类行必然落入 `parser_module.cpp:160-164` 的 "unexpected token at module level" 错误分支，**错误→可解析，零回归风险**。
  - 现状缺口：`Attribute` 从不附着到声明节点（`Module::attributes` 是扁平列表，仅 `VB_Name`/`MultiUse` 被消费），也不存在 `Decl::attributes`。**决策**：给 `InterfaceDecl/CoClassDecl` 各自持有 `attributes` 字段（块级），成员级属性存进成员节点；不引入通用 `Decl::attributes`，避免全 AST 波及。
  - 支持集合（对齐 tB）：接口级 `[InterfaceId]`/`[Description]`/`[Hidden]`/`[Restricted]`/`[OleAutomation]`/`[ComImport]`/`[ComExtensible]`（后四类 P1 仅**接受+存档**，语义在 P6 才消费）；成员级 `[DispId]`/`[PreserveSig]`/`[Description]`；CoClass 级 `[CoClassId]`/`[ComCreatable]`/`[CoClassCustomConstructor]`。**不做** dispinterface 定义、`[Default, Source]` 事件源连接点（红线；仅语法与元数据接受）。

### D2 符号表与 mangle 键（沿用 $ov$ / _G_ 先例）

- 新增 `SymbolKind::Interface`、`SymbolKind::CoClass`（**追加**到 `src/semantics/symbol_table.hpp:23` 枚举末尾，不改既有值序；`Class/ComClass/ComInterface/Delegate` 已在此）。不复用 legacy 的 `Class + isInterface` 标记（`symbol_table.hpp:193-197`，`isInterface` 由 `semantic_analyzer.cpp:246` 与 `driver_compile.cpp:365-384` 的 3.6 阶段设置）——新式接口有显式符号，判定不再靠"被 Implements 才算接口"。
- 槽位（slot）身份键：`lower(<Iface>) + "." + lower(<slotName>)`，与 Overload 的 `name + "$ov$" + fp`（`symbol_table.hpp:329-332`）同族。存储键前缀先例 `$pg/$pl/$ps/$ev/$ty` → 新增 **`$itf$<ifaceLower>.<memberLower>`** 作为"实现映射"键（记录 *类成员 → 接口槽* 的对应，落在 `Symbol` 上的新 `implementsMap`，替代现在"什么都没有、靠字符串前缀猜"的局面：`interfaceMethodParams` 字段至今从未被写入，是死字段）。
- 槽名规范（属性拆三槽，COM 惯例）：`Property Get X` → `get_X`；`Property Let X` → `put_X`；`Property Set X` → `putref_X`；Sub/Function → 原名。C 标识符统一过 `CCodeGen::cIdent`（`src/backend/cgen_base_naming.cpp:49`）。
- 生成的 C 名字（**新前缀，与 legacy 完全隔离**）：
  | 产物 | 名字 | legacy 对照 |
  |---|---|---|
  | 接口槽表类型 | `vb6_ivtbl_<I>` | legacy `vb6_vtbl_<I>` |
  | 类侧槽表实例 | `vb6_ivtbl_<I>_for_<C>` | legacy `vb6_vtbl_<I>_for_<C>` |
  | 每 (类,接口,槽) 适配器 | `vb6_iimpl_<C>_<I>_<slot>` | 无（legacy 直连 `vb6_<C>_<I>_<M>`） |
  | 类级虚表（Inherits） | `vb6_cvtbl_<C>` / `vb6_cvtbl_<C>_impl` | 无 |
  | 对象内接口槽字段 | `__ivtbl[k]`（见 D3） | legacy `__comObj` 保留 |
- 接口内**禁止同名重载**（COM vtable 无重载概念）→ 编译期错误诊断，直接规避与 `$ov$` 体系的交叉复杂度。

### D3 内存布局与派发（P2 起即 COM 形态，避免二次改造）

- 现状：类实例 = `vb6_cls_<Name>`，**首字段固定 `void* __comObj`**（`src/backend/detail/base/cgen_base_generate_c_open.inc:73-112`），成员调用**全部去虚化直调** `vb6_<Class>_<Member>(me, …)`（`src/backend/cgen_util_classcall.cpp:17-228`）；唯一的间接派发是 legacy P6.4 的**胖对** `vb6_iface_<I>{vtbl,obj}` + `…_wrap()`（`src/backend/module/cgen_com.cpp:313-428`，派发在 `src/backend/detail/expr/cgen_expr_call_com_bind.inc:67-97`，赋值改写 `src/backend/detail/stmt/cgen_setlet_set_prop.inc:387-419`），且该 vtable **按成员名索引、无 IUnknown 前缀 → 不是 COM 表**。原生类实例今天**不计数**（裸指针，`_Destroy` 手工释放）。
- **决策 A：新式接口值 = 薄指针（单字 `void*`）**，指向对象内某个"vtable 指针字段"；这与 COM 完全一致，也是 `Set x = y` 语义与身份规则（同对象不同接口 → 不同地址、QI 往返地址一致）唯一自洽的表示。胖对 legacy 路径**原样保留不动**。
- **决策 B：槽表从第一天就是 COM 形态**：`vb6_ivtbl_<I>` = `[QueryInterface, AddRef, Release]` + （可调度时）`[GetTypeInfoCount, GetTypeInfo, GetIDsOfNames, Invoke]` + 展开后的自有槽。理由：内部调用与外部 COM 客户端共用同一张表，P6 不再改布局、不做双表；代价是 P2 就要生成本项目已有先例的适配桩（`vb6_disp_<cls>_<m>_invoke` 一族，见 `src/backend/detail/util/cgen_util_dllentry_collect.inc:245-344`）。`[PreserveSig]` 成员保留原生返回签名、不包 HRESULT。
- 槽序规则：`Extends` 链**深度优先、父先己后**，同层按声明序；IUnknown/IDispatch 前缀之上再排自有槽（即自有槽起始下标 3 或 7）。链上槽名冲突 → 编译错误（无影子槽）。
- 对象布局（仅对"实现≥1 个新式接口"的类改变，其余类逐字节不变 —— 这是护栏）：
  ```
  vb6_cls_Dog { const vb6_ivtbl_IUnknown_*  __ivtbl[k];  // 每接口一槽，声明序
                void* __comObj;  long __refcount;  <原字段…>;  struct vb6_events_X* events; }
  ```
  接口指针 = `&obj->__ivtbl[i]`；适配器拿到 `this` = 接口指针，**编译期已知偏移**做 container_of 减法得到 `vb6_cls_Dog*`，再直调 `vb6_Dog_<M>`。C 代码全按字段名访问（`me->__comObj`，见 `cgen_com_events.cpp:147` RaiseEvent），无裸偏移依赖，故前置新字段安全。
- 阶段接线：P2 先把 QI/IDispatch 前缀槽填**占位实现**（`E_NOTIMPL` / 简易计数），P6 换真实实现 —— 槽号因此从 P2 起就永久正确。

### D4 管线插入点

`src/driver/driver_compile.cpp` 现状阶段序：0 VBP → 1 lex → 1.5 pp → 2 parse → 2.5 typelib 导入 → 2.6 `runGenericsPrepass`(`driver_generics.cpp:91`) → 3 语义 → 3.5 `runCrossModuleResolution`(`driver_crossmod.cpp`) → 3.5b `runGenericsFixpoint`(`driver_generics.cpp:410`) → 3.6 接口标记 → 4 codegen → 5 link。
- **新增 stage 2.7 `runInterfacePrepass()`**（新文件 `src/driver/driver_interface.cpp`）：收集全部 `InterfaceDecl/CoClassDecl` → 建符号 → 解 `Extends` 链 → 产出**只读槽位表**（含序号、键、C 名、GUID）。必须在 stage 3 之前：跨模块引用接口名是常态。
- 跨模块延后（Overload 3.5 的先例）：*实现映射*（类成员 ↔ 接口槽）需要别的模块的成员表，放在 3.5 之后新增 **3.5c `resolveInterfaceContracts()`**；链式特化式的迭代需求用 3.5b 的 fixpoint 模式。
- 新式接口/CoClass 符号必须穿过克隆/替换通路（泛型 `src/ast/ast_clone.cpp:707-720` 与 `driver_generics.cpp:95-146`）—— v1 边界：**泛型类不得实现新式接口**，prepass 直接诊断拒绝（泛型 v1 也已经拒绝 `Implements`）。

### D5 与 legacy VB6 `Implements` 的共存策略

- legacy 三件套一律不动：模块级 `Implements X`（`parser_module.cpp:215-232`，点号拼成扁平串）、`IFace_M` 命名约定警告式覆盖检查（`semantic_analyzer.cpp:233-273`）、前缀扫描 vtable 生成（`cgen_com.cpp:333-375`）、`tests/test_implements_qi.bas` 与 ActiveX DLL 回归继续作为该路径的守卫。
- **分叉点**：`Implements <Name>` 解析后查符号，`kind==Interface`（新式）→ 走新路径（严格签名比对 + 槽表 + 薄指针）；`kind==Class`（legacy .cls 当接口用）→ 走旧路径。同名冲突（既有 `.cls` 叫 IFoo 又有 `Interface IFoo`）→ 编译错误。
- 成员级 `Implements I.M[, I.N]` 尾子句今天**不被解析**（`parser/parser_decl.cpp` 内无 `Implements` 处理）→ 属于"错误→可解析"的安全新增；组合满足多接口是 tB 明确特性，需支持逗号列表。
- 新式路径下 `IFace_M` 命名约定**不再必需**（显式子句或同名隐式匹配即可），但隐式匹配仍按大小写不敏感同名（含属性三槽）。

### D6 继承（P3）

- 语法：`Class Derived [Inherits Base]`（`.cls` 头行形式扩展，位置就在 `parser_module.cpp:89-104` 现有分支）；`MyBase.M(…)`；`Protected`；`Overridable/Overrides/NotOverridable`。
- 可见性：`src/common/types.hpp:54-59` 的 `AccessLevel{Public,Private,Friend}` **新增 `Protected`**（`Friend` 早已"保留未用"）；今天访问性**只在跨模块过滤**处生效（`symbol_table.cpp:383 getPublicSymbols()`、Private→C `static`），**调用点零检查、也没有派生域概念**（`ScopeKind` 只有 Module/Procedure/Block）。P3 需引入"当前类 + 其基链"上下文（语义分析器成员变量即可，勿扩 ScopeKind），并在成员解析处加 Protected 判定。
- 成员合并：派生类符号在 3.5c 之前做基链成员**继承合并**（`memberNames/memberReturnTypes/memberParams/memberProcKinds` 等表按 `Symbol` 现有分表结构逐项继承），派生域遮蔽规则 = 同名即遮蔽，签名不符的 `Overrides` → 错误。
- 虚派发：仅当类链中出现 `Overridable/Overrides` 时才生成 `vb6_cvtbl_<Cls>`（基先己后展平），`resolveClassMemberCall`（`cgen_util_classcall.cpp:17-228`）在"链上无虚成员"时保持今天的直调结果**逐字节不变**；`MyBase.M` 永远直调基实现（去虚化）。
- 构造链：沿用 `Class_Initialize/_Terminat` 现状，派生类初始化先跑基类（显式基限定调用按 tB 要求，v1 先自动链 + `MyBase.Class_Initialize` 支持）。

### D7 CoClass 与激活（P5）

- `CoClass Name … End CoClass`：块内 `[Default] Interface <I>`、`[Default, Source] Interface <E>`（**后者只接受并存档，不实现连接点**）、`[ComCreatable(True|False)]`、`[CoClassCustomConstructor]`（ByRef 实例入参 + 返回 HRESULT 的工厂过程）。
- 底层实现 = tB 口径：CoClass 是一张**契约聚合表**（可创建类 + 接口集合 + 默认接口），背后绑定一个私有 `.cls` 实现类（`Implements` 全部列出接口）；组内用户只见 `New <CoClass>` / `Dim x As <CoClass>`（等价其默认接口）。
- 激活：工程内 `New <CoClass>` 在编译期直接解析到实现类（不经注册表）；`CreateObject("ProgID")` 今天已走真实注册表链路（`src/rtl/core/vb6com/vb6com.c:509-560` 的 `CLSIDFromProgID + CoCreateInstance`，另有 `ComLib=` 免注册旁路 `vb6com.h:15-25`）→ P5 只需把 CoClass→CLSID→默认接口指针接进这条现成链路 + 编译期 ProgID 表。
- 工程侧既有元数据可复用：vbp 三段式 `Class=Name; x.cls; {CLSID}`（`src/project/vbp_parser.cpp:74-105`，消费于 `driver_compile.cpp:76-79` 的 `classClsidMap_`）、`Type=DLL|OleDll`→`ActiveXDLL`（`vbp_parser.cpp:284-286`）。

### D8 COM 兼容（P6）

- 已存在、优先复用不新造：运行时 `src/rtl/core/vb6comserver/`（`vb6comserver_obj.c` = 完整 IDispatch+QI+原子计数；`_factory.c` = IClassFactory；`_cp.c` = IConnectionPoint；`_pci.c` = IProvideClassInfo2；`vb6comserver.c:62-171` = 注册表写入），导出层 `src/backend/detail/util/cgen_util_dllentry_exports.inc:18-87`（DllGetClassObject/DllCanUnloadNow/DllRegisterServer/DllUnregisterServer/DllMain）、`activex_dll.def` 由 `driver_link.cpp:207-225` 生成、MSVC 链接常量已含 `ole32/oleaut32/uuid/advapi32`（`msvc_driver.cpp:229+`）。
- **真缺口**：`src/typelib/typelib_builder.cpp` 只会 `addDispInterface`（TKIND_DISPATCH）与 `addCoClass`，**没有 HK/双接口（TKIND_INTERFACE + 虚表）**，也不写枚举/UDT；`CreateTypeLib2(SYS_WIN64)` 写死（`typelib_builder.cpp:117`）→ x86 客户端读 64 位指针宽度语义，B15 要按 `-Arch` 传 `SYS_WIN32/SYS_WIN64` 并验证。
- GUID 来源：显式 `[InterfaceId]/[CoClassId]` > vbp 三段式 CLSID > **确定性 FNV-1a 生成**（现状 `cgen_util_dllentry_prelude.inc:39-70`，种子 `"iface:<progId>.<Iface>"`）。新式扩展到 `"itf:<Proj>.<Iface>"` / `"coc:<Proj>.<CoCls>"`。禁止随机：类型库与 .tlb 资源每次构建必须可复现。
- ABI 约束（先例已踩过的坑，写进验收）：x86 stdcall vs cdecl 与 `_`+`@n` 修饰；x64 统一调用约定但 `VARIANT.lVal` 截断指针的历史问题（Fix 093，`cgen_util_dllentry_collect.inc:304-343`）与结构体返回 ABI（Fix 184）。新式槽函数**一律 `STDMETHODCALLTYPE`**，参数按 COM 签名（[out,retval] / ByRef 指针 / propputref）。
- 验收（B17）：`tests/test_activex_dll/` 的"编译 DLL→注册→外部 CreateObject 晚绑定断言"模式（含 `tests/test_p613_typelib.bas`）复制一份新式接口版；再加一个原生 C++/PowerShell `CoCreateInstance` 早绑定 QI 冒烟。

### D9 测试与护栏（每批必做）

- 注册位置：`tests/run_tests.ps1` 内的 `Add-BasTest` / `Test-Vbp` / `Test-GuiVbp` 硬编码登记（无清单文件）。断言 = 内联子串数组匹配 stdout（`.bas` 用例）或 `XXX-N:OK` 标记（项目用例），**无期望文件、无 .c 快照**。
- 计划新增用例：`tests/test_interface.bas`、`tests/test_iface_chain.bas`（Extends 槽序/契约报错）、`tests/itf_xmod/`（跨模块接口）、`tests/test_inherit.bas`、`tests/cls_inherit/`、`tests/test_iface_via.bas`、`tests/test_coclass.bas`、`tests/ax_coclass/`（DLL + 外部激活）。诊断类批（B01/B02）用 `-Category syntax|compile`，断言错误文本。
- 门：`scripts/build.bat`（或 `scripts/dev.ps1`）+ `tests/run_tests.ps1 -Category all`，`Results: PASS=… FAIL=0 SKIP=… TOTAL=…` 原文记入 GATE_BASELINE；已知环境性 SKIP 只有 `test_vbman`（`-RequiresCom VBMANLIB.cVBMAN`）。
- **零新语法逐字节护栏**：沿用先例仪式（`.build\C3.exe <src> --emit-c` 与批次前输出 diff；Overload/Generics 分别以 5+3 与 16 文件验证），B04/B05/B08 三批改结构体/派发路径，必须各跑一次；注意 `git show HEAD:<file>` 是 LF，比较前先还 CRLF（`todo/ferock.md:94` 的教训）。
- 编码红线（CONTRIBUTING §）：`.bas/.cls/.frm/.vbp` = **GBK + CRLF**，`.md` = UTF-8，`.ps1/.bat` = GBK。新增测试用例文件必须以 GBK 写。

### D10 风险登记

| # | 风险 | 缓解 |
|---|---|---|
| R1 | 改 `vb6_cls_*` 结构（前置 vtbl/refcount）波及其它类布局 | 仅"实现新式接口的类"加字段；无新语法工程 emit-c 逐字节护栏 |
| R2 | refcount 与现状"裸指针手工 `_Destroy`"混用 → 双释放/泄漏 | 只对被接口接管的对象 Release；`Set x = Nothing` 与 `_Destroy` 的归属在 B05 明确并加测试 |
| R3 | P2 一次做 COM 形态槽表工作量大 | 槽表 COM 化 + 占位 IUnknown，把真实 COM 语义推到 P6，不返工即可 |
| R4 | 新关键字与存量标识符冲突 | 软关键字双登记；语料已核查零真实冲突 |
| R5 | typelib 只支持 dispinterface，dual 接口导出可能触及未知 API | B15 前先用最小 .tlb 手工验证 `ICreateTypeInfo::Layout` 路径；失败则回退为"仅 IDispatch 晚绑定可用"，并在总表记边界 |
| R6 | 共享工作树 3 写者 | 每批开始 `git status` + mtime 判定；他人半成品致构建失败即停并记录 |

### D11 v1 边界（明确不做）

dispinterface 定义；`[Default, Source]` 连接点实现；泛型类实现新式接口；接口内重载；接口成员含 ParamArray/UDT 参数（先警告，P6 视需要升级为错误）；事件在接口上的声明。

### D12 B01 实施中产生的设计校正（2026-09-23，写代码后回灌）

- **属性容器落点**：接口成员属性用 `InterfaceMember{ std::vector<InterfaceAttr> attributes; DeclPtr decl; }`（`src/ast/detail/ast_decl.hpp`），确认**不**引入通用 `Decl::attributes`；块级属性存 `InterfaceDecl::attributes`。
- **诊断文案必须用英文**（对 D1 的硬修正）：`tests/run_tests.ps1` 是无 BOM 的 GBK 脚本，Windows PowerShell 按 ANSI 码页读 `.ps1` → 脚本里的中文断言字面量必然乱码并静默匹配失败。解析器既有的 `expect` 类消息本来就是英文，故新增 4 条接口诊断统一英文 + 新 ID `ParseInvalidInterfaceMember=2011` / `ParseUnknownAttribute=2012`（`src/common/diagnostics.hpp`）。后续批次的契约诊断（B02）沿用此口径：**面向测试断言的错误文本 = ASCII**。
- **泛型拒绝前移到语法层**：`Interface X(Of T)` 与接口成员 `(Of T)` 在 parse 阶段即报 2011（原 D11 把它放在"prepass 直接诊断拒绝"），省掉一条跨阶段不变式。
- **软关键字边界**：`Interface`/`Extends` 进关键字表 + `isSoftKeyword`，但**不进** `Token::isStatementStart` —— 这样 `Dim Interface As Long` / `Interface = 7` / `Debug.Print ... + Extends` 全部照旧，接口块只在**模块级声明位**与**接口成员位**被识别（语句位永不识别）。`tests/test_interface.bas` 里有这段回归保险。
- **死循环守卫**：接口块成员循环遇到 `End` 且其后是 EOF 时必须 break，否则 `advance()` 在 EOF 不前进 → 编译器挂死（已加，B01 期间发现）。
- **属性行的接受位置**：只接受"模块级属性行 + Interface 声明"与"接口块成员位属性行"两处；其他位置（例如属性行后跟 `Sub`）报 `Attribute line must precede an Interface declaration` 并跳行 —— 这类行过去必然 VB2002，仍是"错误→可解析"。
- **B01 正例刻意不使用接口类型**：`Dim x As IShape` / `Implements IShape` 在 B01 还没有接口符号（`Module::interfaces` 不进符号表），用了就编译失败；契约与派生用例从 B02/B04 起补进同一文件。

### D13 与并发写者共处的工作节律（本轮实测）

- `scripts\build.bat` 从 agent 的 cmd 调用不可用（GBK 正文 + LF 行尾 → 错码刷屏后 exit 1，根本没跑 cmake）。构建一律 `powershell -NoProfile -ExecutionPolicy Bypass -File scripts\dev.ps1 -SkipTest`。
- 锁窗口探测：`(Get-Process C3,cl,link,ninja | Measure-Object).Count`。**他人的一轮 `-Category run` 实测跑了 30 分钟以上**，其间 `.build\C3.exe` 被持有（重建必 LNK1168）且 `output/` 被反复改写。故：锁忙时只做编辑/文档/用例编写，不构建、不测量。
- 锁忙时的单文件预检（不产出目标文件、不碰 `.build`）：`cl /nologo /TP /Zs /EHsc /std:c++17 /utf-8 /Isrc <abs>`（包在 `call vcvarsall x64` 里）。B01 靠它提前抓到一个多余右括号。
- **基线纪律**：一次只跑一个回归，日志路径按批次唯一。本轮我自己先用 `Start-Process` 脱管跑了一个，又用后台任务跑了一个，两者写同一个 `.build\gate_base.log`，我从中读到的 `Results:` 行不可归因 → 记为无效；正确做法是跑前 `md5sum .build\C3.exe`、跑后再核一次，两次不同即作废重跑。
- 脱管进程不会被工具的 stop 回收（工具只杀自己的 wrapper），会留下孤儿继续锁 exe；识别办法是 `Get-CimInstance Win32_Process` 看命令行，只对**自己启动的** PID 做 `taskkill /T /F`。

### D14 B02 开工地图（2026-09-23 B01 收尾轮产出，行号为本轮实测）

- 符号登记：`SymbolKind`（`src/semantics/symbol_table.hpp:23-44`）现末尾值组是 ComGlobalNs 等，**在其后追加** `Interface`（再留 `CoClass` 给 P5），不动既有值序；`Symbol` 的新式字段直接加在 legacy 字段区 `isInterface/implementsNames/interfaceMethodParams/interfaceMethodNames`（:193-197，其中 `interfaceMethodParams` 仍是死字段，勿与其同名混用），建议 `slotTable`（vector：槽名/C 名/键/签名）+ `extendsChain` + `ifaceAttrs`。
- stage 2.7 接线：`src/driver/driver_compile.cpp` 阶段序现状——2.6 `runGenericsPrepass()`(:329) → 3 语义 → 3.5 `runCrossModuleResolution()`(:347) → 3.5b `runGenericsFixpoint()`(:358) → 3.6 接口标记(:365)。新 `runInterfacePrepass()` 插在 :329 与 :347 之间语义段之前；数据源=各 `Module::interfaces`（B01 已填）；泛型类宿主拒绝、`.cls` 同名冲突（既有 Class 叫 IFoo + Interface IFoo）在此报。
- Implements 分叉：`src/semantics/semantic_analyzer.cpp:232-273` 是 legacy 覆盖检查（warn 级、`IFace_M` 命名约定）；B02 在其**前面**按 `lookupModule(ifaceName)->kind` 分叉：`==Interface` → 新路径（error 级：缺槽/多槽按 D2 键、签名逐参比对、属性三槽 `get_/put_/putref_`），`==Class` → 原逻辑一行不动。legacy `tests/test_implements.vbp` 回归继续当旧路径守卫。
- 成员级 `Implements I.M` 尾子句：模块级 `parseImplements`（`parser_module.cpp:242`，声明头收集）已核实**不解析过程尾部**；B02 在 `parseSubOrFunction/Property` 尾部（`parser_decl.cpp` 的签名解析完成后、`expectEndOfStatement` 前）加可选 `Implements` 逗号列表，存进过程 Decl 新字段（如 `implementsClauses`）；`Implements` 本是硬关键字（TokenKind::Implements），无软登记问题，但需确认语句位恢复路径：`isStatementStart` 现状含 Implements，故过程尾扫到会立即在 `expectEndOfStatement` 报错——即"错误→可解析"安全新增（同 B01 属性行手法）。
- 诊断文案 = ASCII（D12 硬约束）；测试用例文件 = GBK+CRLF（D9 红线）；新用例登记在 `tests/run_tests.ps1` 的 itf_neg 数组（:864 起）与 bas 队列（:725 附近）。
- B02 门新增断言：契约报错类用 `Test-SyntaxFail`（编译期诊断）；正例契约满足类目前只能到 `--compile-only`/syntax 级（emit 要到 B04），`test_interface.bas` 里以"Implements IShape + 完整同名成员 → 无诊断"形式做 guard（现有 Add-BasTest 是 compile+run，接口类型变量还不可用，B02 正例改放 `.bas` 但**仅经 Test-Syntax 类通路**——注意 run 阶段若 `Set x = New Class` 未涉及接口类型则不受影响）。
- 逐字节护栏复用本轮做法：临时 `git worktree add D:\c3.<tag> --detach HEAD` + 同 cmake(Ninja/Debug 用主 .build 同款 cache 参数) 构建基线 exe，8 文件清单见本轮日志行，`--emit-c` 双路 `cmp`；用后 `git worktree remove --force + prune`。

### D15 B02 实施中产生的设计校正（2026-09-23，写代码后回灌）

1. **契约比对落在 stage 3（语义层）而不是 D4/D14 计划的 3.5c**。理由（实测后确认）：
   实现映射只需要"实现类自己模块的成员表"，而那份表在 stage 3 的模块 AST 里已完整；
   跨模块需要解决的其实是**接口名→契约**，这由 Driver 级登记表 `ifaces_` 一次建好即可，
   不必往每张模块符号表里注入 Interface 符号（那会牵动 `getPublicSymbols()`/3.5 跨模块注入，
   风险面大得多）。分叉点仍在 `semantic_analyzer.cpp` legacy Implements 循环的**开头**：
   命中登记表 → `checkNewStyleInterface()` + `continue`；未命中 → legacy 代码一行不执行改动。
2. **`SymbolKind::Interface` 本批不加**（与 D2/B02 批次描述偏离）：加了没有任何消费者
   （符号表不参与本批判定），等 B04 发码期与消费点同批落地。同理 `Symbol::slotTable`
   等 legacy 符号字段也不动——契约数据全在 `IfaceRegistry` 里。
3. **签名比对口径 = 源码签名**（`interface_sig.hpp`）：类型引用**原文小写** + 参数个数 +
   逐参 `ByVal/ByRef`/`Optional`/`ParamArray` + 返回类型原文。不用 `Vb6Type` 归一：
   Fix 047 同源问题（跨模块 Enum/UDT 在 stage 3 早期还没注入，归一后比会把正确实现判成不符）。
   `As String * N` 记作 `fixedstring`（宁可显式不匹配，也不伪装成 `String`）。
4. **新共享头 `src/semantics/interface_sig.hpp`（header-only）**：槽键规范
   （`get_/put_/putref_`，D2）与签名文本两侧只用这一份定义；`IfaceSlotView` 额外带
   `memberName`（声明原样名），否则诊断文本只能打印小写槽键。
5. **`--syntax-only` 实际会跑到 stage 3/3.6**（`driver_compile.cpp` 的 syntaxOnly 早退在 3.6 之后），
   所以 B02 的契约诊断**不需要 vbp 工程**就能测：单个 `.cls` 直接喂给 `C3.exe … --syntax-only`
   即可，`Test-SyntaxFail`（非空退出码 + ASCII 子串）完全够用。新增 6 条负例
   （n08 缺槽 / n09 签名不符 / n10 未知父 / n11 Extends 环 / n12 接口内同名 / n13 与模块重名）
   + 1 条正例 `tests/itf_pos/p01_contract_ok.cls`（`Test-Syntax` 通路，含 Extends 继承槽与
   属性三槽）→ 门总数 111→118。**这条对后续批次普遍适用**：凡"只报诊断、不发码"的批
   都用 Test-SyntaxFail/Test-Syntax，别急着造 vbp 工程。
6. **泛型模板双登记护栏（本轮补）**：模板 `.cls` 本体与其特化克隆都在 `modules_` 里，
   模板内的 `Interface` 块会被登记两次 → 莫名其妙的重名错。Pass A 直接拒绝
   （新 ID `SemInterfaceNotSupported=3018`）。该分支无用例（需要泛型工程），v1 边界足够。
7. **已知遗留（B02b 第①项）**：成员级 `Implements I.M[, I.N]` 尾子句未开工（需 `parser_decl.cpp`
   + `ProcDecl` 新字段 + 显式绑定优先于同名隐式匹配）。
8. **legacy 交互（P6 必修，现记档）**：新式接口名仍会进 `classSym->implementsNames`
   （收集发生在分叉之前），ActiveX DLL 的 `cgen_util_dllentry_tables.inc:88-130` 会按
   legacy 口径给它 mint 一个确定性 IID 并写进 `g_vb6_ifaceIids_*`。EXE 工程不发 dll_entry
   → 本批测试全绿不受影响；B13/B16 接线时必须在此处分叉（`ifaces_` 命中就走新式 IID）。
9. **构建/门实测节律**：本轮 03:07 开工 → 03:30 首门（25 分钟，PASS=118 FAIL=0 SKIP=1）
   → 复核源码时发现两处应修（见 6、及一处 move 后读键的诊断文本 nit）→ 03:59 重建 →
   04:02 复跑终门。**过门后若再改源码，必须重跑全量门**（否则提交的源码 ≠ 被测二进制）。

10. **重入保护的实测漏洞（本轮撞到）**：上一轮在 03:05 写 `STATUS=IDLE` 并提交 B01（3add1ce），
    **但它在 03:08:06 又提交了第二个 docs 提交 ea47b46**（回记哈希 + 产出 D14 地图），也就是说
    本轮 03:05–03:08 与其尾部重叠。本轮之所以没出事纯属顺序侥幸：它的提交只动总表，且我
    第一次 `git status`（03:05，看到 26 个脏文件）与第二次（03:07，树已 clean）之间正好夹着那次
    commit——中途 `git diff --stat` 返回空也一度让我以为总表在撒谎。**下一轮起加一条硬检查**：
    读总表后立刻 `git log -1 --format='%h %ct %s'`，若"最新提交时间 - LAST_RUN" 的绝对值 < 3 分钟，
    或工作树在一次 `git status` 里从脏变 clean，就 sleep 120s 再看一次，确认没有活的写者才动工具链。
    （`STATUS=BUSY 且 LAST_RUN<55min` 只挡住"提前收尾"的读法，挡不住"写完 IDLE 还在提交"。）

11. **总表的编辑会被对方的提交"顺手带走"**：ea47b46（03:08:06，上一轮的收尾 docs 提交）提交的是
    **工作树里当时已含本轮 BUSY 头**的那份文件 —— 即我 03:07:46 写的状态头被写进了*它的*提交。
    结果是自洽的（本轮末尾又在 beb75a7 里把该头改成 IDLE + 本轮记录），但要明白：**共享工作树里
    总表没有"我先写就是我的"这回事**。安全做法 = 本轮结束时把状态头**重写**一遍（本轮即如此），
    并且只用"批是否打勾 + CURRENT_BATCH 文本"判断进度，别用 LAST_COMMIT/STATUS 的字面值。

## 运行日志

- 2026-09-23 建表：范围确认（含完整COM）、规范文档 018 入库、现状盘点完成。
- 2026-09-23 00:07–00:20 **B00（P0 设计细化）完成**：4 路前端/符号/后端/工具链勘察 + 亲自核实关键接缝（`lexer.cpp:231-244` 方括号扫描、`parser_module.cpp:82-165` 模块级主循环与 Class 头行先例、`parser_helpers.cpp:14-63` 软关键字表、`types.hpp:54-59` AccessLevel、`cgen_com.cpp:313-428` legacy 胖对 vtable、`cgen_expr_call_com_bind.inc:67-97` 派发点），产出 D1-D11 设计记录与 B01-B18 批次表。**未构建**：一是本批纯文档无代码改动，二是工作树当时有活跃并发写者（`src/backend/detail/util/cgen_api.inc`、`src/backend/stmt/cgen_redim.cpp` 于 00:13 被改，`.build/C3.exe` 00:12:58 刚被他人重建），按纪律不得在其之上测量基线或重建 → 故 GATE_BASELINE 仍空缺，留给下次运行在静默树上建立。
- 2026-09-23 01:00–02:01 （上轮）**B01 代码完成**：`parser_interface.cpp` 新文件 + 17 文件登记（词法/AST/diagnostics/parser_module/run_tests 注册 + `test_interface.bas` + `itf_neg/` 7 负例），单文件 `cl /Zs` 预检通过，exe 01:18 已重建含 B01；但随后一轮来源不明的 `-Category all` 回归（孤儿 8204，01:59 起、父进程已死）持锁，未过门即结束。其表内"B01 ☑ + PASS=<FILL>"是**先打勾后补门**的违规写法，本轮已改实。
- 2026-09-23 02:16–03:10 **B01 过门并提交**：02:16 开工时孤儿全量回归（PID 8204，父进程已死，01:59 起）仍持 exe 锁 → 按纪律等其 02:27 自然退出（用户确认其他会话已结束）。核查 B01 全量 diff 无他人改动混入；ninja 报无活可干、`.build/C3.exe`(01:18, md5 5e9eb1cf) 已含 B01 且为最新。首跑 `-File … *> log` 因 `*>` 被当脚本参数传入致 `-Jobs` 转换失败（教训：重定向要在 `-Command` 内层）→ 改脱管 bat + 状态文件哨兵。**全量回归 02:30–02:59**：`Results: PASS=111 FAIL=0 SKIP=1 TOTAL=112`（SKIP=已知 test_vbman 环境项；跑前后 md5 一致，可归因）。B01 新用例 9 条全绿（test_interface x64/x86 + itf_n01..n07）。**逐字节护栏**：临时 worktree 建 HEAD(4fb3506，无 B01) 基线 exe，8 文件（hello/test_rtl/test_array/test_error/test_ndarray/test_generics .bas + M6Test/test_implements .vbp）`--emit-c` 双路 cmp 全同，worktree 用后即删。**GATE_BASELINE 自本行起正式建立**。B01 提交为 `3add1ce`。未开 B02（共享树里半批不可编译的代码会伤害另两位写者），改为产出 D14 开工地图供下轮直接动工。
- 2026-09-23 03:07–04:32 **B02（P1 语义层）主体过门并提交**：新 `src/driver/driver_interface.cpp`
  （stage 2.7 `runInterfacePrepass`：`Module::interfaces` → 工程级 `IfaceRegistry`，Extends 链求解 +
  槽表父先己后展平 + 五类诊断 VB3015-3018）、新 `src/semantics/semantic_analyzer_iface.cpp`
  （`checkNewStyleInterface`，在 legacy Implements 循环开头按登记表分叉，旧路径一行未改）、
  新只读头 `interfaces_registry.hpp` + `interface_sig.hpp`（槽键与签名文本的唯一出处）、
  `driver_compile.cpp` 插 2.7、`driver_semantics.cpp` 注登记表、`CMakeLists.txt` 两条源文件登记。
  用例 6 负 + 1 正（`itf_neg/n08..n13` + `itf_pos/p01_contract_ok.cls`，全 ASCII/GBK 安全的单文件通路）。
  **首门（03:30–03:53，exe b71a99a6）即 118/0/1/119**；随后复核源码发现两处应当修的地方：
  (a) 泛型模板 `.cls` 内的 `Interface` 块会因"模板本体 + 特化克隆都在 `modules_`"被登记两次，
  表现为莫名其妙的重名错 → Pass A 显式拒绝（新 ID 3018）；(b) `checkNewStyleInterface` 里
  `emplace(sig.slotKey, std::move(sig))` 实参求值顺序未定 + 失败分支读到已移空串 → 先把键取成
  局部 `const std::string key` 再用。**过门后再改源码即作废该次测量**，故 03:59 重建
  （md5 13e99638）、04:02 重跑全量终门（04:24 完成）：`Results: PASS=118 FAIL=0 SKIP=1 TOTAL=119`，
  跑前后 md5 一致、legacy `test_implements` 仍 PASS → 记为 GATE_BASELINE。成员级
  `Implements I.M` 子句未开工，登记为 B02b（详见 CURRENT_BATCH 与 D15）。
