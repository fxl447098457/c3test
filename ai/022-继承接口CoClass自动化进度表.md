# 022-继承接口CoClass 自动化进度总表

> 本文件是每小时自动化任务（"实现继承、接口与CoClass"）的**唯一状态源**。
> 每次运行开始先读本文件，结束前必须更新本文件（状态头 + 批次清单 + 运行日志）。
> 规范输入: `ai/讨论记录/018-接口继承与CoClass设计思路.md`（含 tB 文档要点与分阶段设计思路全文）。

STATUS: IDLE               # NOT_STARTED | DESIGN | BUSY | IDLE | ALL_DONE
LAST_RUN: 2026-09-23T09:52:32+08:00   # B06b 已收口（代码 4dc6b7e、总表 docs 提交见 git log）。
               # 自动运行见本行不足 55 分钟请立即跳过。   # 本轮单批（B06b）；过门后另跑 8 文件逐字节护栏。
               # 下一轮自动运行从 **B07** 开工（P3 第一批，地图见 D24）；重入保护照常：STATUS=BUSY 且不足 55 分钟立即跳过。
LAST_COMMIT: 代码批 = 4dc6b7e(B06b)、613d2b8(B06a)、f644003(B05)、6bc97e8(B04)
CURRENT_BATCH: **B07**（P3 第一批）——`Inherits` 语法 + 类链检测（单继承/环/深度）+ 继承成员合并与遮蔽 + 派生域。开工必读 **D24**（B07 地图，行号本轮实测）与 D23；四条硬约束：
               ① **别照 D6 去 `parser_module.cpp:89-104` 那条 Class 头行分支挂 `Inherits`**：它要求 `parseTypeParams()` 见到 `( Of`（`parser_decl_var.cpp:463-471`），今天连 `Class Foo` 写在 `.cls` 首行都是 error。v1 只做独立子句行 `Inherits Base`；要动头行形式 = 放松既有 error path，必须补泛型 `.cls` 用例并重跑护栏。
               ② 合并必须落在 **stage 3.5 之前**，且 `driver_crossmod.cpp:169-190` 是**逐字段手工拷贝**外部 Class 符号：成员表实有 11 张（D6 只列了 4 张，完整清单见 D24），漏一张就是跨模块瞎。链求解照抄 `driver_interface.cpp` 的 Pass A/B/C（环检测用 `seen`，父先序展平）。
               ③ **不内嵌 `vb6_cls_Base`**（连 `__refcount`/`__iv_<I>` 一起继承 = 双计数，撞 D21-1 的单门禁）→ 字段扁平复制进用户字段区，字段 0 仍是 `__comObj`（D19）。遮蔽必须**按小写键裁决**：`cIdent` 保留大小写而成员表键小写，否则 `m_X`/`M_x` 发成两个 C 成员 = 静默错字段。
               ④ 继承方法发**转发桩** `vb6_<D>_<M>` 转调 `vb6_<B>_<M>((vb6_cls_<B>*)me, …)`，别在每个调用点裸强转 `me`（`cgen_form.cpp:210` 有控件强转先例，但它要求前缀布局逐字段一致，且会挡住 B08 的 Overridable 钩子与 B09 的 MyBase 去虚化）。
               另：早退条件必须是“本类或其链用到 Inherits”，不是“工程无新语法”（D24 末两条）。`Test-SyntaxFail` 只接单个 `$Source` → shadow/环/arity 这类**双文件负例**需要新 helper，B07 预算含它。
               遗留登记（不挡 B07）：**B06c** = 接口值作实参 / 进 Variant，归 B13/P6 前处理（要过 `vb6_ComIsDispatchable` 的 7 槽口径，D22-7③）。
GATE_BASELINE: Results: PASS=128 FAIL=0 SKIP=1 TOTAL=129   # exe md5 4202570a（.build/gate_B06b.log，10:06 起跑，最后一次源码改动在起跑前、跑前后 exe 一致；SKIP=已知 test_vbman 环境项）。条目数与 B03/B04/B05/B06a 基线持平——B06b 未新增用例条目，只把 `itf_xmod_writer` 的断言 18→25 条 → 零新增失败。
               # 另附 8 文件 --emit-c 对 pre-B06b 基线（worktree @613d2b8）逐字节全同；A/B 负控见 D23-3。
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
| B02 | P1 | 语义：接口符号注册 + Extends 链 prepass(2.7) + 槽位表 + Implements 契约完整性/签名比对 | ☑ | beb75a7 | `Results: PASS=118 FAIL=0 SKIP=1 TOTAL=119`（gate_B02F.log；exe md5 13e99638 跑前后一致）+ 新增 7 条用例全绿 + legacy `test_implements` 仍 PASS。交付：stage 2.7 `runInterfacePrepass`/`IfaceRegistry`/Extends 链与槽表/五类诊断 + `checkNewStyleInterface` 严格契约比对（D15 记 1–6）。`SymbolKind::Interface` 按 D15-2 推迟到 B04。成员级子句拆给 B02b |
| B02b | P1 | 成员级 `Implements I.M[, I.N]` 尾子句（显式绑定优先于同名隐式匹配）+ 泛型模板内 Interface 的 3018 用例 | ☑ | dde7c32 | `Results: PASS=125 FAIL=0 SKIP=1 TOTAL=126`（gate_B02bF.log；exe md5 cdac040f 跑前后一致）+ 7 条新用例（itf_n14..n19 + itf_p02）全绿 + legacy `test_implements` 仍 PASS。交付：`parseTrailingImplementsClauses` + 三个过程节点的 `implementsClauses`（含克隆路径）+ 槽键兼容 `I.Name`/`I.get_Name` 与链上任一接口名 + `checkMemberImplementsClauses` 兜底（新 ID 3019）。详见 D16 |
| B03 | P1 | `.cls` 头行宿主形式 `Interface IFoo … End Interface`（1 文件 1 接口）+ `Module::isInterfaceModule` + 语法手册页/索引 | ☑ | c47cdce | `Results: PASS=128 FAIL=0 SKIP=1 TOTAL=129`（gate_B03.log；exe md5 053150bf 跑前后一致）+ 3 条新用例（itf_p03 + itf_n20 + itf_xmod_writer）全绿 + legacy `test_implements` 仍 PASS。要点：宿主识别放 stage 2.7（parser 拿不到最终模块名）、VB3002 只豁免宿主自身、跨模块契约已端到端跑通（详见 D18） |
| B04 | P2 | 接口值代码生成：`vb6_ivtbl_<I>` COM 形态槽表 + 类侧实例 + 薄指针表示 + `As <Iface>` 变量登记 + 派发 | ☑ | 6bc97e8 | `Results: PASS=128 FAIL=0 SKIP=1 TOTAL=129`（gate_B04.log；exe md5 ef1a7520 跑前后一致）+ 接口值派发端到端断言 IFV1/IFV2/IFV3 全绿 + legacy `test_implements` 仍 PASS。要点：新增 `cgen_iface_vtbl.cpp` 独立编译单元、`#ifndef VB6_IVTBL_<I>` 守卫替代 D19 设想的工程级去重表、`__iv_<I>` 紧跟 `__comObj`、槽键口径上提到 `interface_sig.hpp` 与语义层同源。B04a/B04b 合并成一批（理由见 D20-1）。逐字节 emit-c 护栏 8 文件对 pre-B04 基线全同 |
| B05 | P2 | 生命周期：实现类结构**前置** vtbl 指针数组（实测不可行，见 D21-1）+ refcount 头 + AddRef/Release + Set/Nothing/作用域释放 | ☑ | f644003 | `Results: PASS=128 FAIL=0 SKIP=1 TOTAL=129`（gate_B05.log；exe md5 c615c657 跑前后一致）+ `itf_xmod_writer` 断言 5→11 条（LIFE1/2/3/9 + `TERM last=bye` + `TERM last=scoped`；实测该工程恰好 2 条 TERM，无误销毁、无重复释放）+ legacy `test_implements` 仍 PASS。要点：`__refcount` 只加在实现新式接口的类上（8 文件 emit-c 对 pre-B05 基线全同）；AddRef/Release 按 (类, 接口) 各一份；QI 仍占位到 B06；`__comObj` 非空时不归 0 销毁。详见 D21 |
| B06 | P2 | 转换与判定：接口↔类、多接口对象、`TypeOf … Is <接口>`、上/下行转换契约校验 | ☑ **B06a**（QI + 跨接口 Set + `TypeOf <接口变量> Is <接口>`）+ **B06b**（下行转换 + `TypeOf <类变量> Is <接口>` + 修 B05 的 Nothing 野地址）；**B06c 遗留**：接口值作实参 / 进 Variant → 随 B13/P6 处理 | 4dc6b7e | B06a：`Results: PASS=128 FAIL=0 SKIP=1 TOTAL=129`（exe 7721bd72）+ 断言 11→18；B06b：`Results: PASS=128 FAIL=0 SKIP=1 TOTAL=129`（gate_B06b.log；exe 4202570a 跑前后一致）+ 断言 18→25（DN0..DN3 + TOC1..TOC3）+ legacy `test_implements` 仍 PASS + 8 文件 emit-c 对 pre-B06b(@613d2b8) 全同 + **A/B 负控证明 D22-10 缺陷真实**（基线二进制 exit=139，本批 exit=0）。详见 D22/D23 |
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
- **GitHub Actions 全量 = 里程碑级，不是每批**（用户 2026-09-23 09:55 定调："这个测试不用每次都做，
  按之前的做就行，等大改全部实现完成之后再跑 github actions 这样的全量测试"）。所以：
  **每批仍走本机 `tests/run_tests.ps1 -Category all` 门 + 逐字节护栏 + 本地 commit**（本文件既有纪律，
  一字不改）；Actions 那一级留到**整条线（B04–B18 / STATUS=ALL_DONE）做完**跑一次，届时：
  仓库 = `https://github.com/fxl447098457/c3test`，主路径 `git push github HEAD:dev` → `ci.yml`
  （`regression` 矩阵 = run_tests.ps1 五段 smoke/bas×2/vbp/compile/syntax，`smoke` 本轮刚补进矩阵；
  CI 是 Release 构建，与本机 Debug 两个口径），体系级 T0/T1/T2 走 tag `github-test-NNN` 或
  workflow_dispatch → `ci_t0.yml`；监控用 `pwsh -File scripts/watch-gh-actions.ps1`
  （自取 remote 内嵌 PAT、输出打码，别手动 echo 那个 URL）。
  另两处事实供那一次参考：`tests_github/` 的用例是 2026-09-20 从 `tests/` **复制**的，本线新增的
  `itf_*` 在 run_t1/run_t2 里 0 命中（**他人正在修清单同步，本任务不动**）；`run_t2.ps1` 也不在
  `run_tests.ps1` 里。gitcode 那条 `origin` 与 Actions 无关。
  **分支名已定：远端 c3test 保持 `dev`，不改名为 `fan/dev`**（用户 2026-09-23 09:58 决定：
  "既然不改也不影响的话，那我就不改了"）。本地 `fan/dev` → 远端 `dev` 用 refspec 映射
  （`git push github HEAD:dev`）即可，全仓唯一引用分支名的 `.github/workflows/ci.yml:5`
  保持 `branches: [main, dev]` —— **不要再往里加 `fan/dev`**（本轮已有一次加了又撤）。
  **红线**：本机门不变 → 平时依旧"永不 push"；只有上面那一次里程碑全量允许推 `github` 远端
  （`dev` 分支或 `github-test-*` tag），不建 MR，且届时先跟用户确认一句。
