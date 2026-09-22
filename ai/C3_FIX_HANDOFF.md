# C3 编译器错误修复 — 任务交接文档

> 用途：新会话恢复上下文用。新开会话后直接说「读取 C3_FIX_HANDOFF.md 并继续修复」。
> 更新日期：2026-09-22
>
> **本文档按 owner 要求只保留「未完成项」与「仍在生效的口径/工具事实」**，已完成修复的过程
> 叙述已删除（3310 行 → 约 200 行）。回溯通道有两条，都不必靠本文档：
> - 删除前的完整版：`git show 1465da1:ai/C3_FIX_HANDOFF.md`（含原 §1–§44 全部叙述与取证）
> - 每个修复在**代码落点处自带注释**：`grep -rn "Fix <N>" src tests`（注释里写的是根因与判据，
>   比本文档的转述更不易过期）。已提交项另见 `git log --oneline`。
>
> §D 是一行式索引，标明每个已完成项原来的 §区间，便于上面那条 `git show` 定点取阅。
>
> **下文里的行号是删除时的位置，重构后可能已漂移**（`src/backend/` 下大量逻辑已拆进
> `detail/{expr,stmt,util,base,module}/*.inc`）；文件路径是本轮复核过的真实路径，定位一律用
> `grep -rn <符号或字面量> src`，不要把行号当坐标。

## A. 当前状态与验收基线

- 长期目标：让 C3 编译**并运行** `D:\vb_yqt4qPac\VBFLXGRD-master\Standard EXE Version\VBFlexGridDemo.vbp`，
  画面与用户给的参考截图一致。
- 现状（2026-09-22 15:00）：demo 编译链接全绿、进消息循环、画面各验收点一致，**鼠标悬停与左键
  点击均不再崩**（用户实测）。全量门禁 **`PASS=93 FAIL=0 SKIP=1 TOTAL=94`**（`regress50`；SKIP =
  `test_vbman`，`VBMANLIB.cVBMAN` 未在 WOW6432Node 注册，属环境）。
- 验收口径（重要，别反向"修好"）：**"与参考图一致" ≠ "完美"**。参考图里 `Partial Scr…` 本身就是
  截断的，不要为了消除截断去改控件尺寸（原 §21/§23/§25）。
- 门禁号与 Fix 号是**两条独立序列**；编号若与代码注释冲突，以代码注释为准并在此说明（原 §28
  有整张重排表）。任务 #33 原挂 "Fix 191"，已让号为 **Fix 192**。
- 分支：工作在 `fan/dev`；`main` 受保护，只能走 MR。

## B. 未完成项（每条都可直接开工；出处 = 删除前的 §号）

### B1 Fix 185 的**边界**：控件级 `_Paint` 只接了 PictureBox（Label / Frame / Image 仍无派发）
Fix 185 已落地（见 D 表），当时按"爆炸半径一个点"的口径把 WM_PAINT 派发与 `Print/Cls` 两条
都门在 `FrmControlType::PictureBox` 上 —— 依据是全工程度量 `<控件>_Paint` 只有 `MainForm.frm:658`
一处（`UserControl_Paint` **不在此列**：它走 Fix 112 的宿主路径 `vb6_<ctl>_ucHostPaint`，
`src/backend/module/cgen_form.cpp:189/241`，本来就通）。所以 **Label / Frame / Image 的
`<控件>_Paint` 现在仍然是死代码**（成员侧不认 `print`/`cls`，调用侧照旧落
`vb6_ComCall(HWND, …)` 运行期 no-op）。要扩面注意两点：
① 先重量真实工程里 `<控件>_Paint` 的出现面，别照抄 PictureBox 分支；
② `Image` 的 WM_PAINT 已被 `vb6_InstallImageSubclass`（`vb6forms_picture_prop.c:305-312`）占走，
而它和代码生成的 `vb6_InstallControlSubclass` **共用同一个 `VB6_OrigProc` 属性名** ⇒ 后装的一方
静默 no-op（见 B13）；扩到 Image 必须先解掉那条，否则"加了派发"和"没加"表现完全一样。

