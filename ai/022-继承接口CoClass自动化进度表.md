# 022-继承接口CoClass 自动化进度总表

> 本文件是每小时自动化任务（"实现继承、接口与CoClass"）的**唯一状态源**。
> 每次运行开始先读本文件，结束前必须更新本文件（状态头 + 批次清单 + 运行日志）。
> 规范输入: `ai/讨论记录/018-接口继承与CoClass设计思路.md`（含 tB 文档要点与分阶段设计思路全文）。

STATUS: IDLE               # NOT_STARTED | DESIGN | BUSY | IDLE | ALL_DONE
LAST_RUN: 2026-09-24T00:12:00+08:00   # B08c 已收口（代码 82b1b34、总表本轮 docs 提交）。本轮一批：B08c（`Protected` 家族外越权的拒绝）。
               # 自动运行见本行不足 55 分钟请立即跳过。   # 门 20:47/21:41/22:24/23:26 四跑：前三跑全被**同一台机器上另一个写入者**打断（它 20:41 把整个工作树复制到 `C:\Users\Administrator\Documents\c3.vb6.pro` 并从那份副本跑 `-Category all`，其间还重链了我这边的 `.build\C3.exe`，v3 更留下一次 `test_softkeyword` 的瞬时 `FAIL (compile)`）；原因与处置见 D34-8。
               # 下一轮自动运行从 **B08e** 开工（虚表线收尾：13 条未接派发的 `resolveClassMemberCall` 消费点逐条接上或判死，清单与四条硬约束见 CURRENT_BATCH）；重入保护照常：STATUS=BUSY 且不足 55 分钟立即跳过。
LAST_COMMIT: 代码批 = 82b1b34(B08c)、df9806e(B08d)、05397be(B08b)、2117d1c(B08a)、b1c0050(B07b)、a056705(B07a)
CURRENT_BATCH: **B08e**（虚表线的收尾，B08d 遗留）——把**剩下的类成员发码路**逐条接上派发或判死。
               B08d 只在两处接了间接调用（`cgen_expr_member_class_module.inc:26` 的 dispatchFn@:49、
               `cgen_expr_member_class_fallback.inc:87` 的 dispatchFnFb@:93），但 `resolveClassMemberCall(`
               全仓有 **15 个消费点**（本轮实测清点，行号即本轮）：
               `cgen_expr_call_callee_ident.inc:144`、`cgen_expr_call_callee_member.inc:141` 与 `:224`、
               `cgen_expr_member_class_fallback.inc:224`、`cgen_expr_member_class_module.inc:20` 与 `:140`、
               `cgen_expr_member_form_builtin.inc:178`、`cgen_expr_member_generic_access.inc:136`、
               `cgen_expr_member_m22_module.inc:120`、`cgen_expr_member_obj_dispatch.inc:143`（= 属性返回
               对象那条 083c/088d 通路，`pvSocket.Pick()`）、`cgen_expr_member_voidptr_com.inc:37` 与
               `:119`、`cgen_expr_with.cpp:82`（= `With w` 块内）。剩下 13 条**逐条**二选一：接
               `virtDispatchCallee(...)`，或按 D33-7 的口径补 `VB3027` 判死。**先证可达性再动手** ——
               这 13 条不是都吃得到虚槽（form/builtin、voidptr_com 那几条是外部 COM 形状），
               每条先给一个能编出"静默直调"的最小用例，编不出来的就在总表记"不可达"并跳过，
               别为了凑数改代码。
               硬约束：① 判定必须走 `virtDispatchCallee` 的 `mustDispatch` 参数（false = 认不出槽就
               回落直调，true = 认不出即诊断），别在每条路上手写槽查找；② 接收者表达式**按两次**
               就是双副作用（`cvtblObjIsPure` 那条"含 `(` 即不纯"的规则已在此用一次，新路口径一致）；
               ③ `Property Let/Set` 方向的槽 3.4b 已判死，收尾时别顺手放开；
               ④ 每条改动都要有运行期断言（`Inh.vbp` 加 `INH27..`），只有 `--syntax-only` 的证据
               抓不到"编得过、调用消失"那一类（D27-13 的教训）。
               B08 已全交付（a/b/c/d，见 D29/D31/D33/D34）；**B09 = `MyBase.M(…)` 显式基调用（去虚化）
               + 构造链顺序**，排在 B08e 之后 —— `MyBase` 要的是"绕过虚表直调基类实现"，与本轮
               刚发的 `vb6_cvtbl_<Cls>` 是同一条链的另一半，先收尾再开 MyBase 才不会两套口径。
               更早的遗留登记：**B06c** = 接口值作实参 / 进 Variant，归 B13/P6 前处理（D22-7③）。
               B08c 之后仍开的洞（都在手册页写明、不算漏做）：数组元素 / 属性返回对象 / `With w`
               内的 `.X` 不判 Protected；家族内经**基类型变量**访问按宽松规则放过（不跟 CLR 的
               "必须是本类型或更深实例"对齐）；同名属性多方向按名字取严。
GATE_BASELINE: Results: PASS=153 FAIL=0 SKIP=1 TOTAL=154   # exe md5 33d68fc7（.build/gate_B08c_v4.log，23:26 起跑、00:07 收线，跑前后 exe md5 一致；SKIP=已知 test_vbman 环境项）。条目 150→154 = 净 +4：`-Category syntax` 74→78（`ci_n21` 家族外字段写 / `ci_n22` 标准模块里 `Protected Sub` 带实参调用 / `ci_n23` 家族外 `Property Get` 读 三条负例 + `ci_pos2_base|derived` 家族内正例）。`cls_inh_pair` 26 条运行期断言、legacy `test_implements`、`itf_xmod_writer` 仍全绿。上一批 B08d = `149/0/1/150`（exe 83c4e49c）。
               # 另附 8 文件 --emit-c 对 pre-B08c 基线（worktree `D:\.wt_b08c_base` @fb6a254，`.build\base_build.ps1` + `.build/byteguard_b08c.py`）逐字节全同 8/8。
               # 门的可信度前提（本轮实测教训）：跑前先 `.build/who_is_building2.ps1` 看清有没有别的构建/套件在跑，跑完立刻核 exe md5；v3 那次 152/1/1/154 的唯一 FAIL 与 B08c 无关（该用例无任何类、走的代码路径碰不到本批判定），同 exe 单独复跑两次均 PASS → 判为与第三方套件争抢 CPU/临时目录的瞬时失败，不记数。
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
| B07 | P3 | `Inherits` 语法 + 类链检测（单继承/环/深度）+ 继承成员合并与遮蔽 + 派生域 | ☑ **B07a**（语法 + stage 2.8 链检测/诊断 + 多文件负例通路）+ **B07b**（stage 3.4 成员合并 + 前缀布局 + 转发桩）；**B07 遗留**：裸名继承调用（要 `Me.`）、继承 `Public` 字段的 COM 对外暴露 → 分别归 B08+/P6 | b1c0050 | `Results: PASS=140 FAIL=0 SKIP=1 TOTAL=141`（gate_B07b.log；exe md5 50ce2e77 跑前后一致）+ `cls_inh` 三级链 12 条断言 + ci_n08/n09/n10 三条边界负例 + legacy `test_implements` 仍 PASS + 8 文件 emit-c 对 pre-B07b(@a056705) 全同。要点：并 8 张成员表（不是 11 张，理由在码内）、祖先私有字段**也复制进布局**、属性三向各一份桩、`_has_` 尾参必须转发、**封掉裸名继承调用的静默错代码**。详见 D27（B08 地图 = D28） |
| B08 | P3 | `Protected` 可见性 + `Overridable/Overrides/NotOverridable` + 类级虚表 `vb6_cvtbl_<Cls>` 与多态派发 | ☑ **B08a**（`Protected`：家族内经 `Me.` 可用，含跨 TU 与 Protected 字段）+ **B08b**（虚修饰符三件套语法 + `Overrides` 覆盖契约：槽键按方向配对、签名复用接口口径 + 把需要动态派发的调用点判死，避免静态绑回基类实现的假虚派发）+ **B08d**（类虚表 + 运行期真派发：3.4b 排每类有序虚槽、`const void* __cvtbl` 字段、表类型/实例/装载三点同源、两处类成员发码路按槽索引改写，并删掉 B08b 的 `Me.X` 拒绝）已交付；**B08c**（家族外访问 `Protected` 的拒绝，诊断 `VB3023`：判定落在 `visit(MemberAccessExpr)` = `obj.<成员>` 的唯一必经点；接收者→工程类靠新加的 `Symbol::srcTypeName`，认不出接收者或当前类未登记一律放过）已交付 → **B08 四条全出**（要点与踩坑见 D34） | 82b1b34(B08c)、df9806e(B08d)、05397be(B08b)、2117d1c(B08a) | `Results: PASS=149 FAIL=0 SKIP=1 TOTAL=150`（gate_B08d_v3.log；exe md5 83c4e49c 跑前后一致）+ `-Category syntax` 73→74（删 ci_n15、增 ci_n19/ci_n20）+ `Inh.vbp` 运行期断言 17→27 条（INH17..23 派发：`b/m/d.PickThru()` 分别 base/mid/mid = 绑最近覆盖者、INH20 叶类覆盖被基类体内看见、INH23 基类型变量持有派生实例不再切片；INH24..26 扇出）+ 8 文件 emit-c 对 pre-B08d(@fb6a254) **8/8 逐字节全同**。更早两轮证据：B08b = `148/0/1/149`（gate_B08b.log、exe 546265e7、syntax 66→73、断言 14→17）。要点：`ProcVirt` 四值枚举而非三 bool；契约检查落 2.8、槽表落 3.4b（3.4 之后分不清"谁声明的"，而"本类有没有入口"要读 3.4 的 inhProcs）；筛选集取**链根**的 dynamicKeys → 叶类也带字段；`Me.X` 不在"优先级2"那一批发码。详见 D31、D33（B08d 地图 = D32，其中 ②③ 已被 D33 修正） |
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

### D25 B07a（`Inherits` 语法 + 类链检测 stage 2.8）实施记录（2026-09-23）

1. **词法接线 = 逐处镜像 `Extends` 四处**，实测确认 D24 的判断：`token.hpp` 枚举插在 `Extends`
   之后 → 位置天然落在 `TrueKeyword..GetObject` 区间内，`token.cpp:7` 的区间判定自动覆盖，
   但仍照 `Interface`/`Extends` 的样子补进 `isKeyword()` 显式列表（保持一致性，不依赖区间）。
   软关键字表 `parser_helpers.cpp` 也补了 `Inherits` → `canBeName()` 仍认它，
   所以 `Dim inherits As Long`、`Implements inherits.X` 这类既有写法不受影响。
2. **零风险实证**：全仓 VB 语料（`*.bas`/`*.cls`/`*.frm`，含 demo 工程）里 `inherits` 这个词
   出现次数 = **0**（大小写不敏感、排除本轮新建的 `tests/cls_*`）→ 把它关键字化不改变任何
   存量输入的词形。护栏另用 8 文件 emit-c 独立证明（第 7 条）。
3. **只做独立子句行**（兑现 D24①）：`Inherits Base` 走模块级主循环新分支，位置就在 `Implements`
   之后；头行形式 `Class D Inherits B` **没有**动 —— 那条分支要求 `(Of T)`，放松它就是改既有
   error path。点号限定名（`Project.IBase`）与 `parseImplements` 同口径吃掉，登记表按整键 +
   末段两级解析（`ivLastSegment` 同思路）。