- **触及类结构体布局 / Set / 派发 / COM 打包的批次，除全量门外必须另跑 T2**（B04–B06a 这三批此前漏了）。


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

### D16 B02b（成员级 Implements 子句）实施记录与设计校正（2026-09-23）

1. **子句的语法位置**：`Sub` 在参数表之后、`Function/Property` 在 `As Type` 之后（tB 口径），
   三处共用 `Parser::parseTrailingImplementsClauses`（`parser_decl.cpp`）。接口块成员签名走
   `parseInterfaceMemberDecl`，**不调用**该函数 → 在 Interface 块里写子句仍然 VB2003，
   与"契约上不该有绑定"一致。畸形子句（只写 `Implements I`）在 parse 期报 2011 且**不入列表**，
   避免语义层再补一条级联诊断。
2. **显式绑定排斥隐式同名**（tB 口径，D5 的"显式优先"具体化）：写了任意子句的成员只按子句入座，
   不再参与同名隐式匹配（`checkNewStyleInterface` 里的 `explicitDecls` 集合）。否则一个成员会
   既按子句进 A 槽、又按同名被 B 接口认领，形成没人能预期的隐式双重认领。
3. **槽键解析要兼容两种写法 + 链上任一接口名**：`I.Name` 按实现成员的 Get/Let/Set 前缀推
   `get_/put_/putref_name`，同时也允许直接写槽名 `I.get_Name`；接口名可写 Extends 链上任意一层
   （`IfaceSlotView::ownerIface` 已带归属接口，父槽用父名或子名都能解）。属性 Set 槽 = `putref_`。
4. **"认领不到的子句"必须报错**，否则子句写成摆设还全绿：`checkNewStyleInterface` 每处理一个新式
   接口就把认领的子句记进 `boundClauses`（键 = (过程节点, 子句序号)），`analyze()` 末尾交给
   `checkMemberImplementsClauses` 兜底。四种成因分别给一句话（宿主非类模块 / 接口名不存在或是
   legacy 类 / 类未实现该接口 / 接口里没有这个成员）。新诊断 ID `SemInterfaceClauseUnbound=3019`。
   兜底同样落在 stage 3：子句解析只需工程级登记表 + 本模块成员表（D15-1 的结论对子句成立，
   不需要 3.5c）。
5. **单文件 `--syntax-only` 通路继续够用**（D15-5 第三次验证）：6 负 + 1 正全部 ASCII，无需 vbp。
   泛型模板内 Interface 的 3018 分支本轮补了用例 `itf_neg/n19_iface_in_generic.cls`（B02b-②）。
6. **`tests/run_tests.ps1` 是 UTF-8 无 BOM + CRLF**（4899 个非 ASCII 字节，`lf_only=0`），不是 D9
   写的 GBK。PowerShell 仍按 ANSI 码页读它 → 中文注释无害、中文字符串字面量必乱码（D12 的根因，
   本轮实测复现）。改它只能走二进制插入：写完核对 ①非 ASCII 字节数不变 ②`lf_only` 仍为 0
   ③`git diff --numstat` 只有预期的 +N/-1。本轮插入 11 行、n13 行加逗号。
7. **AST 新字段的克隆路径**：`SubDecl/FunctionDecl/PropertyDecl` 各加 `implementsClauses`，
   `ast_clone.cpp` 的 `cloneSubDecl/cloneFunctionDecl/clonePropertyDecl` 必须逐个补拷贝——
   泛型特化走的就是这三个函数，漏拷会让特化副本静默丢掉绑定（v1 已拒泛型实现接口，但克隆路径
   仍然要正确）。
8. **过门纪律的具体踩法**：本轮首门（05:01 起，exe 1f387137）跑到一半时又改了两处源码
   （去掉 `IfaceClauseRef` 的重复声明、畸形子句不入列表）→ 那次 125/0/1/126 只能当"存量项无回归"
   的参考，**不能作为提交依据**；必须重建 + 复跑全量门（D15-9 的再验证：门与二进制一一绑定）。
9. **已知不一致（留给 B03/B13，不在本批动）**：模块级 `Implements` 的分叉点用的是
   `ifaceReg_->find(Symbol::toLower(全名))`，即 `Implements Proj.IFoo` 这种带库/工程限定的写法
   **不会**命中新式路径（会掉进 legacy 分支）；成员级子句这边则接受限定名（末段回退，
   `ifaceLastSegment`）。二者对限定名的宽容度不对称。B03 起若要支持工程级接口引用，
   应把分叉点也改成同一套 `lookupWrittenIface`，并顺手补一条正例用例。

### D17 B03 开工地图（2026-09-23 B02b 收尾轮产出，行号为 dde7c32 上实测）

- **头行形式其实已经能解析**（探针实测，省掉一整块语法工作）：`.build/probe_itfhead.cls` =
  `VERSION/BEGIN…END/Attribute VB_Name = "IFoo"/Option Explicit` + `Interface IFoo … End Interface`，
  用 B02b 二进制跑 `--syntax-only` **只**报一条错误：
  `error VB3002: Interface name 'IFoo' collides with a module of the same name`
  （`src/driver/driver_interface.cpp:44-50`，moduleKeys 在 :26-27 预建）。也就是说 B01 的块解析器
  已经天然吃得下"一文件一接口"的头行写法，**B03 不是新语法批次，而是"宿主识别 + 重名放行"**。
- 三步落地：
  1. **宿主标记**：`Module` 加 `isInterfaceModule`（接缝就在 `src/ast/detail/ast_decl.hpp:332-333`
     的 `isClassModule`/`isFormModule` 旁，D1 早已点名这里丢子标记）。置位条件建议：
     `mod.isClassModule && mod.interfaces.size()==1 && ifaceLower(块名)==ifaceLower(mod.moduleName)`，
     在 `parseModuleBody` 的 Interface 块分支尾部扫一遍即可（`src/parser/parser_module.cpp:126-151`）。
     若要"整文件即该块"（块外不得再有声明），参照 Class 头行的 `classHeaderSeen_` 记法
     （`parser_module.cpp:89-104`）加一条收尾校验，报错文案保持 ASCII。
  2. **重名放行**：`runInterfacePrepass` Pass A 的 moduleKeys 集合把"宿主自己的模块名"剔除
     （只豁免它自己声明的那一个接口名；与其它模块/其它接口重名仍照旧报错）。
  3. **确认 legacy 侧不受污染**：头行宿主的成员是**签名节点**、不进 `mod.declarations`，所以它的
     Class 符号 `memberNames` 为空。别的模块写 `Implements IFoo` 时，分叉点
     （`semantic_analyzer.cpp:240-246`）会先命中登记表走新式路径——需要**实测**跨模块场景：
     `tests/itf_xmod/`（D9 已规划）建一个 vbp 工程 = 宿主 `IFoo.cls` + 实现类 `Foo.cls`，
     断言只有新式诊断、没有 legacy 的 `IFace_M` 命名约定警告。这一步也是 B04 发码的前置。
- **手册**：新增 `docs/vb6-manual/02-语句/Interface 语句.md`（模板 = 同目录 `Implements 语句.md`，
  UTF-8），并在 `docs/vb6-manual/README.md` 的语句清单第 157 行（`- [Implements 语句](…)`）之后插
  `- [Interface 语句](02-语句/Interface%20语句.md)`（注意 `%20` 转义；字母序 Implements < Interface < Input）。
- 门：基线 `125/0/1/126`。建议用例 = 1 正（宿主 .cls 单文件 `Test-Syntax`）+ 1 负（宿主名与另一
  接口冲突 → 仍 VB3002）+ 1 个 `itf_xmod` vbp 工程（`Test-Vbp`）。**逐字节护栏**：B03 不动发码，
  但改了 `Module` 结构与 prepass，按 D9 仍跑一次 `--emit-c` 对比（沿用 B01 的 8 文件清单）。

### D18 B03（`.cls` 头行宿主）实施记录与设计校正（2026-09-23）

1. **宿主识别必须放在 stage 2.7，不能放 parser**（D17 地图原建议"在 parseModuleBody 尾部扫一遍"
   实测不可行）：`Module::moduleName` 要到 `src/driver/driver_frontend.cpp:217-250` 才从
   `Attribute VB_Name` 定下来（parser 只见得到属性行），所以 `mod->isInterfaceModule` 由
   `runInterfacePrepass` Pass A 在泛型拒绝之后、登记接口名之前置位。识别条件
   = `isClassModule && interfaces.size()==1 && ifaceLower(块名)==ifaceLower(moduleName)`。