另记一条**覆盖面**事实（本轮度量）：`tests/` 里没有任何用例写 `<控件>.Print`/`.Cls`
（`grep` 只命中 `Debug.Print`），所以门禁**不经过**这条新路径 —— 它既不构成回归风险，
也意味着 Fix 185 没有自动化断言，只能靠 VBFlexGridDemo 的 GUI 用例保证"编得过、跑得活"。

### B2 Fix 180 —— `ReDim` UDT 成员数组不带 `As` 时按 Variant 分配载体（步长不符）
出处 §22 L1539–1545、§30（根因已收窄到一行判定）。现象：`ReDim h1.Items(0 To 1)`（不带 `As`）发成
`vb6_SafeArrayReDim1D(vb6_sa_variant,0,1)` → 16 字节步长，而读写走 `VB6_SA_AT(vb6_type_TOwned,…)` → 越界。
**落点**（已复核仍在）：`src/backend/stmt/cgen_redim.cpp:12-18`（只有 `node.targetExpr` 才走复杂通道）
+ `:74-79` 的 `arrayUdtElemTypes_.find(lowerVar)` —— 键是**裸变量名**，`h1.items` 永远捡不到 →
`isUdtArray=false` → 发 Variant 版；多维分支 `:282` 共用同一个 `udtCType`。
**修法**（约 6–8 行）：按最后一个 `.` 切 owner/member → 用 `knownUdtVars_` 取 ownerCType
（登记处 `src/backend/decl/cgen_decl_var.cpp:180`）→ 调已有的
`udtFieldObjCType(ownerCType, member)`（`src/backend/cgen_util_classtype.cpp:338`）；建议抽成
`resolveUdtMemberArrayCType()`，与 `resolveReDimComplexElemType`(:200-231) 共用。**owner 解析不到时
保持回落 Variant，不要猜。** 边界：类字段 `me->Foo.Items` 与 With 里的 `.Items` 需要
`classMemberVars_`/`withObjectInfoStack_`，本轮不扩。
**验证判据**：生成行变成 `vb6_SafeArrayReDim1D_Udt((int32_t)sizeof(vb6_type_TOwned),0,3)`。
（`--dump-symbols` 不打印 UDT 成员字段，证不了 `typeRefName`，见 C 组第 5 条。）

### B3 Fix 192 —— `ByVal <x> As Currency` 传 Win32 `POINT` 的 x64 编组 + DI 桩按声明形状分流
出处 §43 第 2 条、§40 前的勘察。x64 下**只有打包成单个 64 位整数**（x 低 32、y 高 32）才对：
`.temp/abi_probe.c` 以真原型为基准、在可见窗口上实测 —— 我们的 `(double)` 桩与上游的
`(intptr_t,intptr_t)` 桩**都返回错误的 HWND**，`(int64_t)` 返回正确值。受影响面是
`WindowFromPoint`/`ChildWindowFromPoint` 的 hover 路径（VBFlexGrid + 整个 ComCtlsDemo 家族），
两条线上一直搞错。
**难点不是选边**：DI 桩只按 **API 名**索引，同一 API 的两种真实 VB6 声明形状无法共存（工作树已自
相矛盾：`WindowFromPoint` 用上游 2 参、`ChildWindowFromPoint` 用我们 1 参，而 VBFlexGrid 里两者都是
`As Currency`）。要修必须让**桩名按声明形状区分**（codegen + `scripts/gen_di_stubs.ps1` 同改）。
**但先量冲突面再设计**：`.temp/di_shape_survey.py` 走完 `D:\vb_yqt4qPac` 全部 `.frm/.bas/.cls/.ctl/.pag`
的结果是 **1116 个 Declare 组里只有 1 个 API 的元数不同**（`user32!WindowFromPoint`）；~351 条"同元数
不同 As 类"是 `PtrSafe`/非 `PtrSafe` 双声明惯用法（`As LongPtr` vs `As Long`），桩取 `intptr_t` 无害。
⇒ **不要为单一样本给 583 个桩全部改名**；窄修 = 一个备用符号 + 调用点按元数分流。
（该 survey 的正则需 `re.M`，否则 `^` 只匹配字节 0，会静默报 0 组。）

