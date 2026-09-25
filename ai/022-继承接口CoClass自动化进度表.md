# 022-继承接口CoClass 自动化进度总表

> 本文件是每小时自动化任务（"实现继承、接口与CoClass"）的**唯一状态源**。
> 每次运行开始先读本文件，结束前必须更新本文件（状态头 + 批次清单 + 运行日志）。
> 规范输入: `ai/讨论记录/018-接口继承与CoClass设计思路.md`（含 tB 文档要点与分阶段设计思路全文）。

STATUS: IDLE             # NOT_STARTED | DESIGN | BUSY | IDLE | ALL_DONE
LAST_RUN: 2026-09-25T08:34:00+08:00   # 本轮 = **B16 一格出完并过门（Actions run #62，head `578faa4`）**。
               # 交付：**接口成员的对外调用契约** —— ① 薄面整条 canonical 化：`vb6_ivtbl_<I>` 的 IUnknown
               # 前缀与契约槽、以及它们的实现函数，在 x86 下一律 `__stdcall`（BASE 的 x86 产物里一个
               # 约定修饰都没有 = `__cdecl`，而 COM 规范是 `CC_STDCALL`；x64 下 MSVC 接受并忽略该修饰，
               # 故一份生产码两架构通用）；② **类型库那一档如实发契约成员**（`cFuncs` = 2、每成员
               # `oVft=(3+槽)*指针宽` / `callconv=stdcall` / 原生返回 vt，ByRef 建 `VT_PTR` 链）；
               # ③ **位数 flag 跟 `--arch` 走**（`CreateTypeLib2(is64_ ? SYS_WIN64 : SYS_WIN32)`）——
               # 存量库默认架构逐字节不变、`--arch x86` 只差 32 字节布局字（读数见 D63-3）；
               # ④ 薄面出入口 `vb6_iv_thin_<C>`/`vb6_iv_claim_<C>`：接口 IID 的 QI 交**薄指针**，
               # 包装器把底座引用交还**最后一个薄引用**（`IClassFactory::CreateInstance` 那种"QI 完就
               # Release 包装器"的规范姿势下交出去的指针不悬空）。
               # 三件测量与裁决落 **D63**；canonical 返回形状（`HRESULT` + `[out, retval]`）与
               # 跨世界身份合一按"断不了先断形状"留给 **B17**（B16 判据⑤）。
               # 上一轮 = **B15 已出完并过门（Actions run #59，8/8 job 绿，head `0caa3c5`）**。
               # 门 head 里含用户的 Fix 192/193（`a4e8eb3`/`7e9b6bc`，merge `0caa3c5` 无冲突）。
               # 自动运行见本行不足 55 分钟请立即跳过。