2. **VB3002 放行只豁免宿主自己的那一个块**：`hostOwnName = isInterfaceModule &&
   d.get()==interfaces.front().get()`。其它接口名与工程内模块名撞车仍照旧报错，
   所以 B02 的 `itf_n13_module_collision` 负例不受影响（本轮门内仍 PASS）。
3. **宿主文件的"1 文件 1 接口"约束**用 `SemInterfaceNotSupported`(3018) 报
   `Interface host module 'X' may contain only the Interface block`，且**只报第一条**
   （一条宿主违规背后往往是整批误用，级联文本没有信息量）。`options`/`attributes`
   不算声明——`.cls` 宿主必然带 `Option Explicit` 与 `Attribute VB_Name`。
4. **D17 的"legacy 污染待核点"结论 = 无污染，跨模块已端到端打通**：`tests/itf_xmod/`
   （`IWriter.cls` 头行宿主 + `CWriter.cls` 用 B02b 的成员级子句跨模块绑定三个槽 +
   `XMain.bas` 走具体类调用）编译成 exe 并跑出 `XMOD1:OK`/`XMOD2:OK`。机理：宿主的 Class
   符号仍在、`memberNames` 为空，但 `semantic_analyzer.cpp` 的 Implements 分叉先查登记表 →
   新式路径，legacy 的 `IFace_M` 命名约定一行未执行。**接口类型变量 `Dim s As IWriter` 本批
   刻意不做**（同名 Class 符号会先被类型解析吃掉）→ 归 B04。
5. **语法手册落点**：新页 `docs/vb6-manual/02-语句/Interface 语句.md` 首行用引用块标注
   "本项目扩展，非微软 VB6 原生语句"，与 MSDN 镜像正文区分；README 索引按字母序插在
   `Implements 语句` 与 `Input # 语句` 之间（链接用 `%20`，`#` 不转义）。
   该目录全部是 **CRLF**：Write 产出 LF 后要 `sed -i 's/\r*$/\r/'` 归一，且改 README 之后
   必须复查 `bare_lf==0`（本轮实测：README 524 CRLF / 0 bare-LF）。
6. **逐字节 emit-c 护栏本批未跑**（记录理由，免得下轮以为漏了）：只加了一个 bool 字段 +
   prepass 分支，未碰 `src/backend/**` 与发码路径；替代守卫是让宿主模块进真实工程
   编译+链接+运行（第 4 条）。按 D9 口径，B04/B05/B08 那三类改结构/派发的批次仍必须跑。
7. **用例通路新增第三条**：`Test-Vbp`（`run_tests.ps1` 的 all/run/vbp 块，M7Test 之后）。
   登记 = 插 3 处共 13 行、改 1 行（n19 补逗号），核对口径见 D16-6。

### D19 B04 开工地图（2026-09-23 B03 收尾轮产出；行号本轮实测，含对 D3 的硬修正）

- **⚠ 先改设计再做码：D3 的"前置新字段安全"结论是错的（本轮实测推翻）**。
  `src/rtl/core/vb6comserver/vb6comserver_obj.c:268-272`（`vb6_ComObject_Create` 回填反指针）与
  `:302-303`、`:318`（`vb6_ComObject_FromInstance` 复用/新建包装）都是 **裸偏移 0 读写**
  `void** ppComObj = (void**)instance;`，并且 `vb6comserver.h:126-128` 把它写成明文前置条件
  （"实例所在类的结构体首字段是 `__comObj`"）。而 `src/backend/detail/base/cgen_base_generate_c_open.inc:78`
  从 ExeComBridge 01 起**无条件**把 `void* __comObj` 发在 `vb6_cls_<Name>` 第一位。
  → **B04 落法**：`__comObj` 保持第 0 位不动，接口槽指针数组放在它**之后**
  （`__comObj; const void* __ivtbl[k]; <原字段…>`），container_of 减法按"字段名 + 实测偏移"算，
  不假设任何偏移；这样存量 COM 包装复用路径零改动。D3 里"接口指针 = `&obj->__ivtbl[i]`"的表述仍然成立。
- 结构体接缝现状（`cgen_base_generate_c_open.inc`）：`vb6_cls_<Name>` 开在 :73，`__comObj` :78，
  字段循环 :81-102，空结构补 `_placeholder` :104-106，`events` 指针 :108-111，收尾 :112。
  另外 `usedVb6IfaceTypes_`（`cgen_state.inc:262` 登记 → `cgen_base_generate_epilogue.inc:44-59` 出
  前置 typedef）已经在发 `vb6_vtbl_<I>` / `vb6_iface_<I>` 两个名字，**`vb6_ivtbl_` 前缀今天全仓零占用**，
  D2 的隔离命名可以放心用。
- **legacy 路径不发适配器**（与 D3 的预设有偏差）：`src/backend/module/cgen_com.cpp` 的
  `emitInterfaceVtable(Module&)` :313-428 只遍历 `module.implements` :316，成员靠**实现类自己声明的
  `IFoo_M` 前缀扫描**得到 :330-375，槽位**直接指向** `cProcName(IFoo_M)` 实现函数（:402-413），
  没有任何 thunk；`vb6_iface_<I>{vtbl,obj}` 胖对在 :394-398，`vb6_iface_<I>_wrap()` 在 :416-426。
  → B04 的 `vb6_iimpl_<C>_<I>_<slot>` 适配器是**新物种**，可抄的发码先例是 P6.5 事件包装
  （`cgen_base_generate_body_pass.inc:30-130`，`evtWrapperName`，签名拼装 :89-98）与
  P13.23 的 `emitComVtableSinks()`（`src/backend/module/cgen_com_events.cpp:288+`，声明 :256-270，
  钩子 `evt_decl.inc:76` / `evt_impl.inc:135`）。
- **两个必须新建的基础设施**（勘察实测，别当已存在）：
  1. `CCodeGen` **拿不到 `IfaceRegistry`**：登记表只在 `src/driver/driver.hpp:143 ifaces_`，
     消费者只有语义层（`semantic_analyzer.hpp:127`）。要按 legacy 的做法在模块循环里注入
     （先例：`ifaceImplementersMap` 产于 `src/driver/detail/driver_codegen_dup_module_vars.inc:10-25`，
     注入于 `driver_codegen_module_loop.inc:74`，API 在 `src/backend/detail/util/cgen_api.inc:217-221`）。
  2. **没有工程级"已发表"去重表**：`vb6_vtbl_<I>` 今天是**按实现类重复 typedef** 的。
     新式槽表若同样每个模块各发一遍，链接期必重复定义 → B04 需要一张工程级 set。
- 变量登记与派发落点（`As <Iface>` 走"薄指针"要动的四处 + 现值语义）：
  类型信息在 `Symbol::variableTypeName`（`src/semantics/semantic_analyzer_register.cpp:60-62`，
  `resolveTypeOrDefault`/`resolveTypeRef` 在 `semantic_analyzer_typeref.cpp:29`）；
  C 类型由 `src/backend/cgen_base_type.cpp:193-197` 决定，**当前是胖对 `vb6_iface_<I>` 按值**（门接条件是 legacy 的 `clsSym->isInterface` :194）
  → 薄指针 = 这里分叉（命中登记表走单字 `void*`）。登记 `knownIfaceVars_` 的四处：
  模块级 `src/backend/decl/cgen_decl_var.cpp:141-143`、局部 `src/backend/decl/cgen_localdecl.cpp:237-239`、
  跨模块 `src/backend/detail/base/cgen_base_generate_crossmod.inc:24`、形参
  `src/backend/decl/cgen_decl_func.cpp:109` / `cgen_decl_proc.cpp:116` / `cgen_decl_prop.cpp:117`。
  调用派发 = `src/backend/detail/expr/cgen_expr_call_com_bind.inc`（整体门控 `isComMarker_` :7；
  :67-97 查 `knownIfaceVars_` 后发 `x.vtbl->M(x.obj,…)` :91-93，标记来自
  `cgen_expr_member_obj_dispatch.inc:20-31`）；去虚化直调在 `src/backend/cgen_util_classcall.cpp:17-224`
  （`resolveClassMemberCall`，结果名 `vb6_<Cls>_<M>` 见 :115-118/:198/:223）；
  `Set x = y` 的接口改写 + `wrap()` 在 `src/backend/detail/stmt/cgen_setlet_set_prop.inc:387-435`
  （D3 记的 :419 已漂到 :435），`Set Nothing` :80-91。
- 宿主模块（B03 的 `isInterfaceModule`）**目前后端零消费**（`src/backend/**` 无人读它），
  `currentClassIsInterface`（`cgen_base_generate_decl_pass.inc:70-78`）只认 legacy 符号
  → B04 第一件事就是让宿主模块"只发槽表、不发类实例"，否则 `IWriter.cls` 会被当成普通类发码。
- 构建/管线接线成本很小：stage 4 入口 `src/driver/driver_compile.cpp:438-439` →
  `Driver::runCodeGeneration`（`src/driver/driver.cpp:111`，实现拆在 9 个
  `src/driver/detail/driver_codegen_*.inc`）；新增后端 `.cpp` 要进 `CMakeLists.txt` 的
  `vb6c3-cgen` 列表（:127-179，先例 `:163 src/backend/module/cgen_com.cpp`、
  `:173 src/backend/cgen_util_classcall.cpp`，注释行 :162）。
- 测试面（本轮再核实）：`Add-BasTest` 在 `tests/run_tests.ps1:644-652`（入队 `$basQueue`，
  由 :500 前结束的 `-Jobs` 并发 runner 消费；`test_interface` 的 x64/x86 双登记在 :727-728）、
  `Test-Vbp` :503-579（`itf_xmod` 已在 :800）、`Test-SyntaxFail` :584-601、`Test-Syntax` :603-617。
  **逐字节 emit-c 护栏**：`--emit-c` 开关在 `src/driver/driver_args.cpp:76`（`opts.emitC`，
  链接在 `src/driver/driver_link.cpp:57` 跳过）；8 文件清单与 worktree 基线做法见 D14-15 与
  B01 运行日志行（hello/test_rtl/test_array/test_error/test_ndarray/test_generics `.bas` +
  M6Test/test_implements `.vbp`）。B04 改结构体与派发路径 = **必跑**。