### B4 `Set <接口字段> = Me` 存成类实例而不是 COM 身份
出处 §44 第 3 条。`Set .OriginalIOleIPAO = This` 把 `me` 直接塞进接口字段，而 VB6 语义是存该接口的
**COM 身份**（`me->__comObj`）。后果：`Set … = Nothing` → `vb6_ReleaseObject` 对 `me` 解引用 `lpVtbl`
（`me->__comObj` 为 NULL 时读 `NULL+0x10`）。本轮**只在运行时兜住**（`vb6_ComIsDispatchable` 挡掉非
COM 接收者），代码生成侧未动 —— 该修的是赋值处的右数取 `__comObj`。
同族前置认知（§44）：`As LongPtr` 数组是**手写伪 vtable** 的载体；`QueryInterface/AddRef/Release` 在
**所有**接口 vtable 里固定在槽 0/1/2，按名后期绑定会去调 IDispatch 槽 5 的 `GetIDsOfNames`。

### B5 `Set 变量 = New <本工程类>` 之后再传该变量仍 AV（EXE 工程类 IDispatch 桥接的未覆盖面）
出处：原「合并自 origin/main」块里**唯一不重复**的一段（EXE 工程类 IDispatch 桥接，提交
`3dd3689/17f349a/df42abc/3b797f9`）的"已知限制（未覆盖）"。`comPackExpr` **只识别字面 `NewExpr`**
（已复核：`src/backend/cgen_util_com.cpp:228` 仍只有 `expr.kind == NewExpr` 分支），所以
`Set h = New bHello` 后把变量 `h` 传出去仍走裸结构体指针 → AV。同段另记一条风险：`__comObj` 成为
EXE 类结构体首字段属**全量布局变更**。

### B6 嵌入清单的**三方**不变式没有固化成守卫
出处 §43 第 1 条。`src/driver/c3rtl.rc`（`<id> RCDATA "../<path>"`）↔ `src/driver/rtl_embedded.hpp`
（`RTL_NAME = <id>`）↔ `src/driver/rtl_embedded.cpp`（`{ RTL_NAME, "basename" }`）必须逐个 id 对齐，
而 `.cpp` 只按 **basename** 索引 ⇒ id 错位的表现是"抽出另一个文件"而不是报错。
合并提交 `c9b7986` 就这样丢过 RCDATA 205（`vb6_di_unknown_stubs.c`，Fix 164 的文件）没嵌进 C3.exe，
同族 DI 桩 **489 → 421，162 个手写桩丢失**（桩体已恢复并提交为 `c0b3729`，**不变式检查本身仍只有
`.temp` 里的脚本**：`.temp/yqt_embed_xcheck.sh <rev|WORKTREE>`、`.temp/di_body_audit.py`（比**桩体**
而非只比名字——名字是全并集也可能逐符号取了对方桩体而静默回退手工修复）、`.temp/di_sig_divergence.py`）。
可开工的两步：把这三份脚本收进 `scripts/` 并挂进 CI；在 CMake/构建期加一条 rc↔hpp↔cpp 计数与 id 断言。

### B7 `On <expr> GoTo 100, 200`（数字标签入口分发）解析不通
出处 §12 L1057–1061，**已复核仍在**：`src/parser/stmt/parser_stmt_jump.cpp` 的 `parseOnStmt`(:17-35)
只在 `next_` 是 `Error`/`GoTo`/`GoSub`（或 Identifier=="local"）时分派；`On k GoTo …` 时 `next_` 是
标识符 `k` → 直接报 "expected 'Error GoTo'…"，`parseOnGoToStmt` 不可达。修法很小（`peek()/peek2()`
向前看一步，约 6 行）。相关能力（数字行号标签、`GoTo/GoSub/Resume/On Error GoTo` 数字）已由
Fix 171 落地，缺的是这一条分发。