LAST_COMMIT: 代码批 = 578faa4(B16)、941b6dc(B15)、0caa3c5(B15 门 head = 合并用户 Fix 192/193)、ddf4e9b(B14 测试批)、4534a83(B14 台账)、b10ec1a(B13e)、5d29a5c(B13e 台账)、9df23ba(B13d)、bd38798(B13c)、b1a8e58(B13b)、7b6570a+988c7cb(B13a；门 head = 合并 `9785f4f`)、7f829ee(B11/C05=B12)、b82a184+02d70fe(B11/C04)、c4aaa4c(B11/C03b)、e515d89(B11/C03a)、f0b820d(B11/C02)、e7c7a31(B11/C01)、3c5d8e6(B10)、9eb2ca7(B09c)、debb110(B09b)、02bac92(B09)   # **commit message 一律现写、不复用上批文本**；push 只推 `github/dev`（Actions 门），`origin`(gitcode) 与 `main` 不碰、**绝不建 MR**。
CURRENT_BATCH: **B17 = 外部激活冒烟（真注册 + `CoCreateInstance` 早绑定 + `CreateObject`/IDispatch 晚绑定 双路）**
               （**先读 D63（B16 三件测量与两条押后的裁决）、D60-4、D61-6，以及 004/013 里那两条验收模式的原文**。
               B16 把进程内那条管线推到了"广告 == 应答 + 库里如实发契约"，但**没有一个真注册客户碰过它**：
               `CreateObject` 走注册表、早绑定客户按库里的 `oVft` 直调 —— 两者至今只在本机 `--keep-for-debug`
               的临时产物上被探针模拟过。本批就是把这两条路走通，走不通就把断在哪一级记清楚。）
               1. 动手前先量两件（读数落 D64，范围按读数收窄，宁窄勿滥）：
                  ① **注册与外部激活今天到底通不通**：把 `tests\test_activex_dll\` 那份现成模式
                     （编译 DLL → `DllRegisterServer`（或 `regsvr32`）→ **外部** `CreateObject` 晚绑定断言，
                     含 `tests\test_p613_typelib.bas`）在**新式接口宿主的 DLL**（`tests\cc_dll` 的 `CImpl`
                     或一个手写 `CoClass` 块的工程）上重跑一遍，逐级记读数：CLSID 进没进注册表、ProgID
                     对不对、`CreateObject` 拿到的是不是 IDispatch 包装器、库里契约那一档客户认不认。
                     **顺手回答 B16 那第 4 项**：`DllRegisterServer` 一族对新式 CoClass/类工厂无需新改动
                     （注册写入读的就是同一张服务器表、身份自 B13b 起走 `coclassIds_` 唯一出口；B16 的 A/B
                     里 `.def`/exports/`.rc` 一字未动）—— 但这条只有**真跑一次**才算验收，读码不算。
                  ② **早绑定那条路的外部读数**：`CoCreateInstance` + 按 `.tlb` 导入的接口直调契约槽
                     （= B16 探针那条支路，但这次是**注册过的**、由系统装载的 DLL；x86 与 x64 各一次），
                     并顺带回答「canonical 返回形状（`HRESULT` + `[out, retval]`）不改的话，一个真早绑定
                     客户能不能用」—— 这个答案决定它是排进下一格还是永久不做（B16 只按"断不了先断形状"
                     把它押后，没有断言它必须做）。
               2. 判据（不许半接）：① 外部那三条验收（`test_p613_typelib` 那一族）在新式 CoClass 上**真绿**，
                  且新式接口那一档的 IID/CLSID/ProgID 与 `.tlb`、服务器表三处同值（B13 那套"唯一出口"判据的
                  外部队列版）；② 早绑定客户按库里 `oVft` 直调拿到正确返回值（x86 + x64 双验）；
                  ③ **注册表可清理、用例不依赖机器上装过什么**（`DllUnregisterServer` 后不留
                  `HKCU\Software\Classes` 残留；本机与干净 runner 都要能跑 —— 这条是 B17 能进回归的前提，
                  写不成就只记读数、用例留在 `-RequiresCom` 一类门后）；④ 存量 DLL 的注册与激活行为一字不变；
                  ⑤ 断不了的（canonical 返回形状 / 跨世界身份合一）继续只断形状并写清属谁。
               3. 硬约束与基线：BASE = B16 收线 exe（md5 见 GATE_BASELINE 末行），
                  护栏沿用 `.build/b16_sig.py` 那套白名单分类器（B16 的 `b16_emitc_guard.py`/`b16_ab_all.py`
                  可直接改名复用，允许面按本批靶子重新声明）。
               4. 门与规矩沿用：push 只推 `github/dev` + `git ls-remote` 核 sha；盯门
                  `.build/wait_run2.py <sha> <秒>`；**门跑 Actions（本机只记快检）**；`.build` 里的临时
                  `.ps1` 一律 ASCII only；套件在 CI 上用 **pwsh 7** 跑（`Join-Path` 4 参形式 PS 5.1 静默给空串，
                  本地复现套件也必须用 pwsh）；行尾/编码按文件实测。
               仍开（不在 B17）：**`ComObj_Invoke` 的 invkind fallback**（D61-6，会跨 Get/Let 打到对方；
               收紧属口径题，等拍板）、`.bas` 里声明的新式接口在调用点认不出（D43 第三条）、B10 的三条
               caller 侧洞、B06c（接口值作实参/进 Variant）、⑮d、祖先 Private UDT 进方法签名（D40 末①）、
               `com_entry` 基类 extern 的 `void*` 返回、`Class_Terminate` 在 EXE 里无触发点（D41）、
               **canonical COM 返回形状**与**跨世界身份合一**（B16 按 D63-5/D63-6 押后，取舍看本批第 1 条②）。
GATE_BASELINE: (Actions 级) c3test run **#62 [dev] = completed/success**（https://github.com/fxl447098457/c3test/actions/runs/36077118402；
               head 已核 = `578faa4` = B16 代码；8 个 job 全绿（smoke 1、bas#1 24、bas#2 23、compile 10、syntax 119、asm 13+1SKIP、vbp 见下），
               `Tests (vbp)` = **PASS=36 FAIL=0 SKIP=1 TOTAL=37**（上一批 33/0/1/34，+3 = 本批新增的三条；唯一 SKIP 仍是本机未注册的 `test_vbman`）。
               本批新增两族用例在干净 runner 上全绿，判据四条都真跑：
               `[TLB-CONTRACT] cc_dll_tlb_contract_x64 ... PASS (typelib advertises the shipped vtable)`、
               `[TLB-CONTRACT] cc_dll_tlb_contract_x86 ... PASS`（x86 那条用 x86 读端，`oVft=12/16`）、
               `[DISPATCH] cc_dll_dispatch_iface_only ... PASS (real in-proc IDispatch client)` 与 `_x86` 同批绿；
               原有 `[TLB-ID] cc_dll_tlb_matches_table` / `cc_dll_identity_single_source` / B14 两条 `[DISPATCH]` 同批仍绿
               ⇒ 库里那一档从"真接口 + 0 成员"换成"如实契约"，而对外那条真跑通路没坏。
               本机侧同批证据（**不记本地门数**，理由见运行日志那两个坑）：`--emit-c` 17 件 `violations=0`（`__stdcall` 只出现在
               声明/实现新式接口的工程、薄面那一对只出现在有接口宿主的工程，靠 `EXPECT_FIRING`/`EXPECT_THIN_HOSTS` 分开钉）；
               全产物 A/B `unclassified=0`（只有 `cc_dll` 的 `.tlb` 1484→1624 属预期变化，存量工程 `.tlb` 逐字节不动）；
               负控在 `pre_b16_C3.exe` 上 `pass=0 fail=2`（四条 needle 全 miss + 反面断言 `cFuncs=0` 命中）；
               默认架构存量 `.tlb` 逐字节不变（md5 `87d34673`、diffbytes=0），`--arch x86` 差 **32 字节**、且差集与 BASE→NEW 的差集同址。
               第 4 项（`DllRegisterServer` 一族接线）经读码 + A/B 判定**无需新改动**（注册写读同一张服务器表、身份自 B13b 起走
               `coclassIds_` 唯一出口；`.def`/exports/`.rc` 一字未动），真注册的外部端到端验收归 **B17**。
               收线 exe（= 门 head 的树）md5 `90129adbb5dbb3c5fca14fcc872dc0f2`，已存成下一批 BASE。 # 下一批 BASE = `.build/pre_b17_C3.exe`（md5 90129adbb5dbb3c5fca14fcc872dc0f2）。
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
| B08e | P3 | 虚表线收尾：`resolveClassMemberCall` 其余 13 个消费点逐条接上派发或判死（站点地图与裁决见 D35）——**13 站已在 B08e-6 全部出完** | ☑ **B08e-1**（① `With w` 内 `.M()`）、**B08e-2**（⑤ `Me.<字段>.方法()`）、**B08e-3**（零代码：⑩/⑪ 裁决纠偏 + 找出先决条件⑭）、**B08e-4**（⑭ `suppressVirtDispatch_` + ⑪ `Me.<字段>.<属性>` 的读）、**B08e-5**（⑥⑦ 默认属性调用式 `m_up(9)`）、**B08e-6**（⑨ 接派发 + ⑩⑫ 判死 + `Test-CompileFail` 前置；⑧ 实测已被优先级2 判死、⑬ 改判无需改）已出；下一轮 **⑮**（UDT 字段调用位置的路由缺陷，D35-8）；②③ 判为探针无需改、④ 判为走不到 → **13 站在 B08e-6 后全部出完，⑮ 一出就开 B09（`MyBase`）** | e531d82(B08e-1)、8987386(B08e-2)、392a52d(B08e-4)、d9eca95(B08e-5)、40eea3f(B08e-6) | 最新门 = B08e-6（Actions 级，见状态头 GATE_BASELINE）+ `Inh.vbp` 断言 38→40（INH39 判别/INH40 对照）+ A/B（`pre_b08e6_C3.exe`：INH39 FAIL base、其余 39 条 OK；`ci_n24`/`ci_n25` 改前 `--emit-c` 退出码 0）+ 8 文件 emit-c 8/8 + `-Category syntax` 78→82 全绿。上一条本地门 = B08e-5：`Results: PASS=153 FAIL=0 SKIP=1 TOTAL=154`（gate_B08e5.log；exe md5 ac14cf25 跑前跑后一致）。上一批 B08e-4：同 153/0/1/154（gate_B08e4.log；exe f2547af6）。B08e-3 未跑门（零代码，先例 B00）。 |
| B08f | P3 | UDT 里放工程类对象字段的整条通路（⑮a 写方向 / ⑮b 调用方向 / ⑮c 派发 / ⑮d UDT 自己在第三个模块），实测与落点见 **D36**、真根因与实施见 **D37**；原挂在 B08e 的"站点⑮"，因与虚表无共同判据而单列 | ◐ **B08f-1 已出（`77ecef1`）= ⑮a+⑮b+⑮c 一起**（三处接线：语义层 Variant 分支也存类型名 / `udtFieldObjCType` 按当前符号表回判工程类 / Set 侧按真实 C 类型否决 Variant 容器判定；通路一通，D35 站点④ 才第一次可达，顺手接上派发）。**⑮d/⑮e 仍开**：UDT 声明在使用点之外的模块时同样坏；属性写穿过 UDT 字段两条路都发非法 C，卡在 `symTab_.lookupModule(UDT 名)` 的消费者可见性 —— 与本批不同判据，另批先量可见面再动 | 77ecef1 | 验收只能走**真编译**（`--emit-c` 对坏形状返回 0）：`Inh.vbp` 断言 40→43（INH41/42 判别、INH43 对照），改码前的二进制编不过这个工程；8 文件 emit-c 8/8、`-Category syntax` 82/0 |
| B09 | P3 | `MyBase.M(…)` 显式基调用（去虚化）+ 构造链顺序 + 无新语法逐字节护栏；前置 = `com_entry` 的 typedef 分块顺序（D35-9 ⑤） | ☑ **B09**（`02bac92`，记录见 **D38**）：`MyBase` 走发码层按接收者名字接管（precheck 的 `Err`/`VBA` 先例，lexer/parser 未动）；实现按“就近声明”取 `vb6_<owner>_<M>`，**不查 `__cvtbl`**；读 `Get>Function>Sub>Let>Set`、写只认 `Let/Set`；`VB3028` 判死三条形（无 Inherits / 基面无此名 / 基类 Private = C 层 static）；构造链根→叶 + `vb6_<基>_chain_init` 桥（仅“写了 Class_Initialize 且被谁继承”才发）。B09-0：`com_entry` 的类返回类型 extern 改 `struct vb6_cls_X*`。**仍开（另批）**：**B09b = x86 布局缺陷**（继承的 Private UDT 字段在派生 TU 退化成 `void*` → 前缀错位、`_New` 少分配 4 字节 → 写 `m_pt` 之后的基类字段越界；x64 巧合正确，所以门里看不见，实测见 **D39**）、`Class_Terminate` 反序链、`Set MyBase.<属性> = obj` 未验、基类自家 extern 的 `void*` 返回类型（Fix 184 只做了一半） | `02bac92` | `Inh.vbp` 断言 **43→52**（INH44/45 判别去虚化、INH49/50 构造链与桥、INH51 = B09-0 用例）；**改码前编不过这个工程**（C2143 + 5×C2065）；负例 `ci_n27`/`ci_n28` 走 `--emit-c` 断言 VB3028（改前退出码 0）；8 文件 emit-c 8/8、`-Category syntax` 82→84 全绿 |
| B09b | P3 | x86 布局缺陷：继承字段里 `Private` UDT 在派生 TU 解不出类型名 → 回落 `void*` → 前缀错位（D39 登记，本批治） | ☑ **B09b**（`debb110`，记录见 **D40**）：`runCrossModuleResolution()` 末尾沿 `Inherits` 链注入被继承字段用到的 UDT/Enum 类型符号（含 `udtMembers`；本地同名不抢）；判别实验 = 把 `Private Type` 改成 `Public` 看发射形状 + x86 能否跑起来；顺带清掉从 B07b 挂着的 INH35/INH36/INH39 三条 x86 红字；**门新增 `cls_inh_x86`**（同一份断言清单双架构跑） | `debb110` | `tests/cls_inh` x64 + x86 build+run 各 **52/52、0 FAIL**（改码前 x86 段错误）；8 文件 emit-c 对 `pre_b09_C3.exe` 8/8；`-Category syntax` 84/0 |
| B09c | P3 | 收尾 P3 三条小尾巴：`Set MyBase.<属性> = obj`、⑮e（属性写穿过 UDT 对象字段）、`Class_Terminate` 反序链 | ☑ **B09c**（`9eb2ca7`，三条的实测与处置见 **D41**）：① 需要改码并已修 （改码前只有接收者是裸 `MyBase` → C2065）；② **⑮e 早在 B08f-1 就通了**，D37 那条是在坏元数据上量的 → 只补断言（INH55/INH59）；③ **`Class_Terminate` 链不做** —— EXE 工程三种形状实测都不触发 terminate，发出去就是死代码 → 登记为生命周期/P6 的既有缺口 | `9eb2ca7` | `Inh.vbp` 断言 **52→59**、**x64 + x86 各 59/59、0 FAIL**（A/B：改码前编不过，1×C2065）；8 文件 emit-c 对 `pre_b09c_C3.exe` 8/8；`-Category syntax` 84/0；INH58 = B09b 最深用例（隔两级持有根的 Private UDT 字段）|
| B10 | P4 | `Implements IFace Via <holderVar>` 委托式实现：持有字段 + 自动转调桩 + 签名检查 | ☑ **B10**（`3c5d8e6`，实施与选型见 **D43**）：`Via` 软关键字四件套 → 语法 `ImplementsStmt::viaField` → **stage 2.7 Pass D** 裁决（`vias_`，一份来源同喂语义与发码；`VB3029`/`VB3030` 两类判死含链式 Via）→ 语义层免逐槽 `VB3012` → 发码层转调**持有对象的接口槽**（不直调 Private 成员：C 层 static 跨 TU 连不到）+ Nothing 字段退零值 | `3c5d8e6` | `tests/itf_via` 新工程 **VIA1..VIA9 x64 与 x86 各 9/9**；A/B 改码前 `VB2003`+`VB2002`；4 条负例（n21/n22/n23/n24）+ 1 条软关键字正例 p04；10 文件 `--emit-c` 对 `pre_b10_C3.exe` 全同 10/10；`-Category syntax` 84→89 |
| B11 | P5 | `CoClass…End CoClass` 语法 + `[CoClassId]/[Default] Interface/[ComCreatable]/[CoClassCustomConstructor]` + 契约聚合校验（实施依据换成 `ai/026`，其六节把 B11 拆成 C01–C05） | ◐ **C01 已出（`e7c7a31`）= 块语法 + 属性行落 AST**（零回归靠 `Module::coclasses` 不进 `declarations`；属性行归属按"名字+位置+同行"合判，见 D44/D45）；**C02 已出（`f0b820d`）= 身份求解唯一函数**（`src/semantics/coclass_identity.{hpp,cpp}` 纯函数 + stage 2.7 Pass E + `Driver::coclassIds_`；见 D46/D47）；**C03a 已出（`e515d89`）= 块形状与名字校验**（stage 2.7 Pass F 八条判据 + `VB3031/3032/3033`；宿主同名豁免见 D49-①）；**C03b 已出（`c4aaa4c`）= 契约聚合校验**（新 **stage 3.4c** `runCoClassContractCheck()`：按链倒着走、叶优先，缺槽 `VB3012`/签名不符 `VB3017`，`vias_` 命中的委托免逐槽；`VB3020` 文案收口。`As <CoClass>` 按 D50-② 整条并入 C05）。**C04 已出 = 存量头属性只读折算**（stage 2.7 新 **Pass E0**：四行 `Attribute VB_*` → 一条 `CoClassDecl` 进同一张表，Pass E 仍是唯一身份出口；折算记录**不参与判死**（Pass F/3.4c 跳过它），手写块优先，见 D52/D53）；**C05 已出（`7f829ee`）= 组内激活**（stage 2.7 新 **Pass G**：类型位点上的块名**就地改绑**`[Implementation]` 类 + `CreateObject(ProgID)` 换成 `New`，`VB3039` 挡住没有实现类的块名当类型用；实测 D54、实施 D55。发码侧零新分支，护栏 16/16。这一格同时把 022 的 **B12** 一起交付） | `7f829ee`(C05)、`c4aaa4c`(C03b)、`e515d89`(C03a)、`f0b820d`(C02)、`e7c7a31`(C01) | run **#23**（head 已核 = `4045b42`）+ `-Category syntax` 107→112 + 10 文件 `--emit-c` 对 `pre_b11c03b_C3.exe` 10/10 + A/B（n37/n38 零命中、n39 出旧句）+ `Id.vbp` 真编译真运行且发码逐字节未动 |
| B12 | P5 | 组内激活：`New <CoClass>` / `CreateObject("ProgID")` 编译期映射 + 默认接口派发 | ☑ **由 B11/C05 交付**（`7f829ee`）= Pass G 就地改绑实现类，`As`/`New`/`CreateObject` 三面同归一条工程类路；`As Object` 目标经 Fix 179a 拿到 IDispatch 包装。**口径偏差**：`As <块名>` 的成员面是实现类的公开成员（比默认接口宽），要窄视图写 `As IShape` —— 两条理由记 D54-⑤ | 实测 D54 / 实施 D55 | `-Category syntax` 116→118 + `tests/cc_act` CC1..CC9 **x64+x86 各 9/9** + 护栏 16/16 + A/B 三条坏读数（base 复现、new 消失）；门见状态头 |
| B13 | P6 | ~~IUnknown 三件套真实实现~~ **D56 推翻**：RTL 的 `ComObj_QueryInterface/AddRef/Release` 早是真实现（原子计数、归零销毁、`Class_Terminate` 在 DLL 侧有触发点）⇒ 本格真正的活 = **把 CoClass 块接进对外那一半**（身份出口、块名 ProgID、新式接口对外可调用） | ☑ **B13a（观测面）**：新助手 `Test-VbpDll` + 把现成的 `tests\test_activex_dll` 两份 DLL 工程接进回归（此前门内一次都没链接过 .dll）+ 新工程 `tests\cc_dll`；**B13b 已出**（`dll_entry` 的 CLSID / 默认接口 IID / 组名 ProgID 一律读 `coclassIds_`；`[ComCreatable(True)]` = 组名那一档 ProgID 的唯一开关，`legacyFolded` 继续走原路 = 折算隔离；legacy lambda 降级成「读不到块才用」的兜底，未并掉的两枚见 D57-4/5）、**B13c 已出**（D58）= 接口自己的 IID 也进唯一出口（`resolveIfaceIid` 与块内 `[Default]` 共用一个函数；Pass E 建 `Driver::ifaceIds_`），COM 服务器表 / 新式接口 vtable 的 QI / 类型库三条通道一律读这张表（BASE 实测同一个 `IProbe` 有**四枚** GUID、且 `.tlb` 广告给客户端的那枚服务器不应答 ⇒ D57-5 那条"未证"当场证伪并修好）；类型库的 coclass 默认接口开始跟随块的 `[Default]`（只延后需要重定向的那几行 ⇒ 没有手写块的工程连类型顺序都不动）；D56-5 的对外口径**拍板走 (b)**：`Private` 契约成员不发成 disp id，改打一条 `C3: …vtable interface: 0 Public member…` 信息行说明"注册了却点不到"（D58-5）；新增 `.tlb` 读数工具 `tests\tools\tlbprobe.cpp` + gated 用例 `cc_dll_tlb_matches_table`（助手在 `tests\tlb_identity.ps1`）。**B13d 已出**（D59）= 薄指针的规范 IUnknown：`vb6_iunk_<C>_<I>_QueryInterface` 不再把 `IID_IUnknown` 与本接口并成一个分支回 `self`（两个不同偏移的 `__iv_` 成员 ⇒ 同一对象两个身份），一律回**本类实现序第一个接口**的薄指针 —— 那是唯一一处偏移 0 就是 vtable、既能当身份又能被再次 QI/AddRef/Release 的合法指针，且零布局改动（D19 不入视野）、单接口类拿到的值与改前相同。用例两条：`itf_canonical_iunknown`（`--emit-c` 断形状 + 负控跑过 BASE）与真编译真跑的 `itf_xmod_writer`（QI1..QI4）。**本批最值钱的是顺手读出的一条危险**（D59-4）：RTL 包装器对表里的 `defaultIfaceIid`/`ifaceIids` 一律交回胖指针，而这两处自 B13b/c 起可能装着新式 vtable 接口的 IID 且 `.tlb` 把它当默认接口广告 ⇒ 早绑定客户 QI 成功后按虚表第 3 槽调用会打到 `GetTypeInfoCount`（静默调错函数）。两种收法（对外不发布 / 让包装器真返回薄指针 = B16）等 **B13e** 拍板 —— 本批刻意不动，因为它要把上一批刚立的用例翻面。**B13e 已出**（D60）= 对外默认接口的口径拍板 + 回退：**广告的那一枚必须就是应答的那一枚**。先证伪 D59-4 那条危险（`TypeLibBuilder` 只有 `TKIND_DISPATCH`/`TKIND_COCLASS`，客户从库里学不到接口虚表布局 ⇒ "第 3 槽打到 `GetTypeInfoCount`"不成立），再收同一次读码撞见的真问题：B13c 把 `.tlb` 的 coclass DEFAULT 引用重定向到 `_IProbe`（库里 0 成员），而服务器 `GetIDsOfNames`/`Invoke` 认的是类的公有成员那一档（`_CImpl`）⇒ 两边点不到。三处一起退回（表的 `defaultIfaceIid` 改由类型库回写值说话、`ifaceIids` 里 `[Default]` 同名接口那条覆盖分支删掉、类型库的 DEFAULT 重定向连同延后登记机器拆掉），coclass 的 CLSID 与接口自己的 IID **仍**取唯一出口 ⇒ 甲判据不动；`(i)` 的全量形态（滤掉 `ifaceIids` 里新式那几项）刻意没走，那会让表与 vtable 劈成两枚 GUID，正解归 B16（D60-4）。用例两条翻面/升级：`cc_dll_identity_single_source`（`IID_vb6def_CImpl` 回 `0x7CA8CD81`）、`cc_dll_tlb_matches_table`（甲 + 乙两条判据，负控在 BASE 上被乙判红）；那条 `…is a vtable interface…` 信息行随前提一起删除。**B13 五格到此出完（a/b/c/d/e）** | `9785f4f`(B13a)、`b1a8e58`(B13b)、`bd38798`(B13c)、`9df23ba`(B13d)、`b10ec1a`(B13e) |
| B14 | P6 | IDispatch 四件套接入新式接口（GetTypeInfo/GetIDsOfNames/Invoke + DispId 表） | ☑ **B14 已出，但范围被自己的测量重裁（D61）**：三件测量先行 —— ① **四件套本身是通的**（新探针 `tests\tools\disp_probe.c`：`LoadLibrary` + `DllGetClassObject` + `CreateInstance(IID_IDispatch)`，不查注册表；对真产物 `TestAXDLL.dll` 实测 `Add(2,40)=42`、`SetValue(7)`→`GetValue()=7`、`GetTypeInfo(0)` 回 coclass 那份（`kind=5`、`cFuncs`=0 是 coclass 常态）、未知名 `DISP_E_UNKNOWNNAME`）⇒ **缺的不是四件套，是成员面**；② 薄指针 `vb6_ivtbl_<I>` = `{QI,AddRef,Release,自有槽}`，**没有 IDispatch 那四槽** ⇒ 要接就是 dual/布局改动，且**必须排在 B15 之后**（先动它就重演 D60 的"广告 != 应答"）；③ `comDispid` 全工程只有一处生产者（`driver_codegen_dll_typelib.inc:186`，只收类模块 Public 成员）⇒ **契约成员从来不在这张表里**，要发只有"违口径 (b) 当公有发"（两边假绿）或"真接口 = B15"两条。本批交付 = 把 DLL 这条管线从"字节一致"升到"真调用得通"：探针 + 助手 `Test-DispatchInvoke`（`tests\disp_invoke.ps1`）+ 两条 gated 用例 `ax_dll_dispatch_invoke`（legacy 面真点通）与 `cc_dll_dispatch_iface_only`（只满足新式接口的类：**成员面必须为空** ⇒ `NAMES=ADD hr=0x80020006`；并把"接口 IID 由胖指针应答 `same=yes`"钉成实测事实 ⇒ B16 要改必须故意翻它）。**零编译器改动** ⇒ 产物逐字节不变（收线 exe md5 与开工同一枚），负控 = 换成不存在的 CLSID ⇒ `GETFACTORY hr=0x80040111` 判红（新助手先证明能红）。另登记一条没动的弱点（D61-6）：`ComObj_Invoke` 在 invkind 配不上时退回"第一个同 dispid 表项"，而 VB6 里 Get/Let 共享 dispid 是常态 ⇒ 收紧属口径题，等拍板 | `ddf4e9b` | |
| B15 | P6 | 类型库导出：新式接口在库里发成真接口（`TKIND_INTERFACE`）+ 修"接口宿主被登记成假 coclass"的形状 | ☑ **B15 已出（代码 `941b6dc`，门 head = 合并 `0caa3c5`，Actions run #59 全绿）＝ 只交付"形状与身份那一半"**，成员面按读数刻意押到 B16（理由 D62-1 mode 8 + D62-3，见 CURRENT_BATCH 第 1 条①③）。四件测量落 D62，其中两条推翻既有判断：**① 接口成员的签名从来没到发码现场**（026/B02 口径：Interface 块只进 `Module::interfaces` 不进 `declarations` ⇒ 宿主 Class 符号 `memberNames` 恒空 ⇒ `_IProbe` 的 `cFuncs=0` 是这条路本来不通，D61-3 那条"DispId 表缺一处生产者"的根因订正）；**② `CreateTypeLib2` 的位数 flag 有字节后果**（同一份 `oVft=24`，`SYS_WIN64` 建库读回 24、`SYS_WIN32` 读回 48，两枚 flag 出的 `.tlb` md5 不同）⇒ 本批不动它，登记成 B16 的前置裁决。成立条件实测：`TKIND_INTERFACE` 拒 `FUNC_DISPATCH`（`0x800288BD`）、要 `FUNC_PUREVIRTUAL` + `oVft`；`VT_PTR` 不给 `lptdesc` 会让建库进程当场崩；0 槽真接口可建（mode 9）；coclass 引用真接口并标 DEFAULT 走得通（mode 5）。交付：`TypeLibBuilder::addVtableInterface`（新）+ `driver_codegen_dll_typelib.inc` 那条循环加接口宿主分叉（判据 = Pass E 命中且 `ifaces_` 有同名键，legacy 那条一行不动）⇒ 库里 `coclass IProbe`（一枚 `generateUuid(类名)` 另 mint 的假 CLSID + 一句"可创建"）与 0 成员 `_IProbe` 一并撤掉，换成 `kind=interface name=IProbe` 用 Pass E 那枚 IID；types 4→3、1648→1484 字节。判据换读法不放宽：`Test-TlbIdentitySingleSource` 通道 3 改读真接口行 + 两条**反面**断言（那两行再现即红），负控实测 old=RED(三条全中)/new=GREEN。护栏：16 件 `--emit-c` 逐字节 0 变化、全产物 A/B 只有 `cc_dll` 的 `.tlb` 变、两件存量 DLL 的库一字未动。根因一句话：那条循环是唯一没有 `isInterface` 过滤的 COM 消费者（对照 `cgen_util_dllentry_collect.inc:26`、`driver_codegen_dll_sync.inc:8`）。 | `941b6dc` | 门 #59（8 job 全绿，`Tests (vbp)` 33/0/1）+ `b15_emitc_guard.py` 16/16 + `b15_ab_all.py` 分类 0 + `b15_negctl.ps1` 负控 + `b15_measure1/8.ps1` 七种 mode 建库实测 |
| B16 | P6 | **D62 重裁后的真身**：接口成员的对外调用契约（`vb6_ivtbl_<I>` canonical 化 → 库里那一档发成员）+ 类型库位数 flag/oVft 口径 + 薄指针 IDispatch 面与"胖应答瘦"的收法 + DllRegisterServer 一族对新式 CoClass/类工厂的接线与 x86/x64 双验 | ☑ **B16 已出（代码 `578faa4`，Actions run #62 全绿）** = ① **薄面整条 canonical 化**：`vb6_ivtbl_<I>` 的 IUnknown 前缀 + 契约槽 + 它们的实现函数在 x86 下一律 `__stdcall`（BASE 的 x86 产物 `grep -c __stdcall` = 0，NEW = 21；x64 上 MSVC 忽略该修饰 ⇒ 一份生产码两架构通用）；② **类型库那一档如实发契约成员**：`cFuncs` = 2（`Ping`/`Got`）、每成员 `oVft=(3+槽)*指针宽` / `callconv=stdcall` / 原生返回 vt、ByRef 建 `VT_PTR` 链；③ **位数 flag 跟 `--arch` 走**（`CreateTypeLib2(is64_ ? SYS_WIN64 : SYS_WIN32)`）—— 存量库默认架构逐字节不变、`--arch x86` 只差 32 个字节的位数布局字，x64/x86 两枚读端读数逐项相同（D63-3）；④ **薄面出入口** `vb6_iv_thin_<C>`/`vb6_iv_claim_<C>`：接口 IID 的 QI 交**薄指针**（B14 那条 `QI_EXTRA same=yes` 故意翻面）、包装器把底座引用交还**最后一个薄引用**（类工厂那种"QI 完就 Release 包装器"的规范姿势下交出去的指针不悬空）；⑤ **行里第 4 项（DllRegisterServer 一族接线）经读码 + A/B 判定无需新改动** —— 注册写入（`vb6comserver.c:62-171`）读的就是同一张服务器表、身份自 B13b 起走 `coclassIds_` 唯一出口，B16 的 A/B 里 `.def`/exports/`.rc` 一字未动；**真注册的外部端到端验收归 B17**（D63 与 B17 的 CURRENT_BATCH 都写明）。判据五条全对上读数：`cFuncs`/`oVft`/`callconv` 三样读数（新探针 `tests\tools\tlb_slots.cpp` 升格入库 + 两条 x64/x86 契约用例）+ x86 真跑 `VTBL_GET_AFTER=42` + 存量逐字节（17 件 `violations=0` / A/B `unclassified=0`）；canonical 返回形状与跨世界身份合一按"断不了先断形状"留 B17（D63-5，取舍看 B17 测量②）。 | `578faa4` | 门 #62（8 job 全绿）+ `b16_emitc_guard.py` 17 件 `violations=0` + `b16_ab_all.py` `unclassified=0`（只有 `cc_dll` 的 `.tlb` 1484→1624）+ 负控 `b16_negctl.ps1`（`pass=0 fail=2`，四条 needle 全 miss + 反面断言命中）+ 五条新用例 `b16_cases.ps1`（`pass=5 fail=0`）+ 迁移探针（默认架构 diffbytes=0 / x86 diffbytes=32） |
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

### D35 B08e（把剩下的类成员发码路逐条接上派发或判死）实施记录 + 13 站地图（2026-09-24）

**本轮交付 = B08e-1：`With w` 块内的类成员调用接上虚表派发**（`cgen_expr_with.cpp:82` 那一站）。

1. **为什么这一站是真漏洞而不是收尾**：`With up As InhBase`（`up` 持 `InhDerived` 实例）里写
   `.Speak()`，改动前静默绑到 `vb6_InhBase_Speak` → 运行期答 `"base"`。这就是 B08d 用 INH23 修掉的
   同一个切片，只是换了一种写法，而 B08d 的地图（D32）没把 With 站列进必修两处里。
   **A/B 负控实测**：基线二进制（worktree `D:\.wt_b08e_base` @7f34a91，exe md5 `bc3eaa3b`）跑
   `Inh.vbp` → `INH27:FAIL base` / `INH28:FAIL hi bob (base)`；本批改后二进制（exe md5 `00a3a9b4`）
   → 两条 OK。INH29/30/31 两侧都绿，是"不许改坏"的对照：`With` 内**非槽**成员（`Property Let/Get`
   同名对、无括号裸 `Sub`）必须仍走原直调路。
2. **改法**：与 B08d 两处同口径 —— `resolveClassMemberCall` 的结果先留成 `directFnW`，再问
   `virtDispatchCallee(info.className, node.memberName, tempVar, /*mustDispatch=*/true, node.loc)`，
   认得出槽就换 callee，认不出（该类无表 / 该成员不是槽）保持原样。`With` 的接收者是块入口那个
   类实例 temp（`cgen_with.cpp` 声明成 `vb6_cls_<X>*` 的纯变量名）→ `cvtblObjIsPure` 恒真，
   所以 `mustDispatch=true` 不可能因"形状不支持"误报，纯粹是"不留静默直调"的表态。
   **两个方向坑**：① 属性写（`prop_let_`/`prop_set_`）不改写 —— 3.4b 根本不给 Let/Set 建槽
   （D33-7），拿 `node.memberName` 去查会命中同名的 `get_<X>` 槽，把写变成读；② `asCallCallee_`
   那一支仍走 `pendingChainObj_` 协议（Fix 090s），只是 `lastExpr_` 从裸函数名换成派发表达式，
   外层 `cgen_expr_call_com_bind.inc:343` / `cgen_call.cpp:331` 把它当 callee 拼参数 —— 派发表达式
   以 `->slot` 结尾不带右括号，不会误触发 Fix 083e 的"callee 已是 func(obj) 形式 → 拆开"路径。
3. **护栏**：8 文件 `--emit-c` 对基线 exe（7f34a91）逐字节全同 8/8（`.build/byteguard_b08e.py`，
   BASE/NEW 两个绝对路径改一下即可复用到下一批）。用例：`Inh.vbp` 运行期断言 26→31 条
   （`INH27..INH31`），`-Category syntax` 条数不变。
4. **本轮顺手做掉的可达性探针（下一轮别重做）**：`.build/probe_b08e/`（PB.vbp + PBBase/PBDerived/
   PBHolder，全在 `.build` 里、不入版本库，改完即弃）用**当前**二进制跑出的三条形状：

   | 形状 | 运行结果 | 发射出来的 C |
   |---|---|---|
   | `m_h.Speak()`（模块级字段，裸名接收者） | `derived` ✅ | `((const vb6_cvtbl_PBBase*)((vb6_cls_PBBase*)me->m_h)->__cvtbl)->speak(me->m_h)` |
   | `Me.m_h.Speak()`（同一个对象、同一个成员，多个 `Me.`） | **`base` ❌** | `vb6_PBBase_Speak((void*)me->m_h)` |
   | `With w : .Speak()` | `derived` ✅（本轮修的这条） | 派发表达式 |
   | `arr(1).Speak()`（类数组元素） | 编译不过 | `error C2224`（不是静默错码，是硬失败） |

   → **⑤ 站已被证实"可达且正在产出错代码"**，且接收者 `(void*)me->m_h` 是纯读、一行同口径改写即可
   （`thisArg` 与派发用的对象文本取**同一个串**，别拼两遍）；手册里"字段链 `me.m_oSocket.Pick()`
   正常派发"那句是**错的**，本轮已按此证据改正。②站（裸名模块级字段）证实**已在派发**，不用动。

5. **B08e-2（⑤ 站）实施记录**：`cgen_expr_member_voidptr_com.inc` 的 Fix 088b 分支里，
   `resolveClassMemberCall(fieldCls, …)` 的结果在 `thisArg` 算好之后交给
   `virtDispatchCallee(fieldCls, member, thisArg, /*mustDispatch=*/true, node.loc)`；
   `resolvedFn` 含 `_prop_let_`/`_prop_set_` 时跳过（3.4b 无该方向的槽，按成员名查会命中同名的
   `get_` 槽、把读表式子套到写上）。派发串与 this 实参共用 `thisArg` 一个串。
   **用例**：新类 `tests/cls_inh/InhHolder.cls`（`Private m_up As InhBase` + `Hold` + 三个成员），
   `Inh.vbp` 登记，断言 31→34 条。`INH32`（裸 `m_up.Speak()`）两侧都绿 = 对照；
   `INH33/INH34`（`Me.m_up.Speak()` / `Me.m_up.Greet(who)`）在**修复前二进制**
   （`.build/pre_b08e2_C3.exe`，md5 `00a3a9b4` = B08e-1 收线那颗）上实测
   `INH33:FAIL base` / `INH34:FAIL hi bob (base)`，修复后 OK；发射出来的 C：
   `((const vb6_cvtbl_InhBase*)((vb6_cls_InhBase*)(void*)me->m_up)->__cvtbl)->speak((void*)me->m_up)`。
   **顺带把"纯接收者"边界的第二条钉住了**：`pendingChainObj_`/`lastExpr_` 那套协议里换成派发表达式
   仍然正确（派发表达式以 `->slot` 结尾、不带右括号，不会误触发 Fix 083e 的"callee 已是 func(obj)"拆解）。
   **本批未做的半条**：`Me.m_up.Level`（属性**读**）实测不走 ⑤ 而走 ⑩，仍在静默切片 ——
   试写的 `INH35` 因此在修复后仍拿基类实现的 `5`，已连同 `Level` 属性对一起从用例里撤回（不把错行为
   钉成正例），证据与现成用例代码记在 ⑩ 行，下一轮照抄即可。
6. **B08e-3（本轮，零代码交付）= 把 ⑩/⑪ 两行的裁决钉死**，过程本身就是下一轮的施工图：
   - 先按上一版 ⑩ 行的判断在 `class_fallback.inc:224` 加派发 → `INH35` 仍拿 `5`（发射出来的 C 一字未变），   说明属性读**不走 ⑩**。把派发挪到 `m22_module.inc:120`（⑪）后 `InhHolder.c` 立刻炸出   `error C2039: "prop_let_level": 不是 "vb6_cvtbl_InhBase" 的成员` —— 落点找对了，但撞上了写路径。
   - **站点⑭（新增，是 ⑩/⑪ 的先决条件，不在这 13 个消费点里）**：`src/backend/cgen_util_comwrite.cpp` 的 Pattern C/D2（:216 起、:462 落文案   `/* Property Let via prop_get_ rewrite */`）处理 `obj.Prop = v` 的方式是**字符串级改写**——先按读上下文   发一遍 `vb6_<Cls>_prop_get_<P>(<接收者>)`，再把文本里的 `prop_get_` 换成 `prop_let_`/`prop_set_`。   callee 一旦是 `((const vb6_cvtbl_X*)(...)->__cvtbl)->prop_get_p` 这种派发表达式，盲换后缀就得到一张   **不存在的槽字段**（3.4b 明确不给 Let/Set 建槽 → `prop_let_level` 在表里没有）。
   - 三条可选出路（按破坏性从小到大，下一轮先选一条再动手）：(a) 在 ⑪ 只放行**只读属性**（`Property Get` 无同名 Let/Set 那份）——需要先给分析器加一个"该类面上这个成员有没有写方向"的查询，别用字符串猜；(b) 让 Pattern C/D2 认得派发表达式（识别 `__cvtbl)->` 前缀 → 该类属性**不参与**重写、改走一条真实的`prop_let_` 直调），代价是写方向永远不派发（与 3.4b 的判死一致，说得通）；(c) 把 ⑧⑨⑩⑪⑫⑬ 的"不纯接收者"与这条一起按 `VB3027` 判死，属性读继续切片。本轮**没选任何一条**：三条都要动 `cgen_util_comwrite.cpp` 或分析器查询，超出"一次一个改动"的门内余量。
7. **B08e-4（⑭ + ⑪）实施记录**：选型走的是**比 (a)(b)(c) 更小的一刀** —— 不动 `cgen_util_comwrite.cpp`，
   而是给发码层加一个左值标记：`cgen_state.inc` 新增 `bool suppressVirtDispatch_`，5 个
   `emitExpr(*node.target)`（`cgen_setlet.cpp` 两处、`cgen_assign_com_prop.inc` 一处、
   `cgen_setlet_set_prop.inc` 两处）求值左值期间置 true，①⑤⑪ 三处派发点见到它就直调。
   理由：Pattern C/D2 的字符串级改写**只消费左值文本**，所以只要左值永远不带派发表达式，它就永远看到
   自己认识形状；读上下文不受影响。**这条也解释了 B08e-3 没想到的事实**：裸 `m_up.Level = 5` 一直是对的，
   因为 `cgen_assign_prop_write.inc` 有一条**专门的属性写路径**（`/* Property Let */`），它在左值求值之前就
   把 `obj.Prop = v`（object 必须是 `IdentifierExpr`）直接发成 `vb6_<C>_prop_let_P(obj, v)`；
   带 `Me.` 前缀的写法落不进那条路径的 `IdentifierExpr` 判据，才退到 Pattern C/D2 —— 也就是说
   **⑭ 的触发条件恰好是"左值 + `Me.` 前缀"**，比"属性写"更窄。
   - ⑪ 的接线与 ①⑤ 同口径（`thisArg083d = "(void*)" + objExpr` 派发串与实参共用；`_prop_let_`/
     `_prop_set_` 与 `suppressVirtDispatch_` 双跳过）。发射形状（`InhHolder.ViaLevel`）：
     写 = `vb6_InhBase_prop_let_Level((void*)me->m_up  /* class var .m_up field */, 5);  /* Property Let via prop_get_ rewrite (Pattern C/D2) */`，
     读 = `((const vb6_cvtbl_InhBase*)((vb6_cls_InhBase*)(void*)me->m_up  /* class var .m_up field */)->__cvtbl)->prop_get_level(...)`。
   - 用例：`InhBase` 加 `Protected m_lvl` + `Overridable Property Get Level` + `Property Let Level`，
     `InhDerived` 加 `Overrides Property Get Level`（体内 `Me.m_lvl`），`InhHolder` 加 `ViaLevel()`/
     `ViaLevelBare()`，断言 34→36。**A/B**：基线（`.build/pre_b08e4_C3.exe` = B08e-2 那颗）上
     `INH35:FAIL 5`，本批 OK；`INH36`（裸接收者对照）两侧都绿。护栏 8/8；门 153/0/1/154（跑前后 exe md5 f2547af6）。
   - **仍未闭合的一半**：`Me.<字段>.<属性>` 的**写**永远直调（无槽，符合 3.4b），但若哪天给 Let/Set 建槽，
     `suppressVirtDispatch_` 这条标记要一起放开，否则写方向静默不派发。手册已按本轮事实更新。
   - 为什么本轮不过门：改动全部还原后 `git status --porcelain src tests` 为空 → 树与 8987386（已过门 153/0/1/154）   **逐字节一致**，再跑一次全量门不测任何新东西；只把 `.build/C3.exe` 重建回 HEAD 源码   （增量，`abcac1cd`），并用 8 文件 `--emit-c` 对 `pre_b08e3_C3.exe`（= B08e-2 那颗）全同 8/8 + `Inh.vbp` 跑回 34 条断言全绿，证明还原干净。纯文档批不跑门的先例见 B00。

8. **B08e-5（⑥⑦ 站：默认属性调用式 `m_up(9)`）实施记录 + ④ 判为走不到**：
   - 两处 Pattern L（`cgen_expr_call_callee_ident.inc:144`、`cgen_expr_call_callee_member.inc:224`，
     文本一模一样只差缩进）在 `objExpr` 之后加同口径的
     `virtDispatchCallee(fieldType, "Item", objExpr, /*mustDispatch=*/true, node.loc)`，带
     `suppressVirtDispatch_` 保护；派发串与实参共用同一个 `objExpr`。
   - **可达性是靠两条对照探针定下来的**（兑现 B08e-3 的教训）：同一实例、同一 `Item`，显式
     `m_up.Item(3)` 早已派发（发射 `((const vb6_cvtbl_PBBase*)(...)->__cvtbl)->prop_get_item`），
     默认式 `m_up(4)` 发的是 `vb6_PBBase_prop_get_Item(me->m_up, vb6_VariantFromValue(4))` →
     运行期答 `base4`。修复后两式一致答 `derived`。用例：`InhBase` 加
     `Public Overridable Property Get Item(ByVal v As Variant) As String`、`InhDerived` 加 `Overrides`、
     `InhHolder` 加 `ItemDefault()`/`ItemBare()`，断言 36→38（`INH37` 判别项、`INH38` 两侧都绿的对照）。
     **坑一条**：`Item` 的索引参数**必须声明成 `Variant`** —— 这一路固定用 `vb6_VariantFromValue` 打包，
     声明成 `Long` 时默认式撞 `error C2440: vb6_VARIANT → int32_t`（既有缺陷，与本批无关，也说明
     Pattern L 的适用范围只有 Variant 索引的默认属性）。
   - **④（UDT 对象字段）判为"派发无事可做"**：`u.h.Speak()` 连编译都过不去（C2039，发的是非法 C
     `u.h.Speak()`）—— UDT 对象字段在**调用位置**没拿到 `/* udt objfield */` marker，落到
     `class_fallback.inc` 末尾 `obj + "." + member` 的兜底。**登记为新站点⑮ = 路由缺陷**
     （不是派发缺陷）：先让 `u.h.M()` 编得过，再谈派发；它不在 `resolveClassMemberCall` 那 13 站里，
     别混进 B08e 的收尾计数。


**13 站地图（下一轮直接照此动工；站号 = `resolveClassMemberCall(` 的其余消费点）**

先记两条**判定前提**（本轮读码核实，省得下一轮重查）：
- 能否有槽 = 该类在不在 `ClassChainRegistry::classes_`：`driver_classchain.cpp:381` 的门槛是
  `mod->isClassModule && classTypeParams.empty() && !isInterfaceModule` → **.frm 永远无槽**、
  **泛型模板类（含其特化扁名）永远无槽**、接口宿主无槽；`.cls`/`.ctl`/`.pag` 都算类模块，
  所以 **UserControl 可以有槽**（B08e 不能把 `.ctl` 当外部形状跳过）。
- **cgen 期诊断 `--syntax-only` 抓不到**：`virtDispatchCallee` 的 `mustDispatch` 报错发生在发码，
  而 `driver_compile.cpp` 的 syntaxOnly 早退在 :426、3.4b 在 :376 → 3.4b 的判死（如 Let/Set 槽）
  能用 `Test-SyntaxFail` 断言，**B08e 要加的 VB3027 判死不能** → 需要先给 `run_tests.ps1` 补一个
  "编译必须失败 + 断言 stderr 文本"的 `Test-VbpFail`/`Test-CompileFail` 助手（`Test-SyntaxFail` 的
  `cmd /c` 合并 stderr 写法可照搬，把 `--syntax-only` 换成真编译）。

| 站 | 文件:行 | 接收者形状 | 裁决 |
|---|---|---|---|
| ① | `cgen_expr_with.cpp:82` | With temp（纯） | ☑ **已接派发**（本轮，见上） |
| ② | `cgen_expr_member_class_module.inc:20` | 只做"有没有这个成员"的探针，命中后落进已接派发的优先级2 | 判**无需改**（探针；上面探针表已证 `m_h.Speak()` 真在派发） |
| ③ | `cgen_expr_call_callee_member.inc:141` | 探针（`resolvedFn` 只测空，非空即交给已接派发的 MAE 路） | 判**无需改**（探针） |
| ④ | `cgen_expr_member_generic_access.inc:136` | `(void*)<UDT 对象字段链>` | **B08e-5 实测：这条现在走不到"静默直调"** —— `u.h.Speak()`（`Public Type … h As PBBase` 的 UDT 对象字段）编译期就炸：`error C2039: "Speak": 不是 "vb6_cls_PBBase" 的成员`，发出来的是 `u.h.Speak()` 这种非法 C（UDT 对象字段在**调用位置**没拿到 `/* udt objfield */` marker，落到 `class_fallback.inc` 末尾`obj + "." + member` 的兜底）。→ 派发在这里**无事可做**；真正的缺陷是**路由**，登记为新站点⑮（D35-8），不在这 13 个消费点里 |
| ⑤ | `cgen_expr_member_voidptr_com.inc:37`（Fix 088b typed 字段链） | `(void*)me->m_h`（纯读） | ☑ **B08e-2 已接派发**（`Me.m_h.Speak()`/带参的 `Greet` 两条断言钉住，见 D35-5）。**但只覆盖到 Sub/Function 的调用形状**：属性读走 ⑩，见该行的订正 |
| ⑥ | `cgen_expr_call_callee_ident.inc:144`（Pattern L，默认属性 `Item`） | `emitExpr(callee)` 出来的字段/变量（纯读） | ☑ **B08e-5 已接派发**（D35-8）。显式 `m_up.Item(9)` 早就对（走 ①/B08d），默认式 `m_up(9)` 此前静默切片 |
| ⑦ | `cgen_expr_call_callee_member.inc:224`（Pattern L 的另一半） | 同 ⑥ | ☑ **B08e-5 与 ⑥ 同批接上**（两处代码文本一模一样、只差缩进） |
| ⑧ | `cgen_expr_member_class_module.inc:140`（Fix 083c 属性返回对象） | `(void*)vb6_<Mod>_prop_get_<P>((void*)me)` → 含调用，**按构造即不纯** | **B08e-6 实测：这一支被优先级2 提前接走，对带槽成员不可达** —— `Own.Speak()`（Own 是本模块返回类对象的属性）在**改动前**就报 `VB3027`，报出它的是 `cgen_expr_member_class_module.inc:49`（优先级2 的 dispatch，`mustDispatch = defaultInstCls.empty()`），因为 Fix 090al 的 `inferClassTypeOfExpr` 能给属性标识符推出类名 → `itClassVar` 非空 → 根本走不到 :134 那个分支。`:140` 要命中必须"AST 推不出而符号推得出"，本轮没能构造出这种形状。→ **不改码**，用 `ci_n26_base/derived` 把这条既有契约钉住（判死口径与 D33-7 一致，只是先例已在） |
| ⑨ | `cgen_expr_member_obj_dispatch.inc:143`（083c/088d 同一族） | 属性返回对象，同 ⑧ | ☑ **B08e-6 接上派发（不是判死）** —— 地图把这一条当"同 ⑧"是**错的**：`Fix 088d` 这一支的接收者是 `(void*)vb6_ret_<函数名>`，即**本函数返回值的裸 C 局部**，纯读、`cvtblObjIsPure` 恒真 → 与 ⑤ 同法一行改写就派发。改前 `vb6_VtPos3Base_Speak((void*)vb6_ret_Maker)`、改后 `((const vb6_cvtbl_VtPos3Base*)((vb6_cls_VtPos3Base*)(void*)vb6_ret_Maker)->__cvtbl)->speak(...)`，运行期 INH39 由 `base` 翻成 `derived`（见 D35-9） |
| ⑩ | `cgen_expr_member_class_fallback.inc:224`（Fix 088c AST 兜底链） | `(void*)<任意已发码头>`，多为链式调用 | **上一版（B08e-2）把这条改成"与 ⑤ 同法可接"，本轮实测证否：`Me.<字段>.<属性>` 的读根本不走这一支**（本轮在此处加了派发，`INH35` 照旧拿基类实现的 `5`；把 ⑤ 的发射形状与这里对比后定位到 ⑪）。→ 裁决退回**判死**（与 ⑧⑨⑪⑫⑬ 同批处理：接收者按构造含调用时出 `VB3027`）。本轮改动已 `git checkout` 还原，`src/`+`tests/` 与 8987386 逐字节一致。**☑ B08e-6 已生效**（`ci_n25`）：`k.Own.Speak()` 改前发 `vb6_Vt25Base_Speak((void*)vb6_Vt25Base_prop_get_Own(k))`、运行期答 `base`，改前 `--emit-c` 退出码 0；改后出 `VB3027`、退出码 1。这一支的接收者是"纯读的已发码头"时（`me->m_x` 形状）会直接派发，所以接的是派发而不是判死 —— 判死只落在含调用的那一半 |

| ⑪(已交付) | `cgen_expr_member_m22_module.inc:120`（Fix 083d 非标识符 object）**← B08e-4 已接派发，见 D35-7**；原文如下 | | `"(void*)" + emitExpr(object)`：`Me.<字段>` 这类字段链发出来**是纯读**（地图上一版"按构造即不纯"说过头了） | **属性读的真正落点，但不能直接接派发** —— 本轮实测：这一支发出来的读文本会被`src/backend/cgen_util_comwrite.cpp` 的 **Pattern C/D2 字符串级重写**复用（`obj.Prop = v` 是先发一遍读、再把文本里的 `prop_get_X` 换成 `prop_let_X`），callee 换成派发表达式后它照样盲换 → `__cvtbl)->prop_get_level` 被改成 `->prop_let_level` → `error C2039: "prop_let_level" 不是 "vb6_cvtbl_InhBase" 的成员`。**先决条件 = 站点⑭**：让 Pattern C/D2 认得派发表达式（见 D35-6），否则这条只能连属性读一起搁着 |

| ⑫ | `cgen_expr_member_voidptr_com.inc:119`（Fix 015 方法链） | 内层调用返回实例 → 不纯 | ☑ **B08e-6 判死已生效**（`ci_n24`）：`k.Chain(2).Speak()` 改前发 `vb6_Vt24Base_Speak(vb6_Vt24Base_Chain(k, 2))` 并答 `base`，改后出 `VB3027`。**顺带一条实测**：内层 `Chain` 只要被 `Overrides` 过就会先派发（形状 `vb6_KBase_Speak(((const vb6_cvtbl_KBase*)...)->chain(k, 2))`），外层照旧切片 —— 也就是这一族的"内层对、外层错"是同一个洞的两个半边，判死把错的那半边变成响亮的那个 |
| ⑬ | `cgen_expr_member_form_builtin.inc:178`（`.ctl` 子控件） | `(vb6_cls_X*)vb6_UC_InstanceOf(<hwnd>)` → 含调用，不纯；而 `.ctl` **可以**有槽（见前提） | **B08e-6 改判：无需改**（原判"判死"作废）。接收者文本确实含调用，但 `vb6_UC_InstanceOf` 发的是 `r->me` —— 宿主创建该控件时登记的那个 `.ctl` 类的实例（`src/rtl/core/vb6forms/uc/uc_host.c:309`，登记点 `vb6_UC_HostCreate(typeName, …)`），**动态类型恒等于静态类型**，直调即正确。这与 D35 里"预声明实例 `X_Default()` 传 `mustDispatch=false`"是同一条理由；在这里放 `mustDispatch=true` 只会给**正确**的代码凭空造出 VB3027。VB6 也没有"把一个 `.ctl` 实例换成更深派生类"的途径（子控件不是运行时可赋值的对象变量） |

**D35-9 B08e-6（⑧⑨⑩⑫⑬ 判死批 + `Test-CompileFail` 前置）实施记录（2026-09-24 06:16–）**

1. **前置助手**：`tests/run_tests.ps1` 新增 `Invoke-CodegenProj` / `Test-CompileFail` / `Test-Compile`
   三个函数（`Test-SyntaxFailMulti` 的 `cmd /c ... 2>&1` 合并写法照搬，把 `--syntax-only` 换成
   **`--emit-c`**）。为什么是 `--emit-c` 而不是真编译：`--emit-c` 跑完前端 + 全部语义阶段 + 发码，
   正好越过 `driver_compile.cpp:426` 那个 syntaxOnly 早退点，又不调 cl.exe/link（负例一条几秒 vs
   真编译几十秒），且实测**不在源码目录落任何文件**（`tests/cls_neg` 跑前跑后 `ls` 全同）。
   判据仍是"退出码非 0 **且** 输出含指定 ASCII 文本"。
2. **开工前的实测把三条裁决改了**（地图上一版按"接收者形状"推，本轮按发射文本核）：
   - **⑧ 不是判死点，是已经死了**：`Own.Speak()`（Own = 本模块返回类对象的属性）用**改动前**的
     二进制就报 `VB3027`，报它的是优先级2 的 `class_module.inc:49`（`mustDispatch = defaultInstCls.empty()`），
     因为 Fix 090al 的 `inferClassTypeOfExpr` 给属性标识符也推得出类名 → `itClassVar` 命中 → 走不到 :134。
     `:140` 要命中需要"AST 推不出而符号推得出"，本轮构造不出这种形状 → **不改码**，`ci_n26` 钉住既有契约。
   - **⑨ 不是判死点，是能接**：`Fix 088d` 的接收者是 `(void*)vb6_ret_<函数名>`，本函数返回值的裸局部 →
     纯读 → 与 ⑤ 同法接派发（这是本轮唯一的行为改进，见 3）。
   - **⑬ 判死是错的**：`vb6_UC_InstanceOf(hwnd)` 发的是 `r->me`（`uc_host.c:309`），即宿主为该控件按
     `typeName` 创建的那个 `.ctl` 实例 → 动态类型恒等于静态类型 → 直调本来就正确；在这里放
     `mustDispatch=true` 等于给正确代码凭空造错。改成**无需改**（理由与 D35 里"预声明实例传 false"同源）。
3. **本轮交付**：⑨ 接派发、⑩⑫ 判死（`mustDispatch=true`，三处都带 `suppressVirtDispatch_` 与
   `_prop_let_`/`_prop_set_` 两道既有保护，派发串与 this 实参共用同一个串）。证据三条并排：
   - ⑨ `ci_pos3`：改前 `vb6_VtPos3Base_Speak((void*)vb6_ret_Maker)` → 改后
     `((const vb6_cvtbl_VtPos3Base*)((vb6_cls_VtPos3Base*)(void*)vb6_ret_Maker)->__cvtbl)->speak((void*)vb6_ret_Maker)`。
     运行期 `Inh.vbp` 断言 38→40：**INH39**（`RefOf.Speak()` 在派生实例上，改前 `FAIL base`、改后 OK）、
     **INH40**（基类实例上的对照，两侧都 OK）。A/B 用 `.build/pre_b08e6_C3.exe`（同一个工程：39 OK/1 FAIL）。
   - ⑩ `ci_n25_base/derived`：改前 `--emit-c` 退出码 **0**、发 `vb6_Vt25Base_Speak((void*)vb6_Vt25Base_prop_get_Own(k))`；
     改后退出码 **1** + `VB3027`。
   - ⑫ `ci_n24_base/derived`：改前退出码 0、发 `vb6_Vt24Base_Speak(vb6_Vt24Base_Chain(k, 2))`；改后 1 + `VB3027`。
   - `ci_n26`（⑧）：改前改后**都是**退出码 1 + `VB3027` —— 它的价值是防回归，不是新证据，别当新证据记。
4. **护栏与影响面**：8 文件 `--emit-c` 对 `pre_b08e6_C3.exe` 逐字节全同 8/8；`-Category syntax`
   本地 78→**82 条全绿**（+3 判死负例 +1 正例）。判死只在"该类有虚槽"时生效，而槽要求链上有
   `Overrides` → 全仓除 `tests/cls_inh`、`tests/cls_neg` 与 `.build/probe_*` 外没有任何工程用
   `Overridable`（`grep -rln Overridable --include=*.cls --include=*.bas --include=*.frm` 只剩探针三个文件），
   所以这三条改动的可达面就是本轮新用例本身。
5. **踩到一条既有缺陷（不在本批范围，登记）**：`com_entry.c` 给派生类的**覆盖**过程发 extern 时，
   用的是基类的 C 类型名（`extern vb6_cls_KBase* vb6_KDerived_Chain(...)`），而 `typedef struct vb6_cls_KBase`
   的前置声明排在它**后面** → C 编译期 `error C2143: 缺少"{"`（x64/Debug 实测，`Chain As KBase` +
   `Overrides Chain` 即触发；未被覆盖的继承形状不触发，因为走的是 `void*` 那条）。这属于 P6/B13 的
   "从基类继承的成员的 COM 对外暴露"一片，**未修**，本批探针改成不覆盖返回对象的方法绕开。
6. **剩余**：⑮（UDT 对象字段的调用位置路由，D35-8 登记）与 ②③④⑬ 的"判为探针/走不到/无需改"
   三条已在表里落定 → B08e 的 13 站到此全部出完，下一批 **B08e-7 = 站点⑮**，之后 **B09 = `MyBase`**。

建议次序：**④⑤（纯形状、真收益）→ ⑥⑦（默认属性）→ ⑧⑨⑩⑪⑫⑬（判死，一批一次做完，
共用同一个 `Test-CompileFail` 助手与同一条负例族）**。每站都要"最小用例先证可达"（④⑤⑥⑦ 若编不出
"静默直调"的用例就在总表记不可达并跳过，别凑数改码），⑧⑨⑩⑪⑫⑬ 的负例断言的是"必须报 VB3027"，
本身即证据。全部站号出完再开 B09（`MyBase`），两套口径不能并存。

### D36 站点⑮ 勘察（2026-09-24 07:16–07:45，本轮零代码改动）

> **本节的落点在 D37 被订正**：⑮a/⑮b 不是 cgen 的两处独立缺陷，而是同一个"UDT 成员的对象类型名在语义层就丢了"的两个症状；坏的范围只有"UDT 与类跨模块"。**改法看 D37，别照本节末尾的落点清单动手。**

**结论先说**：⑮ 不是"派发漏接的一站"，而是 **UDT 里放工程类对象字段（`Public Type T … h As <类>`）这条
通路本身不通**。派发是它的第三步，前两步不通就永远没有可运行期观测的第三条。因此 ⑮ 从 B08e
的收尾里**摘出来单列**，B08e 的 13 站在 D35-9 就已经全部出完了。

**实测证据（`.build/probe_b08e7/`，二进制 = `.build/pre_b08e7_C3.exe`，md5 `90907230` = 40eea3f 的产物）**：

| 形状 | `--emit-c` 发出来的 C | 真编译结果 |
|---|---|---|
| `Set u.h = d`（u 是 `Dim … As TWrap`，`h As U7Base`） | `u.h = vb6_VariantFromValue(d);  /* Set */` | **error C2440**：无法从 `vb6_VARIANT` 转 `vb6_cls_U7Base*`（结构体字段本来就是 `vb6_cls_U7Base* h;`） |
| `u.h.Speak()`（无参调用） | `… vb6_BSTR_Concat(L"U1:", u.h.Speak())` | **error C2039**："Speak" 不是 `vb6_cls_U7Base` 的成员（+ 级联 C2198） |
| `p.h.Tag(3)`（带参调用，p 是 UDT **形参**） | `vb6_ComCall(vb6_ComGetObjectProp(p, L"h"), L"Tag", …)` | **编得过、走的是外部 COM 晚绑定** —— 工程类实例不是 IDispatch，运行期必崩（D20 族的老坑） |
| `Set w = u.h` 之后 `w.Speak()`（同一个对象，先落到变量） | `((const vb6_cvtbl_U7Base*)((vb6_cls_U7Base*)w)->__cvtbl)->speak(w)` | ✅ 正确且已按实例派发 |
| `With p : .h.Tag(4)` | 同第三行的 COM 晚绑定 | 同第三行 |

**关键一条.** `--emit-c` 对第一、二行**返回 0、不报任何诊断** —— 这两个缺陷是 C 编译期才炸的，
所以 ⑮ 的负例不能走上一批刚铺好的 `Test-CompileFail`（它跑的是 `--emit-c`），必须走真编译
（`-Category compile`/`vbp`）或者干脆用正例（修好之后必须编得过 + 运行期断言）。别把 ⑮ 的证据
记成"`--emit-c` 退出码 0 = 没问题"。

**三条子缺陷与落点（下一批按这个次序做，每条自带验收）**：
- **⑮a 写方向**：`Set <UDT 变量>.<对象字段> = <对象>` 被当 Variant 字段发。包装发生在
  `src/backend/detail/stmt/cgen_setlet_set_rhs.inc`（:31/:61 那两条注释就是它的动机：`With .SourceFile = obj`
  需要 `void*→vb6_VARIANT`），但它没区分"目标其实是 `vb6_cls_X*` 结构体字段" → 判据现成：
  `udtFieldObjCType(udtCType, 字段)`（`src/backend/cgen_util_classtype.cpp:338`）返回 `vb6_cls_*` 时
  **不包装**、直发 `target = value`。验收 = `Set u.h = d` 编得过 + `Set w = u.h : w.Speak()` 答 `derived`。
- **⑮b 调用方向**：`u.h.M()` 没走"标记 → 类方法分发"那条既有通路。机制本来齐了
  （`appendUdtObjFieldMarker` 在 `cgen_util_classtype.cpp:442-448` 打标记，
  `cgen_expr_member_generic_access.inc:125-150` 消费标记），但 UDT 字段访问**在调用位置**这一支
  发的文本没有标记（无参 → `class_fallback` 末尾 `obj + "." + member` 兜底；带参 → 落 COM 晚绑定）。
  验收 = `u.h.Tag(3)` 发 `vb6_U7Base_Tag((void*)u.h, 3)` 且**不走** `vb6_ComCall`。
- **⑮c 派发**：⑮a/⑮b 通了之后，`thisArg = "(void*)u.h"` 是**纯读**（不含调用括号）→ 与 ⑤/⑨ 同法一行
  接 `virtDispatchCallee(..., mustDispatch=true, ...)`，`u.h.Speak()` 才按实例绑定。硬约束照旧：
  派发串与 this 实参共用同一个串、Let/Set 方向不碰、左值求值期间 `suppressVirtDispatch_` 优先。

**为什么本轮不动手**：⑮a 改的是 `Set` 发码主干（VBMAN 时代的 `With .X = obj` 形状全靠它），
⑮b 改的是成员访问的分支归属（`class_fallback` 与 generic_access 谁先命中），两条都是**存量兼容面
很大**的通路，与本线的"虚表派发"没有共同判据，合起来远超一个可过门的批次。按"宁少勿滥 + 一次一个
改动"的纪律，本轮只把勘察结果与落点钉下来，代码一行未改（因此也没有门可跑，先例 = B08e-3 零代码不跑门）。

### D37 ⑮ 的真根因：UDT 成员的对象类型名在**语义层**就丢了（2026-09-24 07:31–08:05，B08f-1 尝试轮）

D36 把 ⑮a/⑮b 的落点记在 cgen（`cgen_setlet_set_rhs.inc` 的 Variant 包装、`generic_access` 的标记消费），
本轮照那个落点改了一遍，**实测同一条语句一字不变** → 那条守卫是死代码，已 revert。真正的原因在下面：

1. **两套解析各说各话**。UDT 成员 `h As <项目类>` 走 `resolveTypeRef`（`semantic_analyzer_typeref.cpp:29`），
   它对认不出来的类型名**一律回退 `Vb6Type::Variant`**（同文件 Fix 040a/069/157 那一大段注释就是这条兜底的历史账单）。
   项目类名要走到 `symTab_.lookupModule(...)` 才判成 Object，而**跨模块的 Class 符号在这个模块被分析时还没注入**
   （注入在 stage 3.5）→ 于是 `mi.type = Variant`。
2. **名字也一起丢了**。登记处 `semantic_analyzer_decl_type.cpp:31-40` 只在
   `mi.type == UserDefinedType` 或 `== Object` 两个分支里存 `mi.typeRefName = stRef->name`；
   Variant 分支什么都不存 → **类名在 `Symbol::UdtMemberInfo` 里根本没有**，后面谁也补不回来。
3. **cgen 却看得懂同一个字段**。结构体发射器（`cgen_decl_type.cpp:74-80` 那条回退）跑在 stage 3.5 **之后**，
   `lookupModule(类名)->kind == Class` 成立 → 发出来的字段是 `vb6_cls_U7Base* h;`。
   于是同一个字段：**布局侧知道它是项目类，成员元数据侧只知道它是 Variant**。
4. **所有下游都挂在元数据上**。`udtFieldObjCType` 只在 `mi.type == Object` 时才认对象字段
   （`vb6_cls_X*` / `void*` 两分支），Variant 成员一律返回 "" → `appendUdtObjFieldMarker` 不打标记 →
   调用位置落 `class_fallback` 末尾的 `obj + "." + member`（C2039）或被 COM 晚绑定抢走（`vb6_ComCall`）；
   而 Set 侧 `inferUdtFieldVb6Type(...) == Variant` 为真 → 包 `vb6_VariantFromValue`（C2440）。
   **⑮a 与⑮b 是同一个缺口的两个症状，不是两条独立缺陷**（D36 的三分法在这里要收拢）。
5. **判别实验（决定性的一条）**：把同一个 UDT 声明在**类模块内**（`U7Base.cls` 里
   `Private Type TInCls … h As U7Base`），此时该模块自己有 Class 符号 → `mi.type = Object` +
   `typeRefName` 有值 → 实测两条都正确：`Set t.h = me;  /* Set */` 与
   `vb6_U7Base_Speak((void*)t.h)`（仍是直调，派发要等 ⑮c）。
   → **坏的范围只有"UDT 与类跨模块"这一种**，不是整条 UDT 对象字段通路都不通。

**B08f-1 的做法（本轮 08:20 起按此实施）= 两处小改 + 真编译验收**：
- `semantic_analyzer_decl_type.cpp`：SimpleTypeRef 的兜底分支里，**即使解析成 Variant 也把 `stRef->name` 存进
  `mi.typeRefName`**（只多存一个名字，不动 `mi.type`）。**读侧逐条核过（本轮实测 grep，共 9 处）**：
  `cgen_util_classtype.cpp:351`/`:354` 在 `mi.type == UserDefinedType` / `== Object` 分支内、`:537` 判
  `bt == UserDefinedType`；`cgen_util_type.cpp:686`、`cgen_expr_call_callee_member.inc:86`、
  `cgen_expr_call_callee_withm.inc:82` 三处都是 `mi.type == UserDefinedType && !typeRefName.empty()`；
  `cgen_util_classtype.cpp:234-237`/`:273-276` 两处只看"非空"，但拿到名字后还要
  `lookupModule(name)->kind == UserDefinedType` 才认 → **塞进去一个 Class 名，这九处一律看不见**，
  嵌套 UDT 判定也不会因此多认出一个成员。
- `udtFieldObjCType`：加一条 Variant 分支 —— `lookupModule(typeRefName)` 命中 `SymbolKind::Class` 时按
  **类的规范模块名**返回 `vb6_cls_<Module>*`（与结构体发射器同一口径，别用引用名）。
- 验收：`tests/cls_inh` 里加"跨模块 UDT 对象字段"的运行期断言（**必须走真编译**，`--emit-c` 对这两种坏形状返回 0）；
  `Set`/调用两条 + 8 文件逐字节护栏 + A/B。

**B08f-1 实施结果（2026-09-24 09:20，代码 `77ecef1`）**：上面三处接线一起做完，⑮a/⑮b 一次治好，
并且**通路一通，D35 站点④ 才第一次可达** → 顺手把 ⑮c 的派发也接上了（没另开一批：那 8 行不接就是
"编得过、静默绑基类实现"，正是 D27-13 那一类，留着比接上更危险）。实测三条并排：

| 形状 | 改码前（`.build/pre_b08e6_C3.exe`） | 改码后 |
|---|---|---|
| `Set u.h = d` | `u.h = vb6_VariantFromValue(d);` → **C2440** | `u.h  /* udt objfield vb6_cls_InhBase* */ = d;`（合法 C） |
| `u.h.Speak()`（无参） | `u.h.Speak()` → **C2039** | `((const vb6_cvtbl_InhBase*)((vb6_cls_InhBase*)(void*)u.h)->__cvtbl)->speak((void*)u.h)` |
| `u.h.Greet("bob")`（带参） | `vb6_ComCall(vb6_ComGetObjectProp(u, L"h"), L"Greet", …)`（编得过、运行期解 vtable 崩） | 同上形状走 `->greet((void*)u.h, …)` |

验收走真编译：`Inh.vbp` 运行期断言 **40→43**（INH41/INH42 判别、INH43 基类对照），**改码前的二进制
编不过这个工程**（同一份源码：C2440 + C2039）；8 文件 `--emit-c` 逐字节全同 8/8、`-Category syntax` 82/0。

**还开着一层（本轮实测，登记为 ⑮d）**：UDT **本身**声明在"使用它的模块"之外的第三个模块时
（`TWrap` 放 `InhUdt.bas`、`Sub Main` 放 `InhMain.bas`），同样的坏形状**原样复发** —— 这一次卡在
`udtFieldObjCType` 开头那句 `symTab_.lookupModule(udtName)`：消费者模块的符号表里没有那个 UDT 符号
（跟 `mi.type` 无关，是 UDT 名的跨模块可见性）。本轮把测试用例改成"UDT 与使用点同模块"（= 已实测
修好的那一类），**没有**顺手扩大改动面。⑮d 的判据要先量一下：`knownUdtVars_`/`inferUdtTypeOfExpr`
在跨模块 UDT 上的可见面有多窄（`cgen_localdecl.cpp:278` 只登记本模块看到的名字）。

**⑮e（本轮顺手实测，未改，不是本批引入的回归）**：**属性写穿过 UDT 对象字段**仍不通 ——
`u.h.Level = 5` 改前发 `u.h.Level = 5;`、改后发 `u.h->Level = 5;`，**两种都是非法 C**
（`Level` 是属性不是结构体字段，两条路都没接到 `prop_let_`）。要治得走 `cgen_assign_prop_write.inc`
那条专用属性写通路，而它要求 object 是 `IdentifierExpr`（D35-6 记过这条前提）—— 判据与 ⑮a/⑮b 不同，另批。
**同一轮的另一条行为变化**：站点④ 接上派发之后，`Set t.h = me` + `t.h.Speak()`（UDT 声明在类模块内、
改前就编得过的那一类）从 `vb6_U7Base_Speak((void*)t.h)` 变成按 `__cvtbl` 派发 —— 即本批除了修通路，
还把**存量**的"UDT 对象字段 + 覆盖成员"从静默切片改成按实例绑定。这是要的语义，但它作用在已经用这一形
状的老工程上，门里若有钉住旧行为的用例应按新语义改用例（本轮 `bas`/`vbp`/`compile` 全绿，没有这种用例）。

**一条小瑕疵（未改，记着）**：修好之后标记注释会漏进发射语句
（`u.h  /* udt objfield vb6_cls_InhBase* */ = d;`）—— C 语法合法（注释即空白），而且**这是既有通道
本来就有行为**（类模块内声明的 UDT 在改码前就发成这样），故本批不动它；要清就照
`cgen_expr_call_arg_emit.inc:583` 的"发射前剥离标记"办法，在赋值发射点统一剥。

### D38 B09 的实施记录：`MyBase` 去虚化 + 构造链（2026-09-24 08:32–，代码 `02bac92`）

**接管点选在发码层按名字匹配，没有加 token**（与状态头 ⑤ 那条"关键字策略"预告的不同，理由记在这里）：
`MyBase` 现在只是 `MemberAccessExpr(IdentifierExpr("MyBase"), M)`，`cgen_expr_member_precheck.inc`
里 `Err`/`LastError`/`VBA` 那一条**按小写接收者名最早接管**的通路就是为这种伪接收者准备的先例，
在它后面加一支 `if (_objLower == "mybase")` 就够 —— lexer/token 表、parser、软关键字表全不动，
于是"零新语法逐字节不变"这条护栏是**构造上成立**的，而不是靠 Early-return 保证的。
代价：源码里名叫 `MyBase` 的变量会在类模块里被当成关键字（语料核查：`tests/`+`archive/`+`publish/`
的 `.bas/.cls/.frm` 里 `MyBase` 零命中，与 D-关键字策略那条同一份证据）。语义层只补一处：
`mybase` 在类模块里不再报 `VB3001 未声明的标识符`（否则每条合法写法都配一条噪声）。

**取哪一份实现**：`findMyBaseProc(基类视图, 成员名, 读/写)` —— 先查基类**自己的声明**，
没有再沿基类的继承面（`inhProcs`）找，即"就近声明"。这条与 B07b 中转函数用的是同一份 owner，
所以不会出现"桩能连、`MyBase` 连不到"。C 名一律 `cProcName(procBaseName(decl), access, owner 模块名)`
（= 表项与桩的同一套拼名），接收者实参 `((vb6_cls_<owner>*)me)` —— 前缀布局让它必然指向同一偏移。
**不查 `__cvtbl`**：这就是"去虚化"，与 `virtDispatchCallee` 无任何交集，因此 D35 那 13 站的判据、
`mustDispatch`、`suppressVirtDispatch_` 在这里一律不适用。读写分档：读上下文按
`Get > Function > Sub > Let > Set`（与 `resolveClassMemberCall` 一致），写上下文只认 `Let`/`Set`
（拿 `Get` 去写就是 C2198 或值被丢掉）。写侧另有一条入口：`MyBase.X = v` 原来落到
`cgen_assign_stmt_special.inc` 的 `Module.var` 回退，发的是 `vb6_MyBase_X = v`（C2065），
现由 `tryEmitMyBaseAssign` 接管。

**判死（`VB3028`，新增诊断码）**：① 本类没有 `Inherits`；② 基类面上没有这个名字；
③ 目标是基类的 `Private` 成员。③ 不是洁癖：Private 过程在 C 层就是 `static`（Fix 089e），
派生 TU 连不到，放行只会把错误推到链接期。

**构造链**：`New` 派生类时祖先的 `Class_Initialize` 按**根→叶**先跑（`emitClassInitChain` 插在
`emitClassFactory` 里"自家那份初始化"之前，字段默认值已置好）。基类那份是 `static` → 加桥接
`vb6_<基>_chain_init`，**只在**"这个类自己写了 `Class_Initialize` **且**确实被谁继承"时发
（`classIsBaseOfSomething` 扫 `clsreg_` 的 `baseKey`）→ 零继承工程逐字节不变。
`MyBase.Class_Initialize` 复用同一座桥（INH50 钉的就是这条）。`Class_Terminate` 的反序链**没做**。

**B09-0（前置缺陷，D35-9 ⑤ 登记的那条）已修**：`com_entry.c` 的类方法 extern 用**另一个类**的
`vb6_cls_X*` 当返回类型，而本文件的 `typedef struct vb6_cls_X X;` 是按类分块发的 —— 派生类块排在
基类块之前时那一行就是 `error C2143`。改成 `struct vb6_cls_X*`（自带标签，与同一批 extern 里
`_New`/`_Destroy` 的写法同口径），顺序依赖消失。**没修**的另一半：同一个方法在基类自己的 extern 里
发的是 `void*`（`mapType` 把类返回看成 Variant，`variableTypeName` 在自家符号上为空），派生类那份才是
`vb6_cls_X*` —— 指针返回两者 ABI 相同，不崩，但 Fix 184 的意图只做到了派生侧；归 P6/B13 那片复核。

**实测形状**（同一份源码，`pre_b09_C3.exe` → `C3.exe`）：

| VB 写法 | 改码前 | 改码后 |
|---|---|---|
| `MyBase.Speak()`（本类已 `Overrides`） | `vb6_MyBase_Speak()` → C2065 | `vb6_InhBase_Speak(((vb6_cls_InhBase*)me))` |
| `MyBase.Cat("x","y")`（带可选实参） | 同上 | `vb6_InhBase_Cat(((vb6_cls_InhBase*)me), …, 1)` |
| `MyBase.Name = v` / `MyBase.Name` | `vb6_MyBase_Name = v` → C2065 | `vb6_InhBase_prop_let_Name(…)` / `…_prop_get_Name(…)` |
| `MyBase.Class_Initialize` | `vb6_MyBase_Class_Initialize()` → C2065 | `vb6_InhMid_chain_init(((vb6_cls_InhMid*)me))` |
| `New InhDerived` 的初始化 | 只跑自家那份 | `vb6_InhBase_chain_init(me); vb6_InhMid_chain_init(me);` 再跑自家 |

**验收**：`Inh.vbp` 真编译 + 运行期断言 **43→52**（INH44/INH45 判别去虚化 —— 同两个成员经
`Me.`/`obj.` 答案是 "derived"；INH46 就近遮蔽；INH47/INH48 属性写读 + `Public` 字段；
INH49/INH50 构造链与桥；INH51 返回工程类的 `Overrides`（B09-0 的用例）；INH52 中间类实例）。
**改码前的二进制编不过这个工程**：6 个 C 错，其中 `C2143` 正是 B09-0、`C2065` 是 `MyBase` 那几条。
负例 `ci_n27_mybase_no_inherits` / `ci_n28_mybase_no_such_member` 走 `--emit-c`（`Test-CompileFail`
数组现在自带 needle）：改后退出码 1 + `VB3028`，改前退出码 0、静默发未声明符号。
护栏：8 文件 `--emit-c` 对 `pre_b09_C3.exe` 全同 8/8、`-Category syntax` **82→84 全绿**。

### D39 B09 顺手挖出的 x86 布局缺陷：继承字段里的 Private UDT 在派生 TU 退化成 `void*`（2026-09-24 09:55，登记为 **B09b**）

**症状**：`Inh.vbp` 在 **x64 全绿（52 条断言）**，同一份源码 **x86 段错误**。定位过程：把
`INH48`（`MyBase.Label = v` 之后 `MyBase.Label` 读回）拆成"只写"/"只读"两个函数，x86 在
`M48a` 之后、`LabelWrite` 之前崩 —— 也就是**写一个位于 `m_pt` 之后的基类字段**就越界了。

**根因（发射形状，`--emit-c` 一眼可见）**：`InhBase` 里 `Private m_pt As TPoint`（`TPoint` = 两个
`Long`），派生类的结构体把这份**继承来的字段**发成了别的类型：

| 结构体 | `m_pt` 那一行 |
|---|---|
| `vb6_cls_InhBase` | `vb6_type_TPoint m_pt;` （8 字节） |
| `vb6_cls_InhMid` / `vb6_cls_InhDerived` / `vb6_cls_InhSib` | **`void* m_pt;`** （x86 4 字节 / x64 8 字节） |

于是"派生实例的前缀必须与祖先 struct 一字不差"这条 B07b 的核心前提，在**含 UDT 字段**的基类上：
x64 靠 `sizeof(void*) == sizeof(TPoint) == 8` **巧合成立**，x86 上 `void*` 只有 4 字节 —— 派生结构体
比祖先布局**短 4 字节**，`m_pt` 之后的每个字段（`m_name`/`m_lvl`/`Label`/`g_viaRet`/`g_init`）偏移全错，
而且 `_New()` 按 `sizeof(派生)` 分配，基类那份过程按自己的偏移写 → **写出堆外**。

**归属**：**不是 B09 引入的**。`structFieldDecls`（`cgen_inherit.cpp:75`）从 B07b 起就按
`mapTypeRef(var.asType)` 发继承字段类型，而 `TPoint` 是基类的 **Private UDT**，在派生 TU 的符号表里
看不见 → `mapTypeRef` 回退成 `void*`。证据：x86 上**改码前**的源码（`git show 546e7bb` 那一套 +
`pre_b09_C3.exe`）已经在越界写 —— `INH30`（`.Name`）与 `INH39/INH40`（`g_viaRet`）写的都是
`m_pt` 之后的字段，只是那几次的值没触发崩溃（三条 FAIL 而非崩溃）。B09 的 `g_init`/`Label`
把同一个洞撞成了段错误。

**为什么本批不顺手修**：修法是"让派生 TU 拿到祖先 Private UDT 的**定义**并按 `vb6_type_<X>` 发字段"，
需要（a）按**声明所在模块**解类型（与 D37 同一族：消费者模块的可见面），(b）把那份 UDT 定义注入消费者
.c（`VB6_TYPE_<X>_DEFINED` 那个 guard 说明按需注入的先例已有），(c）逐字节护栏要覆盖"字段偏移变化"
这一类**布局**改动 —— 与 B09 的判据（成员解析与派发）不同条线。且它只影响 `Inherits` 这条新线
（全仓除 `tests/cls_inh` 没有别的工程写 `Inherits`），不是存量 VB6 工程的回归。

**B09b 的验收必须包含 x86**：这一族的教训是"x64 巧合等价"掩盖了布局错误。最小用例 =
基类含 `Private <UDT 字段>` + 该字段之后还有别的字段 + 派生类读写它，**x86 真编译 + 真跑**。
`Inherits 语句.md` 的"注意"里那条"基类含 Event / 前缀复制不成立"旁边要补一句本版 x86 的 UDT 字段限制。

### D40 B09b 的实施记录：祖先 `Private` UDT 的**类型名**进派生模块作用域（2026-09-24 10:18–，代码 `debb110`）

**判据先钉住，再动手**（D39 留的三条里第一条就是别照抄 D37 那一版）。做法 = 拿同一个工程改一个词：
`Private Type TPoint` → `Public Type TPoint`，其余一字不动，`--emit-c` 一比：

| 基类里 UDT 的写法 | 派生结构体里那份继承字段 | x86 build+run | x64 build+run |
|---|---|---|---|
| `Private Type TPoint` | `void* m_pt;` | **段错误**（47 行后崩） | 52/52 全绿（假绿） |
| `Public Type TPoint` | `vb6_type_TPoint m_pt;` | 53/53 全绿 | 全绿 |

一张表就把洞定死了：**不缺类型定义、不缺 #include、不缺尺寸，只缺名字**。`visit(TypeDecl)` 发 UDT
定义时不分访问级别（还带 `VB6_TYPE_<X>_DEFINED` 守卫，重复发也安全），定义的文本一直在基类自己的
`.h` 里；而 `mapTypeRef` → `lookupTypeSymbol` 走的是**消费者模块**的符号表，`getPublicSymbols()`
（`driver_crossmod.cpp:33` 那份池子）按 `access != Private` 过滤 → 祖先的私有类型符号从来就没进过
派生模块的作用域 → 解不出就回落到 `void*`。所以这不需要 D39 里设想的“按需把 UDT 定义注进消费者 .h”
那条重活，落点在**注入阶段**，不是发码阶段。

**改了什么**：`runCrossModuleResolution()` 末尾加一条独立 pass（在 O3 重载补跑之前）：对每个
`ClassChainView` 有 `baseKey` 的模块，沿 `chain` 走到每个祖先模块，把它符号表里 `UserDefinedType` /
`EnumType` 中**被继承字段用到**的那些（键 = `inhFields` 里 `VariableDecl::asType` 是 `SimpleTypeRef`
的名字），以 `isExternal + sourceModule=<祖先模块名>` 复制进消费者表，`UserDefinedType` 连
`udtMembers` 一起复制（与既有跨模块 UDT 注入同一条口径）。三道护栏：① 只在 `!classes_.empty()` 时跑；
② 消费者已有同名符号一律不抢（`lookupModule` 命中就跳过）；③ 只沿 `Inherits` 链、只补被字段用到的名字
→ 不写 `Inherits` 的工程一个符号都不会多（8 文件 `--emit-c` 对 `pre_b09_C3.exe` 全同 8/8，且那一版
BASE 是 B09 之前 → 连 B09+B09b 两批一起证了没漂）。

**顺手清掉两条陈旧红字**：`Inh35`/`Inh36`/`Inh39` 从 B07b 起就在 x86 上失败（本批之前实测过：改码前的
源码 + 改码前的二进制，x86 三条 FAIL 且不崩）。它们读的都在 `m_pt` **之后**（`m_lvl` 经 `Level` 属性、
`g_viaRet`）—— 同一个错位洞，不是派发逻辑错。修完 x86 与 x64 各 52/52、0 FAIL。

**门里补的口径（本批真正的长期收益）**：`tests/run_tests.ps1` 把 `Inh.vbp` 的期望清单提成
`$inhExpected` 变量，同一份清单跑 **两遍**：`cls_inh_pair`（默认架构）+ 新增 `cls_inh_x86`
（`-Arch "x86"`）。**布局类改动从此自动双架构覆盖** —— 只测默认架构的门证明不了布局主张（D39 的教训）。
`-Category syntax` 84/0 不变（顺带证明脚本改动能解析）。

**没做的相邻两件事**（别顺手，都要先量）：① 祖先 `Private` UDT 出现在**方法签名**（参数/返回值）上时
仍是同一条回落路径（`inheritedRetType`/`makeParamCType` 也调 `mapTypeRef`）—— 本批只治“按值嵌进结构体”
这一类，签名那类是 ABI 问题、判据不同；② 泛型特化与 `Inherits` 的组合、以及⑮d（UDT 声明在第三个模块）
仍然各自卡在自己的可见性判据上，与本洞不同条线。

### D41 B09c 的三条实测与处置（2026-09-24 10:56–，代码 `9eb2ca7`）

领到的活是"收尾 P3 的三条小尾巴"。**先三条一起量，再决定动哪条** —— 结果只有一条需要改码，
一条本来就修好了，第三条根本不该做：

**① `Set MyBase.<属性> = obj`：确实坏，且只坏在接收者。** 改码前的发射形状（探针实测，非推测）：

```c
vb6_T9Base_prop_set_Peer(MyBase, me);  /* Set Property */
```

函数名那条通路自己查对了（基类的 `prop_set_`），实参顺序也对（this 在前、值在后），
**`MyBase` 被当裸标识符发出去** → C2065。所以修法就是在 `visit(SetStmt&)` 的
Nothing / COM / 链式写分支**之前**接管，复用 Let 侧那条 `tryEmitMyBaseAssign`，加 `forSet=true`：
只认基类面上的 `Property Set`（`procRankForWriteSet`）—— 拿 `Let` 槽接对象引用会静默丢引用，
所以命中 Let 时判死而不是将就。改码后：`vb6_InhBase_prop_set_Peer(((vb6_cls_InhBase*)me), me);`，
`Set MyBase.Peer = Nothing` 同一条路一并成立（值侧就是 NULL）。

**② ⑮e（属性写穿过 UDT 对象字段）：B08f-1 之后其实已经通了，本批只补断言、不改码。**
D37 那条“改后发 `u.h->Level = 5;`，两种都是非法 C”是**在类型名还没修好的元数据上量的**。
现在的实测形状（`h As F9Base`，`u.c.Level = 9`）：

```c
vb6_F9Base_prop_let_Level((void*)u.c, 9);  /* Property Let via prop_get_ rewrite (Pattern C/D2) */
... ((const vb6_cvtbl_F9Base*)((vb6_cls_F9Base*)(void*)u.c)->__cvtbl)->prop_get_level((void*)u.c) ...
```

写绑根的 `prop_let_`（3.4b 不给 Let 建槽，本就应该直调），读按实例派发 —— 运行期 `F9=109`，
**x64 与 x86 都是 109**。仓库里的同形断言 = INH55 / INH59。

**③ `Class_Terminate` 的继承链：不做，因为 EXE 工程里它根本没有触发点。** 探针一次跑三种形状
（`Set d = New X` + `Set d = Nothing`、`Dim d As New X`、过程内局部对象出作用域），
`Class_Terminate` 里的 `Debug.Print` **一条都没出来**。全仓 `_Destroy` 的调用点只有
`cgen_iface_vtbl.cpp:444`（接口引用 Release 归零）与窗体/控件销毁那条。也就是说
“发一条反序的 terminate 链”= 发死代码，而且要验证它得先把生命周期做出来 —— 与
`Inherits` 无关的一个**既有缺口**（普通工程类对象的终止时机），归 P6/生命周期那片另批登记，
不在 B09c 里顺手做。

**两条写错的期望（自己踩的，记下来免得再踩）**：
- INH55 一开始期望 `Level` 经 `InhSib` 的字段读回 54 —— 错在**忘了槽是按分支建的**：`Level` 只被
  `InhDerived` 覆盖，`InhSib` 这条支链上它没有槽，两个方向都该绑根（4 进 4 出）。这正是
  B08d“筛选集合取链根 dynamicKeys”的语义，测试写错了不是编译器错了。
- INH57 期望 `MyBase.Class_Initialize` 重跑后是 `base;sib;base;` —— 错在根类的初始化是
  **赋值** `g_init = "base;"` 不是追加，重跑合法地把叶类那段抹掉，正确答案是 `base;`。
  （判别性没丢：不重跑才是 `base;sib;`。）

**验收**：`Inh.vbp` 断言 **52→59**，**x64 与 x86 各 59/59、0 FAIL**；A/B = `pre_b09c_C3.exe`
编不过这个工程（就那一条 C2065）；8 文件 `--emit-c` 对 `pre_b09c_C3.exe` 全同 8/8；
`-Category syntax` 84/0。INH58 是 B09b 那条线的最深用例（`InhSib` 隔两级持有根的 `Private` UDT
字段 + `SetPt/PtSum` 走真偏移）。

**B09b 的一处加强（同一批用例里顺手拿到）**：`TPoint` 声明在 `InhBase`、被隔两级的 `InhSib` 持有，
而 `InhSib` 自己不加字段 —— 于是“派生链上任一级把 UDT 字段发成 `void*`”在 x86 上会以**破坏**而不是
**恰好重叠**的形式现形（`InhDerived` 那侧因为后面还有自有字段把空洞吃掉了，所以旧用例看不出问题）。

### D42 B10 前置实测（2026-09-24 11:40–，只量不改码；探针在 `.build/probe_b10/`）

**探针形状**：`V10Widget.cls`（一个普通类，成员 `Area`/`Describe`/`Name` 的 Get+Let/`Rename` 全齐）
+ `V10Square.cls`（同文件内声明 `Interface IShapeV10` 与 `Interface INamedV10 Extends IShapeV10`，
再写 `Implements INamedV10 Via m_w` + `Private m_w As V10Widget`）。两文件一起过
`--syntax-only` 与 `--emit-c`（跨模块形状，不建 vbp）。

1. **`Via` 今天连软关键字都不是**（本轮 grep 复核 D1 记的四个登记点：`src/lexer/token.hpp`、
   `src/lexer/token.cpp`、`src/lexer/lexer_keywords.cpp`、`src/parser/parser_helpers.cpp` —— **`Via` 零命中**）。
   实测后果落在**语法层**：`V10Square.cls(19,22): error VB2003: expected end of statement (newline or :)`
   ＋ `(19,26): error VB2002: unexpected token at module level: m_w`，两条路（syntax-only / emit-c）
   退出码都是 1。也就是说模块级 `Implements` 只吃到接口名，`Via` 被当标识符撞在语句结尾上。
   → **B10 的第一层是词法+语法**（四处登记 + `Via <字段>` 尾子句），且**语义层与发码层今天没有观测面**
   （编不过就拿不到发射形状 —— 别一上来就改 `cgen_com.cpp`）。
2. **要免掉的那份手写长什么样，量出来了**：同一份文件去掉 ` Via m_w` → 语义层按 **Extends 展开后的每个槽**
   逐条报 `VB3012 Implements INamedV10: member 'IShapeV10.Area' (Function Area() As double) is not
   implemented by class 'V10Bare'`（本例 5 条：`Area`/`Describe`/`Name` 的 Get/`Name` 的 Let/`Rename`）。
   → Via 的验收面 = **这 5 条 VB3012 全部消失**、且**不写任何一个转发成员**就能按 `IShapeV10`/`INamedV10`
   槽位调通运行期。
3. **顺序约束（先想清楚再动手）**：契约检查在语义层（stage 3，跑在 codegen 之前），转发桩是发码层产物，
   所以"已实现"的判定**必须走符号面**（查 `V10Widget` 的成员表，D2 预留的 `implementsMap`/$itf$ 键正是为此），
   不能等桩发出来再验 —— 反过来桩那侧要能拿到"槽 → 被委托成员"的同一份映射，两边共用一个来源，别各算一遍。
4. **本批边界**（越界就是范围红线）：`Via` 目标只认**本类的对象持有字段**；VB6 老形式
   `Implements x ByRef y As New T`、属性 `Set` 向的委托、`Extends` 深度 >2 的链、以及**B06c**（接口值作实参）
   都不在 B10 —— 先把一条 `Interface … Via <字段>` 的桩与契约同时打通。

### D43 B10 实施（2026-09-24 11:51–，代码 `3c5d8e6`）= `Implements <接口> Via <持有字段>`

**层序被实测走了一遍**：D42 说 Via 今天停在语法层。补完词法+语法之后语义层才露出面，
再补完裁决才有发码层 —— 一层量一层改，每层都有独立可观测面，没有"三层一起猜"。

**五个落点**
1. **词法**：`Via` 走软关键字四件套（`token.hpp` 枚举、`token.cpp::isKeyword` 链、
   `lexer_keywords.cpp` 表、`parser_helpers.cpp` 的 `canBeName` 软表）。语料核查：
   `tests`/`archive`/`publish` 的 `.bas/.cls/.frm/.ctl` 里 `\bvia\b` **零命中** → 登记零误伤；
   另加正例 `itf_p04_via_soft_ident`（`Dim Via As Long` 照样过）。
   `isStatementStart` **故意不加**（与 `Inherits` 同口径：它是子句中间词，不是语句开头）。
2. **语法**：`parseImplements` 吃完点号限定名后可选 `Via <名>` → `ImplementsStmt::viaField`。
   没写 Via 不进分支 = 存量路径一字不动。`ast_clone.cpp` 的 ImplementsStmt 分支跟着拷 viaField。
3. **裁决落在 stage 2.7 新增的 Pass D**（`runInterfacePrepass` 末尾），**不在语义层**：判定要看
   "字段类型那个类自己 Implements 了没有",而那些类的符号要到 3.5 才注入本模块作用域 ——
   只有 2.7 把整工程的模块表看全。产物 `Driver::vias_`（小写类模块名 → `ViaView{ifaceKey,
   fieldName, holderModule}`）**同时**喂语义层与发码层（一份来源，正是 D42-3 立的那条）。
   判死两条：`VB3029`（不在类模块 / 被委托名不是 Interface 块 / 目标不是本类的对象字段 /
   字段类型不是工程内的类）、`VB3030`（那个类没实现该接口，**含链式 Via** —— A Via f(f:B)、
   B Via g 运行期能构成无限回环，一次诊断只报一条）。
4. **语义**：`checkNewStyleInterface` 命中委托 → 该接口逐槽 `VB3012` 全免（D42-2 量出的那份手写
   就是这几条），但本类自家写过的成员**仍按接口槽校签名**。
5. **发码**：`emitIfaceImplTables` 的槽循环里 `ivFindImplMember` 取不到实现时，若本类委托了该接口，
   发一个转调"持有对象同名槽"的适配器：
   `vb6_ivref_<I>* h = me->m_h ? &me->m_h->__iv_<I> : NULL;` 然后 `h->vt-><slotKey>(h, ...)`。

**为什么绕接口表、不直调 `vb6_<持有类>_<成员>`**（本批最关键的一次选型）：实现接口的成员按 VB6
惯例写 `Private`，而 Private 过程发成 C 的 `static`、跨翻译单元连不到 —— 这正是 B09 对 `MyBase`
私有目标判死的那条限制，照直调只会把错误推到链接期。表项发在持有类自己的 TU 里，函数指针从
对象里读，天然可用；代价是一次间接调用，而这与 COM 客户端看到的形状本来就一致。
顺带白拿到**逐槽合成**：本类写了的成员照旧直调，只有没写的槽走表（用例 `CViaDeleg` 的
`Property Get Name` 自家的、其余五个槽委托，VIA5 钉住这条）。

**Nothing 持有字段**：`Sub` 槽 no-op，有返回值的槽回 `<T> __via0 = {0};` —— 标量、指针、聚合
三种返回类型同一个写法都合法（Variant 的 `{0}` 就是 VT_EMPTY），不必按类型分岔。

**顺手量出三条既有洞（与 Via 无关，登记、不在本批修）**
- 接口变量上的 **`Property Let`/`Set` 写**：`s.Name = "x"` 发成 `vb6_s_Name = ... /* Module.Name */`
  → `error C2065`。"经接口变量写属性"这条路从来没通过（`tests/itf_xmod` 的接口只有 Get，
  所以一直没暴露）。后果：Via 的 `put_*`/`putref_*` 槽目前只有**发射形状**可证、运行期到不了。
- **带 `Optional` 形参的接口槽**：调用点发 `s->vt->greet(s, <实参>)`，而表项签名带 `_has_` 尾参
  → `error C2198` 实参太少（B04 的调用点没接可选参数标志位）。
- 接口块声明在 **`.bas`** 里时，`Dim x As I` 的类型认得出（发了 `vb6_ivref_I*`），但成员调用落回
  模块变量形状（`vb6_x_M()`）→ 编不过。`tests/itf_via` 因此改用 B03 的头行宿主。
  D1 说的"块形式可与别的声明共存于 `.bas`"目前只在语法/契约层成立 —— 这条要单独立项。
三条同属"到达槽的那条调用路形状缺失"，改在 caller 侧。

**验收**：`tests/itf_via`（IViaShape/IViaNamed 头行宿主 + `CViaHolder` 全 Private 实现 +
`CViaBare` 纯委托 + `CViaDeleg` 一半自家）断言 VIA1..VIA9 + VIA-DONE，**x64 与 x86 各 9/9**
（布局类纪律：适配器要解引用别的类的结构体字段，D40 那条）；A/B = `.build/pre_b10_C3.exe`
对同一工程停在 `VB2003`+`VB2002`（D42 的原始读数）；负例 `itf_n21`/`itf_n23`/`itf_n24`
（单文件）与 `itf_n22_via_holder_not_impl`（双文件 VB3030）；逐字节护栏**扩到 10 文件**
（新增 `itf_xmod\XWriter.vbp`、`cls_inh\Inh.vbp` —— 本批动的是接口适配器发射器和 2.7，
这两个工程是最直接的压力点）10/10 全同；`-Category syntax` 84→89。

### D44 B11/C01 前置实测（2026-09-24 13:24–，只量不改码；探针在 `.build/probe_b11/P1..P4`）

**结论先行：`CoClass` 今天的可观测面只在语法层，且层与层之间断在第一行。** C01 的验收面因此是
`--syntax-only` + `--dump-ast`（D15-5 那条单文件通路），不是运行期。

四份探针（P1 = `.bas` 里整块含内联 Interface 定义、P2 = `.cls` 头行宿主、P3 = `As Circle`/`New Circle`
消费点、P4 = 畸形块）对 `.build/pre_b11_C3.exe` 的读数：

1. **`CoClass` 不是 token**（`src/lexer/` 零登记，只是 Identifier）→ 模块级主循环落进兜底分支，
   第一行报 `VB2002 unexpected token at module level: CoClass`。关键形状：**错误恢复只 `skipToNextLine`
   跳一行**，块内其余行于是各自按"模块级"重新解析 —— 所以 P1 报出 20 条互相无关的错、P2 只 2 条
   （13 行的 `[CoClassId]` 名字已在白名单 → 14 行 `Interface` 被当成**顶层接口块**吞掉、只剩 17 行
   `End CoClass` 报错）。一句话：现在的 CoClass 块不是"整块被拒"，而是**被拆散后各部分各自为政**，
   这比整块被拒更危险（P2 那份"看起来只差两行"其实把块内的接口声明偷运成了顶层声明）。
2. **属性白名单缺两词**：`itfKnownAttrNames`（`parser_interface.cpp:42-50`）收了 `coclassid`/
   `comcreatable`/`coclasscustomconstructor`/`default`/`source`，但 **`ProgId` 与 `Implementation` 没登记**
   → `VB2012 Unrecognized attribute line`。026 四节样本正好用了这两个词，所以 C01 不补就连样本都进不去。
3. **布尔实参不支持**：`[ComCreatable(True)]` → `VB2012 Attribute line argument must be a string or an
   integer: True`。属性行整体是一个 token，参数用 `strtoll` 现解析，`True` 在这里**不是 token** ——
   所以修法是解析器认 `"True"/"False"` 字面，不是改 lexer。
4. **同行"属性 + 声明"今天不通**：`[Default] Interface ICircle` → `VB2003 expected end of statement`（列 15）
   + `VB2002 Attribute line must precede an Interface declaration`，根因是 `parseBracketAttrLine` 末尾无条件
   `expectEndOfStatement()`。026 四节把 `[Default] Interface ICircle` 写在同一行 → 必须处理。**裁决：不动
   Interface 那条路**（`itf_n06` 负例正守着那句诊断，且改了等于给契约块开新语法），只给 CoClass 块内
   用一个 `requireOwnLine=false` 的变体：属性行吃掉后若同行还有内容，留给块体循环判定。
5. **`End CoClass`** 单独报 `VB2002 unexpected token at module level: End`（P3/P4 各一处）。
6. **语料核查**（026 二节那份的复核）：`tests/` + `archive/` 里 `coclass` 作标识符 **0 次**（唯一命中是
   `coclassid`/`coclassinfo` 之类）→ 软关键字登记安全。
7. **同名不同物，别撞车**：仓库里"coclass"三处既有设施全是**外部类型库侧**的（`typelib_builder_coclass.cpp`
   的 `addCoClass`、`symbol_table.hpp:41` 的 `ComClass`、`vbp_parser.hpp:84-85` 引用工程枚举 coclass 烘 ProgID 表），
   与新的语言块无关；`symbol_table.hpp:248-252` 已有一套"本工程类模块遮蔽类型库 coclass"的机制 —— **C03 做
   `As <CoClass>` 名解析时必须先说清"块名与 typelib 的 ComClass 同名谁赢"**，别顺手新造第三套优先级。

**C01 落点裁决**（严格守在 026 六节"不校验、不发码"那一格）：
- 词法四件套照 B10/Delegate：`token.hpp` 枚举 + `token.cpp::isKeyword` + `lexer_keywords.cpp` +
  `parser_helpers.cpp` 软表；`isStatementStart` 不加。
- AST 新增 `CoClassDecl`（含 `CoClassIfaceRef{ifaceName,isDefault,attributes}`），存进 **`Module::coclasses`** ——
  与 `interfaces` 同一个手法：**不进 `declarations`**，语义/发码层看不见它，所以"零回归"是结构性的而非测试性的。
  可观测面靠 `--dump-ast` 的 printer 一行。
- 块体语法：**只收属性行与 `Interface <名>` 引用行**（`Interface IShape` 在 026 样本里没有 `End Interface`
  = 引用已声明的接口，不是内联定义），`[Default]` 标默认。内联定义/字段/过程一律按非法行报错。
- **属性行归属要单独定规则**（实施时才暴露，026 四节没写）：样本把 `CoClassId/ProgId/ComCreatable/
  Implementation` 四条写在**首个契约条目之前**，而 `[Default]` 只能贴在条目上 —— 若一律"属性行留给下一条
  `Interface`"，那四条就挂到了第一个接口上（`--dump-ast` 一眼看穿：`Interface IShape (4 attrs)`）。
  定案 = 按"**名字 + 位置 + 是否同行**"三条合判：`[Default]` 永远归条目；同行紧跟 `Interface` 的归条目；
  其余在首个条目之前归块、之后归下一条目。**不靠空行**（空行不是语法）。
- 属性通路复用现成的 `InterfaceAttr` 与 `parseBracketAttrLine`（顺带补第 2、3 条：白名单加两词、
  布尔折成 `numValue` 1/0 —— 不加新字段，因为 C02 求解身份时 `True`/`1` 同义）。
- **C01 的代码进了 `parser_interface.cpp`，没另立 `parser_coclass.cpp`**（本条覆盖上面"新文件"的设想）：
  方括号属性行的全部语法（白名单、实参形态、`requireOwnLine`）都在那个文件里，CoClass 块要复用的正是它；
  分家就得把 `itfAsciiLower` 一类判定复制一份，等于把"一处属性行语法"拆成两处。CMakeLists 因此未动。
- **不新增诊断 ID**：畸形子句只用 `Parse*` 家族（2001/2002/2005/2012）；校验语义（宿主 `.frm/.ctl`、
  `[Implementation]` 指向、默认接口 ∈ 集合、契约聚合、拒绝清单四类）整片留给 C03，`As`/`New`/`CreateObject`
  改写留给 C05，身份求解唯一函数留给 C02。
- 于是这几条 C01 **明知不做**（P5 探针量过形状，都在 C03 账上）：同一块里 `Interface X` 写两遍不报重复、
  块名与别的 CoClass/类/接口重名不报、引用的接口名不存在不报、`[Default]` 标两条不报。
- `[Default, Source]` 这种**逗号并列**的属性名今天仍整串比对 → `VB2012` 不认（026 四节末那条"只接受并存档"
  要等真做元数据时才补拆解，不在 C01 顺手加）。
- 一处**顺带**的既有文案要跟着改：`parser_module.cpp:155` 那句 "Attribute line must precede an Interface
  declaration" 现在也要涵盖 CoClass（同批改 `itf_n06` 的 needle，否则负例假绿）。

### D45 B11/C01 实施（2026-09-24 13:24–，代码 `e7c7a31`）= `CoClass…End CoClass` 落到 AST

**落点**（15 文件，全在 `src/` 的语法侧 + 用例；CMakeLists 未动）：

- 词法四件套：`token.hpp` 枚举 + `token.cpp::isKeyword` + `lexer_keywords.cpp` + `parser_helpers.cpp` 软表。
- AST：`ast_enums.hpp` 的 `ASTNodeKind::CoClassDecl` + `ast_fwd.hpp` + `ast_decl.hpp`（`CoClassDecl` /
  `CoClassIfaceRef{ifaceName,isDefault,attributes,loc}` / `Module::coclasses`）+ `ast_visitor.hpp` 的 visit 钩子
  + printer 三处（`visit(CoClassDecl&)`、`visitDeclHelper` 的 case、`print(Module&)` 的循环）。
  **这一串就是"新增一个模块级块类型要登记的位置"清单**，C04（attribute 折算）与以后任何新块照它走，省一轮 grep。
- parser：`parser.hpp` 两个声明（`parseCoClassDecl`、`parseBracketAttrLine` 加 `requireOwnLine`，默认 true
  → Interface 那条路行为不变）+ `parser_interface.cpp` 末尾的 `parseCoClassDecl` +
  `parser_module.cpp` 主循环 Interface 分支旁多一个 CoClass 分支。
- 属性行通路：白名单补 `progid`/`implementation`；实参形态补 `True`/`False`（折进 `numValue`）。

**三条以后还用得上的判断**：

① **结构性零回归优于测试性零回归**。新块存进 `Module::coclasses` 而不是 `declarations`，语义层与发码层
没有任何一条路能看见它 —— 于是"10 文件 `--emit-c` 全同"是**必然**而不是运气（护栏照跑，只是它测的是
"我没有手滑"，不是"我猜到了所有影响面"）。这与 B01 给 `interfaces` 的做法同构，D44 把它写成了选型理由。
② **只到 AST 的批，观测面要两条腿**：`--syntax-only` 静默只证明"不报错"，证明"属性行进到了哪一层"得靠
`--dump-ast`。本批正是 printer 一行 `Interface IShape (4 attrs)` 暴露出"写在首个条目之前的身份四件套挂到了
第一个接口上"—— 光看 `--syntax-only` 退出码这条 bug 会一路带到 C02 的身份求解里才炸。
③ **块的属性归属规则要按"名字 + 位置 + 是否同行"合判，不能靠空行**（空行不是语法）。定案见 D44；
`[Default]` 折成 `isDefault` 布尔位、不再同时留在 `attributes` 里，免得 C03 面对两处真相。

**验收**：`-Category syntax` **89→95**（新用例 `itf_p05`/`itf_p06` + 负例 `itf_n25`..`n28`）；A/B =
`.build/pre_b11_C3.exe` 对 `n25` 停在 `VB2002 unexpected token at module level: CoClass`（D44 的原始读数）；
10 文件 `--emit-c` 对 `pre_b11_C3.exe` **10/10 逐字节全同**；另做一条**惰性证明**（比护栏更贴本批）：
同一份 `p05` 工程**删掉 CoClass 块**后 `--emit-c` 与保留块时逐字节相同（唯一差异是两个源文件名注释，
`.build/probe_b11/inert/{w,n}.c`）—— 块在发码层一行都不发；带块的 `p05` 还做了**真编译 + 真运行**
（打印 `ok`）。C01 没有任何语义可断，所以本批**不建 vbp 工程**（一次显式的"批粒度小于一个工程"取舍，
运行期断言从 C05 起才有承载面）。门 = 本次 push（`3c5d8e6..e7c7a31`）触发的 Actions run，编号与结论记在状态头。

**下一格 = C02**（026 六节）：身份求解唯一函数 `CLSID/IID/ProgID` 三优先级 + 两次构建可复现。C01 已经把
输入面备好了 —— `InterfaceAttr` 的字符串/整数/布尔三形态与 `CoClassDecl::attributes` 就是它的读取起点。

### D46 B11/C02 前置实测与裁决（2026-09-24 14:17–，只量不改码）

**这一格没有运行期可观测面，所以先得把"怎么验收"定下来。** 实测四件事：

1. **现成的 mint 底子是两个 `.inc` 内的局部 lambda，跨不出翻译单元**：
   `cgen_util_dllentry_prelude.inc:40-72` 的 `generateClsid` / `generateIid`（同一套 FNV-1a × 4 条种子链，
   只差 4 个初始常量）。它们是 `CCodeGen::generateDllEntry` 函数体的片段（`cgen_util.cpp:32-36` 把四段
   `.inc` 串成一个函数）→ **别的层想复用只能复制一份**，而那正是 026 三节"禁止三处各读一遍"要防的事。
2. **这条 mint 路今天没有任何测试覆盖**：全部 `tests/*.vbp` 都是 `Type=Exe`，`generateClsid/generateIid`
   只在 ActiveX DLL 的 coclass 表里跑。所以"把 legacy lambda 换成调用新函数"这件事**在 C02 里做不起**
   —— 改了也没有逐字节护栏能证明没改坏。裁决 = **C02 只新建唯一入口，legacy 两枚 lambda 一字不动**，
   替换动作按 026 七-1 / D15-8 归 **B13/B16 分叉**（届时先要有 DLL 形状的用例）。新函数用**不同的种子常量
   + 带前缀的 seed 串**，保证两套 mint 不可能撞车，并把这条写进代码注释。
3. **vbp 三段式与工程名来源都还在 026 引的位置**（复核无漂移）：解析在 `vbp_parser.cpp:66-112`
   （`Class=Name; x.cls; {CLSID}`，只有 `{...}` 形态才存进 `entry.clsidStr`），收集在
   `driver_compile.cpp:74-81` 的 `classClsidMap_[lower(moduleName)]`（**键是类模块名**，不是 CoClass 名）。
   工程名侧现成只有 `projectBaseName_`（`ExeName32` stem，否则 vbp 文件名 stem，单文件编译为空），
   vbp 的 `Name=` 字段只在 `driver_compile.cpp:117` 就地用来兜 DLL 的 `dllProgId`，**没存下来**。
4. **可观测面要找第三趟才对**：`--dump-ast` 在 `driver_compile.cpp:313` 就返回了，跑在 stage 2.7
   **之前**，拿不到工程名与 vbp 表；`--emit-c` 又不许在 C02 发码。本条原写"那就发一条 note 级诊断"，
   **实测是错的**：`Diagnostics::toString()` 确实无条件拼所有级别，但 Driver 只在**某个阶段失败**时
   才把它整体打印（`driver_compile.cpp` 里 10 处 `if (!runXxx()) { std::cerr << diag_->toString(); }`），
   所以 note 在成功的编译里根本看不见 —— 用它当验收面会得到一个"永远为空"的断言。
   定案 = 走 **stderr 的 `C3: ...` 信息行**（`driver_compile.cpp:63` 的 "C3: 加载工程" 是同族先例，
   而且只在真有 CoClass 块时才发，零新语法工程一字不多），并且**不新增诊断 ID**（那条 note 没有读者）。
   两条踩坑留档：① 信息行绝不能走 stdout —— `--emit-c` 的 stdout 就是 C 文本，混一行就毁产物；
   ② 这是 D44/D45 那条"每层的可观测面不一样"的第三次应验，而且这次是**先假设了一个面、量了才发现它不通**。

**C02 落点裁决**：

- 新单元 `src/semantics/coclass_identity.{hpp,cpp}`：`CoClassIdentity{clsid,iid,progId,defaultIface,impl,
  clsidSource,progIdSource}` + **唯一入口** `resolveCoClassIdentity(block, env)` + `mintGuid(seed, 常量组)`。
  放 `src/semantics/` 与 `interfaces_registry.hpp` 同族（plain 结构、只读视图、不拥有 AST）。
- 三档优先级按 026 三节实现，其中 **vbp 档的查表次序**要新定（026 没写）：`[Implementation("X")]` 的 X
  先查 `classClsidMap_`，查不到再用 CoClass 块名查（VB6 的三段式挂在类模块上，而 CoClass 名常与该模块同名）。
- `<Proj>` 取值 = vbp `Name=` > `projectBaseName_` > 字面量 `"VB6EXE"`（第三条兜法沿用
  `driver_codegen_dll_entry.inc:27` 已有的兜序，不新造）。为此在 `driver_compile.cpp` 存一个
  `vbpProjectName_`。**与 com_entry 那侧用 `projectBaseName_` 是分叉的**，理由 = VB6 的 ProgID 语义是
  `<工程名>.<类名>`，而工程名就是 `Name=` 字段；这条分叉要写进 B13/B16 的对账清单。
- IID 档 = 默认接口的 `[InterfaceId]`（`IfaceView::guid` 早在 B02 就存了）> `"itf:<Proj>.<接口名>"` mint；
  块里没有 `[Default]` 条目 → `iid` 留空并在 note 里标 `no-default`（存在性校验按 D44 归 C03）。
- 求解结果挂 **`Driver::coclassIds_`**（key = CoClass 名小写），与 `ifaces_`/`vias_` 同族；
  求解发生在 stage 2.7 的**新 Pass E**（那时接口登记表刚建好、工程名与 vbp 表都已就位）。
  注：**同名两个 CoClass 块 = 后一个不覆盖前一个**（`emplace` 首值胜），这是 C03 的重复名检查欠的账，不在 C02 报。
- 用例面：三条档位各一条 note 断言 + **可复现性两条**（同一输入跑两次逐字节相同；换一个工程名再跑，
  确定性档跟着变而显式档一字不动）。为此给 `run_tests.ps1` 加一个 `Test-SyntaxNote`
  （退出码 0 **且** 输出含 needle —— 现有 `Test-Syntax` 只看退出码，`Test-SyntaxFail` 只看非 0）。
  确定性档的期望串用 python 独立复算 FNV-1a 得到，**不拿编译器自己的输出当基线**（否则等于没测）。


### D47 B11/C02 实施（2026-09-24 14:17–，代码 `f0b820d`）= 身份求解唯一函数

**落点**：`src/semantics/coclass_identity.{hpp,cpp}`（新编译单元，进 `vb6c3-core`）+
`runInterfacePrepass` 末尾的 **Pass E** + `Driver::coclassIds_`（key = 块名小写）+
`driver_compile.cpp` 存一份 `vbpProjectName_`。发码层一行未动。

**唯一入口的形状**：`resolveCoClassIdentity(const CoClassDecl&, const CoClassEnv&)` 是纯函数
（不写诊断、不碰全局），`CoClassEnv{project, vbpClsids, ifaces}` 把"块外面的世界"当参数传进去
—— 这样 C05/B13/B15 的调用点各自给上下文，而**算法只有一份**。三档优先级按 026 三节，
其中三处 026 没写、这次必须定的口径：

- **vbp 档的查表次序**：三段式挂在**类模块名**上（`classClsidMap_` 的 key 就是模块名），而 CoClass
  块名不必等于任何模块名 → 先按 `[Implementation("X")]` 的 X 查，查不到再按块名查。
- **`<Proj>` 的兜序**：vbp `Name=` > `projectBaseName_` > 字面量 `"VB6EXE"`。第三条不是新造的，
  是 `driver_codegen_dll_entry.inc:27` 那段注释里已有的兜法。**与 com_entry 那侧用
  `projectBaseName_` 是分叉的**（VB6 的 ProgID 语义是 `<工程名>.<类名>`，工程名就是 `Name=`），
  这条要进 B13/B16 的对账清单。
- **seed 的大小写**：`coc:`/`itf:` 前缀 + 工程名与块名**一律小写**（VB 大小写不敏感，`Circle`
  与 `circle` 必须同一个 GUID）；而 ProgID 默认值保留块名**原样大小写**（它是给人看的名字）。

**两条判据级别的纪律**：

① **期望串必须由另一套实现算出来**。三条断言里的两个 GUID 是 python 独立复算 FNV-1a（同一
seed 串、同一常量组）得到的，不是拿编译器自己的输出抄回去 —— 否则用例只能"重述实现"，
mint 换算法也测不出来。用例还钉了一条更强的形状：**同一批源文件、只换 vbp 的 `Name=`**，
派生档整串跟着动、显式档一字不动（可复现性 = "只有声明决定身份"这句话的直接证伪面）。
② **legacy 那两枚 mint lambda 一字未动**（D46-2）：它们只在 ActiveX DLL 路径上跑，而全部
`tests/*.vbp` 都是 `Type=Exe` → 改了没有任何护栏能证明没改坏。新 mint 用**不同的常量组**，
并存期间不可能撞车；合并动作按 026 七-1 / D15-8 归 B13/B16。

**验收**：`-Category syntax` **96→99**（`cc_id_explicit_tier` 三条档位全串相等 +
`cc_id_project_name_scope` 换工程名 + `cc_id_repeatable` 同一输入跑两次逐字节相同，新助手
`Test-IdentityNote`/`Test-IdentityStable`）；A/B = `.build/pre_b11c02_C3.exe` 对 `Id.vbp` **一行身份
都不出**；10 文件 `--emit-c` 对 `pre_b11c02_C3.exe` **10/10**；`Id.vbp` **真编译真运行**（`Id.exe`
打出 `cc_id`，三条身份行照出）。门 = push `f660e25..f0b820d` 触发的 Actions run（编号见状态头）。

**C03 从这里接手**：块名重名 / 引用的接口名不存在 / 接口名重复 / `[Default]` 标两条这四条
"明知不做"，加上拒绝清单四类；其中"块名撞类型库 `ComClass`"要先定优先级（`symbol_table.hpp:248-252`
已有一套"工程类遮蔽类型库 coclass"的机制，别新造第三套）。**身份一律只许读 `Driver::coclassIds_`**，
不得再自己解析属性行 —— 这是本格留下的唯一入口约束。


### D48 B11/C03 前置实测与裁决（2026-09-24 15:07–，只量不改码；探针在 `.build/probe_c03/q1..q11`）

**结论先行：C03 要拒的九种形状里，八种今天一声不吭，只有一种已经被既有检查挡住了。**
每种都用 `.build/pre_b11c03_C3.exe`（= C02 收线二进制）跑 `--syntax-only` 量过：

| 形状 | 今天的读数 | 归谁 |
|---|---|---|
| 两个同名 `CoClass CCA` | 静默（`coclassIds_` 首值胜，第二个整块消失） | C03 |
| 块名撞**模块名** | 静默 | C03 |
| 同一块 `[Default]` 标两条 | 静默（求解取第一条） | C03 |
| 同一块 `Interface IOne` 写两遍 | 静默 | C03 |
| 条目引用不存在的接口 | 静默 | C03 |
| `[Implementation("NoSuch")]` 指向不存在 | 静默 | C03 |
| `[Implementation("IOne")]` 指向接口块 / `[Implementation("Q8bBase")]` 指向 `.bas` | 都静默 | C03 |
| `Inherits CCC`（CCC 是 CoClass 块名） | **已经报 `VB3020`**："inherits unknown base class 'CCC' (the target must be a class module in this project)" | 只需改文案 |
| 块列了 `Interface IShape`，实现类**根本没写** `Implements IShape` | **静默** | C03 的正菜 |
| 实现类写了 `Implements IShape` 但缺槽 | `VB3012`（B02 就有，逐槽一条） | 已覆盖 |

三条要记住的实测细节：

1. **`Inherits` 一个 CoClass 已经被挡**（`VB3020`），但理由是错的（"unknown base class"，而这个名字确实存在，
   只是不是类）。026 五-6 说"v1 拒，照 3022 那批同族处理" → 这条的活是**把文案改成指名"CoClass 块不能被继承"**，
   不是新造检查。文案一改，`tests/cls_inh`/`ci_n*` 里凡按 `VB3020` 原句断言的都要跟着核一遍。
2. **契约聚合是真缺口**：B02 的 `checkNewStyleInterface` 只对**写了 `Implements` 的类**跑；CoClass 块里的条目
   不会给实现类补上这份义务，所以"块说 CImpl 满足 IShape、而 CImpl 压根没 Implements"今天无人管 → 这正是
   C03 唯一的实质新增判定，也是 026 五-1"复用比对器"这句话的落点。
3. **聚合检查不能放 stage 2.7**：实现类的成员在 **3.4 `mergeInheritedMembers()` 之后**才带得上祖先实现
   （B07b 的转发桩那时才存在），而 2.7 只看得到本模块自有声明 —— 放 2.7 会把"基类实现了、派生类没重写"
   误判成缺失。裁决 = 新开 **stage 3.4c `runCoClassContractCheck()`**（3.4 之后、3.5 之前），
   读 `Driver::classes_` 的链与合并后的符号，比对器仍走 `interface_sig.hpp` 那套槽键与签名口径。

**C03 这一格切两半（026 六节把 C03 写成一格，但里面有两种性质的活）**：

- **C03a（本轮做）= 形状与名字校验**，全部能在 2.7 就地判定（`modules_` + `ifaces_` 都看得见，同 B10 Pass D
  的判据位置）：块名重名 / 撞模块名 / 撞接口名、条目重复、`[Default]` 多标、条目引用不存在的接口、
  `[Implementation]` 指向不存在或不是类、EXE 工程 `[ComCreatable(True)]`。
- **C03b（下一轮）= 契约聚合**（上面第 2/3 条）+ `As <CoClass>` = 默认接口视图（026 五-3）。
  `As` 实测今天也是**静默通过**（探针 q11：`Dim x As CCC` + `x.A` 一声不吭 → 未知类型名被当 Variant 晚绑定），
  所以它不是"从报错改成通过"而是"从静默错改成有类型"，和聚合一样要动语义层，别和 C03a 混一批。

**新增诊断按族给三个号，不混用**（沿用 3015-3018 的分法：一个号管一类判据）：
`SemCoClassEntryInvalid = 3031`（条目：未知接口名 / 重复条目 / `[Default]` 多标 / 默认接口不在集合里）、
`SemCoClassDuplicate = 3032`（名字撞车：块名重复、撞模块名、撞接口名）、
`SemCoClassNotSupported = 3033`（v1 边界：`[Implementation]` 不是类 / EXE 工程 `[ComCreatable(True)]`）。
文案一律 ASCII（D12）。

**一颗先拆掉的雷**：C02 的用例工程 `tests/cc_id/Id.vbp` 是 `Type=Exe` 且 `CCCircle` 写了
`[ComCreatable(True)]` —— 正是 3033 要判死的形状，而 `Test-IdentityNote` 要求退出码 0，
所以校验一落地，C02 那三条断言立刻变红。定案 = 走 026 的语义正道：**把 `cc_id` 的 `[ComCreatable(True)]`
改成 `False`**（needle 跟着改），EXE+ComCreatable 的拒绝另立 `cc_neg` 负例；不把 `cc_id` 换成 DLL 工程
（那会让身份用例依赖 `Type=DLL` 的 `Startup=`/tlb 行为，把两批的失败面搅在一起）。

### D49 B11/C03a 实施（2026-09-24 15:07–，代码 `e515d89`）= CoClass 块的形状与名字校验

**落点**：`driver_interface.cpp` 的 **stage 2.7 新增 Pass F**（判据位置与 Pass D 同一条理由：
只有此刻整工程模块表 + 接口登记表同时可见）+ 三个诊断号（3031 条目 / 3032 名字 / 3033 v1 边界）+
`runInterfacePrepass(const CompileOptions&)` 多收一个参数（只为 `isDll` 一项，`driver_compile.cpp`
一处调用点跟着改）。发码层零改动。

**八条判据**（D48 表格里"今天静默"的那八种，逐条对上探针）：块名重复 / 撞别的模块名 / 撞接口名（3032）、
条目引用不存在的接口 / 把类模块当接口列进集合 / 条目重复 / `[Default]` 多标（3031）、
`[Implementation]` 指向不存在或不是类模块 / EXE 工程 `[ComCreatable(True)]`（3033）。

**两条要留下的判断**：

① **块名 = 自己宿主模块名必须豁免**。第一版按"撞模块名就拒"写完，实测立刻把自己的正例 p07 打回 ——
而 VB6 最自然的写法就是 `Widget.cls` 里写 `CoClass Widget`（实现类即宿主）。豁免条件取
"撞的就是**装这个块的**那个模块"（与 B03 的接口宿主同一条理由），跨模块撞名照旧拒。
这条 026 没写，是实施时**被自己的用例逼出来的**（又一次"先量再写"，只是这次量的是自己的正例）。
② **上一批的用例是这一批的雷，而且要当面拆**（D48 已预告）：`cc_id`/`p05`/`p07` 三处正例分别声明了
不存在的实现类与 EXE+`[ComCreatable(True)]`，校验一落地就全红。定案不是放宽校验，而是把用例改成正面形状
（`cc_id` 补一个真的 `CircleImpl.cls`、`ComCreatable` 改 `False`、p07 改成宿主同名惯用法），
`True` 的那一面另立负例 `itf_n34`。**一个校验批的完成标志是"旧正例与新负例同时全绿"**，
只加负例不改正例的批都是把红灯留给下一轮。

**验收**：`-Category syntax` **99→107**（`itf_n29`..`n34` 单文件 + `itf_n35_coclass_legacy_cls`、
`itf_n36_coclass_vs_module` 双文件 + p05/p07 改造后仍静默 + `cc_id` 三条身份断言一字不差）；
A/B = `.build/pre_b11c03_C3.exe` 对 `n29`/`n34` **零命中**（这两条诊断本批之前根本不存在）；
10 文件 `--emit-c` 对 `pre_b11c03_C3.exe` **10/10 逐字节全同**（本批没碰发码，护栏测的是"没手滑"）；
`tests/cc_id/Id.vbp` 真编译真运行（三条身份行照出、`Id.exe` 打出 `cc_id`）。门 = push 后的 Actions run，
编号见状态头。

**下一格 = C03b**（D48 已切好边界）：`runCoClassContractCheck()` 放 **stage 3.4c**（3.4 成员合并之后、
3.5 之前），复用 `interface_sig.hpp` 的槽键与签名口径，判"块列出的每个接口，实现类（含祖先）必须满足"；
外加 `As <CoClass>` = 默认接口视图。`Inherits` 一个 CoClass 已被 `VB3020` 挡住，**这条只剩文案活**
（把"unknown base class"改成指名"CoClass 块不能被继承"），改文案时要顺带核 `tests/cls_inh` 与 `ci_n*`
里按原句断言的负例。


### D50 B11/C03b 前置实测与裁决（2026-09-24 15:38–，只量不改码；探针在 `.build/probe_c03b/pa..pf`）

**量的对象 = 契约聚合到底要看得见哪些成员表**（D48-3 留的那句"祖先实现了、派生类没重写 在 2.7
看不见"要落实）。三种供给路径分别探：

| 探针 | 形状 | 今天（C03a 之后）的读数 |
|---|---|---|
| pa | 派生类自己写 `Implements I` + `Inherits` 一个实现了 I 的基类 | `VB3022`：**派生类自己实现新式接口**就不许继承 |
| pb | 基类写 `Implements I`，派生类 `Inherits` 它、自己不写 | `VB3022`：**基类实现新式接口**就不许当基类 |
| pd | 基类**只声明成员、不认领接口**，派生 `Inherits` 它 | **全静默**（契约根本没人查） |
| pe | 绑定类缺一个槽 | 静默（本批之后 = `VB3012`） |
| pf | 绑定类的成员 `ByVal`、接口写 `ByRef` | 静默（本批之后 = `VB3017`） |
| pc | `Dim c As CCCircle`（把块名当类型） | **静默当 `Variant`**，连 `Set c = Nothing` 都不问 |

① **D48-3 的理由成立，但要说准它成立在哪一半**：可达的"祖先供给成员"只有一种形状 —— 祖先自己
声明成员而不写 `Implements`（pd）。带 `Implements` 的两种（pa/pb）今天被 B08f 的 v1 边界整条挡死，
所以"2.7 看不见祖先成员"不是假警报，而是**只有 pd 这一条路真的需要链**。结论不变：契约比对排在
链表（2.8）与成员合并（3.4）之后，也就是新阶段 3.4c。

② **`As <CoClass>` 本批整条推给 C05**（pc 实测是"静默 Variant"）：状态头那条"要么不做、要么只做到
认得这个名字是类型"的半开方案，量出来是**净负收益** —— 认得它是类型之后成员访问立刻开始报错，
而派发仍要到 C05，用户拿到的只是"从静默变成一堆说不清的错"。所以这一格与派发**同批**做。

③ 观测面复用现成的：`--syntax-only` 一路跑到阶段 3.6 之后才 return（`driver_compile.cpp:429`），
所以 3.4c 的诊断天然进得了 `Test-SyntaxFail`/`Test-SyntaxFailMulti` 这条负例通路，不必再找 D46
那种 stderr 信息行的替代面。

④ **号段沿用，不发新号**：`VB3012 = SemInterfaceNotImplemented`、`VB3017 = SemInterfaceSignatureMismatch`
（状态头说的"VB3012 族"就是这两个）。缺槽与签名不符在 `Implements` 那边已是这两个号，CoClass 这边
语义完全同族，只差主语 —— 多发一个号只会让人以为契约有两种算法。

⑤ **没写 `[Implementation]` 的块跳过**（p05 与 `cc_id` 的 `CCMint` 都是合法形状）：没绑实现类就无从
判起。顺手立"块必须绑实现类"是 C05 的事（那时才要拿它 `New`）。

### D51 B11/C03b 实施（2026-09-24 15:38–，代码 `c4aaa4c`）= 契约聚合校验 + VB3020 文案

**落点**：`driver_compile.cpp` 新增**阶段 3.4c**（3.4b 与 3.5 之间，且刻意放在 `modules_.size() > 1`
那个 if **之外** —— 单文件工程里的块同样要判）。实现 `Driver::runCoClassContractCheck()` 写在
`driver_interface.cpp` 末尾，与 Pass D/E/F 同一翻译单元：四条判据共用的前提是"整工程模块表 +
接口登记表同时可见"。零回归 = 工程里没有 CoClass 块立即 `return true`（与 3.4/3.4b 的早退同族）。

**槽表怎么建**（这一格的全部技术内容）：按 `ClassChainView::chain`（自根到叶）**倒着走**、槽键
首见者胜 —— 于是"派生遮蔽祖先"自动成立，且遮蔽规则与 3.4 的成员合并同源，不会两套裁决。
成员级 `Implements I.M` 子句照 B02b 的规矩：写了子句的成员**只**按子句入座，不回落同名隐式匹配。
访问级别不参与判定（B02 就是这个口径，这里另立一套只会让两个比对器互相打脸）。

**两条刻意的取舍**：

① **不重构 `checkNewStyleInterface` 来共用它那张 impl 表**。它的子句记账牵动 `VB3019`（未认领子句
的兜底诊断），改它 = 把 B02 那条已发货路径的回归面全展开；而这里多写的只有"按槽键建索引"二十来行。
两侧真正共用的是 `interface_sig.hpp` 那一套 inline 口径（槽键、签名抽取、签名相等），
所以"什么算同一个槽、什么算签名不符"仍只有一份定义 —— 这是"复用"的正确粒度（026 五-1）。
② 类链登记表只收"能当基类用"的模块，表里没有就当该类没有祖先（只用它自己那张声明表），
不为此扩表。泛型宿主（D11 边界）直接跳过。

**VB3020 文案**（D49 留的尾巴）：只在"基名撞到一个 CoClass 块名"时换成新句，指名那是组契约的块、
没有成员表可继承；其余情形保持原句。号不变，`ci_n01`（真不存在的基名）与 `ci_n07`（基名是接口宿主）
两条按原句断言的用例因此一字未动 —— 改前 grep 过，全仓按那句断言的就只有这两处。

**上一批的用例又一次变成这一批的红灯**（D49-② 的第二次应验，而且这次连"理由"都提前写好了）：
新校验一落地，`p07`/`cc_id/CircleImpl`/`cc_id/VbpImpl` 三条正例全红 —— 前两条是 `ByVal` 对上接口的
`ByRef`（签名口径算它俩不等），第三条是 `VbpImpl` 根本没有 `Move`/`Label`。定案仍是**改正例**：
按接口的写法去掉 `ByVal`、给 `VbpImpl` 补齐两槽，不放宽校验也不给 `ByVal` 开后门。

**验收**：`-Category syntax` **107→112**（`itf_n37` 缺槽 / `itf_n38` 签名不符 / `itf_n39` `Inherits`
块名 三条负例 + `itf_p08` 祖先供给 / `itf_p09` Via 委托供给 两条正例；p09 兼作 B10×B11 的接缝用例，
`vias_` 里查到的委托关系免逐槽 `VB3012`、签名不符照报）；A/B = `.build/pre_b11c03b_C3.exe` 对
`n37`/`n38` **零命中**，对 `n39` 出的是**旧那句** "unknown base class"（新句本批独有）；
10 文件 `--emit-c` 对 `pre_b11c03b_C3.exe` **10/10 逐字节全同**；`cc_id/Id.vbp` 真编译真运行、
且 `--emit-c` 与改码前逐字节相同（本批一克发码都不产），`cls_inh/Inh.vbp` 真编译 exit 0。
门 = push 后的 Actions run，编号见状态头。

**下一格 = C04**（存量 `Attribute VB_Creatable`/`Instancing` 只读折算成 CoClass 记录，`ai/026` C04）：
它独立且小，而 C05（组内激活）按 D50-② 要把 `As <CoClass>` 类型识别与派发**一并**做，开工前先量
"`As <未知类名>` 今天在哪一层吞掉"（`Variant` 兜底点）与"`New <CoClass>` 直调实现类工厂"的落点。

### D52 B11/C04 前置实测与裁决（2026-09-24 19:25–，只量不改码）

**本轮第一件事不是写码**：本地 `fan/dev` 落后 `github/dev` 三枚提交（Asm x64 + 静态库 + 包引用三线，
64 文件 +3409 行，`tests/run_tests.ps1` 也并进来 262 行）→ 先 ff 到 `9099e16`，**逐字节护栏的 BASE
必须用合并后的树重建**（旧 `pre_b11c04_C3.exe` 是 `00ee1b2` 树的产物，拿它比会把别人的改动记到本批头上）。
重建后 syntax 基线 = **112**（别的写者的用例并进来了，不再是 107）。

**① 存量 attribute 到底是什么**（`tests/` + `archive/` 全量 `.cls` 统计，"头属性 = 名字不带点"）：

| 属性行 | 条数 | 值域 |
|---|---|---|
| `Attribute VB_Name` | 252 | 字符串字面量（已被 stage 2 消费成模块名） |
| `Attribute VB_PredeclaredId` | 143 | `False` 142 / `True` 1 |
| `Attribute VB_GlobalNameSpace` | 143 | `False` 142 / `True` 1 |
| `Attribute VB_Creatable` | 143 | **`True` 134** / `False` 9 |
| `Attribute VB_Exposed` | 142 | `True` 115 / `False` 27 |
| 带点的成员级行 | 40+ 种名字 | `m_oSocket.VB_VarHelpID` 12、`Item.VB_UserMemId` 6、`*.VB_Description` … |

三条读法：值域里**只有布尔字面量**（`True`/`False`，无 `-1`/`0`/字符串形态）；`VB_Creatable = True` 是
绝对多数（134/143）；而**整仓 `Attribute Instancing` 0 条** —— VB6 的 instancing 写在 `BEGIN` 头里、
parse 期已进 `Module::instancing`（`parser_module.cpp:39-67`）。026 六节字面写着"折 `Instancing`"，
按语料它没有可折的载体 ⇒ **不折、也不复制**（`Module::instancing` 继续是唯一真相，B13 要读就读它）。

**② 今天谁消费它们**：全仓只有两处 —— `driver_frontend.cpp:222` 的 `VB_Name`、`driver_generics.cpp:108`
对泛型类拒收 `VB_PredeclaredId`/`VB_Exposed`。其余属性行 parse 完就躺在 `Module::attributes` 里无人读
⇒ 状态头那条"根本没被消费"成立，折算是一条全新的信息通道。

**③ 折算会打到谁**（门内 blast radius）：`tests/` 里带这四行的 `.cls` 恰好 **8 个**
（`BalloonTooltips/cTT`+`ISubclass`、`Charts 2020/ClsResizer`、`ctor/CtorPlain`+`CtorThing`、
`pkg_cls/…/PkgThing`、`VBFlexGridDemo/Builds/…/IVBFlexDataSource{,2}`），全部在真编译的门项目里。
其中 `cTT`/`ISubclass`/`ClsResizer`/`PkgThing` 写的是 `VB_Creatable = True` ⇒ **若折算记录参与 Pass F，
它们当场撞 C03a 新立的 `VB3033`**（EXE 工程不得声明可注册 COM 服务器）。这就是状态头预警的那颗雷。

**裁决**：

1. **落点 = stage 2.7 新增 Pass E0**（Pass D 之后、Pass E 之前），造一份 `CoClassDecl` 记进
   `Module::coclasses`。与 Pass D/F 同一条理由（此刻整工程模块表才可见），且 Pass E 因此**仍是唯一身份
   出口**、`coclassIds_` 仍是唯一读数（D47 之后这是硬规矩）。附带的观测面红利：`--dump-ast` 在 stage 2
   就返回 ⇒ 折算记录对 AST 打印**天然不可见**，"手写的"与"折出来的"在两处面上分得清清楚楚。
2. **折算只记信息、不开判死**：Pass F 与 3.4c 一律按 `CoClassDecl::foldedFromAttributes()` 跳过。
   理由按 ③ 的实测数字钉死（134 条 `VB_Creatable=True` 全在 EXE），不是"先宽松以后收紧"。
   3.4c 的早退条件同时改成"只认手写块"，否则每个存量工程都要为一条空契约清单白建一张表。
3. **`CoClassIdentity::legacyFolded` 是给 VB3020 文案用的**：折算记录的名字**恒等于**一个类模块名，
   而 `classes_` 恰好不收接口宿主 `.cls` 与泛型类（`driver_classchain.cpp:381-385`）⇒ 若不加这个位，
   `Inherits <接口宿主名>`（宿主又写了头属性）会收到一句凭空捏造的"那是个 CoClass 块"。
   口径 = 折算侧与登记侧用同一组条件筛（`isInterfaceModule` / `classTypeParams` / 后缀 `.cls`），
   再加 `legacyFolded` 兜底，双保险。
4. **触发条件** = 四行任一存在。只写 `VB_Name` 的模块不折 ⇒ 本工程所有 `.bas` 用例、以及
   `cc_id/Id.vbp` 那三个手写块工程的输出**一字不变**（实测该工程折出行数 = 0）。
5. **手写块优先**：同一模块两者都有 → 不折、只报一行信息（026 六节"以手写块为准"）。
   选"信息行"而不是"警告诊断"：警告计数是门基线的一部分，而这一条既不是缺陷、也不该沉默。

### D53 B11/C04 实施（2026-09-24 19:25–）= 存量 header attribute 只读折算

**三处落点**（全在语义层，发码零改动）：

- `ast_decl.hpp`：`CoClassDecl` 加 `std::vector<std::string> legacyFoldKeys` + `foldedFromAttributes()`。
  **一条字段而不是 bool + 清单两处** —— 后者迟早漂移，而"折了哪几行、各折成什么值"本来就是要打出来的东西
  （元素形如 `VB_Creatable=True`）。
- `driver_interface.cpp`：Pass E0 折算 + Pass F/3.4c 两处跳过 + Pass E 信息行尾追
  ` folded-from-legacy: K=V …`。折算记录写 `[Implementation("<类自己>")]`、`VB_Creatable` →
  `[ComCreatable(n)]`（身份求解读这一枚），另三枚按**原名**带着（今天无人读，B13/B15 的类型库标志位
  从这里取）。每个键**取首行**，与 Pass E 对同名属性行"首值胜"同一口径。
- `coclass_identity.{hpp,cpp}`：`CoClassIdentity::legacyFolded` 由纯函数按
  `block.foldedFromAttributes()` 置位 ⇒ "身份 = f(块)" 这条没被破坏。

**观测面选在 identity 那一行、不另起一行**（D46-① 那条教训的正用）：`Invoke-IdentityText` 今天就是按
`identity:` 过滤 stderr 的，折进同一行 ⇒ **零新 helper** 就能断言 vbp 档那条；同时"哪个类折了、折成了
什么"与它的三档身份天然同行，读日志不必在两行之间做 join。手写与折算在同一份输出里的对比因此是**一眼**的：
`... impl='' comCreatable=False`（手写）vs `... comCreatable=True folded-from-legacy: VB_Creatable=True …`。

**一条 harness 坑**（下次直接避）：`.build/fnv_oracle_c04.py` 以 `coc:proj.name` 形态收参时，MSYS 会把带
冒号的 argv 当路径转换 ⇒ python 里 `split(":")` 少一段。期望值改为**在 python 内联 import 该模块**算。
三条期望 GUID 全部由这枚独立实现算出（`FoldBase` = `{96466C30-E240-55A4-9434-24F1C884C2AB}`、
`FoldWins` = `{EA2B2FD6-E5C6-5192-D0C9-A13BC6FC7859}`），不是把编译器输出抄回去（D47-② 第二次执行）。

**验收**：`-Category syntax` **112→116**（`itf_p10_coclass_fold` = 折算正例，兼作"EXE + `VB_Creatable=True`
不判死"的防回归，并断言带点的成员级行**没有**被折进来、折出来的名字仍能被 `Inherits` 当基类用；
`itf_p11_coclass_block_wins` = 两种形状同处一模块时手写块胜；`cc_id_fold_vbp_tier` +
`cc_id_fold_repeatable` = 一个**从不写 CoClass** 的 `.cls` 靠 vbp 三段式拿到 `(vbp)` 档 CLSID 且整串可复现）；
新增 helper `Test-SyntaxNote`（exit 0 + 必须出现 + 必须不出现：折算这种"只加信息"的批第一次有了
可断言的正向面）；A/B = `.build/pre_b11c04_C3.exe` 对 p10/p11/IdFold 三条**零命中**；护栏 **16/16**
（10 文件严格逐字节 + 6 个"会折算"的工程 `--emit-c` 输出逐字节全同、stderr 增量只允许 `C3: CoClass`
那一条通道）；`ctor_ok.vbp` 真编译真运行（`amt:42`/`CNT:7` 与改码前一致）。门 = push 后的 Actions run。

**下一格 = C05**（组内激活）：`As <CoClass>` 类型识别 + 默认接口派发 + `New <CoClass>` +
`CreateObject(ProgID)` 编译期改写，按 D50-② **同一批**做；开工先量"`As <未知类名>` 今天在哪一层被吞成
`Variant`"。折算记录到 C05 才第一次有运行时后果 —— 届时必须回答"折出来的类能不能 `New`"，
而 `VB_Creatable = False` 那 9 条要给出可解释的差别（今天它们与 True 只差在记录里那一位）。



### D54 B11/C05 前置实测与裁决（2026-09-24 20:42–，只量不改码；探针在 `.build/probe_c05/M1..M6`）

**CURRENT_BATCH 给的三件事全部量到落点**（不是量到结论），另加一条顺带量的既有洞。

**① `As <未知类名>` 到底在哪一层被吞：两层各有一处兜底，而且互相不认识。** 语义层
`semantic_analyzer_typeref.cpp:135` —— `return Vb6Type::Variant;  // 未识别类型 → Variant (宽松策略)`；
它第 4 步查的是 `symTab_.lookupModule`，而**类符号要到 stage 3.5 才跨模块注入**
（`driver_crossmod.cpp:22`；这段历史就写在 `semantic_analyzer_decl_type.cpp:42-52` 的注释里，
所以"别的模块里 `As 真类名` 也先当 Variant、事后靠 `variableTypeName`/`srcTypeName` 捞回"）。
发码层 `cgen_base_type.cpp:293` —— `return "void*";`。⇒ **"认得这个名字是个类型"没有单一入口**，
而且按**名字串**比类的位点不止这两个（`memberFieldTypes`、`srcTypeName`、COM 解包表各比各的）。
这条直接钉死 D55 的选型。另外全仓**没有**"未知类型名"这个诊断号（3xxx 里最近的
`SemUndeclaredIdentifier=3001` 只当警告用在 `Implements` 上）⇒ 新规矩只能新起一号 = `VB3039`。

**② 项目类的新建与释放通道 —— 量完的结论是"C05 不新增任何释放面"。**
`Set x = New C` → `vb6_cls_C_New()`，返回**具体结构体指针**（`cgen_expr.cpp:303-322` +
`cgen_com.cpp:40-110` 的 `emitFactoryBody`）；类变量赋值是**裸 C 赋值**，`Set x = Nothing` 只发
`target = NULL;`（`cgen_setlet_set_prop.inc:111-115`，那里的注释自己写着"项目类没有引用计数、
`vb6_ReleaseObject` 会 AV"），`__refcount` 只给写了 `Implements` 的类生成
（`cgen_iface_vtbl.cpp:353-358`）；`_Destroy` 全仓只有两个调用点（接口 Release 归零、窗体
teardown）⇒ **D41"EXE 里 `Class_Terminate` 没有触发点"那条缺口不因本批扩大、也不在本批修**。
把组名改绑到实现类 = 用户拿到的就是这条既有不变式，不会以为"新语法带来了新生命周期"。

**③ 折算记录让生效面变大这件事，实测结论是"风险还没发生，但闸门现在就得下"。**
`coclassIds_` 里今天确实有 8 个与类模块同名的条目（门内），但**类型路径一个字都不读它** ——
`.progId` 只被 Pass E 打进 stderr，`implName`/`comCreatable` 只被 Pass F 与 3.4c 用于校验。
所以三条准入写死在改名的入口：(a) `legacyFolded` 不进别名表；(b) 块名被同名模块占住时
**类/模块赢** —— `As Widget` 今天的含义不许被一块新语法改掉，而且这条裁决早有先例：
`cgen_expr.cpp:326` 的 Fix 177b 写的就是"coclass 被本工程同名类遮蔽时，创建点改走工程类工厂"；
(c) 块名等于实现类名时不改（改了也无害，三道都留着，抢答这件事不该靠运气）。
M4 对照实测：`As StructImpl`（真类）与接口视图那条路在改前改后**一字不差**。

**④ 三条坏读数**（BASE = `pre_b11c05_C3.exe`，`--emit-c` 实发，`--syntax-only` 全部 rc=0 零诊断）
—— 这三条就是 026 五-3/4/5 选"编译期改写"的立项理由：
M1 `Dim a As PCStruct` → `void* a = 0;` + `vb6_ComCall(a, L"Move", ...)`（空指针上的晚绑定）；
M2 `Set a = New PCStruct` → `vb6_NewObject(L"PCStruct")`（运行期按名字查注册表，EXE 工程根本没注册）；
M3 `Set a = CreateObject("ProbeApp.PCStruct")` → `vb6_CreateObject(...)` 原样发。
M5（块没有 `[Implementation]`）与 M1 同样静默 ⇒ 这一位改判死（`VB3039`）：块没有实现类这件事
编译器已经知道，没有理由让用户去猜。

**⑤ 顺带量到一条与"默认接口视图"直接相关的既有洞**（不是本批引入；D43 第三条的现场复现）：
接口块声明在 `.bas` 时，`Dim iv As IShape : Set iv = New RealImpl : iv.Move 5` 发出的 C 是
`vb6_StructImpl_Move((&(int32_t){5}))` —— **接收者丢了、类还挑错了**（`RealImpl` 的成员发成了
`StructImpl_`）。⇒ 本批**不把 `As <块名>` 做成接口视图**的第二条理由：那条路自己还缺一半。
第一条理由是契约按 026 五-1 只要求"实现类**连同祖先满足**这些槽"，实现类完全可以不写
`Implements`（`p08` 就是合法形状），那时候对象身上根本没有那份接口槽表可指。

### D55 B11/C05 实施（2026-09-24 20:42–，代码 `7f829ee`）= stage 2.7 Pass G 组内名字激活

**选型：就地改 AST 的类型位点，不给语义层和 cgen 各开一个别名入口。** 理由 = D54-① 那句
"类名被读的地方不止两处"：别名要一处不漏，就得每个消费者都记得问一遍，而未来还会再加消费者。
先例是泛型单态化（`src/ast/ast_clone.hpp` 头注"语义/cgen 对泛型零感知"）。收益当场兑现：
**发码侧零新分支**（`As`/`New`/`CreateObject` 三面同归一条已经实测通了的工程类路），且
`--dump-ast` 在 stage 2 就返回 ⇒ 用例 dump 出来的仍是用户写的那个名字。

**三个落点**：
1. 新单元 `src/driver/coclass_activate.{hpp,cpp}`（一个 `ASTVisitor` 管 `New`/`TypeOf` 这两个
   字符串位点，自己写的语句递归管类型引用与 `CreateObject` 的节点替换）+ 新号
   `SemCoClassTypeUnbound = 3039`。覆盖的类型位点：模块字段、局部 `Dim`（含 `Dim x As New`）、
   形参、返回值、`ReDim ... As`、UDT 成员。**不动**的位点记成 v1 边界：`Declare`/`Delegate`/
   事件与接口声明里的类型、`Inherits`（块名照旧 `VB3020`，五-6 没让路）、`Implements`。
2. 接线 = stage 2.7 Pass F **之后**的 Pass G（`driver_interface.cpp`）：身份只从 Pass E 的
   `coclassIds_` 读、**不二次求解**（D47 的规矩），三道重名闸门见 D54-③。
3. `CreateObject` 的改写做成 **AST 节点替换**（`Set x = CreateObject("<已知 ProgID>")` 的整个
   右值换成 `NewExpr(<实现类>)`），不是在 cgen 里仿冒发码文本。理由是实测出来的：
   `cgen_setlet_set_rhs.inc:130-153`（Fix 179a）认 New 形状靠的是**文本前缀 `(vb6_cls_`**，
   换成节点之后 `As Object` 目标的 IDispatch 包装自动成立（`cc_act` 的 CC5 就是这条读数）。
   只认"整个右值就是这一枚调用"这一种形状，嵌套形态（`Foo(CreateObject("p.c"))`）照旧走注册表
   —— 边界写进手册，不静默。信息行只在真被用到时打：
   `C3: CoClass 'X' activated in-project: type name -> class 'Y' (n type reference(s), m CreateObject rewrite(s))`
   （`n` 含 `New`/`TypeOf` 那两处，所以 cc_act 的 Circle 是 10 处引用 + 2 处改写）。

**验收**：`-Category syntax` 116→**118**（`cc_act_group_names` 两块各一行激活 + ProgID 文本；
`itf_n40_coclass_type_no_impl` 断 `VB3039`；`p10` 加第三份文件 + 新 Absent 断言"折算名当类型用
不产生激活行"）；新端到端工程 `tests/cc_act/Act.vbp` 断言 **CC1..CC9 + CC-DONE，x64 与 x86 各
9/9**（CC6 = 派生 `Overrides` 经组名答话 ⇒ 虚表没被静态化；CC9 = 组名与实现类名指同一个实例；
CC2/CC3 走字段/形参/返回值三个位点；CC5 = Object 目标的 COM 包装）；A/B：BASE 对 p10 与 cc_act
**零激活行**、对 n40 **零诊断**，NEW 三条全兑现；护栏 **16/16 逐字节**，其中 `cc_id\Id.vbp`
声明三个手写块而从不当块名当类型用 ⇒ 连 stderr 都不多一个字，正是 D54-③ 要的"闸门有效"证据。

**下一格 = B13**（P6 对外那半的第一格：IUnknown 三件套真实实现 + 对象布局 COM 化收尾）。
本批给 B13 留两条必读：D54-⑤ 那条接口视图旧洞（"默认接口"四个字要写进对外文档之前得收掉）、
D54-② 那条"类变量永不 Release"不变式（对外一旦发 IDispatch/IUnknown，引用计数就得**从无到有**，
`cc_act` 的 CC5 是今天唯一一条包装读数）。折算的 `VB_Creatable = False` 那 9 条与 True 的差别
仍然只在记录里那一位，B13/B16 要给出可解释的处理。

**收线后追加的两条实测（同一个 exe，`.build/probe_c05/M7..M11`；写在这里是因为两条都推翻了我
自己在手册上先写的话）**：
- `TypeOf t7 Is Circle` 编译过、改写也发生（激活行的引用数从 10 涨到 13），运行期答 **False**；
  对照 `TypeOf raw Is ShapeAct`（一份**从不写 CoClass** 的形状）**同样答 False** ⇒ 缺的是
  `TypeOf … Is <类名>` 本身，与 CoClass 无关（`itf_xmod` 的 TOF1..TOF3 走的是接口名那条路，是好的）。
  同一份代码里 `t7 Is Nothing` 答得对。手册已把 `TypeOf` 从"已激活的位点"清单里摘出去、当面写明
  "改写照做、结果仍为否"。登记给后续：**对外那半要用到 `TypeOf`/QI 时这条必须先收**。
- `ReDim arr(1) As Circle` 在 C 层撞 `error C2224: '.Move' 左边必须是结构体/联合`；对照
  `ReDim arr(1) As ShapeAct`（普通类）**报同一处、同一条** ⇒ `ReDim ... As <工程类>` 今天是坏的
  （数组元素没发成类指针），不是本批的改写造成的。手册同步改口径。UDT 成员那一位反而是好的：
  `Private Type THeld : c As Circle : End Type` + `Set h.c = New Circle : h.c.Move 6 : h.c.Area()=14`
  实测 **U1:OK** ⇒ 手册把"UDT 成员"留在已交付清单里，把 `ReDim` 挪出去。

**给下一批的一条流程教训（自己撞的）**：`tests/run_tests.ps1` 在仓库里是 **UTF-8 带 BOM + LF**
（不是记忆里写的 CRLF）。把插入段按 CRLF 写进去 = 一次 29 行的登记变成 1624/1598 的整文件改动，
`git diff --numstat` 一眼就能看出来；插完必须断言 `count(b"\r\n") == 0`。另外 bash heredoc 里的
`"$Tests\itf_neg\n40_..."` 路径里紧跟着出现 `n`（例：`\itf_neg` 后面接 `n40_...`）时会被当成换行吃掉一行，带 `t` 同理 —— 反斜杠一律用 `chr(92)` 拼出来，别指望字符串里连写两个反斜杠。



**D56（B13 开工测量：对外那一半今天到底有什么；`.build/probe_b13/`、`.build/b13_probe.ps1`）**

0. **本格 CURRENT_BATCH 的第 0 步前提是错的，先记账**：`tests/` 下**不是**"全是 `Type=Exe`" ——
   `tests\test_activex_dll\` 里躺着两份 `Type=DLL` 工程（`test_activex_dll.vbp` / `test_event_dll.vbp`，
   5 个类、三段式 `Class=Name; file.cls; {CLSID}` 带显式 CLSID），**今天真编译 25 秒就产出
   259–269 KB 的 `.dll` + `TestAXDLL.tlb` + `activex_dll.def`（导出 DllGetClassObject / DllCanUnloadNow /
   DllRegisterServer / DllUnregisterServer / DllMain）**。它们的错处是**从没登记进 `run_tests.ps1`**
   （grep "activex" 零命中）⇒ 整条 ActiveX DLL 管线**在门内一次都没跑过**，此前每一句"对外还差什么"
   都是没测过的话。⇒ 本批第 0 步从"造工程"改成"**把现成工程接进回归** + 补一份 CoClass 块在 DLL 里的工程"。
1. **dll_entry 的唯一读数通道是"真编译 + 读回临时目录"**：生成的 C 只在 `--keep-for-debug` 时留下
   （stderr 打 `intermediates kept at: <dir>`），而 **`--emit-c` 不含 `dll_entry.c`**（实测 0 命中）。
   `g_vb6_coclasses[]` 每条 = `progId / clsidStr / classVariable / factoryFunc=vb6_cls_<C>_New /
   destroyFunc / methodCount + IDispatch 成员表 / ifaceCount + ifaceIids / defaultIfaceIid /
   sourceIfaceIid / events`。`test_activex_dll` 里 Calc 的四条公有成员都进了 disp 表
   （`L"SetValue", 1, 1`），`clsidStr` **就是 vbp 三段式那一枚** ⇒ CLSID 这一位在"显式写在 vbp 上"时两通道一致。
2. **RTL 侧的 IUnknown 三件套早就有真的**（本格按"要从零写"排期，是错的）：`vb6comserver_factory.c`
   的 `CF_CreateInstance` → `vb6_ComObject_Create` → QI；`vb6comserver_obj.c:24-72` 的
   `ComObj_QueryInterface` 认 IUnknown/IDispatch（返回 `self` = **规范指针**，符合 COM）、认
   `ifaceIids[i]`、认 `defaultIfaceIid`、按需挂 `IConnectionPointContainer` / `IProvideClassInfo2`，
   全不认才 `E_NOINTERFACE`；`AddRef/Release` 是 `InterlockedIncrement(&self->refCount)` + 全局
   `g_vb6_cRef`，**归零时调 `desc->destroyFunc` ⇒ `Class_Terminate` 在 DLL 侧有触发点**（D41 那条
   "没有触发点"只成立于 EXE，本批实测划清）。⇒ **"真实现 IUnknown 三件套"不是 B13 的活**。
3. **两套 mint 的分叉现在看得见，而且比 D46-2 说的更近**：同一份 `tests\cc_dll\CoDll.vbp`
   （新式接口 `IProbe` + `Implements IProbe` 的实现类 + `CoClass PG` 块）一次编译里 `IProbe` 拿到
   **两枚不同的 IID** —— stage 2.7（`coclass_identity.cpp` → `coclassIds_`）给
   `IID={F5CEF988-…} (minted)`、`ProgID=CoDll.PG (minted)`；dll_entry（`cgen_util_dllentry_prelude.inc`
   那两枚 lambda）给 `IID_vb6iface_IProbe = {0AD9CBC7-…}`、注册 ProgID `CoDll.CImpl`。并且
   **`coclassIds_` 的后端消费者为 0**（全仓 grep 只有 `driver_interface.cpp` 与 `driver_classchain.cpp`
   读它：校验、Pass G、VB3020 文案）⇒ **产品注册的从来不是 C02 求解出来的那一份**，
   D47 那句"唯一身份出口"目前只覆盖语义层，没覆盖产物。
4. **块名在对外这一侧完全不存在**：表按**类模块**逐条发（`classVariable="CImpl"`、`progId="CoDll.CImpl"`），
   `CoDll.PG` 在产物文本里 **0 次** ⇒ 组名 ProgID 进不了注册表，外部 `CreateObject("CoDll.PG")` 必失败；
   而组内那半（C05）认的正是 `CoDll.PG` ⇒ **两半不对称**。
5. **只实现新式接口的类对外不可调用**：`cc_dll` 那条 `methodCount=0 / methods=NULL` —— 新式契约的实现
   成员按 VB6 惯例写 `Private`（`p01`/`itf_xmod` 皆然），disp 表只收公有成员 ⇒ 外部 IDispatch 客户
   一个方法都点不到；而 `ifaceCount=1` 只把枚 IID 记进表、QI 命中后仍返回**同一个 IDispatch 指针**
   （`self`）⇒ 今天"新式接口对外"= 伪装成 dispinterface。这条要先定口径再动手（接口槽发 disp id？
   还是明确"新式接口不出 DLL"并给诊断？），不许顺手做。
6. **`comCreatable` / `legacyFolded` 在产品侧的读数（原测量点 ④）**：`[ComCreatable(True)]` 在 DLL 工程
   合法（EXE 才 `VB3033`），但两者后端消费者为 0 ⇒ 表里一条不少、注册一条不漏，
   `VB_Creatable = False` 那 9 条与 True 目前对产品**没有任何可观察差别**。
7. ⇒ **B13 按实测重切成三格**：**B13a（本批已出）= 观测面**（新助手 `Test-VbpDll` + 三条用例进门 +
   新工程 `tests/cc_dll`，零编译器代码改动）；**B13b = 把身份出口落到产物**（dll_entry 只从
   `coclassIds_` 取值、删第二套 lambda、块名 ProgID 上表、`comCreatable`/`legacyFolded` 给读数；
   `cc_dll_identity_two_channels` 就是这条的对照，现在钉分叉、合并后必须翻面）；
   **B13c = 新式接口对外可调用的口径 + 规范 IUnknown**（`vb6_iunk_<C>_<I>` 认 `IID_IUnknown`
   现返回**本接口薄指针**，D22-7④ 要在这里收）。
8. **一条通用教训**：**别把"没有用例"读成"没有实现"** —— 这次两个方向都错了：RTL 三件套比计划书假设的
   完整得多，而两份现成工程因为从未登记被当成"DLL 路径不存在"。测量阶段先花 25 秒真编译一次，
   比读码推断便宜得多，也比它可靠得多。


**D57（B13b 实施：dll_entry 的身份改读唯一出口）**

1. **接线照既有先例，不开新机制**：`CCodeGen` 上三个 setter（`setInterfaceRegistry` /
   `setViaRegistry` / `setClassChainRegistry`，`cgen_api.inc:227-232` + `cgen_state.inc` 成员 +
   `driver_codegen_module_loop.inc:75-77` 注入）已经是同一条路，本批加第四个
   `setCoClassIdentityRegistry(&coclassIds_)`。位置选在 `driver_codegen_dll_typelib.inc`
   （`dllCgen` 那一段）而**不是** `driver_codegen_module_loop.inc`，因为这张表只由
   `generateDllEntry` 消费。
2. **只接手写块，折算记录一律照旧**（`id.legacyFolded` 即跳过）。理由与 C04 同源而且是第二次生效：
   折算记录在没有 vbp 三段式时其 `clsid` 是 C02 mint，与 legacy `generateClsid` **种子不同** ⇒
   覆盖上去等于把"改码前编得过、注册表里躺着的存量 DLL 工程"的身份换掉 = 破逐字节护栏。
   副产物：**`legacyFolded` 第一次有产品级读数** —— 有它 = 不覆盖、且不会多出组名那一行
   （折算块名恒等于类模块名）。
3. **`[ComCreatable(True)]` 第一次有后果**：组名档 ProgID（`<工程>.<块名>`）**只在该位为真时发出**，
   表里就多一行 —— 同 CLSID、同类工厂、同 destroyFunc，只差 ProgID 文本，
   `g_vb6_coclassCount` 1→2。类模块名那一档**保留不删**：存量 DLL 客户照旧
   `CreateObject("<工程>.<类>")`，这是"两半不对称"的收法而不是替换。
   反过来说，一个手写块若不写 `[ComCreatable(True)]`，它的组名对外仍然不存在 —— 这是刻意的
   默认（对外可创建要显式表态），也给了 D56-6 那条"9 条 False 与 True 无差别"一个可解释的答案：
   差别现在存在于**手写块**这一侧，存量 attribute 那一侧继续无副作用（护栏）。
4. **IID 并掉的准确范围**：`iidMap` 建表时，接口名 == 该块 `[Default]` 接口 → 用出口值，
   于是 `IID_vb6iface_<I>` 与 `IID_vb6def_<C>` 对同一个接口第一次给同一个 GUID
   （cc_dll 实测两枚都成 `{F5CEF988-…}`，legacy 那枚 `0AD9CBC7` 从产物里消失）。
   **非默认接口仍走 legacy derive** —— 它要和 `cgen_iface_vtbl.cpp:84` 的 `ivDeriveIid`
   （key `"iviface:"+名`，同族不同 key）一起收，那是**第三通道**，归 B13c，别在这批半接。
5. **本批新量到的第三枚 mint 在类型库侧**（D56 没记，因为它不在 dll_entry 里）：
   `TypeLibBuilder::generateUuid`（`src/typelib/typelib_builder.cpp:24`），
   `driver_codegen_dll_typelib.inc:263-269` 的优先级是 `comClsidStr > classClsidMap_ > generateUuid`，
   **不读 `coclassIds_`**；且 typelib 构建会**回写** `comClsidStr` / `comDefaultIfaceIid` /
   `comSourceIfaceIid` 给 dll_entry 读（`generateDllEntry` 刻意排在 typelib 之后）。
    ⇒ 表内自洽了，但"表 vs `.tlb` 对同一个 coclass 是否同值"**本批未证**（需要能读 `.tlb` 的工具，
   `OleView`/`tlbimp` 不在依赖里）。登记给 B13c 顺手量 + 决定是否把 typelib 也接到出口上；
   026 七-1 说的"合并成一处"到今天为止是**两步里的第一步**。
6. **为什么 EXE 侧不动**（这是本批最重要的一条边界）：`com_entry.c` 由**同一个** `generateDllEntry`
   以 `includeDllExports=false` 生成，把注册表也喂给它 = 让每个带手写块的 EXE 工程换身份。
   所以 `comEntryCgen` 上刻意**不调** setter，EXE 继续整跑 legacy。护栏按构造成立，另加实测：
   `tests\cc_act\Act.vbp`（EXE、**带两个手写块**，是最危险的形状）BASE vs NEW 全产物 md5 对照
   （含 `com_entry.c`）。
7. **验收**：`cc_dll_identity_single_source`（原 `cc_dll_identity_two_channels` 翻面）断
   needle `"CoDll.PG"` + `const int g_vb6_coclassCount = 2;` + `0xF5CEF988`、absent `0AD9CBC7`；
   A/B = `.build/pre_b13b_C3.exe` 跑同一份工程，`CoDll.PG` 0 次、`0AD9CBC7` 1 次（缺陷在 BASE 侧复现）；
   四条 `Test-VbpDll` 全绿（含 x86）；16 件逐字节护栏全同；`.tlb` 与表的一致性**未**由本批证明。
8. **harness 坑（本轮自己撞的两条）**：① `src/` 下的 `.hpp/.inc/.cpp` 是 **CRLF**，而
   `ai/022` 与 `tests/run_tests.ps1` 的 **blob 是 LF**（工作树那份因 `core.autocrlf=true` 是 CRLF）
   ⇒ 任何"插入后断言行尾"的检查必须**按文件各自实测**，不能全仓一套；插入文本要先探测目标文件的
   `nl` 再拼（本批 `b13b_flip_case.py` 就是这么过的）。② Python 里
   `("A" + nl` 换行 `"B" + nl)` 是**语法错**（名字与字符串并置），必须写成 `("A" + nl + "B" + nl)`；
   症状是一句 `SyntaxError: Is this intended to be part of the string?`，跟引号无关，别去找引号。

**D58（B13c 读数与三通道合并：`IProbe` 在一次编译里曾有四枚 GUID）**

1. **先解决"没有读数工具"这件事**：仓里没有能读 `.tlb` 的东西（`OleView`/`tlbimp` 不在依赖里），
   所以自己写了 `tests\tools\tlbprobe.cpp`（`LoadTypeLib` + `GetTypeAttr` +
   `GetRefTypeOfImplType`/`GetImplTypeFlags`，约 150 行，打 LIBID / 每个类型的 GUID /
   coclass 引用了哪些接口且哪个是 DEFAULT / 方法表），并把"三通道同值"做成 gated 用例
   `Test-TlbIdentitySingleSource`（helper 单独放 `tests\tlb_identity.ps1`，`run_tests.ps1`
   里只多一行 dot-source + 一条登记）。D57-5 那条"至今未证"现在有证据了：**证的是它不成立**。
2. **BASE（`473784ed…`）实测 `tests\cc_dll`：一次编译里 `IProbe` 有四枚 GUID**
   - 语义层 = COM 服务器表 `{F5CEF988-…}`（B13b 已并）；
   - 类自己的 vtable QI `vb6_iv_iid_IProbe` = `{74B1BA3B-F40C-A0FE-9ED6-3795C52247D1}`
     （`ivDeriveIid`，key `"iviface:"+名`）；
   - 类型库 `dispinterface _IProbe` = `{1E34D82B-…}`（`generateUuid("_IProbe")`）；
   - 类型库 `coclass IProbe` = `{28B6B94B-…}`（`generateUuid("IProbe")`，接口模块被当类登记）。
   要紧的一条不是"值多"，而是**类型库广告给客户端的那枚，服务器根本不应答**：`dll_entry.c` 的
   `IID_vb6def_CImpl` 是 `F5CEF988`，而 `.tlb` 里 coclass `CImpl` 的 DEFAULT 引用是
   `_CImpl = {7CA8CD81-…}` ⇒ 早绑定客户端照库里的 IID 去 QI 必然 `E_NOINTERFACE`。
   这就是 D57-5 未证条目的真实答案，也是本格第 1 条的全部动机。
3. **合并的形式不是"再开一套 mint"，而是把接口自己的 IID 也搬进唯一出口**：
   `resolveIfaceIid(<Proj>, IfaceView)`（`[InterfaceId]` > `mintGuid(ifaceSeed)`，与
   `resolveCoClassIdentity` 里默认接口那条**共用同一个函数**，结构上不可能分叉）
   + Pass E 建 `Driver::ifaceIds_`（key = 接口名小写，源码序打印信息行）。三个消费者一律只读这张表：
   ① `ivDeriveIid`（vtable QI，命中即 `[InterfaceId]`/mint 都由表给）、② `iidMap`
   （dll_entry 的**非默认**接口，本格把 D57-4 欠的那条收掉）、③ 类型库（`_<接口>` 的 GUID
   + coclass 的 DEFAULT 引用）。**表里没有 = 这个接口不是新式接口** ⇒ 各自 legacy 派生照旧，
   存量工程逐字节不变（C04 折算隔离第三次生效，这次连"新式接口但没有块"也覆盖到了：
   `itf_xmod`/`cc_id` 两个工程没有 CoClass 块，它们的 iv 行照变）。
4. **类型库那半边还有一条结构修正**：coclass 的默认接口今天写死成 `<_类名>`（类自己那堆公有成员
   的 dispinterface），块里 `[Default] Interface IProbe` 是不算数的。本格把它接上：块的 `[Default]`
   指的若是新式接口，coclass 的 DEFAULT 引用改成 `_<接口>`。这里踩到一个登记顺序问题 ——
   符号表遍历序由 `unordered_map` 决定，`_IProbe` 可能排在 `CImpl` 之后，先引用后注册就报
   `TYPE_E_ELEMENTNOTFOUND`；解法是**只把需要重定向的那几行延后**到整轮之后登记，其余仍在原地登记。
   第一版无差别延后所有 coclass 时，`tests\test_activex_dll` 的 `TestAXDLL.tlb` 变了
   （3976→3976 字节，内容同、类型序变），是实测把它否决掉的 ⇒ 存量 `.tlb` 零变化。
5. **本格第 2 条拍板：走 (b)，新式接口的成员不发成 disp id。** 三条理由叠在一起：
   ① 契约成员按 VB6 惯例是 `Private`，塞进 IDispatch 表 = 换语义；② `[Default]` 指向新式接口的
   **声明含义**就是"这个类的门面是那个接口"，客户端因此看不见类的公有成员是这句话的应有之义；
   ③ (a) 其实什么都没解决 —— 类型库里的方法要有 dispid 才对客户端可见，而服务器的
   `GetIDsOfNames` 仍然只认公有成员，两边会假绿。所以今天对外**可调用**的是：类的公有成员
   （`_CImpl` 那一档）+ `[ComCreatable(True)]` 换来的组名 ProgID；新式接口要对外可达得走
   **真接口**（类型库 `TKIND_INTERFACE` + 契约成员进库 + QI 过去），那是 B15 类型库线的活，
   本格不越界。**给用户的读数**：编译时打一条
   `C3: CoClass 'CImpl' default interface 'IProbe' is a vtable interface: 0 Public member exported for IDispatch clients`
   （note 级诊断在成功的编译里根本打印不出来，所以仍走 stderr 的 `C3:` 信息行 —— 该通道第四次被用）。
6. **另一条只有读数能说清的旧账**：`.tlb` 里 `_IProbe` 的 `cFuncs` 是 **0** —— 新式接口模块写的
   `Sub Ping` / `Property Get Got` 从来没进类型库（typelib 那段按"类模块的公有成员"收集，
   接口模块的槽在 `IfaceRegistry` 里，两边从来没接）。这条连同第 5 条的"真接口"一起登记给 B15。
7. **护栏（BASE `473784ed…`）**：① `-Category syntax` **118/0**；② 16 件 `--emit-c` 逐字节护栏
   **升级成分类护栏** `.build/b13c_guard.py`：只允许
   `static const unsigned char vb6_iv_iid_<I>[16] = {…}` 那些行变（两个用新式接口的工程各 15 行），
   其余任何字节变化当场红 → **16/16 OK**；③ 全产物 A/B `.build/b13c_ab_all.py` 三个工程一次量：
   `cc_act`（EXE、带两个手写块）只有 5 个 `.h` 的 iv 行变、`com_entry.c` 等其余全同
   ⇒ D57-6 的"EXE 侧刻意不接"到今天还成立；`test_activex_dll`（存量 DLL）`.tlb` 与所有
   .c/.h/.def 全同（`.rc` 里只有 TYPELIB 的绝对临时路径天然不同）⇒ 折算隔离成立；
   `cc_dll`（手写块 + 新式接口）iv 行 + `.tlb` 变 = 本格的靶子。④ 新助手能红：
   假 CLSID 的负控 `FAIL`、正的 `PASS`。
8. **harness 坑（本轮两条）**：① `scripts\env.ps1` 是 **ANSI/GBK 且首行带 shebang**，
   `pwsh -Command ". scripts\env.ps1"` 会读成乱码并执行失败 ⇒ 自测脚本要 MSVC 环境就照
   run_tests.ps1 自己的办法 `cmd /c "call vcvarsall.bat x64 && set"` 灌进进程，别去 dot-source 它；
   ② 用 heredoc 往 python 里塞带反斜杠的 Windows 路径，`\2019`、`\M` 会被吃掉
   （`D:\Program Files (x86)\Microsoft Visual Studio\2019` 变成
   `…Visual StudioMicrosoft Visual Studio`），症状是一句"系统找不到指定的路径"——
   同一条老规矩第 N 次：反斜杠文本一律走 Write 工具。

**D59（B13d 规范 IUnknown + 读出来的一条危险：胖应答瘦的 IID）**

1. **收掉的东西**：新式接口薄指针的 `QueryInterface` 以前把 `IID_IUnknown` 与**本接口**的 IID
   并成一个分支、回 `*ppv = self`（BASE 实测 `tests\itf_xmod\XWriter.vbp`：
   `vb6_iunk_CWriter_IWriter_QueryInterface` 与 `..._ILog_QueryInterface` 各有
   `if (vb6_IidEqual(riid, vb6_iv_iid_IUnknown) || vb6_IidEqual(riid, vb6_iv_iid_<自己>)) { *ppv = self; … }`）。
   `__iv_IWriter` 与 `__iv_ILog` 是同一个结构体里两个不同偏移的成员 ⇒ 同一个对象从两个接口各问一次
   IUnknown 就是**两个地址**，COM 的"规范 IUnknown 恒等"不成立（D22-7④ 记的就是这个）。
   现在 IUnknown 单独一支，一律回**本类实现序第一个接口**的薄指针：
   `void* canon = &me->__iv_IWriter;  /* 规范指针 (ai/022 B13d): 本类实现的第一个接口 */`。
2. **为什么选"第一个接口"而不是别的落点**：① 零布局改动 —— `__comObj` 是裸回指指针、
   `vb6_cls_<C>` 偏移 0 不是 vtable，把 `me` 当 IUnknown 交出去会让客户对着 `__comObj` 取 vtable
   （= 直接踩飞），而任何 `__iv_<I>` 的地址偏移 0 就是 `vb6_ivtbl_*`，是这批里**唯一**既能当身份、
   又能被再次 QI/AddRef/Release 的合法对象指针；② 不新增字段 ⇒ D19 那条硬约束根本不进入视野，
   存量工程（没有 `__iv_` 字段）连形状都不变；③ "第一个"= `ivImplementedIfaces` 的源码声明序，
   也正是结构体里最靠前的那个 `__iv_`，两个"第一"天然重合，不需要新序规则。
   单接口类拿到的值与改之前**完全相同**（`canon == self`），只有多接口类的身份从 N 个收成 1 个。
3. **本批为什么只能断形状 + 真跑，不能断身份**：整条路径上**没有任何自己生成的调用点**去问
   `IID_IUnknown`（VB 侧没有 `QueryInterface` 面；接口值进 Variant/实参那条是 B06c 未开），
   真正的调用者是外部 COM 客户 ⇒ 端到端的身份断言属 B17。于是这批准据是两条：
   ① 新用例 `itf_canonical_iunknown`（`Test-CanonicalIUnknownShape`，走 B08e-6 那条
   `Invoke-CodegenProj`/`--emit-c` 通道）钉"每个 QI 都有一处规范指针分支（XWriter 期望 2 处）
   **且**旧的 `IID_IUnknown) ||` 合并分支已经不在"；② 真编译真跑的 `itf_xmod_writer`
   （QI1..QI4 + LIFE1..LIFE3 + TOF1..TOF3）保证本接口/兄弟接口两条分支与计数没被改坏。
   负控照例跑过：同一份工程在 BASE 上 `canon` 0 处、合并分支 2 处 ⇒ 用例能红。
4. **顺手读出来的一条危险，本批刻意没动**（这是本批最值钱的部分）：RTL 的
   `ComObj_QueryInterface`（`vb6comserver_obj.c:24-49`）对 `desc->ifaceIids[i]` 与
   `desc->defaultIfaceIid` 一律 `*ppv = self` —— 注释写的是 `// dispinterface: same IDispatch pointer`，
   在 legacy 世界里成立（那些 IID 本来就是 IDispatch 那档的）。但 **B13b/B13c 之后这两个字段里
   装的可能是新式 vtable 接口的 IID**，而 B13c 又让 `.tlb` 把同一个 IID 当 coclass 的默认接口广告出去
   ⇒ 早绑定客户端 QI 成功、拿到的是**胖包装器**指针，然后按接口 vtable 去调第 3 槽 —— 那位置在
   IDispatch 上是 `GetTypeInfoCount`。**调错函数比 `E_NOINTERFACE` 危险得多**（静默踩到别的方法）。
   三条出路，下一批拍板（别默认选）：
   (i) 对外**不发布**新式接口的 IID（表的 `defaultIfaceIid`/`ifaceIids` 滤掉 `iidreg_` 认识的项，
       `.tlb` 的 coclass 默认接口回到 `<_类名>`）= 小、安全、但要把 B13b/B13c 立的
       `cc_dll_identity_single_source` 与 `cc_dll_tlb_matches_table` 两条用例**翻面**；
   (ii) 让包装器**真的**应答新式 IID 并返回 `&me->__iv_<I>` = 要类侧发一张"实例→第 k 个接口指针"
       的映射表给 RTL 用（新增导出，`vb6_CoClassDesc` 加字段）= B16 的正路，也是唯一能让
       早绑定客户真用到接口的那条；
   (iii) 只让 `.tlb` 发布、表不应答 = 现状的反面，同样是客户拿不到，**排除**。
   本批不动它的理由不是偷懒：那是一条产品语义拍板（对外到底暴露不暴露 vtable 接口），而且会把
   上一批刚立的用例翻面 —— 同一批里既改身份口径又改对外发布口径，门就不干净了。