- 建议拆分：B04a = 槽表类型 + 宿主/类侧实例 + `isInterfaceModule` 后端接线 + 工程级去重表
  （只发码不派发，逐字节护栏先保住"无新语法工程零变化"）；B04b = 薄指针类型分叉 +
  `knownIfaceVars_` 四处登记 + 派发与 `Set`；B05 再管生命周期。两块各自过门。

### D20 B04（接口值代码生成）实施记录（2026-09-23）

1. **B04a/B04b 合并成一批**（D19 建议的拆分不成立）：只发槽表不派发没有任何**可观测行为**，
   过门等于没过门。实际节奏改成"先 emit-c 观察生成物 → 再接派发 → 一次过门"。
2. **落地后的内存布局**（`__comObj` 保第 0 位 = D19 硬修正的执行）：
   `vb6_cls_C { void* __comObj; vb6_ivref_<I> __iv_<I>; <原字段…>; }`，
   `vb6_ivref_<I>` 是**单词结构体** `{ const vb6_ivtbl_<I>* vt; }`，接口值 = `&obj->__iv_<I>`，
   适配器里 `offsetof(vb6_cls_C, __iv_<I>)` 做 container_of 再直调 `vb6_C_<M>(me, …)`。
3. **不需要 D19 说的"工程级已发表去重表"**：`vb6_ivtbl_<I>` / `vb6_ivref_<I>` 用
   `#ifndef VB6_IVTBL_<I>` 守卫，发在**每个模块头文件**里 → 同 TU 多次包含天然幂等，
   也不依赖 include 顺序；输出可复现靠"按小写接口名排序"（`unordered_map` 迭代顺序不稳定）。
   三个发射钩子在 `ivreg_` 为空时**零输出**，所以无新语法工程的生成物逐字节不变（已实测）。
4. **CCodeGen 接入方式**：`cgen.setInterfaceRegistry(&ifaces_)`，与 legacy 的
   `setInterfaceImplementers` 同一个注入点（`driver_codegen_module_loop.inc`）；
   新后端文件 `src/backend/module/cgen_iface_vtbl.cpp`（`CMakeLists.txt` 的 `vb6c3-cgen` 列表登记）。
5. **槽键口径只有一份**：成员名→槽键 = 先裸名（Sub/Function）再 `get_/put_/putref_` 前缀回退；
   `ifaceProcClauses` / `ifaceSlotPrefix` / `ifaceClauseSlotKey` 从 `semantic_analyzer_iface.cpp`
   上提到 `interface_sig.hpp`，语义层契约比对与后端绑定查找（`ivFindImplMember`）共用，
   包括"写了子句的成员不再参与同名隐式匹配"这条规则。
6. **实测生成物**（`tests/itf_xmod` 的 Main，`--emit-c`）：
   `vb6_ivref_IWriter* s = NULL;` → `s = &(w)->__iv_IWriter;  /* Set */` →
   `s->vt->emit(s, vb6_BSTR_FromStr(L"delta"));` / `s->vt->total(s)` / `s->vt->get_last(s)`；
   `Set s = Nothing` → `s = NULL`、`If s Is Nothing` 直接可用（薄指针语义免费送）。
   运行断言 `IFV1/IFV2/IFV3:OK` 已并入 `itf_xmod_writer` 的 `Test-Vbp`  needles。
7. **本批边界（后面批次补，别当已存在）**：
   ① 经接口变量**写**属性（`s.Title = v` → `put_` 槽）未接；
   ② 接口↔接口转换、`TypeOf … Is <接口>`（B06）；③ 真实 IUnknown/引用计数（三件套现在是
   `E_NOTIMPL` + 常量 1 的**占位**，槽号自此固定）（B05/B13）；
   ④ 宿主 `.cls` 仍按普通类发一个空结构体（无害，"宿主不发类实例"待 B05 一起做）；
   ⑤ `Set s = <Variant>` 右值推断没有 legacy 那样的唯一实现类兜底，直接报 ASCII 诊断
   `Interface binding: Set … needs a class instance or New …`，**该诊断暂无用例覆盖**。
8. **门数字不变**：129 项（本批只给 `itf_xmod_writer` 加了 3 条断言，没新增用例条目）；
   逐字节 emit-c 护栏 = 8 文件对 pre-B04 基线二进制（worktree @f8ca84e）**全同**。
9. **worktree 卫生**：基线 worktree 用完即 `git worktree remove --force` + `git worktree prune`，
   并确认 `git worktree list` 只剩主目录（历史上留过孤儿 worktree 迷惑后人）。

### D21 B05（生命周期：接口引用计数）实施记录（2026-09-23）

1. **落地形状**：`vb6_cls_C { void* __comObj; int32_t __refcount; vb6_ivref_<I> __iv_<I>; <原字段…>; }`，
   `_New()` 里 `__refcount = 1`。字段**只加在"实现新式接口的类"上**（`ivImplementedIfaces` 为空就一个
   字节都不发）→ 无新语法工程的生成物逐字节不变。这条门控是 D19「`__comObj` 必须第 0 字段」与
   「零回归逐字节护栏」同时成立的唯一解：既不能前置字段，也不能给所有类统一加计数头。
2. **AddRef/Release 必须按 (类, 接口) 各发一份**（B04 的占位三件套是按类一份）：`self` 是
   `&me->__iv_<I>`，container_of 的 `offsetof(vb6_cls_C, __iv_<I>)` 依赖具体接口，多接口类共用
   一份会算错实例地址。`QueryInterface` 仍 `E_NOTIMPL`（**槽号不动**），与 B06 的
   `TypeOf`/接口转换同批落地。
3. **归 0 销毁的守卫**：`if (me->__comObj != NULL) return 0UL;` —— 实例一旦被 COM 包装器接管，
   销毁权在包装器那一套计数（`vb6comserver_obj.c` wrapper refcount → `desc->destroyFunc`）。
   两套计数的统一是 P6/B13 的活；B05 的语义只在 EXE 直调路径上成立。
4. **引用语义三条**：
   - `Set <ivref> = <类变量 | New …>` 发成一个 C 块：存旧值 → 覆盖 → AddRef 新 → Release 旧
     （自引用安全）；RHS 是 `New` 时**不 AddRef**（`_New` 那 1 次初始引用直接移交）。
   - `Set <ivref> = Nothing`：先经槽 Release 再置 NULL。B04 之前这条落到 `vb6_ReleaseObject`，
     等于把薄指针当 `IDispatch*` 用（当时靠 `vb6_ComIsDispatchable` 拒绝才没崩）；现在是真释放。
   - `Dim … As <接口>` 局部变量在过程正常出口统一 Release：新 state 成员 `ivrefLocalsToRelease_`
     + `trackIvrefLocalForRelease()/emitIvrefScopeRelease()`，钩在 `ansiTempsToFree_` 的同一位置
     （Sub/Function/Property 三处出口发码、三处过程起点清空）。
5. **为什么不会误销毁（关键不变式，实测确认）**：类变量（`Dim w As C`）**从不**释放自己那份引用
   ——`vb6_cls_X_Destroy` 全项目只有两个调用点（COM wrapper 归零、UserControl terminate）。所以经
   类变量拿到的实例计数恒 ≥1，唯一能归 0 的路径是「`Set <接口变量> = New …` 且无别的接口变量共享」，
   这也正是本批能观测到 `Class_Terminate` 的唯一途径。（类变量泄漏是既有现状，不在本批处理。）
6. **`Exit Sub/Function` 走的裸 `return` 不清理**：与 ANSI 临时变量同一个既有缺口，本批按同一口径
   处理（不扩大），记为边界。
7. **只有裸标识符目标接计数**：`me-><字段>` 形式的接口变量（类模块成员）沿用 B04 的纯赋值改写、
   不做计数——`knownIvrefVars_` 按短名登记，`me->x` 命不中；留给后续批次。
8. **用例设计约束**：**不能**用 `Set q = p`（接口变量→接口变量）造"两个接口变量共享一个对象"的场景，
   B04 的诊断仍拒它（QI/转换要到 B06）→ 必须经类变量分发（`Set p1 = w2` / `Set p2 = w2`）。
   `CWriter.cls` 的 `Class_Terminate` 打 `TERM last=<m_last>`，用不同 `m_last` 让"哪个实例何时死"可辨：
   (a) `Set z = New` + `Set z = Nothing` → `TERM last=bye`（真归 0）；(b) 类变量 + p1/p2 全释放后
   `w2.Total()` 仍正确且无 TERM（不误销毁）；(c) `Sub ScopeExit` 不写 Nothing，纯靠过程出口 →
   `TERM last=scoped`。实测该工程**恰好 2 条 TERM**（`grep -c` 计数），顺序与预期一致。
   `itf_xmod_writer` 的 needles 由 5 条增至 11 条（本批 +6），**用例条目数不变**。
9. **护栏**：8 文件（hello/test_rtl/test_array/test_error/test_ndarray/test_generics .bas +
   M6Test/test_implements .vbp）`--emit-c` 对 pre-B05 基线二进制（worktree @6bc97e8，exe dc09fd9f）
   **全同**，`guard_fail=0`；worktree 用后即 `remove --force` + `prune`（D20-9 口径）。
10. **一处刻意留到 B06 的文案**：槽表 typedef 里的注释仍写
    `/* IUnknown prefix slots: placeholders in B04, real QI/refcount in B13/B05 */`。
    改它要重建二进制，而门与二进制一一绑定（D15-9），不值得为一条注释重跑 28 分钟全量门；
    B06 动 QI 时必然重编，那时一并更新（发射点是 `cgen_iface_vtbl.cpp` 的 `emitIfaceContractTypedefs`）。

### D22 B06a（QueryInterface / 跨接口 Set / `TypeOf … Is <接口>`）实施记录（2026-09-23）