### B8 160-D —— `With <返回 UDT 的函数>()` 类型推断缺失
出处 §10 L835–854（任务 #14）。复现：`Common.bas:698/708/718` 的 `With GetAppVersionInfo()`。
`src/backend/stmt/cgen_with.cpp:443` 起只有在 `tempType=="void*"` **且** `inferUdtTypeOfExpr`
（`src/backend/cgen_util_classtype.cpp:166`）命中时才切结构体，而 `inferUdtTypeOfExpr`
**没有"调用返回 UDT 的函数"分支**。
两个坑：① 同文件对函数右值发 `&(<expr>)` → 非法，需先落值临时；② 注册表不能按生成
顺序取（`AppMajor`:697 出现在 `GetAppVersionInfo`:726 **之前**）→ 需从符号表拿返回类型名，而
`src/semantics/symbol_table.hpp:224` 的过程符号只有 `Vb6Type returnType`（枚举，表达不出"哪个
UDT"），带名字的是 **Class 符号专属**的 `memberReturnTypes`（`unordered_map<string,string>`，
消费点 `src/backend/cgen_util_classcall.cpp:457`）⇒ 要么给过程符号补一个返回 UDT 名，要么在
`inferUdtTypeOfExpr` 里查 `SymbolTable` 的过程签名。

### B9 `With New <Form 模块>` 类型未推断 → 整块退化成 COM 后期绑定（任务 #13）
出处 §10 L856–867（原记为"160-W 跨过程发射泄漏"：`MainForm.c:617/628` 用了上一个过程的
`_vb6_with_14/15`），§37 L2472 已把根因改判为本条。现象是 `InputForm` 对话框静默失效。

### B10 Fix 170b —— `vb6_VariantArray()` 恒定打 `VT_ARRAY|VT_VARIANT`
出处 §13 L1100–1102。⇒ `VarType(b) = vbArray + vbByte` 这类判定永远不成立（定义在
`src/rtl/core/vb6rtl/vb6rtl_variant.h`）。建议按载体自身 `elemType` 推出元素 VT。
**注意**：`vb6_VariantToByteArray`/`vb6_VariantToSafeArray1D` 在 Variant 已持数组时是直接
`return v.parray`（**别名不拷贝**），改这里要连带回答所有权问题。

### B11 `Print #f, "x="; <返回 String 的用户 Function>` 把字符串打成 int32
出处 §16 L1254–1256（Fix 176 只修了 `Debug.Print`）。**已复核仍在**：
`src/backend/stmt/cgen_file_io.cpp:114` 非 BSTR 分支发 `vb6_Print(fnum, vb6_Str((int32_t)(val)))`，
`visit(WriteStmt&)` 同形（:153）。⇒ 写文件的探针读数会静默误判。

### B12 `uc_host.c:334` 的 UserControl 内嵌 edit 仍用 `DEFAULT_GUI_FONT`
出处 §23 L1601、§27 L1824（**已复核仍在**）。与 Fix 181 同类：VB6 口径是 MS Sans Serif 8.25pt，
现代 `DEFAULT_GUI_FONT` 是 Segoe UI 9pt。改的时候连带看 C 组第 3 条的 GDI 所有权陷阱。

### B13 `VB6_OrigProc` 属性名被两套子系统共用 → 可把 NULL 写进 `GWLP_WNDPROC`
出处 §38 L2506–2515；§40 L2641 自认"对本工程不成立，但**对有子类化的工程仍是隐患**"。
`src/rtl/core/vb6forms/vb6forms_widget.c:33/49` 与 `vb6forms_picture_prop.c:293/310` 共用同名属性：
一套先 `RemoveProp` 后，另一套 `GetPropW` 取到 NULL 并写进 `GWLP_WNDPROC`。正解参照
`vb6forms_shape.c:437/452`（独立属性名 `VB6_GfxBtn_OrigProc`），并加"取到 NULL 就不写"的守卫。

### B14 两条待复验的签名/覆盖面嫌疑（出处清楚，但我这轮**没能**在代码里定位到原行号）
- §40 L2655–2657：`vb6forms_axsite.c` 的 3 条 **C4113**（签名与槽位不符）⇒
  `IOleInPlaceSiteWindowless` vtable 初始化顺序与接口顺序对不上（不是 NULL 槽，但会调错函数）。
  我按原记的行号（:855）没找到，需重新从当次构建的 MSVC 输出取证。