5. **护栏（BASE `759e9f51…`）**：① `-Category syntax` **119/0**（118 + 本批新用例）；
   ② `.build/b13d_guard.py` —— 判定从"只许某类行变"升级成**整段函数体豁免**：14 个不含新式接口的
   工程 `--emit-c` 逐字节全同；`itf_xmod`/`cc_id` 摘掉所有 `vb6_iunk_*_QueryInterface` 函数体后
   剩余行序列全同（QI 体 36→46 行），违例 0；③ `.build/b13d_ab_all.py` 三工程全产物 A/B：
   `cc_act`（EXE、带两个手写块）**一个文件都没变**、存量 `test_activex_dll` 只 `.rc` 那行绝对临时
   路径天然不同、`cc_dll` 只有 `CImpl.c` 的 QI 体变（**`.tlb` 与 `.def` 全同**）= 零未归类变化。
   注意 `cc_id` 一个 QI 体都没有（它声明接口但没人实现）⇒ "改动确实进了产物"这条自检要按**全局**
   行数判，按单工程判会被它误红 —— 本轮就是这么撞了一次。
6. **仍然没并的一条**（写清楚，别当已交付）：跨"包装器 ↔ 薄指针"两个世界的 IUnknown 身份还是两个
   （外部客户拿到 `vb6_ComObject*`，进程内薄指针 QI 拿到 `&me->__iv_<first>`）。把它接成一个 =
   上面 (ii) 那一半，归 B16/B17。