4. **`Module::inherits` 用值类型 `vector<InheritsStmt>`，不需要碰克隆路径**：`ASTCloner::cloneModule`
   本来就不拷 `implements`/`interfaces`（泛型特化副本不带契约），而泛型模板内的 `Inherits` 已在
   stage 2.8 拒绝 → 特化副本天然无继承，与克隆器现状自洽。B02b 那种"新结构体 + 3 处克隆"的工作量
   在这里省掉了，理由是**语义上特化类不该继承模板的继承关系**。
5. **单继承的 arity 检查不需要新代码**：一条子句里写 `Inherits A, B` 撞的是 parse 的
   `expectEndOfStatement()`（VB2003），"分两行各写一条"才由 2.8 报 3022 并只取第一条继续
   （不级联）。自环 `Inherits Self` 单文件即可判定 → 走了既有的 `Test-SyntaxFail` 通路。
6. **两条实测行为，都是刻意保留的**：
   - **一条环只报一份**：Pass B 按登记序解 `baseKey`，A↔B 互指时后解出的那个（PairB）才报
     `SemCircularDependency`，PairA 当时看到的 `baseKey` 还是空 → 不报。与 Extends 链同行为，
     不是漏报（`tests/cls_neg/ci_n06_*` 就是这一发的常驻用例）。
   - **接口宿主不能当基类**：`.cls` 里那一个同名 Interface 块不是实例类，Pass A 不登记它 →
     `Inherits IHost` 得到 3020 "must be a class module in this project"。文案说得过去
     （宿主确实不是可实例化的类），另开用例 `ci_n07_*`。
   深度上限 16（含自身）是**自保兜底**，VB6/tB 都没这个限制；链上每个节点自己数自己，
   所以报错的是最深的那个类。
7. **护栏口径补一个实测结论**（解除 D24 留的疑问）：8 文件清单里的 `tests/test_implements.vbp`
   确实产出 `typedef struct vb6_cls_CRectangle` / `vb6_cls_IShape` → **类布局在这份清单里是有覆盖的**，
   以后"没碰类结构体"这类断言可以拿它当证据。本轮 8 文件对 pre-B07a 基线（worktree @58f02fe，
   自建 Debug exe）`--emit-c` 输出逐字节全同。
8. **stage 2.8 的早退条件 = "工程里一条 `Inherits` 都没有"**（D24 末两条的兑现）：先扫
   `modules_` 再决定是否建表，因此非继承工程的生成物、诊断、阶段耗时无变化。
9. **用例通路扩到"多文件"**：新增 `Invoke-SyntaxProj` + `Test-SyntaxFailMulti` + `Test-SyntaxMulti`
   （C3 CLI 本来就收多个位置参数：`driver_args.cpp` 逐个 push、`driver_frontend.cpp` 按扩展名定
   模块类型）→ 双文件负例（互指环、基是接口宿主）与双文件正例第一次有了零构建通路。
   运行期正例 `cls_inh_pair`（`tests/cls_inh/Inh.vbp`）两条断言：`INH0:derived`（派生类自己能用）
   + `INH1:OK`（**基类自身的字段/方法发码没被派生类影响** —— 这条是对照组，不是新功能）。
   `run_tests.ps1` 登记为纯插入 +66/-0。条目基线 129 → 138（+8 语法 +1 vbp）。
10. **工具链教训（本轮连踩两次，务必记住）**：**用 bash heredoc / `printf` 写 `.bat` 会被 MSYS 改写**
    —— `>nul` 变成 `>/dev/null`、`\2019` 被当八进制转义、`\v` 变成垂直制表符 → `call vcvarsall` 静默
    失败打印"系统找不到指定的路径"，cmake 随后报 `No CMAKE_CXX_COMPILER could be found`，
    看起来像"VS 被卸了"。**基线构建 bat 一律用 python 以 r-string + CRLF 写**，写完断言
    文件里不含 `/dev/null`。
11. **`tests/run_tests.ps1` 是 UTF-8 带 BOM**（`git show HEAD:… | head -c 3` 实测，至少自 B01 起就是），
    项目记忆里"无 BOM 的 GBK"那条已经过期 → 编辑时**保留 BOM**（二进制读 `[3:]`、写回补
    `\xef\xbb\xbf`），"非 ASCII 字节数不变 + 无裸 LF + numstat 纯插入"三条断言照旧有效。
12. **B07b 待做**（本批刻意不碰，避免把合并与语法混在一个门里）：成员合并与遮蔽裁决（11 张成员表、
    大小写键）、继承字段进派生 struct（扁平复制，不内嵌）、继承方法转发桩、
    `resolveClassMemberCall` 等消费点走链、legacy Implements 覆盖检查吃合并表、
    手册页 `docs/vb6-manual/02-语句/Inherits 语句.md`（"实现状态"节要写真实进度，所以随 B07b 一起落）。
13. **门数字**：`Results: PASS=137 FAIL=0 SKIP=1 TOTAL=138`（`.build/gate_B07a.log`，11:25:47 起跑；exe md5 `031f993b` 跑前后一致 → 可归因）。条目 129→138（+8 语法 +1 vbp），零新增失败；legacy `test_implements` 仍 PASS。

### D26 B07b 开工地图（2026-09-23 B07a 收尾轮产出；行号本轮实测）

- **本轮已就位的东西**：`Module::inherits`（值类型 `vector<InheritsStmt>`）、stage 2.8
  `runClassChainPrepass` → `Driver::classes_`（`ClassChainView{name, mod, clause, baseKey,
  baseText, chain(父先己后的小写键), chainBroken}` + `classOrder_`），诊断 3020/3021/3022。
  **B07b 起 `classes_` 才有消费者**：注入点与发码点都要拿它，记得在 `driver_semantics.cpp`
  注给 analyzer（照 `ifaces_` 的做法）+ `CCodeGen` 侧加 `clsreg_` 指针（照 `ivreg_`）。
- **合并必须落在 stage 3 之后、3.5 之前**（B07a 的 2.8 只有"链"，没有"成员表"）：
  成员表是 `SemanticAnalyzer::analyze` 的类模块块（`semantic_analyzer.cpp:35-162`）在
  `symTab_.define` 之前逐声明填出来的，基类自己的表要等基类那轮 analyze 跑完才存在，而
  `modules_` 顺序不保证基先派后 → 合并做成**新的一轮 3.4**（遍历 `analyzers_`，
  按 `classes_[key].chain` 从根往叶把"父表"并进"子表"）。放在 3.5 之前才有意义的原因：
  `driver_crossmod.cpp:169-190` 是把 Class 符号的 11 张成员表**逐字段手工拷贝**给外部工程，
  合并晚于 3.5 就得再抄一遍外部副本。
- **并表规则**（v1）：键 = `Symbol::toLower(成员名)`；**子优先**（子已占的键父不再占），
  父先己后的顺序只在"两边都没写过"时决定 `memberNames` 的次序（对 legacy 契约检查
  `semantic_analyzer.cpp:236-273` 的扫描序可见，不影响新式接口槽序 —— 那是 `IfaceRegistry` 的事）。
  11 张表都要并：`memberNames` `memberReturnTypes` `memberProcKinds` `memberParams`
  `memberFieldTypes` `memberFieldNames` `publicFieldNames` `memberFieldDispids`
  `memberLetParams` `memberSetParams` `eventNames`（`symbol_table.hpp:116-197`；
  `interfaceMethodParams` 是死字段，别碰）。
- **发码侧要动的三处**（实测锚点）：
  1. 结构体字段：`cgen_base_generate_c_open.inc:84-102` 的字段循环只遍历
     `module.declarations` → 改成"先按链序发祖先的非遮蔽字段，再发本模块的"。
     `cIdent` **保留大小写** → 同名不同拼写要在合并阶段就裁决掉，发码只认胜者，
     否则一个 VB 字段发成两个 C 成员（D24③）。
  2. 转发桩：类过程体的发码点在 `cgen_base_generate_body_pass.inc`（`emitClassFieldAccessors`
     在 :27 被调），桩 = 对"链上祖先的、未被本类遮蔽的每个过程成员"生成
     `vb6_<D>_<M>(vb6_cls_<D>* me, …) { return vb6_<B>_<M>((vb6_cls_<B>*)me, …); }`；
     属性要按 `get_/put_/putref_` 三个方向分别发（`memberLetParams`/`memberSetParams` 已经带方向）。
  3. 成员查找：`resolveClassMemberCall` 的 Fix 014 兜底（`cgen_util_classcall.cpp:161-200`）
     读的就是 `classSym->memberNames` —— **合并进符号表之后这里一行都不用改**，
     `findClassMemberCallParams` / `getClassMethodReturnType` / `canonicalClassMemberName`
     同理（这是把合并做在符号表而不是做在发码层的最大收益）。
- **跨 TU 的可行性实测**（这条把 D24④ 的不安解除了一半）：桩里的
  `(vb6_cls_<B>*)me` 只要 `vb6_cls_<B>` 这个**类型名**可见即可 —— 多模块工程里
  `driver_codegen_module_loop.inc:47-59` 会把**所有**其它模块名塞进 `externalModules`，
  `cgen_base_generate_crossmod.inc:46-50` 于是给每个模块的 `.h` 都 `#include` 其它类头，
  而循环 include 时拿到的是 `cgen_base_generate_epilogue.inc:62-67` 那份
  `typedef struct vb6_cls_B vb6_cls_B;` **不完整类型** —— 指针转换对不完整类型合法，
  **成员访问才非法**。所以桩能编译 —— 最小 `cl` 探针（`typedef struct vb6_cls_B vb6_cls_B;` +
  `(vb6_cls_B*)me` 传给 `int vb6_B_M(vb6_cls_B*, int)`）零诊断通过；但 B07b 开工第一件事仍是
  在**真实管线**里实测这一条（最小工程：两个 .cls + 一条继承方法调用，看 cl 是否报 C2223/C2156）。
  本轮 `--emit-c` 观察（`tests/cls_inh`）：两个 `.h` **互相** `#include`（`InhBase.h` 里先
  `#include "InhDerived.h"` 再发自己的 struct），带 include 守卫时**谁先被包含谁就只拿到对方的
  前向 typedef** → 派生 TU 能否看到基类完整定义取决于包含序，不能只靠这条静态观察下结论。
  若实测发现基类不完整，改法是给派生模块的 `.c` 在头部**先**显式 `#include "<Base>.h"`（只此一条，
  不开泛化包含），或把桩发进基类 TU（代价：跨 TU 的符号可见性与 `_New` 顺序都要重看）。
- **v1 边界（写进 2.8 的 3022 里，别悄悄降级）**：
  ① 基类有 `EventDecl` → 拒绝（事件继承的语义 VB6/tB 都没定，且 `events` 字段位置一错就撞 D19 的偏移 0 不变式）；
  ② 基类或派生类实现**新式 `Interface`** → 拒绝（`__refcount`/`__iv_<I>` 前缀复制规则要与 D21-1 的
     "一个对象一个计数门禁"一起设计；`IfaceRegistry` 在 2.7 已就绪，判定条件现成）；
  ③ 基类是 legacy 接口类（被 `Implements` 当接口的 `.cls`）→ 允许（它的成员都是普通字段/过程）；
  ④ 泛型模板类的特化副本不参与继承（2.8 已拒模板内的 `Inherits`）。
- **前缀布局兼容**：v1 只有 `void* __comObj`（字段 0）+ 用户字段 → 派生类 = 祖先字段序 + 自身新字段，
  `(vb6_cls_<B>*)` 看到的偏移天然一致。②的拒绝把 `__refcount`/`__iv_` 的排布问题整体推到 B08+。