- §41 L2696–2700：极小复现 `tests/Charts 2020/ucChartArea/Proyecto1.vbp` **还没沉淀成门禁用例**；
  且 `ucChartBar`/`ucProgressCircular`/`ucTreeMaps` 三个子工程当前**编译失败**（C3 退出码 1，
  VB4001 之后），纳入门禁前必须先修（既有问题，与 Fix 187 无关）。§40 L2674–2675：`run_tests.ps1`
  只注册了主 vbp `Charts2020`，**5 个 UC 子工程完全不在门禁**。

### B15 非 `-g` 构建下关闭路径偶发 AV（约 16 次 1 次）
出处 §42 L2752–2759。`code=0xc0000005 / at rva=0x15a271 / av write target=0x0`，帧
`#4 0x15a271 #5 0xa9df0 #6 0x5ed33 #7 0x138643`（同形态在 Fix 188 之前是 `rva=0x15a201`）。
复现法记在 `.temp/teardown_av_note.md`（`demo_close_repeat.ps1 -Runs 20` + **同一次构建**的 PDB）。

### B16 Date 可见性的三条**故意不做**的缺口（Fix 175 的边界）
出处 §24 L1666–1672，已复核三条**全部仍未实现**：
① 类模块 `Private d As Date` 需要与 `knownDateVars_` 平行的 `classDateMembers_`（`grep classDateMembers_ src` = 0）；
② 局部 `Const X As Date` 未登记（`src/backend/decl/cgen_localdecl.cpp` 一带）；
③ **纯时间** Date（`x<1`，如 `CStr(0.5)`）仍输出 `1899-12-30 12:00:00`，VB6 应只给 `12:00:00`。

### B17 frx 侧的两条静默失败
出处 §26 L1771–1780。① `FrxReader::load` 失败静默（`lastError_` 无消费者）→ 应发 warning；
② **带 GUID 头**的 frx 条目（`0x0344` = VBFlexGrid 的 `WallPaper`；以及 `FormatString = "…frx":0000`
这类"字符串存 frx"）按 `headerSize=12` 读出垃圾 `imgSize` → 返回空。属另一类问题，与 Fix 182 无关。

### B18 库限定名末段不在白名单时仍命中 `Vb` 前缀→Long 兜底
出处 §10 L701。`VBA.Collection` 这类还会命中 `src/backend/cgen_base_type.cpp` 自己的 `Vb` 前缀启发式 →
判成 `Long`（本 demo 无此类取值，所以"未动"）。同类隐患的唯一记录，判据见 C 组第 6 条。

### B19 `vb6_UserControl_ContainerHwnd` 仍是 `int32_t`
出处 §37 L2448。**已复核仍在**（`vb6rtl_userctl.h:45` / `vb6rtl_com.c:530` / 写入点
`uc_host.c:169`），而 `VBFlexGrid.c:13691/34659` 把它当 HWND 传给 `MapWindowPoints`/`GetWindowLong`
⇒ HWND 低 32 位最高位为 1 时符号扩展出错。

### B20 `Byte` 数组元素直接进 `If` 比较不成立
出处 §44 L2809–2811。`If buf(3) = 65` 走 Variant 通道（`vb6_VariantFromValue` + `vb6_VarCmpLongEq`）
且**不成立**，中间转一次 `Long` 就对。目前只在 `tests/test_asany_subscript.bas` 的注释里点名，未动。

### B21 两条仍在推进中的族（无 Fix 号，任务列表里是 #9 / #12 / #19 的尾巴）
- VARIANT ↔ typed 转换族（含 160-F 的重估结论）。
- Extender / Ambient 成员访问器的剩余小簇（Fix 161/162/183 之后仍有零星无赋值路径）。

## C. 仍在生效的口径与工具事实（与本文档等长的一半价值在这里；完整版见记忆库）

1. **RTL 是嵌进 `C3.exe` 的 RCDATA**：改 `src/rtl/**` 必须重编 C3.exe 才生效，真凭据是构建日志里
   出现 `Building RC object CMakeFiles\c3.dir\src\driver\c3rtl.rc.res`。新增 RTL 文件还要同时进
   `C3RTL_EMBEDDED_FILES`（`CMakeLists.txt`），否则照编不误却永远没嵌入（见 B6 的三方不变式）。