**D60（B13e 读数 + 对外口径拍板：广告的那一枚必须就是应答的那一枚；顺带把 D59-4 那条"危险"证伪）**

1. **本格开工第一枪打的是自己的前提**：D59-4 说"胖包装器应答瘦 IID ⇒ 早绑定客户按接口虚表调第 3 槽
   打到 `GetTypeInfoCount`，静默调错函数"。读码 + `tests\tools\tlbprobe.cpp` 实测**不成立**，两条理由：
   ① `TypeLibBuilder` 只会发 `TKIND_DISPATCH`（`addDispInterface`）与 `TKIND_COCLASS`（`addCoClass`），
   仓里**根本没有 `TKIND_INTERFACE` 那条发码路**（那正是 B15 的活）⇒ 客户从 `.tlb` 学到 `_IProbe`
   的 kind 是 `dispinterface`（本轮读数：`CoDll.tlb` 四个类型 = `_IProbe`/coclass `IProbe`/`_CImpl`/
   coclass `CImpl`，一个 `TKIND_INTERFACE` 都没有），按 IDispatch 走，不会去取接口虚表第 3 槽；
   ② 服务器侧 `ComObj_GetIDsOfNames`/`Invoke` 只查 `desc->methods`（= 类的 Public 成员），
   也从没有一条路把 vtable 接口的方法发出去。⇒ 那条"危险"要成立，客户得**从产物之外**自己知道
   接口虚表布局。D59-4 记的机制是错的，但它指的方向是对的 —— 同一次读码撞见了一条**真的**不同值。