- **用例形态**：`tests/cls_inh/Inh.vbp` 已在门内（`INH0:derived` + `INH1:OK`）。B07b 往
  `InhDerived.cls` 加"经继承来的字段与方法"的断言（`INH2..INHk`），并补两条对照：
  遮蔽时子胜（子类自己写同名成员）、以及 `Set d = New InhDerived` 之后**基类自己的实例**
  仍走自己那份发码（防止桩把两个类的符号混起来）。负例走新加的
  `Test-SyntaxFailMulti`（①②两类都是双文件）。
- **手册**：`docs/vb6-manual/02-语句/Inherits 语句.md` 随 B07b 落（"实现状态"节要写真话），
  README 索引 `:158` 旁按字母序插一行；目录全 CRLF，写完归一并复查 `bare_lf==0`（D18-5）。
  页面上必须写清 `Extends`=接口、`Inherits`=类（018 §二十一自己把 `Inherits` 用作接口继承，
  实现走的是 `Extends`，别照字面回头改）。
- **逐字节护栏**：B07b 动结构体与发码 → 必须跑 8 文件清单（`test_implements.vbp` 已实测确认
  产出 `vb6_cls_CRectangle`/`vb6_cls_IShape`，类布局有覆盖，D25-7）。基线 = pre-B07b 的 worktree exe，
  **bat 用 python 写**（D25-10 的 MSYS `>nul` 坑）。

### D27 B07b（继承成员合并 stage 3.4 + 前缀布局 + 转发桩）实施记录（2026-09-23）

1. **合并落点 = 新 stage 3.4 `Driver::mergeInheritedMembers()`**（`driver_compile.cpp` 插在阶段3 与
   3.5 之间，理由照 D26：`driver_crossmod.cpp:169-190` 把 Class 符号成员表逐字段拷给外部工程副本）。
   消费的是 2.8 的 `chain`（父先己后），回填到 `ClassChainView::inhFields/inhProcs`，发码层只读这两张清单
   —— 这样"结构体字段"与"可调用成员"不可能各按一套规则走。
2. **v1 实际并 8 张表而不是 11 张**（过程面 6：`memberNames` `memberProcKinds` `memberReturnTypes`
   `memberParams` `memberLetParams` `memberSetParams`；字段面 2：`memberFieldNames` `memberFieldTypes`）。
   刻意不并的 3 张各有硬理由：`publicFieldNames` + `memberFieldDispids` 是 Fix 099 的 COM 对外暴露清单，
   并了就会让 `dll_entry` 去找派生 TU 根本不发的字段访问器（**链接期才炸**，比编译期难查）→ 归 P6/B13；
   `eventNames` 无需并（带事件的基类已在 2.8 判死）。字段**不进** `memberNames` 是 Fix 099 的硬规定
   （那张表被用来判定"成员访问是否为属性调用"，并字段会把 `Foo(obj.Field)` 改写成 `prop_get_` 调用）。
3. **同名 Property 的 Get/Let/Set 是"一个键、多个方向节点"**：第一版按成员键去重 `own.procs` →
   `Property Let` 的桩整个丢失（只有 `prop_get_Name` 发出来）。改成过程桶**不去重**、成员表按
   `mergedKeys` 每键只并一次、桩按节点逐方向发。这条只有跑真实工程才会露出来（INH9）。
4. **遮蔽裁决 = 近者（叶方向）优先抢键，落表仍按根→叶**：先"叶→根"扫描抢键（`taken` 集合），再按祖先下标
   `stable_sort` 落表，于是 `memberNames` 的次序是"根先、同层声明序"（对 legacy 契约检查的扫描序可见，
   不影响新式接口槽序）。层内多方向不互相遮蔽（`mine` 集合在整层扫完才并入 `taken`）——第一版在层内就
   `taken.insert` 导致第二个方向被自己挡住。
5. **布局 = 前缀复制，祖先的 `Private` 字段也复制**：派生 struct = `__comObj` + 祖先字段（根→叶）+ 自有字段。
   私有字段虽然对派生类不可见（不进成员表可见面），但**必须占布局**，否则基类实现自己的 `me->m_x` 全错位。
   `_New()` 的字段初始化与 `_Destroy()` 的 BSTR/数组释放同样改走 `structFieldDecls()`（继承在前），
   否则继承来的 String 字段既不初始化也不释放。
6. **转发桩**（`src/backend/module/cgen_inherit.cpp`，新独立编译单元）：
   `vb6_<D>_<M>(vb6_cls_<D>* me, …) { vb6_<B>_<M>((vb6_cls_<B>*)me, …); }`，属性的 C 名带
   `prop_get_/prop_let_/prop_set_` 前缀（与 `makePropertySignature`、`ivImplCName` 同口径，否则链接期找不到）。
   形参表与转调实参的口径直接照抄 `ivParamDecls`/`ivForwardArgs`（含 **Optional 的 `int _has_x` 尾参**
   ——不跟着转发基类的 `IsMissing` 就失真，INH6/7/8 专打这一发）。那两个函数在 `cgen_iface_vtbl.cpp` 的
   匿名 namespace 里、跨 TU 取不到 → 本文件重写了一份 5 行的 `paramsOf`，**没有**为此改 B04 的热点文件。
7. **跨 TU 可见性实测（解除 D24④/D26 的悬念）**：不给派生 `.c` 加任何显式 `#include "<Base>.h"`，
   现有"多模块工程互相 include 类头"就够 —— 桩只需要类型名（指针转换对不完整类型合法），而
   **继承来的 UDT 字段**要的完整类型也在基类 `.h` 里、经那道 include 可见（`_New` 发的
   `memset(&me->m_pt, 0, sizeof(me->m_pt))` 在派生 TU 编译通过 = 实证）。真实证据是运行期的
   `INH3`（`d.SetPt 5` → `d.PtSum() = 15`，走基类实现读写继承来的 `TPoint`）——编译通过不等于布局对。
   如果哪天包含关系变了，INH3 会先炸，这就是它当常驻用例的理由。
8. **本轮最有价值的发现：裸名调用继承成员是**静默错代码**，不是编译错误**。`Bump`（不带 `Me.`）在
   `Option Explicit` 下只给一条 VB3001 警告，`cl` 零诊断、exe 照生、**那段调用直接消失**
   （实测 `INH2:FAIL n=0`）。根因：合并只发生在 Class 符号的成员表上，模块作用域里没有这个过程的
   `Symbol`，发码侧认不出这个名字。已在 `visit(IdentifierExpr)` 的"未找到标识符"分支升格为
   VB3022 错误并要求写 `Me.` *成员名*（`SemanticAnalyzer::declaredByAncestor` 只看 2.8 的链与祖先声明，
   不复用 3.4 的回填 —— 判定发生在语义分析途中，那时合并还没跑）。手册"注意"节同步写明。
9. **v1 边界（2.8 Pass D，全部 3022，一个类只报第一条）**：① 基类带 `Event`（`events` sink 挂在结构体
   尾部，前缀复制不成立）；② 基类**或派生类**实现新式 `Interface`（`__refcount`/`__iv_<I>` 跟着复制 =
   一个对象两份计数，撞 D21-1 单门禁）；③ 基类有同名重载过程（桩要带 `$ov$` 变体后缀，与 B08 一起去虚化
   时一起设计）；④ 派生类重声明祖先同名字段（C 结构体容不下两个同名成员）。判据一律只看 AST。
   第一版 `hasOverloadedProcs` 把"同名 Property 的 Get+Let 两个节点"误判成重载，整条链被判死 ——
   与第 3 条同一个坑，两处都得按"成员键"算。
10. **用例与门**：`tests/cls_inh` 从 2 类扩成 3 类链（`InhBase ← InhMid ← InhDerived`），断言 2→12
    （INH0/1 原有；INH2 继承 Sub/Function+私有 Long；INH3 继承 UDT 字段；INH4/5 子胜遮蔽且基类实例不受影响；
    INH6/7/8 Optional 转发；INH9 属性 Get/Let 双向；INH10 中间层成员与祖父成员并存；INH11 实例隔离）；
    新增 3 条双文件负例 `ci_n08_bare_inherited_call` / `ci_n09_redeclared_field` / `ci_n10_event_base`，
    走 B07a 刚建的 `Test-SyntaxFailMulti`（`--syntax-only` 通路，无需 vbp）。`run_tests.ps1` 登记 +16/-1
    （那 1 行删除是把 `Test-Vbp` 的断言列表改成多行）。B07b 后语法类 65/0/0。
11. **工具链新坑（本轮实测）**：`scripts/build.bat` 是 **LF-only + GBK 注释**，从 agent 的 bash 里
    `cmd //c scripts\build.bat` 会被 cmd 在注释的 `）`/`(` 字节上截断 `if (...)` 块，产出一堆
    "'_BIN' 不是内部或外部命令" 式噪声、cmake 那侧看似 VS 丢失。**构建一律用
    `powershell -NoProfile -File scripts/dev.ps1 -SkipTest`**（它打 "All done!"，与既有门一致）。
12. **护栏**：8 文件 `--emit-c` 对 pre-B07b 基线（worktree @`a056705`，自建 Debug exe）**8/8 逐字节全同**；
    基线 worktree 用后即 `git worktree remove --force` + `prune`。
13. **门数字**：`Results: PASS=140 FAIL=0 SKIP=1 TOTAL=141`（`.build/gate_B07b.log`，12:47:44 起跑、13:1x 收；exe md5 `50ce2e77` 跑前后一致 → 可归因）。条目 138→141（三条 v1 边界负例），零新增失败；语法类单跑 65/0/0；legacy `test_implements` 与 `itf_xmod_writer`（25 断言）仍全绿。
14. **B08 的既有事实（本轮顺手实测）**：`AccessLevel` 只有 `Public/Private/Friend`（`types.hpp:54-59`）、
    全仓**没有任何"成员不可访问"的诊断**（`Protected`/`Overridable`/`Overrides` 三个词在 `src/` 里只出现在
    错误处理块的 `inProtectedBlock_`，与继承无关）→ B08 的可见性是**从零加检查 + 新诊断**，不是改口径。

### D28 B08 开工地图（2026-09-23 B07b 收尾轮产出；锚点本轮实测）

- **先拆批**（判据仍是 D20-1"每批都要有可观测行为"）：
  - **B08a = `Protected` 可见性**（语法 + 访问权限检查 + 新诊断）。可观测性天然成立：今天
    `Protected` 这个关键字**根本不存在**，写了就在 parse 期撞 VB2002，加了就能解析 + 报"不可访问"。
  - **B08b = `Overridable`/`Overrides`/`NotOverridable` + 类虚表 + 动态派发**。它要改结构体
    （多一个 `__cvtbl` 字段）与 `Me.` 调用点，必须与 B08a 分开过门、分开跑 8 文件护栏。
- **B08a 的四处接线 = 逐处镜像 `Friend`**（本轮实测锚点）：`src/lexer/lexer_keywords.cpp:31`
  （`{"friend", TokenKind::Friend}` 旁）、`src/lexer/token.hpp` 枚举、`src/lexer/token.cpp:25`
  （`isKeyword` 显式列表）、`src/parser/parser.cpp:288` 与 `src/parser/parser_decl.cpp:28-39`
  （访问修饰符 parse 分支）。`AccessLevel` 在 `src/common/types.hpp:54-59`，只有
  `Public=0/Private=1/Friend=2/Default=Public` → 新值取 `Protected = 3`（**别插在中间**，
  `Default = Public` 的别名与任何按数值的比较都会被插队影响）。
- **两个零成本的前车之鉴**（B07a/B07b 各踩一次）：① 新关键字**同时**进
  `src/parser/parser_helpers.cpp` 的软关键字表（`isSoftKeyword`/`canBeName`），并且**实测**全仓 VB 语料
  里该词出现次数 —— `inherits` 与 `protected` 本轮实测都是 **0 次**，`overridable`/`overrides` 待测；
  ② `isStatementStart()` 刻意不含这些词（D12"语句位永不识别"），别顺手加。