1. **IID 只能按值比，且要按真 GUID 内存序发**：`vb6_iv_iid_<I>` 是 `#ifndef` 块里的
   `static const unsigned char[16]` → 同一接口在**每个编译单元各一份、地址不同**，比较必须走
   RTL 的 `vb6_IidEqual`（16 字节值比）。字节序 = Data1/2/3 小端 + Data4 原序，P6 交给真 COM
   时不必再翻。取值优先级：源码 `[InterfaceId("…")]`（`IfaceView.guid`，B01 起**只写不读**，
   本批起才有消费者）→ 否则 4 词 FNV-1a 从接口小写名派生（键 `"iviface:" + lower(名)`，与
   `cgen_util_dllentry_prelude.inc:56-70` 的 `generateIid` 同族；D8 禁随机）。
   IUnknown 用固定常量 `vb6_iv_iid_IUnknown`（自带 guard，全 TU 一份语义）。
2. **QI 与 AddRef/Release 同规格按 (类, 接口) 各一份**（D21-2 同一理由）：命中兄弟接口要
   `&me->__iv_<J>`，偏移依赖 J。为此 `emitIfaceImplTables` 顶部先发本类**全部 AddRef 的前向
   声明**——接口块的发射顺序与被调用顺序不一致，C 里用到必须先声明。语义：认 IUnknown / 本接口 /
   本类实现的其它接口，成功即对返回的那个指针 AddRef；其它 IID → `E_NOINTERFACE(0x80004002)`
   且 `*ppv=NULL`；`ppv`/`riid` 为空 → `E_POINTER(0x80070057)`。
3. **薄指针前 3 槽固定 = RTL 可以通用调用它**：RTL 新增 `vb6_ivtbl_prefix{QI,AddRef,Release}`
   + `vb6_IidEqual` + `vb6_IfaceSupports(ifacePtr, iid)`（声明在 `vb6rtl_class_com.h`、实现在
   `vb6rtl_com.c`；放 RTL 而非每个模块的 static，免 C4189/C4505 且只有一份）。
   `IfaceSupports` QI 成功后立刻 Release → 净效果只回答"支持不支持"，正是 VB6 `TypeOf … Is` 的口径。
4. **`vb6_TypeOf` 那个恒返 0 的桩刻意没碰**（`vb6rtl_conv.c:316-322`）：既有工程的
   `TypeOf x Is <类>` 一直恒假（`tests/BalloonTooltips/cTT.cls`、VBFlexGridDemo 在用），修它是
   **行为变更**，另批处理。本批只在 `visit(TypeOfExpr)` 开头加"右侧是新式接口"的分叉：左侧必须
   是接口变量，否则报 ASCII 诊断并返回 0（不静默给错答案）。该诊断没有 `--syntax-only` 级用例
   ——`Test-SyntaxFail` 只看语法/语义阶段，codegen 诊断够不着（同 D20-7⑤口径）。
5. **跨接口 `Set` 的引用口径**：发成一个 C 块 —— 存旧值 → `p->vt->QueryInterface(p,
   vb6_iv_iid_<目标>, &got)` → `q = (vb6_ivref_<目标>*)got` → Release 旧值。QI 成功已 AddRef，
   故 q 直接持有那份；失败得 NULL = Nothing（VB6 此处抛 438，本项目先按 Nothing 处理，
   错误码留给 P6/错误处理批次，已记边界）。
6. **用例形状**：`CWriter` 升级为双接口类（`IWriter` + 新增 `ILog`；`ILog` 带
   `Property Get Logged` → 顺带再验一次属性槽键 `get_Logged`）；另加**无实现类**的 `INope`
   （只进登记表、只出 IID）→ 负向 `TypeOf`/`E_NOINTERFACE` 有真实覆盖。新增 QI1..QI4 +
   TOF1..TOF3 共 7 条断言：TOF1 走"从 IWriter 指针 QI 到 ILog"、TOF2 走反向兄弟分支、
   TOF3 走 E_NOINTERFACE。实跑仍**恰好 2 条 TERM**（`last=bye` / `last=scoped`）→ 新增的
   QI/AddRef/Release 收支平衡，没有多一个少一个引用。
7. **本批边界**：① 下行转换 `Set <类变量> = <接口变量>` → B06b（见第 8 条）；②
   `TypeOf <类变量> Is <接口>`；③ 接口值进 Variant / 作实参传递的 QI 化（`cExprIsVariant`
   那条老路与薄指针不兼容，`vb6_ComIsDispatchable` 只查 7 槽，接口槽数少时会读到 vtable 之外）；
   ④ QI 认 IUnknown 时返回的是**本接口的薄指针**而非全对象统一的 IUnknown 指针（内部自洽；
   二进制 COM 兼容由 P6 的包装器负责）。
8. **下行转换（B06b）的真实障碍不是类型可见性，而是"身份验证"**（本条订正 Explore 报告的
   一处判断：`vb6_cls_<C>` 结构体是发在**类自己的 .h** 里的，别处 include 后就是完整类型 ——
   B04 生成的 `s = &(w)->__iv_IWriter;` 能编译过就是证据，所以使用点**看得见**
   `offsetof(vb6_cls_C, __iv_<I>)`，减偏移这件事哪个 TU 都能做）。真正的限制是：槽表实例
   `vb6_ivtbl_<I>_for_<C>` 是 C 的 .c 里的 `static const` 对象，别的 TU 拿不到它的地址，
   于是"这根薄指针到底是不是 C 的实例"没法在使用点判定。B06b 因此按类导出一个验明正身的
   助手（在 C 自己的 .c 里比较 `*(const void**)self == &vb6_ivtbl_<I>_for_<C>`，命中才减
   offsetof 返回实例，否则 NULL），声明放类的 .h 里跨模块可见；不要写成裸强转（对象不是那个类
   时会静默指到别处去）。上行方向（`TypeOf <类变量> Is <接口>`）同一套助手反着用即可。

9. **B06b 设计定稿（下一轮直接动工，不用再调研）**：按类在 `emitIfaceImplTables` 里多发两个
   跨模块助手（声明进类的 .h，实现进类的 .c，与槽表实例同 TU）：
   - `void* vb6_iv_from_iv_<C>(void* self)` —— 下行转换验身：比 `*(const void**)self` 与本类
     每个 `&vb6_ivtbl_<I>_for_<C>` 地址全等，命中才 `(char*)self - offsetof(vb6_cls_C, __iv_<I>)`，
     否则 NULL（= Nothing）。`Set <类变量 w> = <接口变量 s>` 发成
     `w = (vb6_cls_C*)vb6_iv_from_iv_C(s); if (w) s->vt->AddRef(s);`
     —— **必须 AddRef**：类变量那一遍引用永不释放（D21-5 不变式），不加码的话接口侧释放到 0
     会把对象从 w 脚下抽走。
   - `int32_t vb6_iv_test_iid_<C>(void* self, const void* riid)` —— `TypeOf <类变量> Is <接口>`
     用**静态** IID 归属判定（类变量的动态类型恒等于声明类型）：`self != NULL` 且 riid 命中
     IUnknown 或本类实现的任一接口 IID → 1。这条不需要减偏移，也就绕开了"字段是否存在"的
     编译期问题（`TypeOf w Is INope` 对不实现 INope 的类必须能编译并返回 False）。
   两者都只在 `ivImplementedIfaces(module)` 非空时发射 → 逐字节护栏照旧成立。
   静态契约（类不实现该接口时 `Set w = s` 要不要在编译期拒）留到 B13/P6 与 IID 表一起做，
   本批按运行期 NULL 处理并记边界。

10. **B05 留下的一个真缺陷，B06b 必须顺手修（已复现推理，未有用例）**：上转型发的是
    `s = &(w)->__iv_<I>;` —— `w` 是 Nothing（声明了但从没 `Set`）时，C 层面算的是
    `NULL + offsetof(...)` = 一个**非空野地址**，紧接着 `if (s) s->vt->AddRef(s)` 就解引用它 →
    崩溃。修法：发成 `s = (w) ? &(w)->__iv_<I> : NULL;`（三目即可，不需要临时变量）。
    配套负控用例：`Dim nz As CWriter`（永不 Set）→ `Set s = nz` → 期望 `s Is Nothing` 为真。
    `vb6_iv_from_iv_<C>` 同样要自守 `if (!self) return NULL;`。
    为什么 B05 的门没抓到：既有断言全部先 `Set w = New CWriter` 再上转，没有 Nothing 侧路径。

### D23 B06b（下行转换 / `TypeOf <类变量> Is <接口>` / 修 B05 野地址）实施记录（2026-09-23）

1. **按类导出两个跨模块助手**（`emitIfaceImplTables` 尾部发；声明进类的 `.h`、实现进类的 `.c`，
   与槽表实例同 TU）：
   - `void* vb6_iv_from_iv_<C>(void* self)` —— 薄指针 → 实例。先 `if (!self) return NULL;`，
     再取 `vt = *(const void**)self`，与本类每个 `&vb6_ivtbl_<I>_for_<C>` **地址全等**比对，
     命中才 `(char*)self - offsetof(vb6_cls_C, __iv_<I>)`。这就是 D22-8 说的"障碍是身份不是
     布局"的落地：表实例是 owning TU 的 `static const`，别处拿不到地址，所以判身只能在这里做。
   - `int32_t vb6_iv_test_iid_<C>(void* self, const void* riid)` —— 静态 IID 归属判定。
   两者都只在 `ivImplementedIfaces(module)` 非空时发射 → 无新语法工程生成物逐字节不变。
2. **下行转换**：`Set <类变量> = <接口变量>` 发成一个 C 块
   `void* __ivdown = vb6_iv_from_iv_C(s); w = (vb6_cls_C*)__ivdown; if (w) s->vt->AddRef(s);`
   —— **AddRef 是必需的**：类变量那一遍引用按 D21-5 的不变式永不释放，不给它加一次码，
   接口侧 `Release` 到 0 就会把对象从这个还活着的类变量脚下抽走（悬垂）。未命中得 NULL，
   即 VB6 的 `Set w = Nothing` 语义；VB6 这里其实抛 438，**静态契约拒绝留给 B13/P6**（记边界）。