2. **真问题（本批的靶子）**：B13c 把 `.tlb` 里 coclass 的 DEFAULT 引用重定向到块 `[Default]` 那个
   `_IProbe`，而 `_IProbe` 在库里的成员数是 **0**（D58-6 实测 `cFuncs`=0），服务器真正认账的成员面是
   `desc->methods` = 类的公有成员 = `_CImpl` 那一档。于是"广告"与"应答"是两份东西：早绑定客户照库
   编译 ⇒ 一个方法都点不到；晚绑定 `CreateObject` + 公有成员 ⇒ 反倒能用。B13c 那次改动把自己刚立的
   "表与 `.tlb` 逐值一致"从"默认接口 IID 一致"改成了"两边一起指向一枚点不到的 GUID"，一致性看着在，
   可用性掉了。教训：**"两条通道同值"这种判据必须同时钉住"值 + 那一档是谁"**，只比值会被自己骗。
3. **拍板（= D59-4 的 (i) 收窄版）：对外默认视图一律是 `<_类名>`，`[Default]` 只管语言层。**
   三处一起退回，不留中间态：① `cgen_util_dllentry_collect.inc` 不再拿出口的 `iid` 覆写
   `info.defaultIfaceIid`（改由类型库回写的 `comDefaultIfaceIid` 说话，也就是"谁应答谁上表"）；
   ② `cgen_util_dllentry_tables.inc` 删掉 B13b 那条"`ifaceIids` 里 `[Default]` 同名接口改用出口 IID"
   的分支（连带 `CoClassInfo::defaultIfaceName` 这个字段一起没用了）；③ `driver_codegen_dll_typelib.inc`
   删掉 DEFAULT 重定向 + 为它搭的"延后登记"机器（`TlbCoClassRow`/`tlbTypes`/`registerCoClass`）。
   保留的：`identityForClass`（coclass 的 CLSID 仍取唯一出口，与表同值）与 `ifaceIidFromMap`
   （接口模块自己那条 dispinterface 的 GUID 仍读 Pass E 的表）⇒ **甲判据一个字没松**。
   那条 `C3: … is a vtable interface: 0 Public member…` 信息行随之删除 —— 它的前提（库里广告的是
   零成员接口）已经不存在，留着一行只会误导。