- **可见性检查是从零加、不是改口径**（D27-14）：全仓没有任何"成员不可访问"的诊断，`Private` 的
  "不可见"目前只体现为**跨模块注入时不收进去**（`driver_crossmod.cpp` 的 Public 过滤）。所以 B08a 要新加
  诊断 ID（`diagnostics.hpp` 现用最大 3022 → `SemMemberNotAccessible = 3023`、
  `SemOverridesMismatch = 3024` 预留），检查点候选：`resolveClassMemberCall`（成员访问）+
  字段访问的规范化点（`canonicalClassMemberName` 一族），两处都要能看到"访问者所在模块 vs 成员所属类"。
  `Protected` 的判定口径 = 当前模块是基类本身、或**在继承链上**（读 `Driver::classes_[key].chain`）→
  这也是 3.4 之外第二个消费 `chain` 的地方，注入路线照 `analyzer->setClassChainRegistry(&classes_)`
  （`driver_semantics.cpp:44`）。
- **B08b 的挂钩点早就备好了**：B07b 的转发桩（`cgen_inherit.cpp`）是"派生类调用祖先实现"的唯一出口，
  去虚化 = 让 `Overrides` 的桩改指本类实现、动态派发 = 让**基类体内**对 `Me.M` 的调用改走 `__cvtbl` 槽。
  要动的三处：① 结构体加 `__cvtbl`（**位置硬约束**：字段 0 仍是 `__comObj`（D19），`__iv_<I>` 紧跟其后
  是 B04 定死的 → 新指针只能排在 `emitIfaceClassFields` 之后，且**只给链上含 `Overridable` 的类**加
  —— 与 `__refcount` 同一套"按 feature 加字段"的保护栏手法，见 D21）；② `resolveClassMemberCall`
  （`cgen_util_classcall.cpp`）读 Class 符号判虚；③ 虚表实例按类一份、函数指针槽序稳定
  （根→叶、同层声明序 —— 就是 3.4 落表顺序，别另起一套）。
- **`Overrides` 的签名比对用源码签名口径**（与 D16 同一理由：跨模块 Enum/UDT 在 stage 3 早期尚未注入，
  归一成 `Vb6Type` 再比会误判）→ 直接复用 `src/semantics/interface_sig.hpp` 的 `ifaceSigFromDecl`，
  那里是签名文本的唯一出处。
- **继承与新式接口的互斥要重新审视**：B07b 在 2.8 Pass D 判死了"任一侧实现新式 Interface"（D27-9②），
  若 B08b 的虚表与 `__iv_<I>` 要在同一个类上共存，`__refcount`/`__cvtbl`/`__iv_` 三者的定序要一次定清
  （写进 D29，别在实现中途改）。
- **门与护栏**：B08a 不动发码 → 8 文件护栏可省（按 D18-6 的口径记录理由 + 用真实工程编译替代）；
  B08b 动结构体与派发 → **必须**跑，基线 exe 用 worktree（bat 用 python 以 r-string + CRLF 写，D25-10；
  构建本身用 `powershell -File scripts/dev.ps1 -SkipTest`，D27-11）。
- **用例形态**：B08a = `tests/cls_neg/ci_n11_protected_*`（跨模块访问 Protected → 3023）+
  `ci_pos_protected`（链内访问放行）；B08b = `tests/cls_inh` 再加一层（`InhBase` 的 `Overridable Sub Speak`、
  `InhDerived` `Overrides Speak`）+ 断言"基类指针调 `Speak` 也走派生实现"—— 这条是唯一能证明真虚派发的形状。

### D29 B08a（`Protected` 访问级别 + 家族内可见）实施记录（2026-09-23）

1. **拆批又拆了一层**：D28 原本把 B08a 定为"`Protected` 可见性"，做完才发现"可见性"有两半 ——
   "家族内可用"（本批交付）与"家族外越权要拒绝"（记为 **B08c**）。后者今天做不了：语义层
   `visit(MemberAccessExpr)` 根本不把 `obj` 解析成 Class（`semantic_analyzer_expr.cpp:141` 一路落到
   `lastExprType_ = Variant`），要拒就得给分析器补一套 obj→Class 解析；而"偷懒的做法"（在
   `driver_crossmod.cpp` 的逐字段拷贝里按访问级别过滤成员表）会让越权访问**退化成晚绑定 COM 调用**
   —— 从"太宽松"变成"运行期才炸"，比静默更糟。所以本批只做能诚实交付的那一半，边界写进手册。
2. **接线 = 逐处镜像 `Friend`，一共 13 处**（其中三处是静默陷阱，不写下来一定会踩）：
   `types.hpp` 枚举、`token.hpp` 枚举、`lexer_keywords.cpp`、`token.cpp::isKeyword`、
   `parser_helpers.cpp` 软关键字表、`parser.cpp::isDeclarationStart`、`parser_decl.cpp` 修饰符分派、
   `parser_decl_var.cpp` 行首修饰符消费、`parser_interface.cpp` 接口成员修饰符拒绝、
   `symbol_table.hpp` 新表、`semantic_analyzer.cpp` 填表、`driver_crossmod.cpp` 拷贝、
   `driver_classchain.cpp` 合并 + `cgen_base_generate_decl_pass.inc` 的 .h static 判定 + `accessStr` dump。
   - **陷阱①**：`parser_decl.cpp:30-37` 是 `case` 列表 + if/else，**`else` 兜底是 Private**。
     加了 `case TokenKind::Protected` 而忘加 if 分支 → `Protected Sub X` 静默变成 Private（不报错、行为相反）。
     同样形状的还有 `parseAccessDeclInBody`（过程体内 `Public/Private x As Long`，本批刻意**不**支持体内 Protected）。
   - **陷阱②**：`isDeclarationStart`/`isStatementStart` 的 `default: return false` → 漏加就是 parse 错误。
     注意 `isStatementStart` 里连 `Friend` 都没有，所以 Protected 也**不进**（过程体内它不是语句起点）。
   - **陷阱③**：`.h` 声明的 static 判定枚举的是 `Public || Friend`，而 `.c` 定义的判定是 `== Private`。
     只加访问级别不改 `.h` 那三处，就会得到"声明 `static`、定义非 static"的对不上 ——
     Protected 必须与 Public/Friend 同等（派生模块 TU 的转发桩要能看见它）。`.c` 侧本来就对（非 Private 即非 static）。
3. **新成员表 `Symbol::memberAccessLevels`（键小写 → AccessLevel）只有 3 个 touch point**：
   声明 + 填表 + 那两处拷贝（crossmod 与继承合并）。之所以这么少，是实测确认**全仓没有 Symbol 的
   序列化/反射**（`getPublicSymbols` 只看符号自己的 `access`，不看成员表）。填表按既有口径：
   属性只在"读上下文胜出"时写级别（与 `memberProcKinds` 同一处 `if (wins)`）；事件也记。
4. **Protected 的真正语义落点 = stage 3.4 的可见面过滤**：那张过滤今天只挡 `Private`
   （`driver_classchain.cpp:401`），于是 Protected 的基类成员**自动**被派生类继承并可经 `Me.` 使用，
   一行都不用改；而它不进 `publicFieldNames`（填表处只认 `AccessLevel::Public`，Fix 099）→
   天然不进 COM 对外暴露清单与 TypeLib 表（`driver_codegen_dll_typelib.inc` 判 `== Public`）✓ 这两处
   "什么都不做"正是对的，写下来免得后人以为漏了。
5. **可观测性（D20-1）**：`Protected` 今天根本不存在（写了撞 parse 错误）→ 本批的可观测面是三件事：
   家族内可用（`INH12` 经 `Me.SetSecret/Me.Secret` 用基类 Protected 方法、`INH13` 用基类 Protected 字段）、
   `.h` 非 static 带来的跨 TU 链接（`INH12` 能跑出来就是它通了）、以及 `Interface` 块内仍被拒（VB2011，
   `ci_n11_protected_in_interface`）。门 `Results: PASS=141 FAIL=0 SKIP=1 TOTAL=142`；语法类 66/0/0；
   **护栏 8/8 逐字节全同**（基线 = worktree @`b1c0050`，自建 Debug exe）—— 本批动了发码判定，按 D9 必须跑。
6. **语料实测**：`protected` / `overridable` / `overrides` / `mustinherit` / `notoverridable` 五个词在
   `tests/**` 的 `.bas/.cls/.frm/.ctl/.pag` 里出现 **0 次** → 关键字化对存量输入零影响；照 D25 的口径
   仍然一并进了软关键字表（`canBeName` 仍认它）。
7. **委托勘察的复核教训**：派 Explore 摸"访问级别都在哪儿被消费"是对的（它给出的
   `getPublicSymbols`/TypeLib/static 三组分类直接用上了），但它的两条锚点不准 ——
   `token.cpp:97-98` 其实是 `isStatementStart`（不是 token 名转字符串表，那张表不存在），
   `driver_crossmod.cpp` 的拷贝写的是 `extSym->memberSetParams = srcSym->...`（不是它报的 `dst->/src->`）。
   **动手前逐条 grep 复核锚点**只花几分钟，而错锚点在带 `assert count==1` 的打补丁脚本里会直接 no-op。
8. **B08b 待做**（本批刻意不碰）：`Overridable/Overrides` + 类虚表 + 动态派发（地图 D30）；
   B08c 待做：家族外越权访问 `Protected` 的拒绝（要先给分析器补 obj→Class 解析）。

### D30 B08b 开工地图（2026-09-23 B08a 收尾轮产出；锚点本轮实测）

- **本批要解决的唯一硬问题**：今天 `Me.M` 在**基类体内**是静态绑定（`resolveClassMemberCall` 拿到
  当前类符号 → 直接发 `vb6_<Base>_M`），所以"派生类覆盖了 `M`、基类的 `Talk` 里调 `M` 仍走派生实现"
  这条真虚派发**必须**有一个运行期入口。方案：给需要虚表的类发一份 `vb6_cvtbl_<Cls>` +
  struct 里一个 `__cvtbl` 字段，基类体内对可覆盖成员的调用改走槽位。
- **字段位置是硬约束**（D19 + D21 + B07b）：字段 0 恒为 `__comObj`，`__iv_<I>` 紧跟其后是 B04 定死的，
  继承前缀布局又要求"祖先字段先于自有字段" → `__cvtbl` 只能排在 `emitIfaceClassFields` 之后、
  用户字段之前，且**只给链上含 `Overridable` 的类加**（照 `__refcount` 的按 feature 加字段手法，
  这样无新语法的工程逐字节不变）。定序一次写死：`__comObj` → `__iv_<I>` → `__cvtbl` → 祖先字段 → 自有字段。
- **虚表槽序 = 3.4 的落表序**（根→叶、同层声明序、每键一份），别另起一套计数；否则同一个基类在
  不同派生类下的槽号会漂移。**新增一张派生表**存"哪些键是虚的"：`Symbol::memberVirtual`
  （小写键 → 槽号 + 是否 Overridable/MustOverride），touch point 与 D29-3 的 `memberAccessLevels` 完全一样
  （声明 + 填表 + `driver_crossmod.cpp:188` 那块 + `driver_classchain.cpp` 的 `if (firstTime && src)` 块）。
- **AST 侧要加标志位**：`SubDecl/FunctionDecl/PropertyDecl` 各加 `bool overridable/overrides/mustOverride`
  （现成先例：`PropertyDecl::propKind` 与 `implementsClauses`）。parse 顺序是
  `[访问修饰符] [Overridable|Overrides|NotOverridable] Sub|Function|Property` —— 修饰符在
  `parser_decl.cpp` 消费后要**再看一眼**第二个修饰符位，漏了就是 VB2002。
  v1 边界照 B07b 的口径写进 2.8（`SemInheritsNotSupported`）：泛型模板内、非类模块内出现这些词 = 拒。