3. **修掉 B05 的真缺陷（D22-10）**：上转型现在发成
   `s = ((w) ? &(w)->__iv_IWriter : NULL);`。守卫**只加在"裸类变量"这条路上** —— 给 `New`
   那条加三目会让 `_New()` 求值两次、凭空多创建一个实例（这个坑我在写的时候避开了，
   注释里写明原因）。配套负控用例 `DN0`：`Dim nz As CWriter` 永不 Set → `Set snz = nz` →
   `snz Is Nothing` 必须为真。**A/B 实测（同一份最小工程，只在 .build/negctl 里做，未入库）**：
   pre-B06b 基线二进制（@613d2b8，exe 67a28bfe）编译出的可执行文件 **段错误 exit=139**
   （= 0xC0000005，正是 `AddRef` 解引用 `NULL + offsetof` 那个非空野地址）；
   本批二进制（exe 4202570a）同一工程 exit=0 且打印 `NEGCTL:Nothing-OK`
   → 缺陷是真的、不是理论上的，且本批确实修掉了。`itf_xmod` 里的 DN0 就是这个 case 的常驻版本。
4. **`TypeOf <工程类变量> Is <新式接口>` 走静态 IID 归属**（`vb6_iv_test_iid_<C>`）：类变量的
   动态类型恒等于声明类型，所以这条**不需要减 offsetof**，也就不要求"这个类恰好实现该接口"
   才编译得过 —— 不实现时必须老实返回 False（用例 `TOC3` 用无实现类的 `INope` 打这一发）。
   左侧现在三类形态：接口变量（B06a，`vb6_IfaceSupports` 走真 QI）／类变量（B06b，静态判定）／
   其他（ASCII 诊断，仍无 `--syntax-only` 级用例覆盖）。`vb6_TypeOf` 那个恒返 0 的桩照旧一个字没动。
5. **两侧都只认裸标识符**：`me-><字段>` 形式的类字段／接口字段、属性目标、Variant 右值都不接
   （接口字段的声明侧 `Dim x As IWriter` 在类模块里会进 `knownIvrefVars_` 的短名，但 `Set me->x = …`
   的目标文本带 `me->` 前缀命不中 → 沿用 B04 的纯赋值路径，不计身份）。记为边界。
6. **实跑与负控**：`itf_xmod_writer` 断言 18→25 条（DN0..DN3 + TOC1..TOC3），实跑全中、
   仍然**恰好 2 条 TERM**（`last=bye` / `last=scoped`）→ 新增的下行转换 AddRef 与三处上转型
   守卫没有多一个或少一个引用。
7. **待回记**：门数字与逐字节护栏见总表 GATE_BASELINE（同批跑）。

### D24 B07 开工地图（2026-09-23 B06b 收尾轮产出；行号为 fa877ef/613d2b8 之后本轮 Explore 实测，含对 D2/D6 的硬修正）

- **`Inherits` 今天会被硬拒**（不是静默接受，好事）：`inherits` 词化成 `Identifier`，
  在 `src/parser/parser_module.cpp:187-191` 落到 fall-through，报
  `VB2002 unexpected token at module level: inherits`（`ParseUnexpectedToken=2002`,
  `src/common/diagnostics.hpp:49`），随后 `advance()+skipToNextLine()` → 整行丢弃。
  全仓（`src/` + `tests/` + `docs/vb6-manual/`）对 `Inherits`/`MyBase`/`Protected`/
  `Overridable`/`vb6_cvtbl_` 的命中数 = **0**，B07 起全是绿地。
- **语法接线 = 逐处镜像 `Extends`，四处**：`src/lexer/token.hpp:144-149`（枚举；
  `:149` 注释"仅接口域; 类继承用 Inherits, P3"就是本批的占位说明）、
  `src/lexer/lexer_keywords.cpp:51-54`、`src/parser/parser_helpers.cpp:56-57`（软关键字表，
  `canBeName()=Identifier||isSoftKeyword` 在 `:67-69`）、`src/lexer/token.cpp:31`
  （`isKeyword()` 显式列表）。枚举位置在 `TrueKeyword`(:24)~`GetObject`(:289) 区间内，
  所以 `token.cpp:7` 的区间判定自动覆盖。**切勿**进 `isStatementStart()`
  （`token.cpp:93-126` 连 `Interface`/`Extends` 都不在，D12 的"语句位永不识别"红线）。
  基类名解析照抄 `parseImplements()`（`parser_module.cpp:242-259`，Fix 083 的点号循环在 `:253-257`）。
- **D6 的头行接缝是错的，放松它=改既有 error path**：`parser_module.cpp:89-104` 那条
  `Class` 头行分支要求 `parseTypeParams()` 见到 `( Of`（`parser_decl_var.cpp:463-471` 否则返回空）
  → `:95-97` 直接报"泛型类头行需要 (Of T[,U])"。即今天 **`.cls` 首行写 `Class Foo` 本身就是错**。
  B07 若要支持 `Class Derived Inherits Base` 头行形式，必须放松该守卫 → 按 D9 跑逐字节护栏
  （`test_generics.bas` 在 8 文件清单内）并补一个泛型 `.cls` 用例。建议 v1 只做**独立子句行**
  `Inherits Base`（B01 的 `Interface…Extends` 也是两条路分开走的先例）。
- **AST 增量**：`Module` 加 `inheritsName`（`src/ast/detail/ast_decl.hpp:332-360` 字段区，
  `isClassModule:332`/`implements:349`/`interfaces:352` 旁），`ImplementsClause:40-44` 是"新结构体
  + clone 路径"的先例（B02b 动 3 处，`src/ast/ast_clone.cpp:707-720` 一带，见 D4）。
  `ast_printer_visitor_decl.inc` 既不印 `implements` 也不印 `interfaces`（grep 0 命中）→ 无需动打印器。
  下一个空闲**语义**诊断 ID = **3020**（`SemInterfaceClauseUnbound=3019`, `diagnostics.hpp:81`；4xxx 才是 codegen）。
- **类链求解照抄接口登记表的三趟结构**（`src/driver/driver_interface.cpp`）：Pass A 工程级唯一名 +
  与模块名冲突（`:29-84`，冲突判定 `:61-67`）、Pass B 父未知（`:90-94`）+ `seen` 集环检测
  （`:96-113`）、Pass C **父先序**展平 + `used` 键冲突诊断（`:115-167`，`chain.insert(begin)` 在 `:126`）。
  产出 = **每类一张只读"有效成员表"**（新 `ClassChainRegistry`，Driver 级，与 `IfaceRegistry` 平级），
  不改 AST、不动符号表枚举。理由与 D2 一致：每模块一张符号表而类名是工程级唯一。
  合并必须落在 **stage 3.5 之前**（`driver_compile.cpp`：2.6 泛型 :329 / 2.7 接口 :338 /
  3 语义 / 3.5 `runCrossModuleResolution` :356 / 3.5b :367 / 3.6 :372-387），否则外部工程的逐字段
  拷贝看不见派生域。单继承 =  arity 检查，另加深度上限。
- **D6 的合并字段清单不全**：`driver_crossmod.cpp:169-190` 是逐个字段手工拷贝外部 Class 符号，
  成员表实有 11 张（`symbol_table.hpp:116-197`）：`memberNames` `memberReturnTypes` `memberProcKinds`
  `memberParams` `memberFieldTypes` `memberFieldNames` `publicFieldNames` `memberFieldDispids`
  `memberLetParams` `memberSetParams` `eventNames`。漏一个 = 跨模块消费点瞎。**`interfaceMethodParams`
  是死字段**（D2 当时就记了从没被写入），别往里挂东西。
- **要"走基链"或吃合并表的函数（点名到行）**：`resolveClassMemberCall`
  （`cgen_util_classcall.cpp:17-224`；Fix 014 的 `classSym->memberNames` 兜底 `:161-200` 就是守门人，
  发出名在 `:115/:118/:198/:223`，注意它**不走** `cProcName`，是手拼 `vb6_<Cls>_<prefix><Member>`）、
  `findClassMemberCallParams`（`:237`，Phase A :248 / Phase B ~:407 / `memberReturnTypes` 兜底 :439）、
  `findClassMemberWriteParams`（消费点 `cgen_util_comwrite.cpp:275,:387`）、
  `getClassMethodReturnType`（`:400`）、`canonicalClassMemberName`/`canonicalClassFieldName`
  （`cgen_util_comwrite.cpp:178`）、`inferClassTypeOfExpr`（`cgen_util_classtype.cpp:15-124+`）、
  `knownClassVars_` 注册（`cgen_util.cpp:28`、`cgen_decl_{func,proc,prop}.cpp:104/110/112`）、
  driver 两张字段图扫描（`driver_codegen_typedfield_scan.inc:10-85`、`driver_codegen_voidfield_scan.inc:11-92`）、
  `classVariantMembers_` 扫描（`cgen_base_generate_state_scan.inc:56-114`）、
  `emitClassFieldAccessors`（声明 `cgen_helpers.inc:89`，调用 `cgen_base_generate_body_pass.inc:27`）、
  legacy Implements 覆盖检查（`semantic_analyzer.cpp:236-273`，扫 `memberNames` 在 `:271`
  → 继承来的实现**应当**满足契约，本批顺手让它走有效成员表）。
- **布局：不内嵌 `vb6_cls_Base`，改字段扁平复制**。内嵌一条就同时继承基类的 `__refcount` 与
  `__iv_<I>` 字段 → 双计数（撞 D21-1"一个类一个计数门禁"）、且 derived `_New` 要重复初始化继承槽的
  vtbl（`emitIfaceNewInit`, `cgen_iface_vtbl.cpp:353-365`）。B07 = 把基类私有字段按原序前置进派生
  struct 的用户字段区（发码点 `cgen_base_generate_c_open.inc:84-102`，字段 0 仍 `void* __comObj` `:76`，
  D19 不变）。