2. **符号化崩溃必须 `-g`**：`--keep-for-debug` **不产 PDB**，那种 exe 的 RVA 一个名字都出不来。
   x64 用 `.temp/sym3.ps1`（基址 `0x140000000`），x86 用 `.temp/sym_x86.ps1 -Exe <exe> -Base <hSelf>`
   （`sym3.ps1` 硬编码 x64 基址，x86 上是错的）。`/Od` 下栈扫描有假阳性，RVAs 大于镜像即丢。
   给人手测崩溃用 `.temp/case190g.ps1`（`-g --keep-for-debug --output-dir .temp\case190g` +
   `C3_CRASH_TRACE`/`C3_COM_TRACE`，留进程不杀）。
3. **改 per-control GDI 对象成共享缓存前先 `grep` 谁删它**：`vb6_SetControlFontFromLogFont`
   （`vb6forms_ctrl.c`）无条件 `DeleteObject(hOld)`，只挡 `OBJ_FONT`，进程级缓存同样会被第一个写
   `Font.*` 的控件销毁（GDI 会复用句柄值 → 别处字形乱变）。
4. **"0 个 MSVC 错误 / 它能跑" ≠ 健康**：C3 只在**有错时**写 `_c3_msvc_out.txt`，警告全丢，所以
   `void*` 喂给 typed `X*` 这类静默误编会给出绿色构建 + 运行期崩。`/W3` 的诊断事后不可恢复。
   同理"截图截到了"也不代表没崩：`WM_CREATE` 里的 AV 常常照样留下窗口 —— 用重定向的 stderr 判活。
5. **`--dump-*` 打在 `.vbp` 上不是只读**：它 dump 完会继续 codegen + 调 MSVC（别人门禁在跑时会抢 CPU）。
   要 dump 就单 `.bas` + `--output-dir .temp/scratch`。另 `--dump-symbols` **从不打印 UDT 成员字段**，
   回答不了"`typeRefName` 填了没"，这类问题去看生成的 C。
6. **类型名解析对了 ≠ 载体/落点对了**：`TypeSystem::resolveTypeName` 是**首命中即返回**的启发式瀑布，
   语义层与代码生成共用它，所以错得很"自洽"（表现成"类型一致，只是符号缺失"而不是不匹配）。而
   `mapSaElemType`/`mapSaElemCType`、`inferExprType`（会拿**元素**类型回答整数组引用）、按 **C 类型串**
   登记的键（`Date`/`Double`/`Currency` 都发 `double`）都是**独立的第二道映射**，哪里漏了都会
   "绿色构建 + 运行期崩/静默错值"。问"改了没生效"先看 `--dump-symbols` 与生成的 C。
7. **门禁与验证**：`.temp/gate.ps1 -Tag regressNN` → `output/yqt_regressNN.log`（UTF-8；裸 `>` 重定向
   给 UTF-16LE，`grep` 读不出东西）。快验收只跑 GUI 面用 `.temp/gate_vbp.ps1`（`-Category vbp`，约
   255 秒 vs 全量 20–35 分钟）。判完成 = 有 `Results:` 行 **且** 无存活 C3/cl/link/ninja。
   两类**环境性**红要先排除再相信：`fatal error C1060`（散在无关 `rtl/*.c` = 内存压力，重跑）、
   大面积 `FAIL (compile)` + 某段异常快 = `INCLUDE` 被污染（把 `scripts/dev.ps1` 和门禁链进同一个
   PowerShell 进程即触发，**必须分两次启动**）。
8. **写用例的已知坑**：`Test-Syntax` 只看退出码（证明不了运行期语义，`Add-BasTest` 才会跑）；
   VB6/C3 的 `String` 是 Unicode，`StrPtr` 给 **UTF-16** 字节；`vbNullChar` 编成 `vb6_BSTR_Empty()`；
   控制台输出用 `Debug.Print` 而不是 `Print`。