- **`Overrides` 的校验**：基类同名键存在、且基类那份是 `Overridable`（或 `MustOverride` 时派生必须写）、
  签名一致 → 用 `interface_sig.hpp::ifaceSigFromDecl` 的**源码签名**口径（D16 的理由：跨模块 Enum/UDT 在
  stage 3 早期未注入，归一成 `Vb6Type` 会误判）。新诊断从 **3024** 起（3023 预留给 B08c 的越权拒绝）。
- **派发点只有两处**（其余调用形状继续沿用 B07b 的桩）：① 类体内 `Me.M` / 裸名 `M`（后者今天被 D27-8
  拒了，正好少一处）；② 入口函数本身 —— 建议 `vb6_<Cls>_M` 保持"具体实现的直调"，虚派发放到
  "**基类体内对可覆盖成员的调用**"这一处，别让每个外部调用点都查表（那会把 B09 的 `MyBase` 去虚化
  逼成另一套命名）。
- **用例形态（这是唯一能证明"真虚"的形状）**：`InhBase` 加
  `Public Overridable Function Speak() As String`（返回 "base"）+ `Public Function Talk() As String`
  （`Talk = Speak()`，走 `Me.`）；`InhDerived` 写 `Public Overrides Function Speak()`（"derived"）。
  断言：`d.Talk() = "derived"`（虚）而 `b.Talk() = "base"`（基类实例不受影响）、
  `d.Speak() = "derived"`、`b.Speak() = "base"`。负例：派生类 `Overrides` 一个基类没有的成员、
  以及基类成员没标 `Overridable` 却被 `Overrides` → 各一条 3024（双文件，走 `Test-SyntaxFailMulti`）。
- **与既有边界的冲突要先解**：2.8 Pass D 目前判死"基类/派生类实现新式 Interface"与"基类有重载成员"。
  B08b 若想让 `Overridable` 与新式接口共存，得先把 `__iv_<I>` 的计数/槽语义和虚表对齐（那是 B08+/P6 的活），
  v1 建议**保持这两条拒绝不变**，只在纯具体类链上做虚派发。
- **护栏**：本批改结构体 + 派发 → 8 文件 `--emit-c` 必须全同（基线 = pre-B08b worktree，exe 自建；
  bat 用 python r-string + CRLF 写，构建本身用 `dev.ps1 -SkipTest`，见 D25-10 / D27-11）。

### D31 B08b（`Overridable`/`Overrides`/`NotOverridable` 语法 + 覆盖契约 + 封掉假虚派发）实施记录（2026-09-23）

1. **拆批拆到第三层**：D30 把 B08b 定成"修饰符 + 类虚表 + 动态派发"，本轮交付的是**能诚实交付的那半**
   （语法 + 覆盖契约校验），类虚表/派发独立成 **B08d**（地图 D32）。判据不是工作量而是 D27-13 那条判例：
   只上语法时，"基类体内 `Me.M`"会**编得过、跑出基类实现**（静态绑定），后代的 `Overrides` 静默失效 ——
   "看着像虚调用其实不是"比编译失败危险得多。所以本批的交付里**必须包含把这条路判死**，
   而不是"先收语法、以后再修"。
2. **修饰位建模成四值枚举 `ProcVirt{None,Overridable,Overrides,NotOverridable}`，不是三个 bool**：
   三件套互斥，写成三个字段就有 6 种非法组合要在 parse 与语义两层各防一遍。`None` 与
   `NotOverridable` 运行语义相同但**不合并**：后者是显式意图，"覆盖一个 `NotOverridable` 成员"要报得准。
3. **AST 侧零签名改动**：三个过程 decl 用 `ProcVirt virt = ProcVirt::None;` 成员默认值 + parse 后赋值
   （与 `typeParams`/`implementsClauses` 同一手法），于是 `parser_interface.cpp`（接口成员也 new 这三种
   节点）与 `ast_clone.cpp` 的构造点**一个都不用改**。`ast_clone` 刻意**不拷** `virt` —— 泛型模板内的虚
   修饰符已被 E0 拒绝，特化副本永远不会带它（同 B07 `Inherits` 的取舍）。
4. **parse 的修饰符要"吃两次"**：VB6 里访问修饰符可省（`Overridable Sub X`），所以起手先吃一轮，
   进了 `Public/Private/…` 分支、消费访问修饰符**之后**再吃一轮（`Public Overridable Sub X`）。
   `isDeclarationStart` 三个 case 必加（漏了就是 parse 错误），`isStatementStart` **不加**（过程体内它
   不是语句起点，与 `Friend` 同口径 —— D29-2 陷阱②的重复确认）。这次没有再踩"else 兜底 Private"，
   因为新枚举是三态显式分派。
5. **契约检查落在 2.8 新增 `Driver::runVirtualContractChecks`，不是 3.4**：它要回答"祖先**自己声明**过
   这个槽吗、那份带没带 `Overridable`"。放 3.4 之后 Class 符号的成员表已被合并污染（含祖先条目与遮蔽
   结果），"谁声明的"就查不回来；而 2.8 已解好 `chain`，直接读各模块 decl 就够。属性按 `ifaceSlotKey`
   的**方向**配对（`get_/put_/putref_`），签名比对复用 `ifaceSigFromDecl`/`ifaceSigEqual` —— 与 Interface
   契约检查同一套函数，不会出现两套判据漂移（D16 的源码签名口径同理由）。
6. **`dynamicKeys` 存成员名小写、不存槽键**：语义层判定发生在 `visit(MemberAccessExpr)`，那里只有
   `memberName`、拿不到"这次访问用的是哪个方向"（`Me.X` 可能是 Get 也可能是 Let/Set）。用名字做键
   = 属性三向一起判，方向偏保守（可能多拒），但**不会漏**——漏就是静默绑错。B08d 发虚表时要另起一张
   按槽键的表，别复用这张（D32-2）。
7. **拒绝点在语义层，一共三处**：`visit(IdentifierExpr)` 的"查到符号"分支（裸名值引用/隐式调用）、
   `visit(IndexOrCallExpr)` 的**裸 callee `if (sym)` 分支**（它自己 `symTab_.lookup`、**不经过**
   `visit(IdentifierExpr)` → 少埋这一处就漏掉 `Speak(5)` 这种带实参调用），以及
   `visit(MemberAccessExpr)` 的 `Me.X` 形状（`Me.X(…)` 经 `analyzeExpr(callee)` 落在这里，一处覆盖两形态）。
   选语义层而不是发码层的理由是**可测性**：`--syntax-only` 就能断言（见 [[c3-build-test-hazards]] 的
   "`--syntax-only` 不是 parse-only"），不必为编译期诊断去搭 `.vbp` 负例工程。
8. **本轮唯一"差点误杀正例"的点**：`Speak = "base"` 是 VB6 的**返回值赋值**，标识符恰好就是函数名，
   与"类体内调用 `Speak`"在 `visit(IdentifierExpr)` 里长得一模一样 → 最早的写法把 `Overridable` 成员
   **自己的函数体**判死了（第一次编译正例就报 VB3027）。修法是豁免 `currentProc_` 同名的裸名引用。
   记在这里是因为它不报错、只是"最正面的用法突然不能用"，很容易被当成边界凑合过去。
9. **早退条件扩成 `anyClause || anyVirtual`，但 E0（位置合法性）只读 `modules_`、不建登记表**：若为了 E0
   也去建链登记表，"写了 `Overridable` 但一条 `Inherits` 都没有"的工程会第一次拿到非空 `classes_`，
   后端 `classChainOf()` 从 nullptr 变成有值 —— 那是给存量工程开了一条从没走过的路径，正是要避免的
   "顺带改了行为"。现在：没有 `Inherits` 就在 `checkVirtualPlacement()` 之后直接返回。
   8 文件 `--emit-c` 逐字节全同（@`2117d1c` 基线）就是这条的证据。
10. **工具链一条**：基线构建别再试 `scripts/dev.ps1`（worktree 里它只 `cmake --build`、不 configure →
    `is not a directory`），也不用回退到手写 bat —— `.build\base_build.ps1`（vcvars + cmake -B + --target c3，
    纯 ASCII）一次通过，比 D25-10/D29 那套"python 写 bat 防 MSYS 改写"省事，因为 PowerShell 不经 MSYS。
11. **可观测面（D20-1）**：正例 `INH14`（派生对象上直调被覆盖成员 = 派生实现）、`INH15`（基类实例不受
    影响）、`INH16`（`NotOverridable` 成员仍按继承走转发桩）；负例 7 条 = 契约 4 条（`ci_n12` 链上无目标
    3024 / `ci_n13` 目标未标 Overridable 3025 / `ci_n14` 签名不符 3026 / `ci_n15` 类体内调用被覆盖成员 3027）
    + 位置 3 条（`ci_n16` 无 `Inherits` 却写 `Overrides`、`ci_n17` 标准模块里写 `Overridable`、
    `ci_n18` `Interface` 块里写虚修饰符）。诊断 ID 按 D30 的预约从 **3024** 起，**3023 仍留给 B08c**。
    门 `Results: PASS=148 FAIL=0 SKIP=1 TOTAL=149`；`-Category syntax` 66→73；`Inh.vbp` 断言 14→17 条。

### D32 B08d 开工地图（类虚表 `vb6_cvtbl_<Cls>` + 动态派发；2026-09-23 B08b 收尾轮产出，锚点本轮实测）

- **本批唯一的硬问题**：把 B08b 用 `SemVirtualNotSupported`（VB3027）判死的"类体内调用被后代覆盖的
  成员"翻成真的按实例类型派发。三处拒绝点都在 `semantic_analyzer_expr.cpp`（搜 `vtblNeededMsg`），
  B08d 的正是要**删掉这三处判定**并让发码走槽位 —— 别留着判定再"另外"发虚表，那会变成永远到不了的码。
- **`dynamicKeys` 换成"槽表"**：B08b 存的是**成员名小写**（分析器拿不到属性方向，见 D31-5），发码需要的是
  `ifaceSlotKey` 级的**有序槽清单 + 每槽的实现选择**。所以在 `ClassChainView` 上另加一张
  `std::vector<VirtSlot>{ slotKey, nameKey, const Decl* 声明处, Module* owner }`（根→叶、每槽键一份，
  顺序就是 3.4 的落表序，别另起计数），`dynamicKeys` 保留做分析器侧的快路径（或改读新表）。
- **字段定序一次写死**（D19 偏移 0 + B04 的 `__iv_` + B07b 前缀布局）：
  `__comObj` → `__iv_<I>…` → `__cvtbl` → 祖先字段 → 自有字段。**只给链上任一类带过 `Overridable`/
  `Overrides` 的类加**（照 `__refcount` 的按 feature 加字段手法），否则 8 文件 `--emit-c` 护栏必挂。
  实现位置：`cgen_base_generate_c_open.inc` 的字段循环之前，与 `emitIfaceClassFields` 同一层。
- **类型要跨 TU 可见，表实例不用**（本条是读完 `cgen_inherit.cpp` 后改正的版本，原来写的
  "表与表项都必须非 static"是错的）：每个具体类只需要**一张**表实例，装它的是自己 TU 里的 `_New`
  → `static const vb6_cvtbl_<D> vb6_cvtbl_<D>_impl` 发在 `<D>.c` 就够；但**类型**
  `vb6_cvtbl_<X>` 与函数指针别名 `vb6_pfn_<X>_<slot>` 必须发在 `<X>.h`，因为祖先体内那句
  `Me.M()` 的发码在祖先 TU 里，它要按**自己那张视图**（`vb6_cvtbl_<祖先>`）去索引派生对象装进去的表。