- **遮蔽判定必须大小写无关**（这是本轮发现的真实静默错字段风险）：`:84-102` 的字段循环**零去重、
  零归一**，而 `cIdent` 保留大小写（`cgen_base_naming.cpp:49-78`）、成员表键却是小写
  （`memberFieldNames`/`memberFieldTypes`）→ `m_X` 与 `M_x` 今天会发成**两个不同 C 成员**共享一个
  VB6 逻辑字段。合并时按小写键裁决胜者，struct 与所有表都用同一个拼写。
- **方法复用 = 转发桩，不做基类方法直调强转**。基方法符号是 `vb6_<Base>_<M>(vb6_cls_<Base>* me,…)`
  （属性 `vb6_<Base>_prop_get_<P>`），定义在基类 TU；`cProcName` 在
  `cgen_base_naming.cpp:249-265`（类方法一律带模块段），重载后缀 `_ov<fp>` 在 `:271-274`。
  `(vb6_cls_Base*)me` 强转有先例（`cgen_form.cpp:210,:237-250,:269` 的 `(vb6_cls_<ctl>*)me`），
  但它要求前缀布局逐字段一致，还得给每个调用点插桩；桩顺带绕过 `_ov` 后缀、B08 的 Overridable
  钩子、B09 的 MyBase 去虚化三件麻烦 → B07 发
  `vb6_<Derived>_<M>(vb6_cls_<Derived>* me,…){ vb6_<Base>_<M>((vb6_cls_<Base>*)me, …); }`
  只对**未被子类重名遮蔽**的继承成员生成。`me` 的类型来自 `classMeParam()`（`cgen_decl_prop.cpp:324-328`），
  `visit(MeExpr)` 在 `cgen_expr.cpp:396-411` 发裸 `me`。
- **B07 语义层有个真空**：全仓没有"项目类成员不存在"的诊断（`cgen_expr_member_precheck.inc` 只管
  `Err`/`VBA`/控件），今天写 `x.NoSuchMethod()` 会发成一个不存在的 C 调用 → **MSVC 编译错，不是 C3 诊断**。
  后果：负例断言只能挂在**本批新加的 3020 诊断文本**上，不能靠既有行为。
- **用例形态**：单 `.cls` 负例照 `tests/itf_neg/n08_missing_slot.cls`（`run_tests.ps1:866-890` 的
  `$itfNeg`，`Test-SyntaxFail` 在 `:584`，缺失文件→SKIP 不是 FAIL `:891-894`）——`Inherits NotThere`
  这类"单文件即可判定"的负例零成本。**但 shadow / 单继承 arity / 环 A↔B / 深度上限都是双文件用例**：
  `Test-SyntaxFail` 只接一个 `$Source`（`:590` 单引号参数）。两条路：给 helper 加文件列表
  （CLI 本身收多个位置参数：`driver_args.cpp:145` 逐个 push，`driver_frontend.cpp:149-163` 按扩展名定
  模块类型），或新写 `tests/cls_inh/*.vbp` + `Test-VbpFail`。B07 预算里含这个 helper。
  正例落地沿用 `tests/itf_xmod/` 的 vbp + `Test-Vbp` 断言（`:503`，只支持正向 needle、无 exclude）。
- **手册**：新建 `docs/vb6-manual/02-语句/Inherits 语句.md`（模板 = 同目录 `Interface 语句.md`，
  首行引用块标"本项目扩展"，末尾"实现状态"节，D18-5 的 CRLF 归一 + README `bare_lf==0` 复查照做），
  README 索引在 `:158`（`- [Interface 语句](…%20语句.md)`）旁按字母序插一行。
  **必须写明**：`Extends`=接口继承、`Inherits`=类继承，两者永不同义——018 自己 §二十一
  （`ai/讨论记录/018-接口继承与CoClass设计思路.md:1210-1224`）用 `Inherits` 写接口继承，
  而 `:1549-1551` 又把它划给类继承，实现按 `Extends` 落地，别照 018 的字面回头改。
- **D6 其余漂移**（按 D22-8 的规矩不原地改历史，只在此登记）：`getPublicSymbols` 实在
  `symbol_table.cpp:376`（D6 写 :383，7 行漂移）；D2 计划的 `SymbolKind::Interface` 至今没加
  （`symbol_table.hpp:23-45` 末位是 `ComGlobalNs`），B07 一切接口信息仍以 `IfaceRegistry` 为准。
- **护栏口径校正**：逐字节 `--emit-c` 对比是**每批手工仪式**，不是 CI/测试套属性
  （`grep emit-c tests/run_tests.ps1` = 0 命中），且其机理是**按 feature 早退**
  （`cgen_iface_vtbl.cpp:341-342/:354-356/:372-373`）。B07 的早退条件必须是
  "本类或其链用到 `Inherits`"，不是"工程里没有新语法"。8 文件清单里的
  `tests/test_implements.vbp` 是唯一带 `.cls` 的类工程输入，B07 开工时先**实测确认**它确实
  产出 class struct（否则"类布局未变"这句断言无覆盖）。
- **B07 边界（建议，超出的往后批）**：只做 `Inherits` 语法 + 链检测 + 有效成员表 +
  字段/方法继承的具体类调用；`Protected`/`Overridable`/`Overrides` = B08，`MyBase` = B09，
  接口实现经继承满足契约的**新式**路径与派生类 vtbl = B08+，CoClass = P5。

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
- 2026-09-23 04:42–05:56 **B02b（成员级 `Implements I.M` 子句 + 泛型 3018 用例）过门并提交 dde7c32**：
  开工按 D15-10 的硬检查等到 04:44 才确认上一轮收线（它 04:35 落代码后 04:37/04:40 还在追加 docs 提交）。
  改动 9 文件：`parser_decl.cpp` 新增 `parseTrailingImplementsClauses`（Sub 在参数表后、Function/Property 在
  `As Type` 后；点号拼接与模块级 Fix 083 对称，`A.B.C` → 接口 `A.B` + 成员 `C`）、`ast_decl.hpp` 加
  `ImplementsClause` 结构 + 三个过程节点的 `implementsClauses`、`ast_clone.cpp` 三个 clone 函数补拷贝
  （泛型特化走这条路，漏拷会静默丢绑定）、`semantic_analyzer_iface.cpp` 显式绑定入席（槽键兼容 `I.Name`
  与 `I.get_Name` 两种写法、Extends 链上父/子接口名任一；写了子句的成员**不再**参与同名隐式匹配）+ 新
  `checkMemberImplementsClauses` 兜底未认领子句（四种成因各给一句话，新 ID `SemInterfaceClauseUnbound=3019`）、
  `run_tests.ps1` 二进制插 11 行（该文件实为 **UTF-8 无 BOM + CRLF**、4899 个非 ASCII 字节，与 D9 写的
  GBK 不符，见 D16-6；改完核对非 ASCII 字节数不变 + `lf_only=0` + numstat 只 +11/-1）。用例 6 负
  （n14 无此成员 / n15 类未实现该接口 / n16 宿主非类模块 / n17 子句未限定 / n18 显式绑定下签名不符 /
  n19 = B02b-② 泛型模板内 Interface 的 3018 分支）+ 1 正（p02 覆盖"一成员认领两接口同名槽""继承槽用
  父名或子名""属性三槽 Get/Let""成员名与槽名完全无关"）。**首门（exe 1f387137，05:01–05:24）已
  125/0/1/126 零失败，但门后又改两处源码（去掉重复的 `IfaceClauseRef` 声明、畸形子句不入列表）→
  按 D15-9 那次测量不作提交依据**：05:24 重建（cdac040f）、单文件复验 7 条行为不变、05:25–05:48 复跑
  全量终门 `Results: PASS=125 FAIL=0 SKIP=1 TOTAL=126`（跑前后 md5 一致、legacy `test_implements` 仍
  PASS）→ 记为 GATE_BASELINE，B02/B02b 收口。未开 B03（余下时间不足一个"构建+25 分钟门"周期），
  改为产出 **D17 开工地图**：探针实测已把 B03 从"新语法批"降格为"宿主识别 + VB3002 重名放行 + 手册页"，
  并给出全部锚点行号与 legacy 污染待核点。
- 2026-09-23 06:08–06:46 **B03（P1 收尾：`.cls` 头行宿主形式）过门并提交 c47cdce**：人工续跑轮
  （用户"继续完成"），同轮先收 B02b（dde7c32）再做 B03。按 D17 地图开工，其中"在 parser 里识别宿主"
  一条实测不成立（`Module::moduleName` 要到 `driver_frontend.cpp:217-250` 才从 `Attribute VB_Name`
  定下来）→ 识别改放 stage 2.7 Pass A，置位新增的 `Module::isInterfaceModule`；VB3002 重名检查只豁免
  宿主自己那一个块（`hostOwnName`），`itf_n13` 撞车负例照旧报错；宿主文件里出现其它声明 → 3018 且只报
  第一条。**D17 留的"legacy 污染待核点"实测为无污染**：新用例 `tests/itf_xmod/`（头行宿主 `IWriter.cls`
  + `CWriter.cls` 用 B02b 的成员级子句跨模块绑定 Emit/Total/Last 三槽 + `XMain.bas` 具体类调用）编译
  链接成 exe 并跑出 `XMOD1:OK`/`XMOD2:OK`，登记走**第三个用例通路** `Test-Vbp`；接口类型变量
  `Dim s As IWriter` 本批刻意不做（同名 Class 符号会先被类型解析吃掉）→ 归 B04，已写进 CURRENT_BATCH。
  文档：新页 `docs/vb6-manual/02-语句/Interface 语句.md`（页首引用块标注"本项目扩展，非 MS 原生"）+
  README 索引按字母序插一行；该目录全 CRLF，Write 产出 LF 需 `sed -i 's/\r*$/\r/'` 归一并复查
  `bare_lf==0`（细节见 D18-5）。**门：06:21–06:45 全量
  `Results: PASS=128 FAIL=0 SKIP=1 TOTAL=129`**（exe md5 053150bf 跑前后一致；126→128 为新增
  p03/n20/itf_xmod_writer 三条，legacy `test_implements` 仍 PASS）。逐字节 emit-c 护栏本批未跑，
  理由与替代守卫记在 D18-6。P1 阶段（B01/B02/B02b/B03）至此**全部收口**，下一批进入 P2 发码期（B04）。