4. **为什么没有把 `ifaceIids[]` 里"新式接口"那几项滤掉**（D59-4 的 (i) 全量形态）：滤掉就把甲判据
   从三条通道削成两条，而且表与 vtable 会**当场自相矛盾** —— 类自己的 `vb6_iv_iid_<I>` 还在应答那枚
   GUID，服务器表却装着另一枚。"胖应答瘦"的残余今天仍在一处：客户若已知接口 IID 并对着胖指针 QI，
   拿到的是包装器而非 `&me->__iv_<I>`。正解是 (ii) —— 让包装器真返回薄指针（`vb6_CoClassDesc` 加一张
   "实例→第 k 个接口指针"的映射，即 **B16**），那时这枚 IID 该应答成瘦指针、也就更不该滤。故本批
   刻意只收"广告档"，把这条写进 B16 的前置，不半接。
5. **判据落进回归**：`cc_dll_identity_single_source` 翻面（`IID_vb6def_CImpl` 回到 `0x7CA8CD81` 并
   新增为 needle，`IID_vb6iface_IProbe` 仍是 `0xF5CEF988`，撤掉那条日志断言）；
   `cc_dll_tlb_matches_table`（`tests\tlb_identity.ps1`）从一条升级成两条 —— 甲：表的
   `IID_vb6iface_<I>` == `<C>.h` 的 `vb6_iv_iid_<I>[16]` == 库的 `_<I>`；乙：表的 `IID_vb6def_<C>`
   == 库的 coclass DEFAULT 引用 == 库的 `_<C>` 那一档（再加 CLSID 与 `clsidStr` 对一次）。
   负控照例跑在 BASE 上：同一份助手喂 `pre_b13e_C3.exe` ⇒ 乙 判红（DEFAULT `{F5CEF988-…}` vs
   `_CImpl` `{7CA8CD81-…}`），喂本轮 exe ⇒ 全绿。D59-4 那句"要把两条用例翻面"也确实兑现了。
6. **护栏（BASE `905c9392cede13ab9d915f05b091ed3c`）**：① `-Category syntax` **119/0**；
   ② `.build/b13e_ab_all.py` 全产物 A/B，四工程一次量（比 B13d 多带 `test_event_dll`，因为
   `addCoClass` 从 lambda 搬回原地、事件源那条参数表达式被重写）：`cc_act`(EXE) **0 个文件变化**、
   两枚存量 DLL 连 `.tlb` 都**逐字节不变**（只 `.rc` 那行绝对临时路径天然不同）、`cc_dll` 只许
   `dll_entry.c` 的 `IID_vb6def_*` 那 1 行 + `.tlb` ⇒ 零未归类变化，且"改动确实进了产物"按全局
   行数判（`def_lines>=1`）；③ `-Category vbp` 见状态头门读数。
7. **一条 harness 坑**：PowerShell 的双引号串里写 `"…_$Var: …"` 会被解析成**作用/驱动器限定符**
   （`Variable reference is not valid`），整个测试文件 ParserError —— 消息文本要紧跟变量名时一律
   写 `${Var}`。本轮踩在 `tests\tlb_identity.ps1` 的一条中文消息上，被 `-Category syntax` 之外的
   dot-source 自测挡下，没进门。


**D61（B14 三件测量 + 范围重裁：DLL 那条对外管线第一次被真客户端走通）**

1. **测量① —— 胖包装器的 IDispatch 面（读码 + 真跑两头都量）**：
   `ComObj_QueryInterface`（`src\rtl\core\vb6comserver\vb6comserver_obj.c:24-72`）认 `IID_IUnknown`、
   `IID_IDispatch`、`desc->ifaceIids[i]`、`desc->defaultIfaceIid`，再加 CPC/PCI 两件 —— 前四处一律
   `*ppv = self`。真跑读数（新探针 `tests\tools\disp_probe.c`，进程内、不查注册表）：
   `DllGetClassObject` → `IClassFactory::CreateInstance(IID_IDispatch)` = S_OK；
   `QI(IUnknown)` `same=yes`（规范身份在胖侧成立）；**对 `IID_IProbe` 的 QI 也 S_OK 且 `same=yes`**
   ⇒ D59-4 / D60-4 那条"胖应答瘦"从推理变成实测事实（今天它仍然不可从产出的库到达：库里那一档是
   dispinterface，客户按 IDispatch 用它恰好是对的）。
   `GetTypeInfoCount` 恒 1；`GetTypeInfo(0)` 载入内嵌 `.tlb` 后**按 CLSID** 取 coclass 那份
   （实测 `TYPEINFO0_kind=5` = `TKIND_COCLASS`、`cFuncs`=0 —— coclass 的 TYPEATTR 本来就不带成员，
   成员在它引用的 implType 里，不是缺陷）；`GetTypeInfo(1)` = `DISP_E_BADINDEX`。
   `GetIDsOfNames` **完全不看 `riid`**，只在 `desc->methods` 里按名字 `wcscmp`；`Invoke` 同样不看 `riid`。
   未知名 → `DISP_E_UNKNOWNNAME`(0x80020006)，实测。
   **结论：四件套本身是通的** —— `Add(2,40)=42`、`SetValue(7)` 之后 `GetValue()=7`（跨两次 Invoke
   保住状态 ⇒ 同一个实例，包装器复用那条路也对）。所以 B14 原命题里"没接"的那半**不是**四件套缺失，
   而是**成员面为空**（见测量③）。
2. **测量② —— 薄指针那一侧没有 IDispatch 的位置**：`vb6_ivtbl_<I>` = `{ QI, AddRef, Release, <自有槽…> }`
   （`src\backend\module\cgen_iface_vtbl.cpp:297-330` 的头文件发射 + `:550-556` 的实例表）——
   IUnknown 形，**四件套那四槽不在里面**。给薄指针一张 IDispatch 面 = 在 `Release` 之后插四槽 ⇒
   接口自有成员的槽号全体后移 4。产品内部自洽（调用点全从同一份结构体发码），但这正是"真接口 / dual"
   的布局改动，而且**必须先有 B15 把接口成员发进库**，否则就是重演 D60：库里那一档 0 成员、
   服务器多应答一枚 IID，门内全绿、外面点不通。⇒ **B14 的布局那半排到 B15 之后**，本批刻意不做。
3. **测量③ —— DispId 表只有一处生产者，而且不产接口成员的**：全工程只有
   `src\driver\detail\driver_codegen_dll_typelib.inc:186` 写 `comDispid`，位置就在"类模块 Public 成员"
   那条收集循环里（`refs` 按 `access == AccessLevel::Public` 过滤）；消费者是
   `cgen_util_dllentry_tables.inc:49`（COM 表的 dispid）与类型库的 `<_类名>` dispinterface —— 两边同源
   （M29 那条回写）。**接口契约成员从来没进过这张表**。要发它只有两条路：把契约成员当公有发
   （口径 (b) 已禁止，且 `GetIDsOfNames` 只认公有成员 ⇒ 只会两边假绿）或走真接口（B15）。
4. **本批交付 = 把这条管线从"字节一致"升到"真调用得通"**：新增不查注册表的进程内客户端
   `tests\tools\disp_probe.c`（`LoadLibrary` + `DllGetClassObject` + `CreateInstance(IID_IDispatch)`，
   四件套各问一遍，另可追问第二枚 IID 看应答的是哪份指针）+ 助手 `Test-DispatchInvoke`
   （`tests\disp_invoke.ps1`，与 B13c 的 tlb 探针同一套形态：现编现用、`.obj` 落在产物目录）+ 两条
   gated 用例：`ax_dll_dispatch_invoke`（存量 `TestAXDLL.Calc` 真点通）与
   `cc_dll_dispatch_iface_only`（只满足新式接口的类：成员面**必须**为空 ⇒ `NAMES=ADD hr=0x80020006`，
   同时 `QI_EXTRA ... same=yes` 钉住今天这枚 IID 由胖指针应答）。**新助手先证明能红**：同一份用例喂一个
   不存在的 CLSID ⇒ `GETFACTORY hr=0x80040111 CLASS NOT REGISTERED` → 判红（实测）。
   这一格不改任何编译器源码 ⇒ 产物逐字节不变（BASE 与收线 exe 同一枚 md5），护栏就是这条本身。
5. **范围重裁（拍板）**：B14 收缩成本批这一格；"薄指针的 IDispatch 面 / 接口成员进库 / dual 布局"
   三条合并成 **B15 的前置问题**，顺序定死：**先把接口在库里发成真接口（`TKIND_INTERFACE` + 成员进库），
   再谈让 QI 交回薄指针（B16）**。理由就是 D60 教训的反向应用 —— 任何"让服务器多应答一枚 IID"的改动，
   都必须先满足"库里那一档的成员面 == 服务器应答的成员面"。
6. **读码撞见的一条弱点（登记，本批没动）**：`ComObj_Invoke`（`vb6comserver_obj.c:166-186`）先按
   dispid + invkind 精确配，**配不上就退回"第一个同 dispid 的表项"**。而 VB6 语义里 Property Get/Let/Set
   共享同一 dispid 是常态 ⇒ 一个只发成 Get 的属性，用 `DISPATCH_PROPERTYPUT` 问也会把 Get 那一项调出去。
   收紧（配不上即 `DISP_E_MEMBERNOTFOUND`）会改到存量工程的可观察行为，属口径题不是 bug 修 ⇒ 交下一批拍板。
**D62（B15 四件测量：一张 `TKIND_INTERFACE` 到底要什么、接口成员签名从哪来、位数 flag 有没有后果、今天的形状）**

1. **测量① = 成立条件**（`.build\tlb_iface_exp.cpp` + `.build\b15_measure1.ps1`，逐个 mode 真建真读；
   读回用新写的 `.build\tlb_slots.exe`：它打 oVft/callconv/funckind/返回 vt/参数 vt（含 `VT_PTR` 链内层）。
   `tests\tools\tlbprobe.cpp` 的格式是回归 needle 的来源，刻意没去动它）。七条读数：
   - `TKIND_INTERFACE` + `FUNC_DISPATCH` + `oVft=0`（= 照抄 `addDispInterface` 那套）⇒ **`AddFuncDesc` 就拒**
     （`0x800288BD`）。最少集合是四步：`FUNC_PUREVIRTUAL` + `oVft=(3+槽号)*指针宽` + `SetFuncAndParamNames`
     + `LayOut()`，之后 `SaveAllChanges` 出来的库读回 `kind=interface / FUNC 0 memid=1 name=DoIt invkind=1 nparams=1`。
   - 接口那一档**不吃** `SetTypeFlags(TYPEFLAG_FCANCREATE)`（同一个 `0x800288BD`，但库仍建成）⇒ 可创建位只属于 coclass。
   - `tdesc.vt = VT_I4|VT_PTR` 而 `lptdesc` 留空 ⇒ **建库进程当场崩、一行输出都没有**。ByRef 的正规建法实测可用：
     `VT_PTR` + `lptdesc` 指向内层 `TYPEDESC{VT_I4}`，读回 `vt=VT_PTR-> vt=0x0003`（mode 7）。
   - coclass 可以**引用真接口**并标 DEFAULT（`AddRefTypeInfo` + `AddImplType` + `SetImplTypeFlags`，mode 5 全 S_OK，
     读回 `REF 0 kind=interface … flags=0x1 DEFAULT`）⇒「类实现 IProbe」这一档在库里走得通，B16 不用先解这个结。
   - **0 槽的真接口建得起来**（mode 9：`CreateTypeInfo(INTERFACE)` + `SetGuid` + `LayOut` ⇒ 读回 `cFuncs=0`）
     ⇒ 本批「只修形状与身份、成员面押后」不是被建库逼的，是自选口径。
   - mode 8：把生成的 `vb6_ivtbl_IProbe` 逐字段照抄进库（两槽、`CC_CDECL`、`Got` 直接返回 `VT_I4`）**照样建成并原样读回**
     （`FUNC 0 Ping cdecl oVft=24 ret=VT_VOID` / `FUNC 1 Got invkind=2 cdecl oVft=32 ret=VT_I4`）
     ⇒ `CreateTypeLib2` **不校验 COM 规范**。这条既开门也开坑：想发什么发得出去，发错了没人拦 ⇒ 成员面的门槛
     不在库里，在我们自己的槽不是 canonical COM（见 5）。
2. **测量② = 接口成员签名的来源，并且订正 D61-3 的根因**：`Driver::ifaces_`（`IfaceView::slots`，每槽 `sig`
   指向 `SubDecl`/`FunctionDecl`/`PropertyDecl`，`body` 恒空）就是接口成员的唯一事实面，主 cgen 循环早就在用它
   （`driver_codegen_module_loop.inc:76` `setInterfaceRegistry(&ifaces_)`），而类型库这段在同一个函数体内 ⇒ 直接可读。
   **反面对照**：今天这段沿用的是**类符号**那条收集路（`clsSym.memberNames` + `lookupModuleByKind`），而 026/B02 的口径是
   Interface 块只进 `Module::interfaces`、**不进 `declarations`** ⇒ 接口模块的 Class 符号 `memberNames` 是空的
   ⇒ 实测 `_IProbe` 的 `cFuncs=0`。所以「接口成员点不通」不是 D61-3 说的「DispId 表少一处生产者」，是**成员压根没到发码现场**。
   类型 → `ELEMDESC` 的缺档（`typelib_builder.cpp:51-65` `mapVartype`，switch 只认精确枚举值）：`LongPtr`/`LongLong`/
   `UserDefinedType`/带位组合的 `Long|Array` 全落到 `default: VT_VARIANT`；`Object` 恒 `VT_DISPATCH`（工程类/接口指针
   该是 `VT_UNKNOWN` 或那枚接口的 alias）；ByRef 只写 `PARAMFLAG_FOUT`、**从不建 `VT_PTR` 链**（= 测量①那条崩溃的成因）。
   ⇒ 成员面要发，得连「参数类型保真」一起做，不是加一个 loop 就完事。
3. **测量③ = `CreateTypeLib2(SYS_WIN64)` 有后果，本批不动**：mode 2 与 mode 6 逐调用只差那枚 flag ⇒ `.tlb` **字节不同**
   （md5 `D700E512…` vs `92547F82…`，长度都是 1236），而且同一份 `oVft=24` 在 32 位库里读回 **48** ⇒ 位数声明会换算
   槽偏移，不是元数据摆设。**顺带一条流程教训**：③ 第一次跑出来是「两枚 md5 相同」的**假读数** —— cl 编译失败但旧
   `tlb_iface_exp.exe` 还在原地，脚本拿 `Test-Path exe` 当构建成功判据，于是把上一版二进制又跑了一遍。已改成先
   `Remove-Item` 旧 exe/tlb、再拿 cl 的 stdout 里有没有 `error C` 判成败（`.build\b15_measure8.ps1`）。
   ⇒ 改 flag = 全部存量 `.tlb` 重排 + oVft 语义变 ⇒ 登记为 **B16 的前置题**（真接口的 oVft 要按 `--arch` 给对，
   先得定 flag 怎么传）。另有一条今天的实测：同一工程 `--arch x86` 与 x64 的 `.tlb` **逐字节相同**
   （md5 `B0AB5567…`）—— 因为库里目前没有任何随指针宽度变的东西（`oVft=0`、无 `VT_PTR` 链）。
4. **测量④ = 今天的形状与根因**（`.build\b15_measure34.ps1` + `tlb_slots` 读 `CoDll.tlb`）：
   `TYPE 0 kind=dispinterface name=_IProbe guid={F5CEF988-…}`（Pass E 那枚，`cFuncs=0`）+
   `TYPE 1 kind=coclass name=IProbe guid={28B6B94B-B7A4-…}`（**第五枚 mint**：`TypeLibBuilder::generateUuid(clsSym.name)`，
   不在 COM 表、不在 `coclassIds_`、不在任何 QI 应答面）引用前者并标 DEFAULT。
   ⇒ 客户端导入后看到的「接口」= 一枚假 CLSID + 一张空 dispinterface + 一句「可创建」。
   根因一句话：`driver_codegen_dll_typelib.inc:84` 那条循环是**唯一没有 `isInterface` 过滤**的 COM 消费者
   （对照 `cgen_util_dllentry_collect.inc:26`、`driver_codegen_dll_sync.inc:8,35` 三处都有），而 stage 3.6
   （`driver_compile.cpp:623-641`）把被 `Implements` 点名的类一律标 `isInterface=true`（新式接口走的也是这条）
   ⇒ 接口模块在 COM 表里不存在、却在库里占一整行 coclass。
5. **范围拍板（按读数收窄，宁窄勿滥）**：B15 = **形状与身份那一半**：
   ① 新式接口宿主（判据与 B13c 同一条：`ifaceIds_` 命中 **且** `ifaces_` 有同名键）在库里发成 `TKIND_INTERFACE`，
   GUID 用 Pass E 那枚，行名用接口自己的名字（`IProbe`，不再 `_<名>`）；② 撤掉那枚假 coclass 行与它的 `generateUuid`
   mint；③ **成员面本批不发**，理由用实测表述：mode 8 证明建库什么都收 ⇒ 门槛在我们自己的槽不是 canonical COM
   （`CC_CDECL` + 原生返回值 + `vb6_ivtbl_<I>` 里没有 IDispatch 前缀 = D61-2），发成员必须先定 x86 calling convention
   与 oVft/位数 flag 口径（测量③），那是布局批（B16）的活，且按老规矩布局改动要 x86 真编译真跑，不与本批混；
   ④ 判据改点不放宽：`Test-TlbIdentitySingleSource` 通道 3 从「`kind=dispinterface name=_<接口>`」换成
   「`kind=interface name=<接口>`」，另加两条反面断言（库里不得再出现 `kind=coclass name=<接口>`、不得再出现
   `kind=dispinterface name=_<接口>`）⇒ 回退即红；⑤ 存量护栏：非新式接口宿主的每一档（`_CImpl`、coclass `CImpl`、
   stock `VBMANLIB`/`EventCalc`…）逐字节不变。

**D63（B16 三件测量 + 实施 + 判据与护栏：x86 调用约定、canonical 化的波及面、位数 flag/oVft 的裁决与迁移口径）**

1. **测量① = x86 调用约定今天不成立**：`--arch x86 --emit-c` 的产物里**一个约定修饰都没有** ——
   `vb6_ivtbl_IProbe` 的 IUnknown 前缀与契约槽全是裸声明（`long (*QueryInterface)(void* self, …);`
   = `__cdecl`），`grep -c __stdcall` = **BASE 0 / NEW 21**（`cc_dll`）。x64 是单一 ABI 看不出来，
   所以这条只能用 x86 产物说话。COM 规范要 `CC_STDCALL` ⇒ 整条薄面（前缀三槽 + 契约槽 + 它们的
   实现函数）一律加 `__stdcall`；x64 下 MSVC 接受并忽略该修饰（同 `cgen_delegate`/`cgen_com_events`
   先例）⇒ 一份生产码两架构通用。**差的另一样（canonical 返回形状）本批刻意不补**，理由见 5。
2. **测量② = canonical 化的波及面（分类护栏逐条逼出来的，不是猜的）**：三层，全部落进白名单：
   ① `__stdcall` 出现在"声明了/实现了新式接口"的工程的槽表与实现函数上（`cc_act`/`cc_dll`/`cc_id`/
   `itf_xmod` 的 `.h` 与 `.c`）；② 薄面出入口 `vb6_iv_thin_<C>`/`vb6_iv_claim_<C>` **只出现在有接口宿主**
   （类 `Implements` 了新式接口）的工程（`cc_act` 的 `com_entry.c` 6 行、`itf_xmod` 的 `CWriter.c` 15 行、
   `cc_dll` 的 `CImpl.c` 14 行 + `CImpl.h` 2 行）；③ COM 服务器表（`dll_entry.c`/`com_entry.c`）
   **每个 coclass 行**多两个字段 `ifaceThinPtr`/`instanceClaimRelease` —— 新式宿主填函数名、存量/无接口
   填 `NULL`（`ax_dll` 4 个 coclass 共 8 行、`ax_event` 5 个共 10 行）。**`cc_id` 只声明接口**
   （`Interface` + CoClass 折算）、没有宿主 ⇒ 产物里只有 `__stdcall`、没有薄面那一对。
3. **测量③ = 位数 flag 的裁决与迁移口径**：**flag 跟 `--arch` 走**（`CreateTypeLib2(is64_ ? SYS_WIN64 :
   SYS_WIN32)`）。理由：不发成员时不疼，一发成员 oVft 就是"客户端拿到的槽偏移"，恒 64 位会让 x86 客户端
   读到翻倍的偏移（D62-3 已实测换算）。**代价的实测边界**（这决定判据④怎么写）：默认架构（x64）下存量
   工程 `.tlb` **逐字节不变**（`ax_dll`/`ax_event` 在全产物 A/B 里无差异；单工程探针 md5 同 `87d34673`、
   diffbytes=0）；`--arch x86` 的存量 DLL 同尺寸（3976 字节）、**32 个字节不同** —— 首处 @20 是头部的
   位数声明字（x64 `43 00 00 00` / x86 `41 00 00 00`），其余 31 处是布局字（oVft 一族按 4/8 换算），
   且**两架构出的库恰好在同样这 32 处不同**（把 flag 的全部后果关在这一处），x64/x86 两枚读端读回的值
   **逐项相同** ⇒ 这是"声明位数如实"的必然修正，不是内容变化。判据④的口径因此写成
   "**默认架构一字不动 + x86 只有这 32 个字节的位数布局字**"。
4. **实施（按读数收窄，一次一个改动）**：`cgen_iface_vtbl.cpp` = 薄面整条 `__stdcall` + 发射
   `vb6_iv_thin_<C>`（按 IID 交薄指针）/`vb6_iv_claim_<C>`（包装器放手时退掉底座引用）；
   `cgen_util_dllentry_collect/prelude/tables.inc` = 服务器表行加 `ifaceThinPtr`/`instanceClaimRelease`；
   `vb6comserver.{h,c}` + `vb6rtl_class_com.h` = 包装器与类工厂按这两个字段接线（**接口 IID 的 `QI` 从
   "交胖指针"改成"交薄指针"**）；`typelib_builder.{hpp,cpp}` = `addVtableInterface` 改发**如实契约**
   （每成员 `FUNC_PUREVIRTUAL` + `oVft=(3+槽)*指针宽` + `CC_STDCALL` + 原生返回 vt，ByRef 建 `VT_PTR` 链），
   `CreateTypeLib2` 的 flag 跟 `is64_`；`driver_codegen_dll_typelib.inc` = 接口宿主分叉把
   `IfaceView::slots`（D62-2 认定的唯一事实面）逐槽喂进去。**零布局改动**（D19 不入视野），`__refcount` 头照旧。
5. **范围拍板（断不了的行为先断形状，写清哪半属 B17）**：① **canonical COM 的返回形状不做** —— 现在是
   "原生返回类型 + `__stdcall`"（库读数 `ret=0x0003` = `VT_I4` 直出，不是 `VT_HRESULT` + `[out, retval]`）；
   做它要改每个槽的签名与全工程调用点，且**是否必要取决于真早绑定客户能不能用**（B17 测量②）；
   ② **跨世界身份仍是两个值**：同一次 `QueryInterface` 问 `IID_IUnknown` 拿到包装器指针、问接口 IID 拿到
   薄指针（探针 `QI_IUNKNOWN … same=yes` 与 `QI_EXTRA … same=no` 并排就是这条读数）—— 合一属 B17；
   ③ 薄指针本身**可交付**：前 3 槽就是 IUnknown 三件套，客户端 `IUnknown_Release((IUnknown*)thin)` 是
   规范姿势（本批探针就这么收尾的）。
6. **判据（五条都对上读数）**：① `cFuncs=2` + 每成员的 `oVft`/`callconv`/`funckind`/返回 vt/参数 vt
   都有读数 —— 新探针 `tests\tools\tlb_slots.cpp`（自 `.build` 升格入库，与 `tlbprobe.cpp` 刻意分开：
   后者的打印格式是 identity 判据的 needle 来源）+ 助手 `tests\tlb_contract.ps1` + 两条 gated 用例
   `cc_dll_tlb_contract_x64/x86`（**x86 那条必须用 x86 读端**：同一份库 x64 读端会把 12 读成 24）。
   ② 真跑：`tests\tools\disp_probe.c` 长一条"按库里形状直调 vtable 槽"的支路 —— `CREATE_IFACE` 拿薄指针、
   `VTBL_GET_BEFORE=0` → 经槽 3 写 21 → `VTBL_GET_AFTER=42`（x64 与 **x86** 各一条 gated 用例，
   x86 那条是"布局改动只能靠真跑收"的那一半）。③ 「广告 == 应答」：`QI_EXTRA` 从 B14 钉的 `same=yes`
   **故意翻成 `same=no`**（= 交的是薄指针、不再是 IDispatch 包装器），且薄指针的槽 3/4 与库里那一档的
   `oVft=24/32` 一一对应（同一份形状）。④ 逐字节：`--emit-c` 17 件 `violations=0/17`
   （`cc_act`/`cc_dll`/`cc_id`/`itf_xmod` 四件按白名单分类，其余全同）+ 全产物 A/B `unclassified=0`
   （只有 `cc_dll` 的 `.tlb` 1484→1624 字节的**预期变化**，存量工程默认架构的 `.tlb` 与全部 `.def`/`.rc`
   一字不动）。⑤ 见 5。
7. **负控与"用例真能红"**：`.build\b16_negctl.ps1` 拿收线前的 `pre_b16_C3.exe` 跑两条契约用例 ⇒
   `pass=0 fail=2`、四条 needle 全 miss、且反面断言 `cFuncs=0` 命中；`.build\b16_cases.ps1` 拿新 exe 跑
   五条（契约 x64/x86 + 调度 x64/x86 + identity）= `pass=5 fail=0`。**注意 `.build` 里那两个 harness 是
   临时件（gitignored）**，入库的是 `tests\tlb_contract.ps1` + `tests\tools\tlb_slots.cpp` +
   `tests\tools\disp_probe.c` 的修改。