- **多视图靠"字段类型 `void*` + 用点强转"统一**（这是 B08d 的关键设计，别改成 typed 字段）：
  A 声明 `Overridable` → 它的视图是 `{speak}`；B 又新声明一个 `Overridable` → B 的视图是 `{speak, extra}`。
  前缀布局要求 B 的 `__cvtbl` 与 A 的是**同一个字段**（同偏移），类型却不同 → 字段只能发成
  `void* __cvtbl;`，用点写成 `((const vb6_cvtbl_<本类>*)me->__cvtbl)-><槽字段>((vb6_cls_<本类>*)me, …)`。
  槽序定死为"链上虚槽、根→叶、每槽键一份"，于是**任何祖先视图都是派生表的前缀**，强转才成立。
- **装载点**：`vb6_cls_<D>_New`（`cgen_com.cpp` 的 `_New`/`_Destroy` 已经在 B07b 走过同一张
  `structFieldDecls`）→ `me->__cvtbl = (void*)&vb6_cvtbl_<D>_impl;`。**每个具体类只装自己那一张表**
  （同一祖先链、不同覆盖集 = 不同表实例），类型用本类视图 `vb6_cvtbl_<D>`、实例名 `vb6_cvtbl_<D>_impl`
  —— 定死这一对命名，`.h` 的类型、`.c` 的实例与 `_New` 的装载三处必须一致。
- **`Overrides` 的契约检查一行都不用改**（2.8 `runVirtualContractChecks`），它只保证"目标存在且可覆盖、
  签名一致"，与派发机制无关；批完后把 `dynamicKeys` 的**语义**从"要拒绝"改成"要发槽"即可。
- **用例翻转（这是本批的验收证据）**：`ci_n15_base.cls` 的 `Talk = Me.Pick()` 形状搬进
  `tests/cls_inh/InhBase.cls`，断言改成 `d.Talk() = "derived"`、`b.Talk() = "base"`、
  `m.Talk() = "mid"`（若 `InhMid` 也覆盖）；三级链里"中间类覆盖 + 叶类不覆盖"必须单独有一条断言，
  它才是"祖先体内调用绑到最近覆盖者"的证据。`ci_n15_*` 从负例清单里删掉（别留成 skip）。
- **与既有边界的冲突**：Pass D 仍判死"任一侧实现新式 Interface"与"基类有重载" → v1 的虚表只在纯具体类链上
  工作；`__iv_<I>` 与 `__cvtbl` 共存要等 P6 一起设计（`Inherits` 一个实现了接口的类目前根本进不来）。
  `MyBase.M`（B09）的**去虚化**正好依赖本批的表结构（`MyBase.M` = 直调 `vb6_<Base>_M`，不查表），
  所以表项别顺手做成"只能查表"的形态。
- **要复用的取名机器（`cgen_inherit.cpp` 实测，别另写一套）**：C 符号名 =
  `cProcName(procBaseName(decl), accessOf(decl), moduleName)`，属性名自带 `prop_get_/prop_let_/prop_set_` 前缀
  （与 `makePropertySignature`/`resolveClassMemberCall` 同源）；形参表 = `classMeParam()` + 逐个
  `makeParamCType(p, false)` + Optional 的 `int _has_<x>` 尾参；返回类型 = `inheritedRetType(decl)`。
  B08d 的函数指针别名与表项**必须**用这四件套拼，否则要么签名不一致、要么链接期找不到定义。
  填表时把派生实现强转成祖先视图的函数指针类型（前缀布局保证 ABI 一致），
  或者复用 B07b 已经发出来的转发桩做 entry —— 后者更稳，桩的签名天生就是"派生类的 me + 祖先的实现"。