9. **共享工作树纪律**：本文件与整棵树同时有另一个会话在写。**动手前后都 `git status --porcelain`**；
   `git diff --numstat | awk '$2 > $1+5'` 抓"一次 Edit 静默删掉大块"（曾把 `vb6rtl_builtin.h` 从
   315 行砍成 144 行，症状是**所有**测试红）；提交**按位置逐 hunk 筛**，不整文件 `git add`；
   不杀别人的 `C3.exe`/`cl.exe`，不 `checkout`/`restore` 别人的文件，不用裸 `git stash`。
10. **临时目录会被冲掉**：`%TEMP%\C3C\<pid>\` 里放的是 `_c3_msvc_out.txt` 与生成的 `.c/.h`，
    任何一次后续编译（含测试套件）都可能删掉 ⇒ 构建的**同一条命令**里把产物快照到 `.temp/gen/`。
    取会话目录用 `grep -a "kept at" | tr -d '\r' | sed 's#.*kept at: ##'`（`[0-9]{13}` 会截断 15 位 id）。
    生成 `.c` 的行号**不映射** `.bas` 行号。
11. **控件族"方法"的调用侧有两条互不相通的路径，只改一处会剩一半 no-op**（Fix 185 落地时实测）：
    带实参的 `Pic.Print "x"` 走 `src/backend/detail/expr/cgen_expr_call_callee_withm.inc` —— 该片段
    include 在 `cgen_expr_call_com_bind.inc` **之前**（`src/backend/expr/cgen_expr_call.cpp:42-43`），
    所以新的控件特判必须追加在 withm 末尾才抢得到通用 COM 绑定前面；**无括号**的 `Pic.Cls` /
    `List1.Clear` 根本不进 withm，而是落在 `src/backend/stmt/cgen_call.cpp` 的 `isComMarker_` 分支
    （Fix 086 的 `List1.Clear` 就是同一形态先例）。配套协议：语句自己 `c_.emitLine(...)` 之后把
    `lastExpr_` 置 `"0"`，`cgen_call.cpp` 的 `else if (callExpr == "0")` 分支保证不会再补发一条裸 `0;`。

## D. 已完成项一行索引（叙述已删；原文在 `git show 1465da1:ai/C3_FIX_HANDOFF.md` 的对应 §区间）

| 原 § | 内容 | 状态 |
|---|---|---|
| §1–§3 | vbman 无窗体 bisect 基线 65 → 7、含窗体全量 129 条目 → **0 编译错**；阻塞项"陈旧对象"解除 | 已收敛（092r-z 时代） |
| §4, §5, §5b | Fix 084 系列、`m_uData` 模块级 UDT（010n 扩展）、088e/089 系列 552 → 216 | 已提交 |
| §6, §7, §8 | 剩余错误分布快照（已过期）、关键文件地图（已过期：`cgen_*.cpp` 已拆成 `src/backend/detail/{expr,stmt,util,base,module}/*.inc`）、会话纪律 | 快照过期，纪律仍生效（已上收 C 组第 9 条） |
| §9 | ToolsTlsThunks C2224 ×37（COM 集合簇）"建议方向未实施" | 实已实施：`src/backend/detail/expr/cgen_expr_member_generic_access.inc:123-124` |
| §10(链接期) | 443 未解析符号、LNK2005 重定义 | 已由 093a/093b 解决，规范叙述在 `ai/开发历程/88-*`、`89-*` |
| §10(158a–160-G) | VBFlexGridDemo 编译错 236 → **31**：Variant 比较 C2088、`_Generic` 转换族、`Mid$/Left$/Right$` 实参、Property Let 槽位、ByVal String 隐式 CStr、`Form.hWnd` 成员误判、LSet 三处、`MSDATASRC` 别名、`ERROR_NOENTRY` RTL 中断 | 已提交（含 1 次 Edit 连带删 171 行原型的事故记录） |
| §11 前后 | Fix 161（`VB.`/`VBA.` 库限定名被降成 Long）30 → 16、162（Extender Width/Height 无赋值）、163（跨单元无原型 `void*` 调用）、164（DI `unknown` 族缺导入库 → 64 例链接失败）、165（GUI 入口点按启动对象决定 + 找回被生成器删掉的 39 个桩）、166（`&(void*){…}` 多一层间接） | 已提交 |
| §12, §13 | Fix 167（`Sub Main` 驻留语义）、168/169（Extender 结构体守卫、COM `VARIANT*` 裸拼 → Err 380）、170（`arrName() = expr` 整数组赋值当成 0 号元素写）、172/173 —— **demo 第一次真正进消息循环** | 已提交；尾巴见 B7/B10 |
| §14–§20 | 参考图差异逐项定位：174/176（`Debug.Print` 打包）、177（单元格存**悬垂 BSTR**，`477c90f`）、178 前两轮猜测 | 已提交；178 见下一行 |
| §20–§22 | Fix 178：UDT 赋值/`LSet` 对含所有权成员的结构体做**深拷贝**（C 的浅拷贝导致"行 ≥2 别名到行 0"，整格显示第 0 行） | 已落地并验证 |
| §23–§28, §18 | Fix 181：控件默认字体改 VB6 口径 MS Sans Serif 8.25pt + `NONANTIALIASED_QUALITY`；悬垂 HFONT 隐患按"每控件一份字体"收口 | 已落地（尾巴见 B12） |
| §25–§26, §33 | Fix 182：容器子控件**第二条发射路径**缺设计期属性（frx/Text/List…），`b953147`；CellPicture 预览空白由此关闭 | 已提交（尾巴见 B17） |
| §29–§32 | Fix 184：网格只画 13 行的根因是 RTL 里**缇/像素两套 DPI 口径混用**（`*15` 硬编码 vs 真实 DPI），统一走 `vb6_DpiX/Y`；Fix 183 验证失败**已回退并作废** | 已落地（dpi=96 下逐位相同，所以控制台用例不动） |
| §34–§35 | Fix 185：控件级 `_Paint` 从不派发（`Picture2_Paint` 是死代码）+ `Print/Cls` 编成 `vb6_ComCall(HWND,…)` 运行期 no-op ⇒ "Drag/drop me" 框空白。四处协同：`SubclassInfo.hasPaint` + WM_PAINT 派发（`BeginPaint`→挂 `VB6_PaintDC`→调用户过程→`RemoveProp`+`EndPaint`，**`return 0`**）、成员侧 PictureBox 分支（必须落在 023e/089d 兜底**之前**）、调用侧 withm（带实参）+ `cgen_call.cpp`（无括号）两条、RTL `vb6_ControlPrint/vb6_ControlCls`。落地后 demo 画面与参考图**逐项一致**（最后一个已知差异关闭）。另记：`.temp/fix183_block.bin` 永久作废 | 已落地；边界 → B1 |
| §36–§41 | Fix 187：`Declare As String` 只有名字以 A 结尾才做 ANSI 编组 → `GetProcAddress` 返回 0 → Charts2020 自造子类化 thunk `call NULL`（关闭崩溃）；含极小复现与一次无效取证（反向 A/B 未设 `C3_CRASH_TRACE`）的更正 | 已落地；尾巴见 B13/B14 |
| §42 | Fix 188：`Startup = Sub Main` 的工程关窗后进程不退出（没人投 `WM_QUIT`，按 Forms 计数收口） | 已落地；偶发 AV 见 B15 |
| §43 | Fix 189（**仅测试侧**）：门禁 `Test-GuiVbp -AutoExitSec` 无条件 `pass++`，窗口出现后自退崩溃照记 PASS；现改为区分"我们杀的"与"它自己退的"并读退出码 | 已落地（同节两条遗留 → B6/B3） |
| §44 | Fix 190：`As Any` ByRef 实参的 `[]` 下标链判为非左值 → 把元素**值**当 memcpy 目的地址（悬停即写 0x0）；Fix 191：`As LongPtr` 数组按 Variant 载体分配打断手写伪 vtable + `AddRef/Release` 直发槽 1 + `vb6_ComIsDispatchable` 守卫 | 已落地 `1465da1`（遗留 → B4/B20） |
| 合并块 2858–2867 | EXE 工程类 IDispatch 桥接（`VBMAN_DEMO` 启动即崩 → 通过），提交 `3dd3689/17f349a/df42abc/3b797f9` | 已提交（未覆盖面 → B5） |