8. **流程教训（三条本批新踩的）**：① **套件在 CI 上用 pwsh 7 跑**（`ci.yml:121 shell: pwsh`），
   `Join-Path $a b c`（4 参 = `-AdditionalChildPath`）是 PS7 才有的形式，PS 5.1 下**静默给空串**
   ⇒ 本地复现套件必须用 `pwsh`，否则 x86 助手"建不出来"是假象（第一次跑 harness 就这么误红过）。
   ② **套件的 `$env:LIB` 是 x64 在前**，用 x86 的 `cl` 链接会把 x64 的 `ole32.lib` 挑走
   （实测 `LNK4272` + 10 个未解析外部）⇒ x86 助手/探针构建时临时 `$env:LIB = $msvc.LibX86`（两个助手都这么修）。
   ③ 探针里按槽直调要**先解一层**：薄指针是 `{ vt }`，`((probe_ivtbl*)p)->Got(p)` 是把 `p` 当槽表
   （实测读到对象后面的堆、当场崩）；正确写法 `*(probe_ivtbl**)p`。另：崩溃型探针必须
   `setvbuf(stdout, NULL, _IONBF, 0)`，否则块缓冲把已打印的读数全吞掉（B15 那条 `Test-Path exe` 教训的同族）。

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
- 2026-09-24 00:55–01:45 **B08e-1（13 站地图的第 ① 站：`With w` 块内 `.M()` 接上类虚表派发）过门并提交 `e531d82`**：开工时 00:55 见上一轮（B08c）已把状态头改回 IDLE、门 153/0/1/154，按流程重占 BUSY 后从 `CURRENT_BATCH=B08e` 动工。改动一处（`cgen_expr_with.cpp` 的 ClassInstance 分支，+13/-1），口径照 B08d：`resolveClassMemberCall` 结果交给 `virtDispatchCallee(..., mustDispatch=true)` 改写，属性写方向显式排除（3.4b 无 Let/Set 槽，拿成员名去查会命中同名的 `get_` 槽、把写变成读）。**门一次过**：01:11 起跑（跑前 `who_is_building2.ps1` 静默）、01:37 收线 `Results: PASS=153 FAIL=0 SKIP=1 TOTAL=154`，跑前后 exe md5 均 `00a3a9b4` → 可归因；条目数与上一批相同，因为新证据 （`INH27..INH31`）全在既有用例 `cls_inh_pair` 内部。**A/B 负控**：worktree `D:\.wt_b08e_base` @7f34a91 建的基线二进制 `bc3eaa3b` 跑同一份用例 → `INH27:FAIL base` / `INH28:FAIL hi bob (base)`，本批两条 OK；**逐字节护栏** 8/8 全同。另外做了三件不改码的勘察（结论都在 D35）：① **13 站逐条裁决**（②③ 探针无需改、④⑤⑥⑦ 可接派发、⑧⑨⑩⑪⑫⑬ 只能判死）；② **可达性探针实证 ⑤ 站正在产出错代码** —— `Me.m_h.Speak()` 静默答 `base`，同一对象裸写 `m_h.Speak()` 已正确答 `derived`，`With w : .Speak()` 本批修好；类数组元素 `arr(1).Speak()` 是硬失败（C2224）不是静默；③ **cgen 期诊断 `--syntax-only` 抓不到**（`driver_compile.cpp` syntaxOnly 早退在 :426、3.4b 在 :376）→ 判死那一批要先给 `run_tests.ps1` 补 `Test-CompileFail` 助手。手册 `Inherits 语句.md` 顺手订正一句**把洞说成已支持**的错话（原文称字段链 `me.m_oSocket.Pick()` 正常派发，实为 ⑤ 的静默直调），并补上 `With` 已交付、`Me.<字段>.成员名` 未交付。**本轮环境事故（记给下一轮）**：给基线 worktree 建二进制时，后台 bash 任务在 ~8 分钟处被回收、把 ninja 打断在 79/114；随后 `base_build.ps1` 无条件重跑 `cmake -S` 触发 `ninja -t restat build.ninja: failed recompaction: Permission denied` →  configure 步骤假失败（对象文件其实全在，`ninja: no work to do` 即证明）。留下 `.build/base_build2.ps1`（`build.ninja` 存在就跳过 configure），下一轮建基线直接用它的 `-Wt` 形式。基线 worktree `D:\.wt_b08e_base` 本轮收线后已 `git worktree remove`。
- 2026-09-24 01:55–02:35 **B08e-2（⑤ 站：`Me.<字段>.方法()` 接上类虚表派发）过门并提交 `8987386`**：开工时 01:55 见上一轮（B08e-1）已收口为 IDLE，重占后按 `CURRENT_BATCH` 从 ⑤ 动工。改 `voidptr_com.inc` 一处（Fix 088b 分支，+10 行）：`thisArg` 算好后交给 `virtDispatchCallee(..., mustDispatch=true)`，`_prop_let_`/`_prop_set_` 跳过。新类 `tests/cls_inh/InhHolder.cls` + `Inh.vbp` 登记 + 断言 31→34。**门一次过**：02:03 起跑（跑前 `who_is_building2.ps1` 静默）、02:29 收线 `Results: PASS=153 FAIL=0 SKIP=1 TOTAL=154`，跑前后 exe md5 `62609690` 一致；日志里 43 处 "FAIL" 字样全是 `[SYNTAX-FAIL]` 用例名。**A/B 负控**：本轮起改了取基线的办法 —— 不再每批重建 worktree，而是**改码前** `cp .build/C3.exe .build/pre_b08e2_C3.exe`（= B08e-1 收线那颗 `00a3a9b4`）当"修复前"侧，省十分钟以上；用它跑同一份用例 → `INH33:FAIL base` / `INH34:FAIL hi bob (base)`，本批两条 OK，`INH32`（裸字段接收者）两侧都绿=对照。8 文件 `--emit-c` 对同一基线 8/8 全同。**本批最重要的一条是撤回**：顺手加的 `INH35`（`Me.m_up.Level` 属性读 + `Level` 的 Overridable/Overrides 对）在 ⑤ 改完后**仍拿基类实现的 `5`** —— 属性读根本不走 ⑤，而是落 ⑩ `class_fallback.inc:224`（发射出来的形状是 `vb6_InhBase_prop_get_Level((void*)me->m_up  /* class var .m_up field */)`）。据此把 ⑩ 的裁决从"统一判死"订正为"与 ⑤ 同法"（marker 只是 C 注释 → `cvtblObjIsPure` 判纯，可接派发），并**把 `Level` 属性对与 `INH35` 一起从用例里撤走**（不把错行为钉成正例），现成用例代码连同"错侧"证据写进 D35 的 ⑩ 行，下一轮直接照抄。两条踩过的坑记此：① `InhDerived` 里覆盖属性时裸写 `m_lvl` 会撞 VB3022（继承成员不许裸名），必须 `Me.m_lvl`；② 状态头那次"02:20 中途重占"写的是未来的时间（真实 02:03 才起跑门），收线时已改回事实。手册 `Inherits 语句.md` 同步：把"字段链已派发"写实、把 `Me.<字段>.<属性>` 的读列进未交付。
- 2026-09-24 02:55–03:15 **B08e-3：零代码交付（裁决纠偏 + 找到先决条件站点⑭），本轮不提交代码**：按上一轮 ⑩ 行写下的"与 ⑤ 同法可接"动工 —— 先在 `class_fallback.inc:224` 加派发，`INH35` 照旧拿基类实现的 `5`、发射出来的 C 一字未变 → ⑩ 不是属性读的落点（上一轮的推断错在这里）；换到 `m22_module.inc:120`（⑪）后立刻炸出 `error C2039: "prop_let_level": 不是 "vb6_cvtbl_InhBase" 的成员` → 落点找对了，但撞上写路径：`cgen_util_comwrite.cpp` 的 Pattern C/D2 处理 `obj.Prop = v` 是**字符串级改写**（先发一遍读再把 `prop_get_` 换成 `prop_let_`），callee 变成派发表达式后被盲换成一个 3.4b 故意不建的槽字段。**据此把 ⑩ 行退回"判死"、⑪ 行改成"真落点但被⑭卡住"，新增站点⑭（D35-6，含三条出路，建议选 (b)：让 Pattern C/D2 认得 `__cvtbl)->` 前缀）**，并把 `INH35`/`Level` 属性对与两处发码改动一起还原 —— `git status --porcelain src tests` 为空即树与 8987386 逐字节一致，**故本批不跑门**（先例 B00：纯文档批次测不到新东西），只把 `.build/C3.exe` 增量重建回 HEAD 源码并用 8 文件 `--emit-c`（对 `.build/pre_b08e3_C3.exe` 全同 8/8）+ `Inh.vbp` 34 条断言全绿来证明还原干净。**记一条流程教训**：上一轮的 ⑩ 行写着"实测订正"，但那次"实测"只看了运行结果、没核对发射出来的 C 出自哪一支，于是把错误的落点判断连用例配方一起传给了下一轮 —— 从此 B08e 系列的负控一律要附**发射形状对比**（本轮就是靠对比 ⑤/⑩/⑪ 三处的输出形状在 5 分钟内定位的）。另记：改码前 `cp .build/C3.exe .build/pre_<批>_C3.exe` 当"修复前"侧，比每批重建 worktree 省十分以上（本轮沿用）。
- 2026-09-24 03:19–04:28 **B08e-4（站点⑭ + ⑪：`Me.<字段>.<属性>` 的读接上类虚表派发）过门并提交 `392a52d`**：上一轮 D35-6 给了三条出路，本轮实际走了**第四条**——不动 `cgen_util_comwrite.cpp`，改在发码层加`bool suppressVirtDispatch_`（`cgen_state.inc`），5 个 `emitExpr(*node.target)` 求值左值期间置位，①⑤⑪ 三处派发点见到就直调：Pattern C/D2 只消费**左值文本**，只要左值永远不带派发表达式，它看到的仍是自己认识的老形状，读上下文照常派发。顺带挖出 B08e-3 没想到的事实：裸 `m_up.Level = 5` 一直正确，是因为`cgen_assign_prop_write.inc` 有一条专门的属性写路径（要求 object 是 `IdentifierExpr`）会在左值求值之前直接发`vb6_<C>_prop_let_P(obj, v)`；带 `Me.` 前缀才落进该判据之外、退到字符串级重写 —— **⑭ 的真实触发条件是"左值 + `Me.` 前缀"**，比"属性写"窄得多。⑪ 接线同 ①⑤（`thisArg083d` 一份两用、`_prop_let_`/`_prop_set_` 与 `suppressVirtDispatch_` 双跳过）。用例：`InhBase` 加 `Protected m_lvl` + `Overridable Property Get Level`/`Property Let Level`、`InhDerived` 加 `Overrides Property Get Level`（体内 `Me.m_lvl`）、`InhHolder` 加 `ViaLevel()`/`ViaLevelBare()`，断言 34→36；**A/B**：基线 `.build/pre_b08e4_C3.exe` 上 `INH35:FAIL 5`（基类 getter），本批 OK，`INH36`（裸接收者对照）两侧都绿；8 文件 `--emit-c` 对基线 8/8 全同。**门 03:29–04:20**：`Results: PASS=153 FAIL=0 SKIP=1 TOTAL=154`（跑前后 exe md5 f2547af6 一致才记账）—— 起跑那一刻 `who_is_building2.ps1` 抓到另一个写入者正从 `C:\Users\Administrator\Documents\c3.vb6.pro` 那份副本跑 `Inh.vbp`（正是 D34-8 记过的那棵树），本轮按纪律**不杀不动**，靠 md5 前后一致判定测量仍可归因；门比平常慢约一倍。**两条记过的失误**：① 提交标题从上一批复制串了行，写成"With 块内…（B08e-1）"而内容是 B08e-4 —— 共享树里不改历史，已在状态头按哈希声明清楚；② 往 `.cls` 里用 `sed -i` 插含 `&` 的行会被替换式吃掉（探针文件被弄坏一次，改用 Write 工具重写），沿用本仓"别拿 shell 内联改文件"的教训。手册 `Inherits 语句.md` 同步：`Me.<字段>.<属性>` 的读从"未交付"移到"已交付"，并写明写方向仍直调。
- 2026-09-24 04:31–06:00 **B08e-5（⑥⑦ 站：默认属性调用式 `m_up(9)` 接上类虚表派发）过门并提交 `d9eca95`**：原判的"先做 ④"被实测改写了：**④ 现在走不到静默直调** —— `u.h.Speak()`（UDT 里的对象字段）发的是非法 C `u.h.Speak()`，编译期就 `error C2039`，属**路由缺陷**，另登为站点⑮（不在 13 站计数里）。真正可达的是 ⑥⑦：同一实例、同一 `Item`，显式 `m_up.Item(3)` 早已派发、默认式 `m_up(4)` 发 `vb6_PBBase_prop_get_Item(me->m_up, vb6_VariantFromValue(4))` → 运行期答 `base4`。两处 Pattern L（`call_callee_ident.inc:144`、`call_callee_member.inc:224`，文本一模一样只差缩进）加同口径派发，带 `suppressVirtDispatch_` 保护。用例：`InhBase` 加 `Overridable Property Get Item(ByVal v As Variant)` + `InhDerived` 的 `Overrides` + `InhHolder` 的 `ItemDefault()`/`ItemBare()`，断言 36→38；A/B 用 `.build/pre_b08e5_C3.exe`（INH37 修复前 FAIL base9）；8 文件 `--emit-c` 全同 8/8；修复前后的发射形状两行 C 一起抄进 D35-8（B08e-3 的教训兑现）。**踩到的一条既有限制**（不是本批引入）：Pattern L 固定用 `vb6_VariantFromValue` 打包索引，`Item` 声明成 `ByVal i As Long` 时默认式撞 `error C2440: vb6_VARIANT → int32_t` → 本用例的 `Item` 必须是 Variant 索引。**门 04:49–05:50（61 分钟）**：`Results: PASS=153 FAIL=0 SKIP=1 TOTAL=154`、跑前后 exe md5 `ac14cf25` 一致；慢的原因是另一棵树（`C:\Users\Administrator\Documents\c3.vb6.pro`）同时在做大构建（一眼数到 43 个 C3.exe / 20 cl.exe / 37 link.exe），按纪律不碰它，只在 05:16 中途重占过一次锁。**流程变更（用户 05:36 指示）**：全量回归改到 GitHub Actions —— 推 `github` remote 的 `dev` 分支触发 `.github/workflows/ci.yml`（build + 分片 regression 矩阵），用 `scripts/watch-gh-actions.ps1` 盯；本地 `all` 门降级为里程碑级，批次内只做 `-Category syntax` + 目标用例单跑 + `--emit-c` 护栏。本批仍按本地门记账（起跑时还不知此指示）。**失误一条**：本批 commit 标题又从上一批复制串了行（连续第二次），这次在推送前用 `--amend` 改回 d9eca95 的正确标题，并用 `git diff a81d779..HEAD --stat` 校验只含本批 6 个文件、内容零变化；教训写进状态头：message 一律现写。
- 2026-09-24 05:58–06:10 **门流程切到 GitHub Actions（用户 05:36 指示）**：`git push github HEAD:dev`（快进 `a227c7a..f8e3a1a`，含本批 `d9eca95` + `f8e3a1a`）→ `.github/workflows/ci.yml` run **#10 [dev] = completed/success**，05:58 触发、06:08 收线，**约 11 分钟**跑完 7 个 job（build / smoke / bas#1 / bas#2 / syntax / compile / vbp；CI 是 **Release** 构建，与本机 Debug 门两个口径，记账时分开写）。对比：本批那轮本地全量门 04:49–05:50 = **61 分钟**（另两棵树在抢 CPU 时更慢），一小时一轮的自动化根本放不下 → **从此批内不做本地 `-Category all`**，只做三项快检（`-Category syntax` 或目标用例单跑 + 目标 vbp 单跑 + 8 文件 `--emit-c` 逐字节护栏）+ A/B 负控，全量门推 Actions 后用 `watch-gh-actions.ps1` 盯。**脚本必须用 pwsh 7 跑**：`"C:/Program Files/PowerShell/7/pwsh" -NoProfile -File scripts/watch-gh-actions.ps1 [-Once]` —— Windows PowerShell 5.1 会把这个 BOM-less UTF-8 脚本按 ANSI 读，中文注释直接炸成 `ParserError: UnexpectedToken`，看着像脚本坏了其实是用错 shell。红线不变：只推 `github` 的 `dev`，不建 MR、不碰 main、不推 `origin`(gitcode)。
- 2026-09-24 06:16– **B08e-6（⑨ 接派发 + ⑩⑫ 判死 + `Test-CompileFail` 前置）提交 `40eea3f`**：开工前按发射文本核了一遍 ⑧⑨⑬，**三条里两条裁决要改**（D35-9 ②）：⑧ 的 `Own.Speak()` 用**改动前**的二进制就报 `VB3027`（报它的是优先级2 `class_module.inc:49`，Fix 090al 的 `inferClassTypeOfExpr` 给属性标识符也推得出类名 → `itClassVar` 命中，:140 那一支对带槽成员根本走不到）→ 不改码，只用 `ci_n26` 把既有契约钉住，并写明它是防回归不是新证据；⑨ 的 `Fix 088d` 接收者是 `(void*)vb6_ret_<函数名>`（本函数返回值的裸局部，纯读）→ **能接派发**，成了本轮唯一的行为改进；⑬ 的 `vb6_UC_InstanceOf(hwnd)` 发的是 `r->me`（`uc_host.c:309`，宿主按 `typeName` 建的那个 `.ctl` 实例）→ 动态类型恒等于静态类型，直调本来就正确，**判死是给正确代码凭空造错**，改判无需改。真正落地的：⑨ 派发（`Inh.vbp` 断言 38→40，**INH39** 改前 `FAIL base`/改后 OK，**INH40** 两侧都绿当对照）、⑩⑫ `mustDispatch=true` 判死（`ci_n25`/`ci_n24` 改前 `--emit-c` 退出码 0 + 静默直调的 C 各一行，改后 1 + `VB3027`，形状都抄进 D35-9③）。**前置助手**：`tests/run_tests.ps1` 加 `Invoke-CodegenProj`/`Test-CompileFail`/`Test-Compile`，跑的是 **`--emit-c`** 而不是真编译 —— 它跑完前端+语义+发码（正好越过 `driver_compile.cpp:426` 的 syntaxOnly 早退），又不用付 cl.exe/link 的钱，实测不在源码目录落文件。护栏：8 文件 `--emit-c` 对 `pre_b08e6_C3.exe` 全同 8/8、`-Category syntax` 78→**82 全绿**；判死只在链上有 `Overrides` 时生效，而全仓除 `tests/cls_inh`/`tests/cls_neg`/`.build/probe_*` 没有任何工程写 `Overridable`（一条 `grep -rln` 就是证据），可达面即本批用例本身。**登记一条既有缺陷（未修）**：`com_entry.c` 给派生类的**覆盖**过程发 extern 时用了基类 C 类型名，而 `typedef struct vb6_cls_<基>` 的前置声明排在它后面 → `error C2143`（`Overrides` 一个返回工程类类型的方法即触发）；属 P6/B13 那片，本批探针改成不覆盖返回对象的方法绕开。**全量门 = Actions**（推 `github/dev` 触发 `ci.yml`，`watch-gh-actions.ps1` 用 pwsh 7 盯）：结果记在状态头 GATE_BASELINE。至此 13 站全部出完，下一批 **B08e-7 = 站点⑮**（UDT 对象字段的调用位置路由），⑮ 一出就开 **B09 = `MyBase` + 构造链顺序（那处要顺手把 `com_entry` 的 `Overrides` 返回类型缺陷一起处理，B09 的 `MyBase` 转发桩会先撞上它）**。
- 2026-09-24 07:16–07:50 **B08e-7 勘察轮（零代码改动，不跑门，先例 B08e-3/B00）**：按状态头领的活是"站点⑮ = UDT 对象字段的调用位置路由"，实测把它**推翻成三条不相干的缺陷**，并且**它根本不属于 B08e**（B08e 的 13 站在上一轮就出完了）：① `Set u.h = d` 发成 `u.h = vb6_VariantFromValue(d);  /* Set */` → 结构体字段本来就是 `vb6_cls_U7Base* h;` → C 编译期 **C2440**；② `u.h.Speak()`（无参）发成 `u.h.Speak()` → **C2039**；②' 更坏的一条：`p.h.Tag(3)`（带参、p 是 UDT 形参）居然**编得过**，因为发的是 `vb6_ComCall(vb6_ComGetObjectProp(p, L"h"), L"Tag", …)` —— 把工程类实例当 IDispatch 晚绑定，运行期必崩（D20 那一族的老坑），`With p : .h.Tag(4)` 同一条路；③ 对照组 `Set w = u.h : w.Speak()` 现在就正确（发射形状已是 `((const vb6_cvtbl_U7Base*)…)->speak(w)`）。**方法论一条**：`--emit-c` 对 ①② 返回 **0**、什么都不报（这两个是 C 编译期才炸的），所以上一批刚铺的 `Test-CompileFail` 帮不上 ⑮ 的忙 —— ⑮ 的验收只能走真编译（`-Category compile`/`vbp`）或正例运行期断言，别把"`--emit-c` 退出码 0"当成"这形状没问题"的证据。三条与落点（`cgen_setlet_set_rhs.inc:31/61` 的 Variant 包装没区分 `vb6_cls_X*` 结构体字段；`appendUdtObjFieldMarker`/`generic_access:125-150` 的标记通路在调用位置没被用上；判据 `udtFieldObjCType` 现成）连同实测表写进 **D36**，批次表新增 **B08f**（⑮a 写方向 / ⑮b 调用方向 / ⑮c 派发），下一轮 **B08f-1 = ⑮a**；手册 `Inherits 语句.md` 的未交付那条从"连编译都过不去，属路由缺陷"改成实测的三种行为。本轮**未动任何代码**（`.build/probe_b08e7/` 是探针，gitignore 内），因此没有门可跑、也没有可提交的构建。
- 2026-09-24 07:31–09:30 **B08f-1（跨模块 UDT 对象字段：⑮a+⑮b+⑮c 一次做完）提交 `77ecef1`**：开头按 D36 的落点去改 Set 发码（新增 `udtFieldCTypeOfTarget` + 在 `cgen_setlet_set_rhs.inc` 否决 Variant 判定），改完实测**同一条语句一字不变** → 那条守卫当时确实是死代码，revert；顺着"为什么改不到"再挖一层才碰到真根因（**D37**）：`As <项目类>` 跨模块引用时 `resolveTypeRef` 认不出来（Class 符号要到 stage 3.5 才注入本模块作用域）→ 一律回退 `Variant`，而 UDT 成员登记只在 `UserDefinedType`/`Object` 两个分支里存 `typeRefName` → **类名连名字都没留下**；同一个字段因此有两套口径：结构体发射器（跑在 3.5 之后）按名字查得到类、发 `vb6_cls_X* h;`，成员元数据说是 Variant。**判别实验**钉住结论：把同一个 UDT 声明在类模块内，`Set t.h = me` 与 `t.h.Speak()` 两条**改前就正确**（metadata 全对）。三处接线一起改：语义层 Variant 分支也存类型名（只加元数据、不动 `mi.type`；`typeRefName` 全仓 9 处读侧逐条核过，要么限定 `mi.type`、要么查到名字后还要 `kind == UserDefinedType` 才认 → 塞一个 Class 名它们全都看不见）、`udtFieldObjCType` 按当前符号表回判、Set 侧按真实 C 类型否决 Variant 容器判定（`void*` 的 COM 字段维持 Fix 084n 原样）。**通路一通就把派发接上**（D35 站点④ 至此第一次可达，那 8 行不接就正好是 D27-13 判过的"编得过、调用静默退化"，留着比接上更危险）：`u.h.Speak()` 从非法 C 变成 `((const vb6_cvtbl_InhBase*)((vb6_cls_InhBase*)(void*)u.h)->__cvtbl)->speak(...)`。**验收只能走真编译**（`--emit-c` 对这些坏形状全返回 0 —— 上一轮记的那条方法论这轮用上了）：`Inh.vbp` 断言 40→43（INH41/INH42 判别、INH43 基类对照），**改码前的二进制编不过这个工程**（C2440 + C2039，A/B 用 `pre_b08e6_C3.exe`）；8 文件 `--emit-c` 全同 8/8、`-Category syntax` 82/0。测试用例写成"UDT 与使用点同模块"，因为另两层本批没碰：**⑮d**（UDT 声明在第三个模块 → 消费者模块 `lookupModule(UDT 名)` 查不到，同一坏形状原样复发）、**⑮e**（属性写穿过 UDT 对象字段：改前 `u.h.Level = 5;`、改后 `u.h->Level = 5;`，两种都是非法 C，不是本批引入的回归）。另记一条小瑕疵：标记注释会漏进发射语句（`u.h  /* udt objfield … */ = d;`，C 语法合法、且类模块那一形改前就这样），未动。**门 = GitHub Actions**（推 `github/dev` 的 `40eea3f..77ecef1`，pwsh 7 跑 `watch-gh-actions.ps1` 盯），结论与 run#head_sha 核对记在状态头。下一批 **B09 = `MyBase` 显式基调用 + 构造链顺序**。
- 2026-09-24 08:32– **B09（`MyBase` 去虚化基调用 + 构造链根→叶 + 前置 B09-0）提交 `02bac92`**：`MyBase` 走**发码层按接收者名字接管**（precheck 的 `Err`/`VBA` 先例），lexer/parser 一行未动 → 零新语法逐字节护栏构造成立；实现取“就近声明”（基类自己 > 基类的 `inhProcs`），C 名与 B07b 中转函数、虚表表项同一套拼名，**不查 `__cvtbl`**；读按 `Get>Function>Sub>Let>Set`、写只认 `Let/Set`；`MyBase.X = v` 从 `Module.var` 回退里抢回来。判死三条形 `VB3028`（无 Inherits / 基面无此名 / 基类 Private 成员 = C 层 static）。构造链 `emitClassInitChain` + 桥接 `vb6_<基>_chain_init`（仅“写了 Class_Initialize 且被谁继承”才发），`MyBase.Class_Initialize` 复用同一座桥；`Class_Terminate` 反序链未做。前置 B09-0：`com_entry.c` 的类返回类型 extern 改 `struct vb6_cls_X*`，去掉 typedef 分块顺序依赖（`Overrides` 一个返回工程类的方法即 C2143）。`Inh.vbp` 断言 43→52（**改码前编不过这个工程**：C2143 + 5×C2065）、负例 `ci_n27`/`ci_n28` 走 `--emit-c` 断言 VB3028（改前退出码 0）、8 文件 emit-c 8/8、`-Category syntax` 82→84 全绿。门 = GitHub Actions（推 `github/dev`，pwsh 7 跑 `watch-gh-actions.ps1`），结论与 run#head_sha 核对记在状态头。下一批 **B10 = `Implements Via`（委托式实现，免手写转发桩）**；仍开：`Class_Terminate` 继承链、`Set MyBase.<属性> = obj` 未验、基类自家 extern 的 `void*` 返回类型。
- 2026-09-24 09:43 **B09 的门 = GitHub Actions run #13 [dev] = completed/success**（head_sha 已核 = `02bac92`，本机 `git rev-parse HEAD` 同值；build + smoke / bas#1 / bas#2 / syntax / compile / vbp 全绿，Release 配置）。收线前另做了一项目前不在门里的检查：**x86 侧 `Inh.vbp` 段错误**，拆用例定位到`m_pt` 之后的基类字段写入越界，根因 = 继承的 **Private UDT 字段**在派生结构体里发成 `void*`（x64 8 字节巧合对齐、x86 4 字节 → 前缀布局错位、`_New` 少分配 4 字节）。**B07b 起就有，不是本批引入**（改码前的源码在 x86 已经越界写 `g_viaRet`/`m_name`，只是没崩）。登记为 **B09b**（实测与验收要求见 **D39**），下一批先修它再开 B10。本轮零代码改动（代码已在 `02bac92`，门已过）。
- 2026-09-24 10:18– **B09b（祖先 `Private` UDT 的类型名进派生模块作用域 = x86 继承字段错位）提交 `debb110`**：先用“改一个词”的判别实验钉死洞的性质（同一工程把 `Private Type TPoint` 写成 `Public`，派生结构体立刻从 `void* m_pt;` 变 `vb6_type_TPoint m_pt;` 且 x86 从段错误变全绿）→ 缺的**只是名字**，不是定义、不是 include、不是尺寸：`getPublicSymbols()` 按 `access != Private` 过滤，祖先的私有类型符号从来没进过消费者作用域，`mapTypeRef` 于是回落 `void*`。落点在 `runCrossModuleResolution()` 末尾加一条pass（沿 `Inherits` 链、只补被 `inhFields` 用到的 UDT/Enum 名、本地同名不抢、`udtMembers` 一并复制）。**顺手清掉三条从 B07b 就挂着的 x86 红字**（INH35/INH36/INH39 读的都排在 `m_pt` 之后 = 同一个洞，不是派发逻辑错）。**门里新增 `cls_inh_x86`**（同一份 `$inhExpected` 清单跑 x64+x86 两遍）→ 布局类改动从此自动双架构覆盖。实测：`tests/cls_inh` 两架构 build+run 各 **52/52、0 FAIL**（改码前 x86 崩）；8 文件 `--emit-c` 对 `pre_b09_C3.exe` 全同 8/8（这一版 BASE 在 B09 之前，连 B09 一起证没漂）；`-Category syntax` 84/0。门 = GitHub Actions（推 `02bac92..debb110`，pwsh 7 盯 `watch-gh-actions.ps1`），结论与 run#head_sha 核对记在状态头。下一批 **B10 = `Implements Via`（委托式实现，免手写转发桩）**。
- 2026-09-24 10:56– **B09c（`Set MyBase.<属性>` 目标侧 + P3 收尾两条实测）提交 `9eb2ca7`**：三条候选先一起量再动手 —— 只有 ① 需要改码（改码前发 `vb6_T9Base_prop_set_Peer(MyBase, me)`，函数名与实参序都对、**错的只是接收者** → 在 SetStmt 各分支之前接管，`tryEmitMyBaseAssign(forSet=true)` 只认 Property Set，命中 Let 判死）；② ⑮e 早在 B08f-1 就通了（D37 那条是在坏元数据上量的，现测：写绑根 `prop_let_`、读按实例派发、运行期 109 两架构一致）→ 只补断言；③ `Class_Terminate` 继承链**不做** —— EXE 工程三种形状实测都不触发 terminate（`_Destroy` 只有接口 Release 归零与窗体销毁两个调用点），现在发就是发死代码，登记为生命周期/P6 那片的一个既有缺口。自己写错的两条期望也记进 D41（`Level` 在 `InhSib` 分支上没有槽 = 按分支建表是对的；根类 `Class_Initialize` 是赋值不是追加）。验收：`Inh.vbp` 断言 **52→59**、**x64 与 x86 各 59/59**、A/B 改码前编不过（1×C2065）、8 文件 emit-c 对 `pre_b09c_C3.exe` 8/8、`-Category syntax` 84/0。门 = GitHub Actions（推 `debb110..9eb2ca7`，pwsh 7 盯 watcher）。**P3（继承线）到此收口**：B07a/B07b/B08a-d/B08e 13 站/B08f-1/B09/B09b/B09c 全出，下一批 **B10 = `Implements Via`（委托式实现，免手写转发桩）**。
- 2026-09-24 11:40– **B09c 收线（只读 + 文档，未改编译器代码）**：Actions run **#15 [dev] = completed/success**， 收线后核 **head_sha = `9eb2ca7`** = 本机 `git rev-parse HEAD`，7 个 job（Build + smoke/bas#1/bas#2/syntax/compile/vbp） 全 success；vbp 分片日志里 `cls_inh_pair ... PASS` 与 `cls_inh_x86 ... PASS` 逐条可见，该分片 `PASS=18 FAIL=0 SKIP=1 TOTAL=19`。 状态头按此记账（`STATUS=IDLE`、CURRENT_BATCH 交出 **B10**、BASE 换 `pre_b10_C3.exe`，本轮 exe md5 `4e3c5cb9`）， 手册 `Inherits 语句.md` 补三处：`MyBase` 的 `Set` 向（含"基类那个名字只有 Let"→ `VB3028`）、 `Class_Terminate` 反序链**为什么不做**（EXE 里没有触发点，实测三种形状）、属性写穿过 UDT 对象字段 从"未交付"移到"已实测通过"（D41 订正）。**顺手把 B10 的第 0 步量掉了**（记录见 **D42**）： `Implements I Via m_f` 今天停在**语法层**（`VB2003` + `VB2002`，因为 `Via` 连软关键字都没登记 —— 四个登记点零命中）， 而去掉 `Via …` 的那份手写就是 **5 条 `VB3012`**（Extends 展开后逐槽报）→ Via 的验收面 = 这 5 条消失且零转发成员。 **P3（继承线）至此完全收口。**
- 2026-09-24 11:51– **B10（`Implements <接口> Via <持有字段>` 委托式实现）提交 `3c5d8e6`**：四个落点一次接完 —— `Via` 软关键字登记、`ImplementsStmt::viaField`、**stage 2.7 新增 Pass D** 把"字段是不是本类对象字段 / 字段类型那个类实现没实现该接口"裁决成 `vias_`（判定必须在这里做：那些类的符号 3.5 才注入，只有 2.7 看得见整工程模块表），语义层据此免掉逐槽 `VB3012`、发码层据此给没写的槽转发到 **持有对象自己的接口槽** `h->vt-><slot>(h, …)`。**关键选型**：不直调 `vb6_<持有类>_<成员>` —— 接口实现按惯例是 `Private` = C 层 `static`，跨翻译单元连不到（B09 就是这条判死了 `MyBase` 的私有目标），绕表白拿"逐槽合成"。新工程 `tests/itf_via`（纯委托 / 一半自家 / Nothing 字段 / 引用计数 / `TypeOf`）**x64+x86 各 9/9**；A/B 是 D42 那条原始读数（改码前 `VB2003`+`VB2002`）；护栏扩到 10 文件（把两个接口/继承工程也纳入逐字节对照）10/10；`-Category syntax` 84→89。**顺带量出三条既有洞并登记**（见 D43 末）：接口变量的 `Property Let/Set` 写、带 `Optional` 的接口槽调用点少发 `_has_`、接口块放 `.bas` 时调用点认不出 —— 三条都是"到达槽的 caller 形状缺失"，不是委托分支错，另批处理。下一批 **B11 = P5 `CoClass…End CoClass` 语法 + 属性折算 + 契约聚合校验**。
- 2026-09-24 13:15– **B10 收线（门 = Actions run #16 全绿，本轮只推送+盯门+记账，未改代码）**：上一次推 `github/dev` 连撞 GitHub 502/504（`api.github.com` 也 502），按"门没跑就不收 IDLE"的规矩把 STATUS 留在 BUSY 并把下一步写死在 CURRENT_BATCH 里；本轮 `-c http.version=HTTP/1.1` 推成 `9eb2ca7..3c5d8e6`，watcher 收线后核 **run#16 head_sha = 3c5d8e6**、7 个 job 全 success，vbp 分片日志里 `itf_via_pair`/`itf_via_x86` 逐条 PASS（该分片 20/0/1/21）。GATE_BASELINE 换成本批，P4 的 Via 正式记成已交付，下一批 **B11 = P5 CoClass**（实施依据 `ai/026` 的 C01→C04，开工先量 `CoClass` 块的今天行为并写 D44）。
- 2026-09-24 13:24– **B11/C01（`CoClass…End CoClass` 块语法 + 属性行落 AST）提交 `e7c7a31`**：按 022 清单第 1 条先量后写（D44 四条读数：`CoClass` 不是 token 故只第一行报 VB2002、`ProgId`/`Implementation` 不在属性白名单、`[ComCreatable(True)]` 布尔实参被 `strtoll` 判死、`[Default] Interface X` 同行写法不通），再落 C01 的三个落点（词法软关键字四件套 / AST 六处登记含 printer / parser 在 `parser_interface.cpp` 复用属性行 machinery 并加 `requireOwnLine`）。实施中暴露一条 026 没写的规则：**属性行归属要按"名字+位置+同行"合判**（`--dump-ast` 抓到身份四件套挂到了第一个接口上），顺手把 `itf_n06` 的 needle 跟着改成涵盖 CoClass。验收 = `-Category syntax` 89→95（`itf_p05`/`p06` + `itf_n25`..`n28`）+ A/B（`pre_b11_C3.exe` 停在 D44 原始读数）+ 10 文件 `--emit-c` 10/10 + 带块工程真编译真运行；C01 无语义可断，故本批**不建 vbp 工程**（运行期断言从 C05 起才有承载面）。门 = 本次 push 触发的 Actions run，结论见状态头。
- 2026-09-24 14:17– **B11/C02（身份求解唯一函数 CLSID/IID/ProgID + 可复现性）提交 `f0b820d`**：先量四件事再动手（D46）—— 现成 mint 是 `.inc` 里的局部 lambda、跨不出翻译单元；legacy 那两枚 lambda 没有任何用例覆盖（全部 `tests/*.vbp` 都是 Type=Exe）所以本批一字不动；vbp 三段式与工程名的行号复核无漂移；**"发一条 note 诊断当验收面"这个设想实测不通**（Driver 只在阶段失败时整体打印诊断，note 在成功的编译里看不见）→ 改走 stderr 的 `C3:` 信息行，且不能走 stdout，因为 `--emit-c` 的 stdout 就是 C 文本。落点 = 新单元 `src/semantics/coclass_identity.{hpp,cpp}`（纯函数唯一入口；026 没写的三条口径这次定了：vbp 查表次序 = 先 `[Implementation]` 再块名、`<Proj>` 兜序 = `Name=` > 工程基名 > "VB6EXE"、seed 一律小写而 ProgID 保留原大小写）+ stage 2.7 Pass E + `Driver::coclassIds_`。验收 = `-Category syntax` 96→99（三条断言的期望 GUID 由 python 独立复算 FNV-1a 得到，不拿编译器自己的输出当基线；含"换 vbp `Name=` 只有派生档动、显式档一字不动"与"同一输入跑两次逐字节相同"）、A/B（`pre_b11c02_C3.exe` 一行身份都不出）、10 文件 `--emit-c` 10/10、`Id.vbp` 真编译真运行（`Id.exe` 打出 cc_id）。门 = push `f660e25..f0b820d` 触发的 run#19，结论见状态头（GitHub 连撞 502/504，第 4 次才推上去，且**必须用 `ls-remote` 核实**：第 3 次那句 "Everything up-to-date" 是假象，远端当时还停在 f660e25）。另有两条工具性教训：`watch-gh-actions.ps1` 在 502 上会整脚本抛错退出（`ErrorActionPreference=Stop`），不能当可靠哨兵；CJK 长串经 `python - <<EOF` 的 stdin 会被按 cp936 解码，改用 UTF-8 脚本文件。）
- 2026-09-24 15:07– **B11/C03a（CoClass 块形状与名字校验，stage 2.7 Pass F）提交 `e515d89`**：D48 量出 C03 要拒的九种形状里**八种今天一声不吭**（唯一已挡住的是 `Inherits` 一个 CoClass 名 → `VB3020`，只是理由文案说成 unknown base class，这条只剩改文案）。本批按判据族给三个号：`VB3031` 条目（未知接口 / 把类模块当接口 / 条目重复 / `[Default]` 多标）、`VB3032` 块名撞车、`VB3033` v1 边界（`[Implementation]` 不是类模块 / EXE 工程 `[ComCreatable(True)]`）。两条实施教训：**块名等于自己宿主模块名必须豁免**（第一版把正例 p07 打回了 —— VB6 最自然的写法就是 `Widget.cls` 里写 `CoClass Widget`，与 B03 接口宿主同一条理由）；**上一批的用例是这一批的雷**——`cc_id`/`p05`/`p07` 三处正例分别声明了不存在的实现类与 EXE+ComCreatable，校验一落地全红，定案是改用例为正形状（补 `CircleImpl.cls`、ComCreatable 改 False、p07 换成宿主同名惯用法）而不是放宽校验，`True` 那一面另立 `itf_n34` 负例。验收 = `-Category syntax` 99→107（n29..n34 单文件 + n35/n36 双文件 + 旧正例全绿 + cc_id 三条身份断言一字不差）、A/B（`pre_b11c03_C3.exe` 对 n29/n34 零命中）、10 文件 `--emit-c` 10/10、`Id.vbp` 真编译真运行。门 = push 后的 Actions run，结论见状态头。**下一格 = C03b**：`runCoClassContractCheck()` 放 stage 3.4c（2.7 判不准祖先实现，D48-3）+ `As <CoClass>` 默认接口视图 + `VB3020` 文案。
- 2026-09-24 15:38– **B11/C03b（CoClass 契约聚合校验 = 新阶段 3.4c）提交 `c4aaa4c`**：D50 量出可达的"祖先供给成员"只有"祖先声明成员而不写 `Implements`"这一种形状（带 `Implements` 的两种被 B08f 的 `VB3022` 挡死），所以 D48-3 那条"2.7 看不见祖先成员"要说准成立在哪一半 —— 但结论不变，比对排在链表与成员合并之后；同批量到 `As <CoClass>` 今天**静默当 Variant**，于是把 026 五-3 整条推给 C05 与派发同批做（只做"认得是类型"的半开会把静默变成说不清的错）。实施：`runCoClassContractCheck()` 按链倒着走、槽键首见者胜（派生遮蔽祖先，与 3.4 同源），成员级子句沿用 B02b 规矩，缺槽/签名不符复用 `VB3012`/`VB3017` 两个号（主语换成 CoClass 块）；**刻意不重构** `checkNewStyleInterface` 来共用它的 impl 表 —— 那牵动 `VB3019` 的子句记账，而两侧共用的 `interface_sig.hpp` 已经保证"槽"与"签名相等"只有一份定义。`VB3020` 的文案活按 D49 收掉：只有"基名是个 CoClass 块"换新句，`ci_n01`/`ci_n07` 原句不动。D49-② 那条教训第二次应验 —— 新校验把自己的三条旧正例（`p07`/`CircleImpl`/`VbpImpl`）打红，两处 `ByVal` 撞 `ByRef`、一处压根没实现，定案照旧是改正例。验收 = `-Category syntax` 107→112（n37/n38/n39 + p08/p09）、A/B 零命中（n39 出旧句，新句本批独有）、10 文件 `--emit-c` 10/10、`Id.vbp` 真编译真运行且发码逐字节未动。**下一格 = C04**（存量 attribute 只读折算），C05 连带 `As <CoClass>` 一起做。
- 2026-09-24 19:25– **B11/C04（存量 header attribute 只读折算）**：开工先同步 —— 本地落后 `github/dev` 三枚提交，ff 到 `9099e16`（Asm x64 / 静态库 / 包引用三线并入），BASE 用合并后的树重建（`pre_b11c04_C3.exe` md5 `dfb2fbfd`），syntax 基线因此 107→**112**。实测 D52（四行头属性各 143 条、`VB_Creatable=True` 占 134 且全在 EXE 工程、`Attribute Instancing` 整仓 **0** 条、门内 8 个 `.cls` 会触发折算）⇒ 裁决"折算只记信息、不开判死"，并用 `legacyFolded` 守住 VB3020 那句文案。实施 D53（Pass E0 + `CoClassDecl::legacyFoldKeys` + identity 行尾追 `folded-from-legacy:`）。验收 = 112→116、A/B 三条零命中、护栏 16/16（含 6 个会折算工程的 `--emit-c` 逐字节对照）、`ctor_ok.vbp` 真编译真运行。门 = 本次 push 触发的 Actions run，编号见状态头。
- 2026-09-24 20:37– **B11/C04 收线（门 = Actions run #31 全绿，本轮只盯门 + 记账，未改编译器代码）**：
  收线前核 **head_sha = `c3c00e0`** = 本机 `git rev-parse HEAD`（该 head = 本批 `b82a184`/`458d9bc`/`02d70fe`
  三枚 + 并入上游 Asm 线 `3c8b204` 的合并），**8 个 job 全 success**。本批过程中 `github/dev` 被他人推进两次
  （三枚到 `9099e16`、一枚到 `3c8b204`），两次都先把本地 ff/merge 上去、再以**合并后的树**重算基线：
  syntax 107→112→**116**、逐字节护栏 10→**16**、门 job 数 7→**8** —— 下一批开工别看错数；共享分支上
  "开工先同步、收线前再同步一次"从现在起是本条线的固定动作。GATE_BASELINE 换成本批，
  BASE = `.build/pre_b11c05_C3.exe`（本轮收线 exe md5 `0bc91f03`，已核与 `.build/C3.exe` 同 md5），
  状态头收 `STATUS=IDLE`、CURRENT_BATCH 交出 **C05（组内激活）**，并把 C04 新引入的连带风险
  （`coclassIds_` 现在为每个带 header attribute 的存量类模块存着一个**与模块同名**的条目，门内 8 个）
  写进 C05 的第 0 步测量点 ③。