- **护栏**：改结构体 + 改派发 → 8 文件 `--emit-c` 必须全同（基线 = pre-B08d worktree，自建 Debug exe）。
  本轮 B08b 用的 `base_build.ps1` + `byteguard_b08b.py` 已在 `.build\` 里，改两个路径就能复用
  （dev.ps1 在 worktree 里跑不通：它只 `cmake --build`、不 configure，见 D31-10）。

### D33 B08d（类虚表 `vb6_cvtbl_<Cls>` + 运行期动态派发）实施记录（2026-09-23）

1. **槽表落在 3.4b 而不是 2.8**（`Driver::buildVirtualSlotTables`，紧跟 `mergeInheritedMembers`）：
   判"这个槽在**本类面上**有没有可调用入口"要读 3.4 回填的 `inhProcs`（转发桩清单），在 2.8 判会漏掉
   遮蔽。诊断因此也是语义层的（`--syntax-only` 断言得到，`ci_n19`/`ci_n20` 就是这么测的）。
2. **筛选集取链根的 `dynamicKeys`，不是本类的**（D32② 的修正）：`virtSlots(X)` = 链根→X 之间
   **首次声明**为 `Overridable` 的槽 ∩ `dynamicKeys(chain[0])`。取根那份是关键 —— 叶类自己的
   `dynamicKeys` 恒为空（它没有后代），但**它必须带 `__cvtbl` 字段**，否则经基类型变量调 `x.Pick()`
   时基类视图会去读一个不存在的字段（= 别人第一个数据成员的地址，野指针）。前缀性质由"首次声明位置"
   定序保证，而链根集合对链上所有类相同 → 任何祖先的表都是后代表的前缀。另外**链根自己也要建表**：
   它的 `chain` 只有一个元素，照 `chain.size() < 2` 早退就会漏掉根（第一轮实测 `b.Speak()` 静默绑回
   基类实现，就是这个原因）。
3. **表类型只需本类 TU 可见，但必须发在跨模块 include 之后**（D32③ 的修正）：调用点用的永远是
   *接收者静态类型自己那张视图*，所以 `vb6_cvtbl_X` 发在 `X.h` 就够 —— 不过 `InhMain.c` 这类消费者
   TU 会 include `InhDerived.h`，视图类型必须在那批 include **之后**才落地，否则 C2440/C2100
   （`(const vb6_cvtbl_InhDerived)` 被当成非类型名）。定在类 struct 定义之后、`emitInterfaceVtable`
   之前；表实例 `extern const` 同处声明，定义在 `.c` 末尾。
4. **字段类型 `const void*`**（D32② 写的是 `void*`）：装载点写 `me->__cvtbl = &vb6_cvtbl_X_impl;`
   不带强转 → 不会出 discard-const 警告。表项的 me 形参一律用**本类**的 `vb6_cls_<X>*`（祖先视图与
   派生视图各自自足，两个视图之间从不转换指针）→ 填表、调用点都不需要强转，
   `vb6_cvtbl_A*`↔`vb6_cvtbl_B*` 这种不兼容转换在整个工程里根本不出现。
5. **表项 = 本类面上的入口声明**（`cvtblImplName` = `cProcName(procBaseName(impl), accessOf(impl),
   本类模块名)`）：impl 要么是本类自己的声明，要么是 B07b 判定要发桩的那份祖先声明 —— 两者的 C 名与
   签名由同一套取名机器拼，所以填表零强转、链接期不可能找不到定义。Private 过程在 `.c` 里是 `static`
   且**没有前置原型** → 表实例必须后置到 epilogue（与 `emitDelegateThunks()` 同一层）。
6. **`Me.X` 的发码不在"优先级2"那一支**：那一支（`cgen_expr_member_class_module.inc`）整块在
   `node.object` 是 `IdentifierExpr` 的分支里，而 `Me` 是 **MeExpr** → 真正发码的是
   `cgen_expr_member_class_fallback.inc`（它按 emit 出来的对象文本 `"me"` 查 `knownClassVars_`）。
   只改前者会得到"外部落点对了、类体内仍直调"的**假成功**（本轮第一轮就是这个形状：INH18/19/20/22
   全 FAIL 且全部返回 base，而 `InhMain.bas` 里的 `b.Speak()`/`d.Speak()` 已经是对的 —— 这种"半对"
   最容易误判为通过）。两处都得改写，且 fallback 那处必须派发。
7. **接收者必须是纯读表达式**（间接调用要把它的文本用两次：取表一次、me 实参一次）。判据取"文本里
   不存在紧跟标识符/`)`/`]` 的 `(`" —— 强转的 `(` 前面是运算符，所以 `(*c)`、`((vb6_cls_X*)b)`、
   `me->m_oSocket` 都算纯读；`vb6_cls_X_Default()`、`vb6_X_prop_get_pvSocket(me)` 不算。带调用的接收者
   里**默认实例那一路** `mustDispatch=false` 保持直调（它的动态类型恒等于静态类型，直调本来就正确）；
   其余用点**报错**，不退成静默直调（D27-13 判例）。
8. **两条新的 v1 判死**（都用 `SemVirtualNotSupported`＝VB3027，负例可断言）：`Overridable`/`Overrides`
   落在 `Property Let`/`Property Set` 上（写上下文发码在 `tryRewriteCOMLvalue` 那条路，本批不碰）；
   某槽在**本类面上没有入口**（祖先那份是 Private，或被另一个方向的同名成员遮蔽 → 3.4 不发桩），
   后者若放过就是表项指向一个不存在的函数。裸名（`Talk = Pick()`）仍按 B08b 的形状判死：那条路吃的是
   模块作用域的过程符号，与 B07b"继承成员必须写 `Me.`"同一条边界，文案已改成陈述这件事而非"虚表还没发"。
9. **验收形状**：`Inh.vbp` 的运行期断言 17→27 条（新增 INH17..INH26）。INH19（`d.PickThru()="mid"`：中间类覆盖、
   叶类不覆盖）是"绑到最近覆盖者"的直接证据；INH23（`Dim up As InhBase: Set up = d: up.Speak()`）是
   "基类型变量持有派生实例不再切片"的证据 —— 这条在 B07b/B08b 一直是**静默错**的，本批顺带修掉。
   INH24..INH26 是**扇出**（`InhSib` 与 `InhDerived` 同以一个基类分叉）：先探针验证过前缀性质
   （`base/sib2` 与 `base/dark2` 互不污染）才升格成常驻用例。`ci_n15_*` 从负例清单删除并删文件
   （形状升格为正例），新增 `ci_n19`（Let/Set）、`ci_n20`（裸名）。
10. **本批没做**：`With Me` 块内的接收者、属性返回对象作接收者（`pvSocket.Pick()` 那条 083c 通路）的
    派发 —— 两者都还在别的发码路上；家族外访问 `Protected` 的拒绝（B08c，诊断 3023 仍预留）。

### D34 B08c（家族外访问 `Protected` 的拒绝）实施记录（2026-09-23 手工续跑轮）

1. **判定落点**：`SemanticAnalyzer::visit(MemberAccessExpr)` 是 `obj.<成员>` 的**唯一必经点** —— 读、
   写（`AssignmentStmt`/`LetStmt`/`SetStmt` 都 `analyzeExpr(*node.target)`）、`CallStmt` 与
   `IndexOrCallExpr` 的 callee 全从这一处过，所以一个 `checkProtectedVisibility()` 调用就覆盖全部
   语句形状（探针实测 7 种接收者形状：模块级字段 / 局部 `Dim` / `ByVal` 形参 × 字段写 / `Sub` 带参
   调用 / `Function` 读 / `Property Get` 读，全中）。诊断 `SemProtectedOutsideFamily = 3023`
   （`diagnostics.hpp` 的预留兑现），文案 ASCII。
2. **接收者→类只能新加字段**：`Symbol::variableTypeName` 只在 `registerVariable`（模块级字段）里填，
   局部 `Dim w As C` **只在类型是委托时**才填、`ByVal w As C` 参数根本不填 → 第一版判定对局部变量和
   形参静默不响（探针实测）。为什么不把 `variableTypeName` 补满：它被后端十余处按"非空即类实例"
   消费（`inferClassTypeOfExpr` 的 084g 分支、`cgen_with`、`comwrite`、`dllentry_collect`…），补上就是
   **改发码**，直接破本批"发码零改动 + 8 文件全同"的验收口径 → 新增 `Symbol::srcTypeName`（四类登记点
   各留一份 `As <类型>` 原文：`registerVariable` + `visit(LocalDeclStmt)` + Sub/Function/Property 三处参数
   注册），目前全仓只有 B08c 一个读者。
3. **注册表按 `Protected` 也建**（本轮最大的设计修正）：开工地图假设"越权判定读 `ClassChainRegistry`
   就够了"，实测**只有 `Protected`、一条 `Inherits` 都没有**的合法工程会在 2.8 开头早退、`classes_`
   全空 → 判定整个静默失效。早退条件扩成 `anyClause || anyVirtual || anyProtected`
   （`moduleHasProtectedMember`），并让"有 Protected 无 Inherits"也走 Pass A/C。这类视图全是**单元素链**
   → 后端 `classChainOf()` 的 `chain.size() < 2` 守卫照旧返回 nullptr、3.4 合并与 3.4b 槽表在同一守卫上
   空转，所以这条只喂语义层，不给存量工程开任何新发码路（护栏实测 8/8 全同）。
   D30-⑦ 那条"不为 E0 建表"的理由**仍然成立**，别混淆：E0 只看 `modules_`，本来就不需要表。
4. **裁决顺序**：② 沿接收者链**叶优先**找最近声明者（与 3.4 的遮蔽裁决同向）—— 抢到键但不是
   Protected 就放过；③ 再看当前模块的链里有没有那个声明者（声明者=本类或本类祖先 → 家族内）。
   当前模块是类模块但**没登记**（泛型模板 / 接口宿主）→ 放过：证明不了越权就不报。
   标准模块 / 窗体不是任何类的家族 → 直接判。**漏报优先于误报**是这批的取舍（与 B08b 的"宁多拒"
   相反，因为这里多拒=把能编译的存量代码判死）。
5. **成员口径**：字段 / `Sub` / `Function` / `Property` 四类，与 driver 侧 `memberAccess()` 严格一致 ——
   两边认同同样的成员才不会出"表没建→判定静默失效"的缝（`Event` 因此不在判定内：带 Event 的基类
   在 2.8 已被判死，且 `memberAccess()` 不认它）。属性按**名字**取严（`MemberAccessExpr` 上拿不到
   Get/Let/Set 方向，与 B08b `dynamicKeys` 同一个限制）：任一方向 `Protected` 即按 `Protected` 论。
6. **不判的形状**（都写进手册，别当已完成）：数组元素 `w(1).X`、属性/函数返回对象 `pvSocket.X`、
   `With w` 内的 `.X`、嵌套接收者。CLR 那条"必须经 `Me` 或本类型更深实例访问"也没做 —— 家族内的
   基类型变量 `o.m_secret` 允许，正例 `ci_pos2_*` 就是钉这个决定的常驻证据。
7. **用例**：`-Category syntax` 74→78。负例三条各钉一种语句形状 —— `ci_n21`（家族外类里字段写）、
   `ci_n22`（标准模块里 `Protected Sub` 带实参调用）、`ci_n23`（家族外类里 `Property Get` 读）；
   正例 `ci_pos2_base/derived`（家族内经基类型变量与 `Me.` 访问，必须静默）。发码零改动，
   `Inh.vbp` 的 INH12（家族内 `Me.`）与 26 条运行期断言全不变。
8. **本轮环境事故（值得下一轮记住）**：全量门**连跑三次**才拿到 —— 前两次不是代码问题：
   (a) 20:50 另一个写入者把整个工作树复制到 `C:\Users\Administrator\Documents\c3.vb6.pro` 并从那份副本
   跑 `-Category all`，其构建在 20:50:23 重链了我 `.build\C3.exe`（对象文件全在，`[1/1] Linking` 即复原），
   我的门跑到 `test_generics_x86` 起连续 `FAIL (compile)` 后 powershell 以 exit=127 死掉；更早一次
   `.build\C3.exe` 直接**消失**（`FileNotFoundError`），当时 `tasklist` 里有 3 个 `cl.exe`。
   (b) 教训：**跑长门前先 `who_is_building.ps1` 看清有没有别的构建/套件在跑，跑完立刻核 exe md5**；
   门的 `Results` 只在"跑前后 md5 一致"时才可信。工具脚本 `.build/who_is_building.ps1` /
   `who_is_building2.ps1`（列 cmake/ninja/cl + 反查父进程与命令行）本轮留下复用。

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

- 2026-09-23 10:38–11:55 **B07a（`Inherits` 语法 + 类继承链 prepass stage 2.8）过门并提交 a056705**：
  词法接线逐处镜像 `Extends`（枚举位置天然落在 `isKeyword` 区间，仍补显式列表；软关键字表加 `Inherits` → `canBeName` 仍认它），
  并且实测**全仓 VB 语料里 `inherits` 这个词出现 0 次** → 关键字化对存量输入零影响，这一点又被 8 文件 emit-c 逐字节护栏独立证明（对 worktree @58f02fe 基线全同）。
  stage 2.8 `runClassChainPrepass` + Driver 级只读 `ClassChainRegistry`（Pass A/B/C 照抄 `driver_interface.cpp`）：登记/基类求解/环检测/链展开/深度 16，
  新诊断 3020/3021/3022；早退条件 = 工程里一条 `Inherits` 都没有（兑现 D24）。刻意保留的两条实测行为：**一条环只报一份**（与 Extends 同行为，
  `ci_n06_*` 常驻）、**接口宿主不能当基类**（宿主那一个 `.cls` 不是可实例化类 → 3020，`ci_n07_*`）。
  本批另开两条通路：`Test-SyntaxFail` 只接单源文件 → 新增 `Invoke-SyntaxProj`/`Test-SyntaxFailMulti`/`Test-SyntaxMulti`（C3 CLI 本就收多个位置参数），
  双文件负例第一次有零构建通路；运行期正例 `tests/cls_inh/Inh.vbp`（`INH0:derived` + `INH1:OK`，后者是“基类自身成员未被派生类影响”的对照组）。
  门 `Results: PASS=137 FAIL=0 SKIP=1 TOTAL=138`（exe 031f993b，11:25:47 起跑、最后一次源码改动在起跑前）。`run_tests.ps1` 登记为纯插入 +66/-0。
  本轮踩到的工具链坑（已进记忆）：heredoc/`printf` 写 `.bat` 会被 MSYS 改写（`>nul`→`>/dev/null`、`\2019` 当八进制）→ vcvars 静默失败、cmake 报找不到编译器，
  看起来像 VS 被卸；基线 bat 一律用 python r-string + CRLF 写并断言不含 `/dev/null`。另更正记忆：`run_tests.ps1` 是 **UTF-8 带 BOM**（自 B01 起就是），编辑时剥了要写回。
  B07b 地图 = D26（含“合并落 3.4”、11 张成员表、转发桩三处锚点、跨 TU 可见性待实测、v1 四类边界）。

- 2026-09-23 11:59–13:15 **B07b（继承成员合并 + 前缀布局 + 转发桩）过门并提交 b1c0050**：
  合并做成新的 **stage 3.4**（语义之后、跨模块之前），把祖先"自己声明"的成员并进派生类的 Class 符号 —— 实际并 **8 张**表而不是地图里说的 11 张：
  `publicFieldNames`/`memberFieldDispids` 并了会让 `dll_entry` 去找派生 TU 根本不发的字段访问器（**链接期才炸**）→ 推到 P6/B13，`eventNames` 无需并（带事件的基类已在 2.8 判死）；字段绝不进 `memberNames`（Fix 099 硬规定，会把字段访问改写成 `prop_get_` 调用）。
  发码走**前缀复制 + 按类转发桩**：派生 struct = `__comObj` + 祖先字段（根→叶，**含祖先私有字段**，否则基类实现的 `me->x` 全体错位）+ 自有字段，
  桩 `vb6_<D>_<M>(vb6_cls_<D>* me,…) { vb6_<B>_<M>((vb6_cls_<B>*)me,…); }` 用 `prop_get_/prop_let_/prop_set_` 三向各一份、Optional 的 `int _has_` 尾参一并转发（否则基类 `IsMissing` 失真）。
  D24④/D26 悬着的**跨 TU 可见性**这条实测解除：不用给派生 `.c` 显式 `#include "<Base>.h"`，现有互相 include 就给了桩所需的类型名，  而继承来的 `Private Type TPoint` 字段要的完整类型也在（`_New` 的 `memset(&me->m_pt,…,sizeof(me->m_pt))` 在派生 TU 编得过）；真正的证据是跑出来的 `INH3`（`d.SetPt 5` → `d.PtSum()=15`）——编译通过不等于布局对。
  **本轮最有价值的发现**：裸名调用继承成员**不是编译错误而是静默少一段代码**（`Option Explicit` 下只给 VB3001，exe 照生、`INH2` 实测 n=0）；  已按 `declaredByAncestor` 升格为 VB3022 并要求写 `Me.`，手册"注意"节同步。2.8 Pass D 另封四类边界（事件基类 / 任一侧新式接口 / 基类重载 / 重声明继承字段），
  两处踩到同一个坑：属性的 Get/Let/Set 是"一个成员键、多个方向节点"，按键去重会丢桩、按节点数判重载会误判整条链。
  门 `Results: PASS=140 FAIL=0 SKIP=1 TOTAL=141`（exe 50ce2e77，12:47:44 起跑）；护栏 8 文件对 worktree @a056705 基线 8/8 逐字节全同、用后即删。
  工具链两条：`scripts/build.bat`（LF-only + GBK 注释）在 agent 的 cmd 里会被注释中的括号字节截断 `if (...)` 块 → 构建改用 `dev.ps1 -SkipTest`；  `git checkout --` 在这个 autocrlf=true 的仓里会把工作树改成 CRLF（本轮差点用它"确认"，结果抹掉了我自己的 STATUS 改动）。
  本轮另外收了 B07a 的账（a056705 + 总表 296c556）。B08 地图 = D28（先拆 B08a=`Protected` / B08b=虚表，含四处镜像锚点与新诊断起点 3023）。

- 2026-09-23 13:22–14:06 **B08a**（P3 第三批第一半 = `Protected`）交付，代码 2117d1c。
  **拆批又拆了一层**：D28 把"Protected 可见性"当成一件事，做完发现它是两件事 —— 家族内可用（本批交付）与家族外越权要拒绝（→ 新登记 **B08c**）。后者今天做不了：`visit(MemberAccessExpr)` 不把 `obj` 解析成 Class，而"在跨模块逐字段拷贝里按访问级别过滤成员表"的偷懒做法会让越权访问**退化成运行期才炸的晚绑定 COM 调用**（比静默更糟）。手册页按这个边界如实写。
  落地面：`AccessLevel::Protected = 3`（末尾追加；`Default = Public` 是别名，插中间会撞值）、关键字接线 **13 处**（三处静默陷阱记在 D29-2：`parser_decl.cpp` 的 `else` 兜底是 Private、`isDeclarationStart` 漏 case 即 parse 错、`.h` 侧 static 判定与 `.c` 侧不同源）、新表 `Symbol::memberAccessLevels`（只有 3 个 touch point，实测全仓无 Symbol 序列化）、`.h` 声明三处放行。
  门 `Results: PASS=141 FAIL=0 SKIP=1 TOTAL=142`（exe 52bfaa97 跑前后一致）；`-Category syntax` 66/0/0；护栏 **8 文件对 worktree @b1c0050 基线 8/8 逐字节全同**（本批动了发码判定 → 按 D9 不省）；Inh.vbp 现在 14 条断言（INH12 = 跨 TU 用基类 Protected 方法、INH13 = Protected 字段读写回转）。
  两条工具/勘察教训：委托 Explore 摸消费点是划算的，但它两条锚点不准（`token.cpp:97` 其实是 `isStatementStart`；crossmod 的拷贝写的是 `extSym->/srcSym->`）→ **动手前逐条 grep 复核**，错锚点在带 `assert count==1` 的打补丁脚本里会直接 no-op。B08b 地图 = **D30**（`__cvtbl` 定序、`memberVirtual` 表、两处派发点、3024 起诊断、真虚断言形状）。

- 2026-09-23 14:12–15:30 **B08b**（虚修饰符语法 + 覆盖契约 + 封掉假虚派发）交付，代码 05397be。
  **拆批拆到第三层**：D30 把 B08b 定成"修饰符 + 类虚表 + 动态派发"，本轮只做前两件事**并把需要派发的调用点判死**，类虚表独立成 **B08d**（地图 D32）。驱动力是 D27-13 那条判例：只收语法的话，"基类体内 `Me.M`"会编得过、跑出基类实现，后代的 `Overrides` 静默失效 —— 那比编译失败危险。
  落地面：`ProcVirt` 四值枚举（**不是三个 bool**：三件套互斥，那样有 6 种非法组合要两层各防一遍）；AST 三个过程节点加 `virt` 成员（零构造函数签名改动，`ast_clone` 刻意不拷 → 模板内已拒）；parse 的修饰符**吃两次**（`Overridable Sub` 与 `Public Overridable Sub` 都收）；契约检查落在 **2.8 新增 `runVirtualContractChecks`**（放 3.4 之后 Class 符号成员表已被合并污染，分不清"谁声明的"），属性按 `ifaceSlotKey` 分方向配对、签名复用 `ifaceSigFromDecl`/`ifaceSigEqual`（与接口契约同一套函数，不留两套判据）。
  两处值得单独记：**① 拒绝点要三处**（`visit(IdentifierExpr)` 的查到符号分支、`visit(IndexOrCallExpr)` 的裸 callee 分支——它自己查符号、不经过前者，少埋一处就漏 `Speak(5)`；`visit(MemberAccessExpr)` 的 `Me.X`）；**② 返回值赋值差点被误杀**：`Speak = "base"` 在 `Speak` 自己体内长得和调用一模一样，要靠 `currentProc_` 同名豁免，否则最正面的用法第一刀就死。
  门 `Results: PASS=148 FAIL=0 SKIP=1 TOTAL=149`（exe 546265e7，14:56:16 起跑）；`-Category syntax` 66→73（新增 `ci_n12`..`ci_n18` 七条负例：无目标 / 未标 Overridable / 签名不符 / 需要动态派发 / 无 Inherits 写 Overrides / 标准模块写 Overridable / Interface 块写虚修饰符）；`Inh.vbp` 断言 14→17；护栏 8 文件对 worktree @2117d1c **8/8 逐字节全同**。
  工具链一条：worktree 基线构建用 `.build\base_build.ps1`（PowerShell 不经 MSYS，一条命令 configure+build 就过）；`scripts/dev.ps1` 在 worktree 里不行，它只 `cmake --build`、不 configure。诊断 ID 从 **3024** 起到 3027，**3023 仍留给 B08c**。下一批 **B08d**（地图 D32）。  收尾后为 B08d 摸了一遍 `cgen_inherit.cpp`，顺手改正 **D32 的一条**：表实例其实可以是 owning TU 的 `static const`（只有**类型**要跨 TU 发进 `.h`），并且多视图逼出 `__cvtbl` 只能是 `void*` + 用点强转 —— 原写法“表与表项都必须非 static”会让下一轮白改一遍发码；另把要复用的取名四件套（`cProcName`/`procBaseName`、`classMeParam`+`makeParamCType`、`inheritedRetType`）记进了 D32。
- 2026-09-23 16:15–19:45 **B08d 过门并提交（代码 `df9806e`）**：stage 3.4b `Driver::buildVirtualSlotTables`（每类有序虚槽表：按首次声明位置 根→叶、每槽键一份，筛选集取**链根**的 `dynamicKeys` → 祖先视图恒为派生表的前缀）+ 发码四件（`const void* __cvtbl` 字段 / `.h` 里本类视图的 `vb6_cvtbl_<X>` 类型 + `extern const` 表实例 / `_New()` 装载 / epilogue 末尾的表实例定义）+ 两处类成员发码路按槽索引改写 + 语义层按 D32① 删掉 `Me.X` 拒绝。
  三条**地图写错、实测才对**的教训（D32 的 ②③ 已由 D33 改正）：① `__cvtbl` 不能只给「链上带过虚修饰符的类」——**叶类也必须带**，否则基类型变量 `x.Pick()` 会让基类视图去读一个不存在的字段（= 别人第一个数据成员的地址，野指针）；判据换成「链根的 dynamicKeys 非空」就自然覆盖整条链。② 链根的 `chain` 只有一个元素，照 `chain.size() < 2` 早退会把**最该建表的那个类**漏掉（第一轮 `b.Speak()` 静默绑回 base 就是它）。③ 表类型不需要跨 TU，但**必须**发在 `<X>.h` 那批跨模块 include **之后**（消费者 TU 用的是接收者静态类型自己那张视图），且表实例要后置到 epilogue（Private 过程在 `.c` 里是 `static` 且没有前置原型）。
  一条**假成功**的形状值得记住：只改「优先级2」那一支之后，`InhMain.bas` 里的 `b.Speak()`/`d.Speak()` 已经派发对了、而类体内 `Me.Pick()` 仍是直调 → 靠 INH18/19/20/22 四条运行期 FAIL 才暴露；根因是 `Me` 是 **MeExpr**，进不了那支要求 `node.object` 为 IdentifierExpr 的分支，真正发码的地方是 `cgen_expr_member_class_fallback.inc`（它按 emit 出来的对象文本 `"me"` 查 `knownClassVars_`）。**「部分用例变绿」不等于机制接通** —— 运行期断言必须覆盖「类体内那条调用」。
  本轮跑了**三次全量门**：18:03–18:31（149/0/1/150，`gate_B08d.log`）、18:33–19:02（把扇出探针升格成常驻用例 `InhSib.cls` + INH24..26 之后重跑，同数）、19:06–19:34（发现自己这轮新写的一处代码注释把机制说反了 → 注释也参与编译、会改 exe，为保证「被测二进制 == 提交树」重建后第三次跑，最终 exe `83c4e49c`、`Results: PASS=149 FAIL=0 SKIP=1 TOTAL=150`）。教训：**升格用例与注释订正都该在起跑全量门之前做完**，一轮门就够。人工 19:35 问过「为什么一直在跑测试」，已解释，并提出可选口径（纯注释级改动以 vbp+syntax 两分类 + 8 文件护栏代证、不重跑全量）——**未拍板**，默认仍按「提交树 == 被测树」跑全量。
  其他证据：8 文件 `--emit-c` 对 worktree @fb6a254（自建 Debug exe）**8/8 逐字节全同**；`-Category syntax` 73→74（删 `ci_n15`、增 `ci_n19`/`ci_n20`）；`Inh.vbp` 运行期断言 17→27 条。扇出（一个基类两个分支）先在 `.build/probe_fan/` 探针实测（F0..F5 = `base/base2`、`bright/base2`、`bright/dark2`、上转型同值、`base/sib2`）再升格为常驻用例。工具链两条：`scripts/build.bat` 在这台机器上是 **LF-only** 的 .bat，cmd 解析 `for /f` 会碎掉（表现为一堆「不是内部或外部命令」）→ 主树构建走 `scripts/dev.ps1`，别去改别人的脚本；`cmd //c` 经 MSYS 会吞参数，PowerShell `-Command "& scripts\dev.ps1 ..."` 一条就够。下一批 **B08c**（家族外越权访问 `Protected` 的拒绝，诊断 3023 已预留，地图见 CURRENT_BATCH）。
- 2026-09-23 19:55 ~ 09-24 00:15 **B08c（`Protected` 家族外越权访问的拒绝）过门并提交 82b1b34**：
  判定落点选 `visit(MemberAccessExpr)` —— 读、写、`Set`/`Let`、`CallStmt` 与调用 callee 全都从这一处过，
  一个 `checkProtectedVisibility()` 就覆盖全部语句形状；沿 2.8 的类链按**叶优先**找最近声明者（与 3.4
  遮蔽裁决同向），再看当前模块的链里有没有那个声明者；认不出接收者、或当前类没登记（泛型模板 /
  接口宿主）一律**放过**（这里"多拒"= 把能编译的代码判死，与 B08b 的"宁多拒"取舍相反）。诊断 `VB3023`。
  两处开工地图没料到的前提：① 局部变量与参数**根本没有** `variableTypeName`（7 种接收者形状的探针实测
  有 2 种静默不响），而补满它会喂到后端十余处"非空即类实例"的消费点 = 改发码 → 新增只有本判定读的
  `Symbol::srcTypeName`（模块级字段 + 局部 + Sub/Function/Property 三处参数共 5 个登记点）；
  ② "只有 `Protected`、一条 `Inherits` 都没有"的合法工程在 2.8 开头早退 → `classes_` 全空 → 判定**整个
  静默失效** → 早退条件扩成 `anyClause || anyVirtual || anyProtected`（这类视图全是单元素链，后端
  `classChainOf()` 的 `chain.size() < 2` 守卫照旧返回 nullptr，所以只喂语义层）。
  用例：`-Category syntax` 74→78（`ci_n21` 家族外字段写 / `ci_n22` 标准模块里 `Protected Sub` 带实参调用 /
  `ci_n23` 家族外 `Property Get` 读，三条各钉一种语句形状；`ci_pos2_base|derived` 钉"家族内经基类型变量与
  `Me.` 访问必须静默"）。发码零改动 → 8 文件 `--emit-c` 对 pre-B08c(@fb6a254) **8/8 逐字节全同**；
  `Inh.vbp` 的 26 条运行期断言（含 INH12 家族内 `Me.`）与 legacy `test_implements`、`itf_xmod_writer` 全不变。
  **本轮最大的非代码收获 —— 全量门跑了四次**：前三次都被同一台机器上另一个写入者打断。它 20:41 把整个
  工作树复制到 `C:\Users\Administrator\Documents\c3.vb6.pro` 并从那份副本跑 `-Category all`，其间（20:50:23）
  还重链了我这边的 `.build\C3.exe` → v1 跑到 `test_generics_x86` 起连续 `FAIL (compile)`、powershell 以
  exit=127 死掉；更早一次 `.build\C3.exe` **直接消失**（对象文件全在，`[1/1] Linking` 即复原，`FileNotFoundError`
  不是代码问题）。v3 跑完了全程但留下 `test_softkeyword ... FAIL (compile)` —— 该用例一个类都没有、根本走不到
  本批判定，同 exe 单独复跑两次均 PASS，判为与第三方套件争抢 CPU/临时目录的瞬时失败，**不记数**；
  v4（23:26–00:07，153/0/1/154，跑前后 exe md5 33d68fc7 一致）才作为 GATE_BASELINE。
  固定动作从此加两条：跑长门前用 `.build/who_is_building2.ps1`（本轮新留：列 cmake/ninja/cl + 反查父进程与
  命令行）确认没有别的构建或套件在跑；跑完立刻核 exe md5，md5 变过的门一律重跑。