- 2026-09-23 06:46–07:00 **B03 收尾后追加 D19（B04 开工地图），未开 B04**：B04 是第一个改结构体与
  派发路径的发码批，按纪律不在共享树里留半批不可编译的后端代码。派一路只读勘察核实后端接缝，
  结果里有一条**推翻我自己 D3 的硬假设**并已亲自复核：`src/rtl/core/vb6comserver/vb6comserver_obj.c:268-272`
  与 `:302-303`、`:318` 用 `void** ppComObj = (void**)instance` 做**裸偏移 0** 读写，
  `vb6comserver.h:126-128` 还把"首字段是 `__comObj`"写成明文前置条件 →
  D3 说的"C 代码全按字段名访问、前置新字段安全"不成立，B04 必须让 `__comObj` 保持第 0 位、
  把 `__ivtbl[k]` 放在它之后。地图另记：legacy 槽表**不发适配器**（槽位直指 `IFoo_M` 实现函数，
  `cgen_com.cpp:402-413`）、`CCodeGen` 今天拿不到 `IfaceRegistry`（只有语义层注入）、
  **没有工程级"已发表"去重表**（`vb6_vtbl_<I>` 按实现类重复 typedef）、
  `isInterfaceModule` 后端零消费、胖对按值的门在 `cgen_base_type.cpp:193-197` 的 `clsSym->isInterface`、
  `vb6_ivtbl_` 前缀全仓零占用可用；建议拆 B04a（只发槽表 + 宿主不发类实例 + 逐字节护栏）
  与 B04b（薄指针 + 四处 `knownIfaceVars_` 登记 + 派发/`Set`）。**P1 阶段（B01/B02/B02b/B03）至此收口**，
  门基线 128/0/1/129（本轮未改代码，故未复跑）。

- 2026-09-23 07:05–08:04 **B04（P2 第一批：接口值代码生成）过门并提交 6bc97e8**：人工确认"没有其他写者"
  后直接开工。**D19 建议的 B04a/B04b 拆分未采纳**（只发槽表不派发没有任何可观测行为，过门等于没过门），
  节奏改成"先 `--emit-c` 观察生成物 → 再接派发 → 一次过门"。交付：新编译单元
  `src/backend/module/cgen_iface_vtbl.cpp`（14 个成员，`CMakeLists.txt` 的 `vb6c3-cgen` 登记）+
  三个发射钩子（头文件尾 / 类结构体 `__comObj` 之后 / `_New()` 里 `vt` 初始化）+
  `Set`/`MemberAccess`/`IndexOrCall` 三处接线 + `cgen_base_type.cpp` 在 Class 分支**之前**返回薄指针类型。
  两处对开工地图的修正：**不需要**工程级"已发表去重表"（`#ifndef VB6_IVTBL_<I>` 守卫 + 按小写接口名排序
  即可幂等且可复现）；槽键口径不必复制一份（`ifaceProcClauses`/`ifaceSlotPrefix`/`ifaceClauseSlotKey`
  从 `semantic_analyzer_iface.cpp` 上提到 `interface_sig.hpp`，语义比对与后端绑定查找同源）。
  实测生成物：`vb6_ivref_IWriter* s = NULL;` → `s = &(w)->__iv_IWriter;` → `s->vt->emit(s, …)` /
  `s->vt->total(s)` / `s->vt->get_last(s)`，`Set s = Nothing` 与 `If s Is Nothing` 白送（薄指针语义）。
  门 `Results: PASS=128 FAIL=0 SKIP=1 TOTAL=129`（exe md5 ef1a7520 跑前后一致；用例条目数与 B03 持平，
  本批只加 3 条断言）+ 8 文件 `--emit-c` 对 pre-B04 worktree(@f8ca84e) 基线逐字节全同。
  基线 worktree 已 `remove --force` + `prune`，`git worktree list` 只剩主目录（D20-9）。

- 2026-09-23 08:08–08:59 **B05（P2 第二批：接口引用计数）过门并提交 f644003**：B04 收口后同轮续做。先派一路 Explore 专查"实例生命周期现状"，三个实测结论直接改写了方案
  （D21-1/4/5）：① 项目类实例**从来没有**作用域末尾释放——`vb6_cls_X_Destroy` 的调用点只有 COM wrapper
  归零（`vb6comserver_obj.c:83-87`）与 UserControl terminate（`cgen_form.cpp:250`）两处，所以"计数归 0"
  想可观测只有一条路：让 `New` 的那一次引用被接口变量直接收下（不 AddRef）；② 给所有类统一加计数头会
  改结构体布局、破逐字节护栏 → 字段只落在实现新式接口的类上，且在 `__comObj` 之后（D19 硬约束）；
  ③ `Set <薄指针> = Nothing` 原本落到 `vb6_ReleaseObject`，等于把薄指针当 `IDispatch*` 用（靠
  `vb6_ComIsDispatchable` 拒绝才没崩）→ 本批改成经槽真 Release。另：AddRef/Release 从 B04 的"按类一份"
  改成"按 (类, 接口) 一份"——`self` 是 `&me->__iv_<I>`，container_of 依赖具体接口的 offsetof，多接口类
  共用一份必然算错实例地址（本批开工即改）。门 `Results: PASS=128 FAIL=0 SKIP=1 TOTAL=129`（exe c615c657），护栏 8 文件对 pre-B05 基线全同（worktree 用后即删）。

- 2026-09-23 09:00–09:46 **B06a（QI + 跨接口 Set + TypeOf Is 接口）过门并提交 613d2b8**：
  开工先派一路 Explore 摸 B06 接缝，三条实测结论改写了方案（D22-1/4/8）：
  ① `IfaceView.guid` 自 B01 起**只写不读**，IID 常量在生成侧一处都没有 → 本批起消费，且必须
  **按值比**（`#ifndef` 块发在每个模块头里，同接口常量各 TU 一份、地址不等）；按真 GUID 内存序
  发 16 字节，P6 交真 COM 时不用翻。② `vb6_TypeOf` 是恒返 0 的桩 —— 既有工程 `TypeOf x Is <类>`
  **一直恒假**（cTT.cls 在用），改它是行为变更 → 本批只加“右侧是新式接口”的分叉，桩一个字没动。
  ③ 跨 TU 强转薄指针不可能（类结构体只在自己 .c 里定义）→ 下行转换切给 B06b。
  实现上沿用 D21-2 的教训：QI 也按 (类, 接口) 各一份（命中兄弟接口要按各自 offsetof 取地址），
  并在类块顶部先发全部 AddRef 前向声明（发射顺序 ≠ 调用顺序）。RTL 加 `vb6_IidEqual` /
  `vb6_IfaceSupports`（QI 后立即 Release = VB6 TypeOf 的净效果），放 RTL 而不是每个模块的 static
  （免未引用警告、只一份）。用例把 CWriter 升成双接口类（IWriter+ILog，ILog 带 Property Get 再验一次
  属性槽键），并加**无实现类**的 INope 让 E_NOINTERFACE 分支真有覆盖。实跑 18 条断言全中、
  TERM 仍恰好 2 条（QI 收支平衡）。门 `Results: PASS=128 FAIL=0 SKIP=1 TOTAL=129`（exe 7721bd72）；护栏 8 文件对 pre-B06a
  基线全同（worktree @f644003，用后即删）。

- 2026-09-23 09:52–10:37 **B06b（下行转换 + `TypeOf <类变量> Is <接口>` + 修 B05 的 Nothing 上转型野地址）过门并提交 4dc6b7e**：
  实现只有两件按类导出的助手（`vb6_iv_from_iv_<C>` 先比 `vt` 与自家 `&vb6_ivtbl_<I>_for_<C>` **地址全等**才减 `offsetof`；`vb6_iv_test_iid_<C>` 静态 IID 归属），
  这就是 D22-8 校正的落地——跨 TU 的障碍是身份验证不是布局（槽表实例是 owning TU 的 `static const`）。下行转换**必须 AddRef**：类变量那一遍引用按 D21-5 的不变式永不释放，
  不给它加一次码就会在接口侧 Release 到 0 时把对象从还活着的类变量脚下抽走。`TypeOf <类变量> Is <接口>` 走静态判定（类变量的动态类型恒等于声明类型 → 不需要 offsetof，
  也就不要求该类实现该接口，`TOC3` 用无实现类 `INope` 打这一发），`vb6_TypeOf` 恒返 0 的桩照旧未动。
  **必修项 D22-10 做了 A/B 实测**（最小工程只在 `.build/negctl` 里跑，未入库）：pre-B06b 基线二进制 exit=139（=0xC0000005，正是 `NULL + offsetof` 那个非空野地址被 AddRef 解引用），
  本批二进制 exit=0 且打印 `NEGCTL:Nothing-OK`；`itf_xmod` 的 `DN0` 是它的常驻版本。守卫只加在裸类变量路径——给 `New` 那条加三目会二次求值 `_New()`、凭空多创建一个实例。
  门 `Results: PASS=128 FAIL=0 SKIP=1 TOTAL=129`（exe 4202570a，10:06 起跑、最后一次源码改动在起跑前）；护栏 8 文件对 worktree @613d2b8 基线逐字节全同、用后即删；
  实跑 25 条断言全中且 **TERM 仍恰好 2 条** → 新增 AddRef 与三处上转型守卫没有多算也没有漏算引用。
  本轮另外两件事：①按用户指示把 CI 记账口径写进总表（**Actions 级全量是里程碑级**，每批仍跑本机门）；②派 Explore 出 **D24 = B07 开工地图**，其中含对 D2/D6 的 4 处硬修正
  （`SymbolKind::Interface` 从未加、`getPublicSymbols` 行号漂移、成员表清单缺 7 张、Class 头行分支要求 `(Of T)`）。边界照旧记录：`me->字段` 形式的接口/类字段不接、
  VB6 的 438 不抛（静态契约拒绝留 B13/P6）、B06c（接口值作实参/进 Variant）随 P6 处理。