- 2026-09-24 20:42– **B11/C05（组内激活，同批交付 022 的 B12）提交 `7f829ee`**：先按 CURRENT_BATCH 给的三点量（D54）—— ① 吞点在**两层**且互相不认识（语义 `semantic_analyzer_typeref.cpp:135` 回退 `Variant`、发码 `cgen_base_type.cpp:293` 回退 `void*`），类符号还要 3.5 才注入 ⇒ 没有单一入口，别名表这条路要 N 个消费者都记得问；② 项目类**没有引用计数**、`Set x = Nothing` 只发 `target = NULL;`、`_Destroy` 全仓两个调用点 ⇒ 本批不新增释放面、也不碰 D41；③ 折算名今天**根本不被类型路径读**（`coclassIds_` 只喂校验与 stderr）⇒ 闸门现在下：折算记录不进表、同名模块占位时类/模块赢（先例 = Fix 177b 那句遮蔽裁决）、块名等于实现类不改。④ 三条 BASE 坏读数（`void* a = 0` + 晚绑定 / `vb6_NewObject(L"块名")` / 注册表 `vb6_CreateObject`，全 rc=0 零诊断）就是 026 五-3/4/5 的立项理由；⑤ 顺带复现 D43 第三条：`.bas` 里声明的接口当类型用时 `iv.Move 5` 发成 `vb6_StructImpl_Move((&(int32_t){5}))`（接收者丢了、类挑错了）⇒ v1 不把组名做成接口视图的第二条理由。实施（D55）：新单元 `src/driver/coclass_activate.{hpp,cpp}` 做 **stage 2.7 Pass G 就地改名**（选型照泛型单态化"语义/cgen 零感知"，兑现的收益是发码侧零新分支），`CreateObject` 走 **AST 节点替换**而非 cgen 仿冒文本 —— Fix 179a 按发码文本前缀认 New，替换成节点后 `As Object` 的 IDispatch 包装自动成立。新号 `VB3039`。验收 = syntax 116→**118**、`tests/cc_act` CC1..CC9 **x64+x86 各 9/9**、A/B 三条坏读数 base 复现 new 消失、护栏 **16/16**（`cc_id` 三个块零激活行 = 闸门证据）。门 = 本次 push 触发的 Actions run，编号见状态头。

- 2026-09-24 22:05– **B11/C05 收线（门 = Actions run #37 全绿，本轮只盯门 + 记账，未改编译器代码）**：
  收线前核 **head_sha = `0dca0d0`** = 本机 `git rev-parse HEAD`（该 head = 代码 `7f829ee` + 两处文档订正
  `df3f737`/`0dca0d0`），**8 个 job 全 success**；同一批里 `df3f737` 的 run #34 也已全绿（收线门取订正后的 head）。
  vbp 分片含本批新增的 `cc_act_pair` / `cc_act_x86`（脚本末行 `if ($script:fail -gt 0) { exit 1 }` ⇒
  job 绿就等于这两条真跑真过，不是被 Test-Path 跳过）。本批共享分支无并发：开工与收线都是
  本地 = `github/dev`（开工 `cb33ab3`、收线代码 head `0dca0d0`），没有出现 C04 那种被他人抢先推的情况。
  GATE_BASELINE 换成本批，BASE = `.build/pre_b11c06_C3.exe`（本轮收线 exe md5 `d010be65`，已核与
  `.build/C3.exe` 同 md5），状态头收 `STATUS=IDLE`、CURRENT_BATCH 交出 **B13**（P6 第一格：IUnknown
  三件套真实现 + 对象布局 COM 化收尾；第 0 步 = 先造一份 ActiveX DLL 形状的能编能跑用例），并把这批
  量到的三条既有洞（`.bas` 里声明的新式接口在调用点认不出、`TypeOf x Is <工程类名>` 恒 False、
  `ReDim a(1) As <工程类>` 撞 C2224）留进 B13 的"仍开"段供后续裁决。

- 2026-09-24 23:40– **B13a（DLL 管线的观测面）提交 `7b6570a`（测试与工程）+ `988c7cb`（D56 与手册），收线前并入 Asm 线后门 head = 合并提交 `9785f4f`**：开工第一件事就是把 CURRENT_BATCH 的前提送去实测，两枚都炸（D56）—— ① `tests/` 下**不是**全是 `Type=Exe`：`tests\test_activex_dll\` 躺着两份 `Type=DLL` 工程，真编译 25 秒产出 266 KB 的 `.dll` + `TestAXDLL.tlb` + `activex_dll.def`（五个导出函数齐全），它们的错处只是**从没登记进 `run_tests.ps1`** ⇒ 整条对外管线在门内一次都没跑过；② 「IUnknown 三件套要从零写」也是错的：`vb6comserver_obj.c:24-99` 的 QI/AddRef/Release 是真实现（原子计数、归零调 `destroyFunc` ⇒ `Class_Terminate` 在 DLL 侧**有**触发点，D41 那条只成立于 EXE）。于是本批改交付「让这一半第一次可被断言」：新助手 `Test-VbpDll`（DLL 没有 stdout ⇒ 断产物存在 + `dll_entry.c`/`activex_dll.def` 内容 + stderr 身份行；`--emit-c` **不含** `dll_entry.c`，唯一读数通道是 `--keep-for-debug` 打出的临时目录）+ 三条用例（两份现成工程 + 新写的 `tests/cc_dll/CoDll.vbp`）。D56 量出对外那半真正缺的四条、并且能指名道姓：**块名 ProgID 在产物里 0 次**、同一份编译里 `IProbe` 拿到**两枚 IID**（语义层 `{F5CEF988-…}` vs 产物 `{0AD9CBC7-…}`，因为 `coclassIds_` 后端消费者为 0）、只实现新式接口的类 `methodCount=0` 对外点不到任何方法、`comCreatable`/`legacyFolded` 对产品零影响 ⇒ B13 重切成 a/b/c 三格。本批自身零编译器代码改动；收线前并入 Asm 线 8 枚（合并提交 `9785f4f`）⇒ BASE 与基线数字一律按**合并后的树**重算（收线 exe md5 见状态头，`-Category syntax` 与逐字节护栏都在那棵树上复量）。另记一条 harness 坑：本仓 `core.autocrlf=true` ⇒ `tests/run_tests.ps1` 的**工作树** checkout 后是 CRLF（仓库里的 blob 仍是 LF+BOM），所以「插入后断言 count(CRLF)==0」这类自检要从「文件字节」改成「committed blob」（`git show HEAD:<path>`），否则合并完第一次 splice 就会被自己的断言挡下。本批真正咬人的是另一条老坑复发：注册文本经 bash heredoc 落盘时 `"$Tests\test_activex_dll"` 里的 `\t` 被吃掉成 TAB，症状是编译器报「`.vbp文件中没有源文件: D:\c3.vb6.pro\tests est_activex_dll …`」，而 `Test-VbpDll` 把它报成 `FAIL (compile)` —— **看起来像 x86 工具链问题或并发争用**（我第一反应就是后者，还「复现失败」了一次才去查字节）。⇒ 带反斜杠的 PowerShell 文本一律走 Write 工具或 `chr(92)`；新助手第一次红，先 `hex()` 看路径字节，再怀疑工具链。门 = 本次 push 触发的 Actions run，编号见状态头。

- 2026-09-25 00:58– **B13b（`dll_entry` 改读唯一出口）提交 `b1a8e58`**：接线照 `ivreg_`/`ivviareg_`/`clsreg_` 的先例加第四个只读 setter，注入点选在 `dllCgen`（`driver_codegen_dll_typelib.inc`）而**不是**模块循环 —— 这张表只有 `generateDllEntry` 消费。三条裁决：① **只接手写块**，`legacyFolded` 一律跳过（折算的 clsid 在没 vbp 三段式时是 C02 mint，种子与 legacy 不同 ⇒ 覆盖 = 换掉存量 DLL 的注册身份 = 破护栏；C04 的隔离原则第二次生效，顺带给 `legacyFolded` 第一个产品读数）；② **`[ComCreatable(True)]` 是组名档 ProgID 的唯一开关** —— 写它表里多一行（同 CLSID 同工厂，count 1→2），`<工程>.<类模块名>` 那档保留不动，存量 DLL 客户照旧 `CreateObject`；③ 块内 `[Default]` 接口的 IID 取出口值 ⇒ `IID_vb6iface_<I>` 与 `IID_vb6def_<C>` 第一次对同一个接口同值（实测两枚都成 `F5CEF988`，legacy 那枚 `0AD9CBC7` 从产物消失）。**验收证据 = 用例翻面**：`cc_dll_identity_two_channels`（B13a 钉的就是那枚分叉）改名 `cc_dll_identity_single_source`，needle/absent 整个反过来 —— 这条按 D56-7 预设的方式兑现。**没做完的两件事点名留给 B13c**（不是漏的）：非默认接口仍走 legacy derive（它要和 `ivDeriveIid` 一起收，那是第三通道），以及本批新量到的**类型库侧第四枚** `TypeLibBuilder::generateUuid`（它回写的值 dll_entry 在读，表与 `.tlb` 是否同值今天仍未证，D57-5）。**EXE 侧刻意不接**（`comEntryCgen` 不调 setter），并实测到位：`cc_act`（EXE、带两个手写块）BASE vs NEW 全产物 12 文件零 DIFF。护栏 16/16、syntax 118/0、DLL 用例 4/4（含 x86）。本轮另记两条 harness 坑（D57-8）：`src/` 是 CRLF 而 `ai/022`/`run_tests.ps1` 的 blob 是 LF（工作树因 autocrlf 又是 CRLF）⇒ 行尾断言必须按文件实测；Python 里 `("A" + nl` 换行接字符串是语法错，症状却报 「Is this intended to be part of the string?」，别去查引号。门 = 本次 push 触发的 Actions run，编号见状态头。

- 2026-09-25 01:24– **B13c（三通道 IID 合并 + 新式接口对外口径）代码提交 `bd38798`**：开工第一件事照例是把 CURRENT_BATCH 的前提送去实测，这一轮的读数是**类型库从来没有和服务器对过话** —— D57-5 说"未证"，实测答案是"**证伪**"：BASE（`473784ed…`）编 `tests\cc_dll`，同一个 `IProbe` 拿到**四枚** GUID（出口/表 `{F5CEF988-…}`、vtable QI `{74B1BA3B-…}`、`.tlb` 的 `_IProbe` `{1E34D82B-…}`、`.tlb` 的 coclass `IProbe` `{28B6B94B-…}`），而 `.tlb` 里 coclass `CImpl` 的 DEFAULT 引用是 `_CImpl = {7CA8CD81-…}`，服务器手里那枚是 `F5CEF988` ⇒ **早绑定客户端照类型库去 QI 必然 `E_NOINTERFACE`**。为了把这个"没工具"的死角变成可断言的东西，新写 `tests\tools\tlbprobe.cpp`（`LoadTypeLib`+`GetTypeAttr`+`GetRefTypeOfImplType`，151 行）与助手 `Test-TlbIdentitySingleSource`（`tests\tlb_identity.ps1`，`run_tests.ps1` 只多一行 dot-source）。合并的做法仍是不开第二套 mint：**把接口自己的 IID 也搬进唯一出口**（`resolveIfaceIid` 与块内 `[Default]` 那条共用一个函数；Pass E 建 `Driver::ifaceIds_`），三个消费者一律读它，`ivDeriveIid`/`generateIid`/`generateUuid` 降级成"表里没有（= 不是新式接口）才用"的兜底。类型库另外补了一条**结构**修正：coclass 的默认接口以前写死 `<_类名>`，现在块的 `[Default]` 指新式接口时跟随它；踩到 `unordered_map` 遍历序导致的 `TYPE_E_ELEMENTNOTFOUND`，解法刻意收窄成**只延后需要重定向的那几行** —— 第一版无差别延后所有 coclass 时存量 `TestAXDLL.tlb` 变了 3976 字节（内容同、类型序变），是实测把它否决掉的。第 2 条口径**拍板走 (b)**：契约成员（`Private`）不发成 disp id（那是换语义，而且服务器的 `GetIDsOfNames` 只认公有成员，(a) 只会两边假绿），改成编译时打一条 `C3: CoClass … is a vtable interface: 0 Public member exported for IDispatch clients`；真接口那条正路（`TKIND_INTERFACE` + 成员进库）连同实测到的新洞（`_IProbe` 的 `cFuncs` 恒 0 —— 接口模块成员从来没进类型库）一起归 B15。护栏两层都升级：16 件 `--emit-c` 改成**分类**护栏（`.build/b13c_guard.py`，只许 `vb6_iv_iid_*` 行变）→ 16/16；全产物 A/B（`.build/b13c_ab_all.py`）三工程一次量 → `cc_act` 只有 5 个 `.h` 的 iv 行变且 `com_entry.c` 全同（D57-6 的 EXE 不接仍然成立）、存量 `test_activex_dll` 含 `.tlb` **全同**、`cc_dll` 允许 `.tlb` 变 = 本批靶子，零未归类变化。门内读数：`-Category syntax` 118/0、`-Category vbp` 27/0/1（`test_vbman` SKIP 是本机没注册 VBMAN）、DLL 侧 5 条用例全绿（含 x86 与新用例）。另记两条 harness 坑（D58-8）：`scripts\env.ps1` 是 ANSI 且首行带 shebang，pwsh dot-source 会读成乱码 ⇒ 自测脚本要 MSVC 环境就自己 `cmd /c "call vcvarsall.bat x64 && set"`；heredoc 塞 Windows 路径第 N 次咬人（`\2019` 被吃掉成"系统找不到指定的路径"）⇒ 反斜杠文本一律走 Write 工具。门 = 本次 push 触发的 Actions run，编号见状态头。

- 2026-09-25 02:33– **B13d（规范 IUnknown）代码提交 `9df23ba`**：单文件改动（`cgen_iface_vtbl.cpp` 的 QI 发射器）。读数先行的老规矩又兑现一次：BASE 的 `itf_xmod\XWriter.vbp`（`CWriter` 同时实现 `IWriter`+`ILog` = 最小两接口形状）实测两处 QI 都是 `if (IidEqual(riid, IID_IUnknown) || IidEqual(riid, IID_<自己>)) *ppv = self` —— 而 `__iv_IWriter` 与 `__iv_ILog` 是同一结构体两个不同偏移的成员，所以"同一对象问 IUnknown"按入口接口不同给出两个地址（D22-7④ 从记录变成可复现的读数）。收法选**"本类实现序第一个接口的薄指针"当规范指针**：① 它是唯一一处偏移 0 就是 vtable 的合法 COM 对象指针（`vb6_cls_<C>` 偏移 0 是裸回指 `__comObj`，把 `me` 交出去等于让客户对着指针取 vtable ⇒ 直接踩飞）；② 不加字段 ⇒ D19 那条布局约束根本不进入视野，存量工程（没有 `__iv_`）形状不动；③ "实现序第一个"与"结构体最靠前的 `__iv_`"天然重合（同一份 `ivImplementedIfaces` 喂两边），不需要新序规则。单接口类 `canon == self`，值一字未变。 **本批真正的新料是顺手读出的一条危险**（D59-4）：`ComObj_QueryInterface` 对 `desc->defaultIfaceIid`/`ifaceIids[i]` 一律 `*ppv = self`（注释还写着 "dispinterface: same IDispatch pointer" —— legacy 世界里是对的），可这两个字段自 B13b/B13c 起装的**可能是新式 vtable 接口**的 IID，而 `.tlb` 自 B13c 起把同一个 IID 当 coclass 默认接口广告 ⇒ 早绑定客户 QI 成功、按接口第 3 槽调用打到 IDispatch 的 `GetTypeInfoCount` = **静默调错函数**，比 `E_NOINTERFACE` 危险。两种收法（对外不发布新式 IID / 让包装器真返回 `&me->__iv_<I>` = 实质是 B16）留 **B13e** 拍板，本批刻意不动 —— 动它要把上一批刚立的 `cc_dll_identity_single_source` 与 `cc_dll_tlb_matches_table` 两条用例翻面，同一批里既改身份口径又改对外发布口径，门就不干净了。 断言只能断形状：全链路**没有任何自生成调用点**会去问 `IID_IUnknown`（VB 没有 QI 面，接口值进 Variant/实参是未开的 B06c），端到端身份属 B17；于是新用例 `itf_canonical_iunknown` 走 B08e-6 那条 `--emit-c` 通道钉"两处 QI 各有一处规范指针分支 + 旧的 `IID_IUnknown) ||` 合并分支已消失"，负控在 BASE 上跑过（`canon` 0 处、合并分支 2 处 ⇒ 用例能红），行为那半靠真编译真跑的 `itf_xmod_writer`（QI1..QI4 + LIFE + TOF）兜。 护栏：① `-Category syntax` **119/0**（118 + 新用例）；② `.build/b13d_guard.py` 把豁免粒度从"某类行"升到"**整段函数体**"——14 个不含新式接口的工程 `--emit-c` 逐字节全同，`itf_xmod` 摘掉所有 `vb6_iunk_*_QueryInterface` 后剩余行序列全同（QI 体 36→46 行），`cc_id` 一个 QI 体都没有；③ `.build/b13d_ab_all.py` 三工程全产物 A/B：`cc_act`（EXE、带两个手写块）**零文件变化**、存量 `test_activex_dll` 只 `.rc` 那行绝对临时路径天然不同、`cc_dll` 只 `CImpl.c` 的 QI 体变（**`.tlb` 与 `.def` 一字未动**）= 零未归类变化；④ `-Category vbp` **27 PASS / 0 FAIL / 1 SKIP**（SKIP = `test_vbman`，已知基线）。手册两条：`Interface 语句.md` 补规范 IUnknown 已交付 + 跨包装器/薄指针两个世界的身份还没接成一个；`CoClass 语句.md` 补一条用户能踩的已知边界（拍板前别把"默认接口是新式接口"的 CoClass 暴露给早绑定客户）。harness 坑一条："`改动确实进了产物`"这类自检要按**全局**行数判 —— 先按单工程判时被 `cc_id`（声明接口但无人实现 ⇒ QI 体 0 处）误红了一次。 门 = 本次 push 触发的 Actions run，门 = run **#50**（8 job 全绿、head 已核 `2c7c5e2`）。

- 2026-09-25 03:23– **B13e（对外默认接口的口径：广告 == 应答）代码提交 `b10ec1a`**：本格的前提是上一批自己写的，所以第一枪照例打自己 —— D59-4 那条"胖包装器应答瘦 IID ⇒ 早绑定客户按接口虚表调第 3 槽打到 `GetTypeInfoCount`、静默调错函数"**实测不成立**：`TypeLibBuilder` 只会发 `TKIND_DISPATCH`/`TKIND_COCLASS`（本轮 `tlbprobe` 读数：`CoDll.tlb` 四个类型，`TKIND_INTERFACE` 0 个，`_IProbe` 的 kind 是 dispinterface），客户从库里学不到接口虚表布局；服务器侧 `GetIDsOfNames`/`Invoke` 也只查 `desc->methods`。方向对、机制错，真问题在旁边那条：B13c 把 `.tlb` 里 coclass 的 DEFAULT 引用重定向到块 `[Default]` 那个 `_IProbe`，而 `_IProbe` 在库里成员数是 **0**（D58-6），服务器认账的成员面是类的公有成员 = `_CImpl` 那一档 ⇒ 广告与应答是两份东西，**B13c 那次"让两条通道同值"把自己立的判据做成了空壳**（值一致、可用性没了）。拍板走 D59-4 的 (i) 收窄版：对外默认视图一律 `<_类名>`，`[Default]` 只管语言层，三处一起退回不留中间态 —— `collect.inc` 不再拿出口 `iid` 覆写 `info.defaultIfaceIid`（改由类型库回写值说话）、`tables.inc` 删掉 B13b 那条"`ifaceIids` 里 `[Default]` 同名接口改用出口 IID"的分支（`CoClassInfo::defaultIfaceName` 字段随之删）、`driver_codegen_dll_typelib.inc` 拆掉 DEFAULT 重定向连同为它搭的延后登记机器（`TlbCoClassRow`/`tlbTypes`/`registerCoClass` 与那条 `…is a vtable interface: 0 Public member…` 信息行）。**保留** `identityForClass`（coclass 的 CLSID 仍取出口）与 `ifaceIidFromMap`（接口模块那条 dispinterface 的 GUID 仍读 Pass E 的表）⇒ 甲判据"一个接口一次编译一枚 GUID"一个字没松。没走 (i) 全量（把 `ifaceIids[]` 里新式那几项滤掉）的理由写进 D60-4：那会把表与 vtable 劈成两枚 GUID、当场自相矛盾，而"胖应答瘦"的正解是 B16 让包装器真返回 `&me->__iv_<I>`。用例两条：`cc_dll_identity_single_source` 翻面（`IID_vb6def_CImpl` 回 `0x7CA8CD81` 并新增为 needle、`IID_vb6iface_IProbe` 仍 `0xF5CEF988`、撤掉那条日志断言），`cc_dll_tlb_matches_table` 从一条判据升成两条（乙 = 表的 `IID_vb6def_<C>` == 库的 DEFAULT 引用 == 库的 `_<C>`），负控跑在 BASE `905c9392…` 上 ⇒ 乙 判红（`{F5CEF988}` vs `{7CA8CD81}`）、喂新 exe 全绿。护栏：① `-Category syntax` **119/0**；② `.build/b13e_ab_all.py` 全产物 A/B 四工程（比 B13d 多带 `test_event_dll`，因为 `addCoClass` 从 lambda 搬回原地、事件源那条参数表达式重写了）：`cc_act`(EXE) **0 个文件变化**、两枚存量 DLL 连 `.tlb` 都逐字节不变（只 `.rc` 那行绝对临时路径天然不同）、`cc_dll` 只 `dll_entry.c` 的 `IID_vb6def_*` **1 行** + `.tlb` = 零未归类变化，且按全局 `def_lines>=1` 自证"回退确实进了产物"；③ `-Category vbp` **27 PASS / 0 FAIL / 1 SKIP**（SKIP 还是 `test_vbman`），收线 exe md5 `1c09fa371ad0882d2e1229fc4ebc809c`（跑测试前后一字不差）。手册一条订正：`CoClass 语句.md` 里 B13c 写的"类型库的 coclass 默认接口现在跟随块"与那条"已知边界"（第 3 槽打到 `GetTypeInfoCount`）**都是错的**，改成"对外广告的那个视图 == 服务器应答的那个视图" + 把证伪过程留在 D60-1。harness 坑一条：中文消息里紧跟变量名写 `"…_$Var: …"` 会被 PowerShell 当成作用限定符 ⇒ 整个 `tests\tlb_identity.ps1` ParserError，一律写 `${Var}`。门 = 本次 push 触发的 Actions run，编号见状态头。


- 2026-09-25 04:10– **B14（IDispatch 那一半：第一次被真客户端走通）代码提交 `ddf4e9b`（纯测试批，零编译器改动）**：照例先送测量，三件里有两件把自己的排期打了。① **四件套本身是通的**：新写 `tests\tools\disp_probe.c`（不查注册表的进程内客户端：`LoadLibrary` + `DllGetClassObject` + `CreateInstance(IID_IDispatch)`，然后把 `GetTypeInfoCount`/`GetTypeInfo`/`GetIDsOfNames`/`Invoke` 各问一遍），对真编译出来的 `TestAXDLL.dll` 实测 `Add(2,40)=42`、`SetValue(7)` → `GetValue()=7`（跨两次 Invoke 保住状态 ⇒ 同一实例、包装器复用那条路也对）、未知名 `DISP_E_UNKNOWNNAME`、`GetTypeInfo(0)` 回的是 **coclass** 那份（`kind=5`、`cFuncs`=0 是 coclass 的常态，不是缺陷）、`GetTypeInfo(1)` = `DISP_E_BADINDEX`。所以"IDispatch 四件套没接新式接口"这句话里，**缺的不是四件套，是成员面**。② 薄指针 `vb6_ivtbl_<I>` = `{QI, AddRef, Release, 自有槽…}`，**四件套那四槽不在里面** ⇒ 要给薄指针一张 IDispatch 面就得插四槽、接口自有成员槽号全体后移，这是 dual/真接口的布局活，而且**必须先有 B15 把接口成员发进库**，否则就是重演 D60（库里 0 成员、服务器多应答一枚 IID，门内全绿外面点不通）。③ 全工程只有一处生产者写 `comDispid`（`driver_codegen_dll_typelib.inc:186`，在"类模块 Public 成员"那条收集循环里），**接口契约成员从来没进过这张表** ⇒ 要么违口径 (b) 把契约成员当公有发（`GetIDsOfNames` 只认公有成员，那是两边假绿），要么走真接口 = B15。 **范围重裁（D61-5 拍板）**：B14 收缩成"把这条管线从字节级升到真跑级"这一格；"薄指针的 IDispatch 面 / 接口成员进库 / dual 布局"合并为 **B15 的前置**，顺序定死 B15 → B16（先让库里那一档有成员面，再让 QI 多交一枚指针）。交付 = 探针 + 助手 `Test-DispatchInvoke`（`tests\disp_invoke.ps1`，与 B13c 的 tlb 探针同一形态：现编现用、`.obj` 落产物目录）+ 两条 gated 用例：`ax_dll_dispatch_invoke`（存量 legacy 面真点通）与 `cc_dll_dispatch_iface_only`（只满足新式接口的类：成员面**必须**为空 ⇒ `NAMES=ADD hr=0x80020006`；同时 `QI_EXTRA ... same=yes` 把"今天这枚接口 IID 由胖指针应答"钉成实测事实，B16 要改就得故意翻它）。新助手先证明能红：喂一个不存在的 CLSID ⇒ `GETFACTORY hr=0x80040111` → 判红。零编译器改动 ⇒ 产物逐字节不变（开工/收线 exe 同一枚 md5 `1c09fa37…`），这条本身就是护栏。 **读码顺手登记的一条弱点（D61-6，本批没动）**：`ComObj_Invoke` 先按 dispid + invkind 精配、**配不上就退回"第一个同 dispid 的表项"** —— 而 VB6 里 Property Get/Let/Set 共享同一 dispid 是常态，所以一个只发成 Get 的属性用 `DISPATCH_PROPERTYPUT` 问也会把 Get 调出去；收紧会改到存量工程的可观察行为 ⇒ 属口径题，等拍板。harness 坑一条：拿 `'` 当 PowerShell 的行连接符（真身是反引号）会让整个 `run_tests.ps1` 变 ParserError，而 `-File` 那条包装只要末尾有 `echo` 就仍回 exit 0 ⇒ 本地读数要看日志内容不能只看退出码；本轮 `-Category vbp` 第一次就是这么"跑绿"的。 门 = 本次 push 触发的 Actions run，编号见状态头。


- 2026-09-25 04:51–07:00 **B15 一格出完并过门（Actions run #59，head `0caa3c5`）**：开工先做四件测量（读数与两条订正落 **D62**），按 D62-5 把范围收窄成"类型库的形状与身份那一半"后动工 —— `TypeLibBuilder::addVtableInterface`（`TKIND_INTERFACE` + Pass E 那枚 IID + 接口自己的名字，不收 `FCANCREATE`）+ `driver_codegen_dll_typelib.inc` 那条循环加接口宿主分叉，**撤掉** `coclass IProbe`（连带那枚谁也不认的 `generateUuid(类名)` mint 与"可创建"这句假广告）和 0 成员的 `_IProbe`。库里 types 4→3。判据换读法不放宽：`Test-TlbIdentitySingleSource` 通道 3 改读真接口行 + 两条反面断言，负控（`.build/b15_negctl.ps1`）实测旧库 RED 三条全中、新库 GREEN；`--emit-c` 16 件全同、全产物 A/B 只 `cc_dll` 的 `.tlb` 变。成员面（`cFuncs` 仍 0）连同 `CreateTypeLib2` 位数 flag 排 B16，理由都在 D62-1/D62-3 里有读数。代码提交 `941b6dc`；**门 head 是合并后的 `0caa3c5`**（用户在 `github/dev` 上的 Fix 192/193 —— `a4e8eb3` 元素类型为项目类的数组、`7e9b6bc` `TypeOf … Is <项目类>` —— merge 进来无冲突，两条已从本表"仍开"清单撤下并记在 CURRENT_BATCH 末）。收线 exe（merged tree）md5 `a7e41e5d…` 已存成 `.build/pre_b16_C3.exe`。 **本轮四条流程教训（都是自己的锅，记下来给后续批次）**：① **`Test-Path exe` 不是构建成功的判据** —— 一次 `cl` 失败但旧实验 exe 还在原地，脚本照样跑完，量出来的"③ flag 无后果"是**上一版二进制的读数**（重测后结论翻转，见 D62-3）；改成先删旧产物、再看 cl 输出里有没有 `error C`。② **PS 5.1 按 ANSI 读无 BOM 的 `.ps1` 时，行尾的中文字节会把换行吃掉** —— `.build` 里一个临时脚本的 `param()` 因此静默失效（参数全空、脚本空跑），临时脚本一律 ASCII only。③ **bash 传给 `powershell -File` 的 Windows 路径里反斜杠会被 MSYS 吃掉** —— `-OutputDirectory D:\c3…` 变成仓库根下一个 `c3.vb6.pro.build…` 目录；要么写 `"D:\\…"` 要么用正斜杠，且**别把带 `-File` 的调用当可以直接信 exit code**（同 B14 那条）。④ **本地全量/分类门这一轮两次作废**：第一次是自己 `rm -rf` 了正在被自己那次跑使用的产物目录（把 M7Test 与三条 DLL 用例弄成假红），第二次是默认 `output/` 与另两位写者的并发套件互踩（22 条假红）⇒ 本地只记快检（emit-c 护栏 / A/B / 负控 / 单工程 tlbprobe 读数），门数一律以 Actions 为准，这正是"门跑 Actions"的理由。

- 2026-09-25 07:20–08:34 **B16 一格出完并过门（Actions run #62，head `578faa4`）**：开工先做三件测量（读数与裁决落 **D63**）—— ① **x86 调用约定今天不成立**：`--arch x86 --emit-c` 的产物里一个约定修饰都没有（`grep -c __stdcall` **BASE 0 / NEW 21**，`cc_dll`），而 COM 规范是 `CC_STDCALL` ⇒ 薄面整条 canonical 化；② **canonical 化的波及面**靠分类护栏逐条逼出来（`__stdcall` 只出现在声明/实现新式接口的工程、薄面那一对 `vb6_iv_thin_/vb6_iv_claim_` 只出现在**有接口宿主**的工程、服务器表每个 coclass 行多 `ifaceThinPtr`/`instanceClaimRelease` 两个字段 = 新式宿主填函数名、存量填 `NULL`），`cc_id` 只声明接口没有宿主 ⇒ 只有 `__stdcall`（护栏因此把 `EXPECT_FIRING` 与 `EXPECT_THIN_HOSTS` 分开写死）；③ **位数 flag 拍板跟 `--arch` 走**并把代价量到字节：默认架构存量库**逐字节不变**（md5 同 `87d34673`、diffbytes=0），`--arch x86` 只差 **32 个字节**（@20 头部位数声明字 `41`/`43` + 31 处布局字），且两架构的差集与 BASE→NEW 的差集**同址**、x64/x86 两枚读端读回的值逐项相同。实施**零布局改动**（D19 不入视野）：`cgen_iface_vtbl.cpp`（`__stdcall` + 薄面出入口）、`cgen_util_dllentry_collect/prelude/tables.inc`（表行两个新字段）、`vb6comserver.{h,c}` + `vb6rtl_class_com.h`（接口 IID 的 QI 交薄指针、包装器把底座引用交还最后一个薄引用）、`typelib_builder.{hpp,cpp}`（如实发契约 + flag 跟位数）、`driver_codegen_dll_typelib.inc`（喂 `IfaceView::slots`，= D62-2 认定的唯一事实面）。判据五条全上读数：新探针 `tests\tools\tlb_slots.cpp` 升格入库 + 助手 `tests\tlb_contract.ps1` 两条契约用例（x64 `oVft=24/32`、x86 `oVft=12/16`，**x86 那条必须用 x86 读端**）+ `tests\tools\disp_probe.c` 长一条按库里形状**直调 vtable 槽**的支路（`VTBL_GET_BEFORE=0` → 槽 3 写 21 → `VTBL_GET_AFTER=42`，x64 与 x86 各一条真跑用例）+ `QI_EXTRA same=yes→no` 的**故意翻面**（接口 IID 现在交薄指针）。护栏：`--emit-c` 17 件 `violations=0`、全产物 A/B `unclassified=0`（只有 `cc_dll` 的 `.tlb` 1484→1624 的预期变化）、负控在 `pre_b16_C3.exe` 上 `pass=0 fail=2`（四条 needle 全 miss + 反面断言 `cFuncs=0` 命中）。**三条流程教训**（写进 D63-8）：套件在 CI 上用 pwsh 7（`Join-Path` 4 参形式在 PS 5.1 下静默给空串 ⇒ 本地复现也得用 pwsh）、套件 `$env:LIB` 是 x64 在前（x86 助手链接要临时换 `$msvc.LibX86`，否则 LNK4272 + 10 个未解析外部）、探针直调薄指针的槽要**先解一层**（`*(probe_ivtbl**)p`；`((probe_ivtbl*)p)->Got(p)` 是拿指针当槽表，实测读到堆外当场崩）——顺带：崩溃型探针必须 `setvbuf(stdout, NULL, _IONBF, 0)`，否则块缓冲把已打印的读数全吞掉。行里第 4 项（DllRegisterServer 一族接线）经读码 + A/B 判定**无需新改动**（注册写入读同一张服务器表、身份自 B13b 起走唯一出口；`.def`/exports/`.rc` 一字未动），真注册的外部端到端验收归 **B17**。收线 exe（= 门 head 的树）md5 `90129adbb5dbb3c5fca14fcc872dc0f2`，已存成 `.build/pre_b17_C3.exe` 作下一批 BASE。