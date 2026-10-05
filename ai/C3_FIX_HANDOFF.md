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
- **画面验收于 2026-09-22 16:30 关闭**：最后一个已知差异（"Drag/drop me" 框空白）随 **Fix 185**
  落地，提交 `76fd19a`，门禁 `regress51` = `PASS=93 FAIL=0 SKIP=1 TOTAL=94`（与 `regress50` 同分）。
  即：**demo 的运行期画面已与参考图逐项一致**，B 组剩下的都是非画面项（潜在错值/崩溃、覆盖面、
  口径缺口）。注意参考图原件 `Desktop\2026-09-21_231559.png` 已不在盘上，比对基线只剩
  `.temp/demo_shot*.png` 这一串（`demo_shot39.png` = 改前基线，`now185.png` = 改后）。
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
### B14 两条待复验的签名/覆盖面嫌疑（出处清楚，但我这轮**没能**在代码里定位到原行号）
- §40 L2655–2657：`vb6forms_axsite.c` 的 3 条 **C4113**（签名与槽位不符）⇒
  `IOleInPlaceSiteWindowless` vtable 初始化顺序与接口顺序对不上（不是 NULL 槽，但会调错函数）。
  我按原记的行号（:855）没找到，需重新从当次构建的 MSVC 输出取证。
- §41 L2696–2700：极小复现 `tests/Charts 2020/ucChartArea/Proyecto1.vbp` **还没沉淀成门禁用例**；
  三条**2026-10-04 真编译重测**（`.build/b187out/`，`--arch x64` + `--arch x86` 各一遍，工程名一律是目录下的 `Proyecto1.vbp` 而不是 `<UC名>.vbp`）：**ucChartArea 与 ucPieChart 现在编得过、两份 exe 都出** ⇒ 可以直接升格进门禁；仍失败的只有 `ucChartBar` / `ucProgressCircular` / `ucTreeMaps` 三件，而且**三件的根因互不相同** ⇒ 拆成 B29 三条分别开工。§40 L2674–2675：`run_tests.ps1`
  只注册了主 vbp `Charts2020`，**5 个 UC 子工程完全不在门禁**。

### B15 非 `-g` 构建下关闭路径偶发 AV（约 16 次 1 次）
出处 §42 L2752–2759。`code=0xc0000005 / at rva=0x15a271 / av write target=0x0`，帧
`#4 0x15a271 #5 0xa9df0 #6 0x5ed33 #7 0x138643`（同形态在 Fix 188 之前是 `rva=0x15a201`）。
复现法记在 `.temp/teardown_av_note.md`（`demo_close_repeat.ps1 -Runs 20` + **同一次构建**的 PDB）。
· **账 #184 之后先重测再决定去留**：本条与 B23（已出）是同一族——被 OS/COM 按 `__stdcall`
  调的过程以前一律以 cdecl 发码。关闭路径正好走 `RemoveWindowSubclass` + 一批 subclass thunk，
  所以这条的 1/16 有可能已经跟着 #184 一起没了；先跑那 20 次，别先动手改代码。
· **2026-10-04 重测完了（照上面那条指示，只测量、没动代码）⇒ 三组各 20 次全 0，但这组读数判别不了**：
  ① 修后 x64（`.build/b23fix64/VBFlexGridDemo.exe`）plain start→WM_CLOSE 20 次：`codes: 0x00000000=20 clean=20 crashfiles=0 stuck=0`；
  ② 修后 x86（`.build/b23fix3`）按「先点 FlexGrid 再关闭」那形 20 次：同样 20/20 rc=0、0 crashfile；
  ③ **修前基线件**（`.temp/demo188/VBFlexGridDemo.exe`，md5 `0595d20e83e8518bde410fa9c70ed60f`，原样拷到 `.build/b15base/` 再跑，原工件没动）
     同 shape 20/20 0，**连当年那条 1/16 用的原夹具**（`.temp/demo_close_repeat.ps1 -Runs 20`，按 stderr 里 grep `C3_CRASH` 计数）也是 20/20 `traces=0`。
  ⇒ 口径：基线本人不复现 ⇒ 「0/20」**既不能记给 #184，也不能证明这条已经没了**（p=1/16 时 20 次全绿的概率是 (15/16)^20≈0.28，本来就不够判）。
  ⚠ 另有一条分析侧的订正，比读数更要紧：**#184 那一刀在 x64 没有字节后果**（MSVC 在 x64 忽略 `__stdcall`，桩只是 cdecl 转发），
     而 B15 的原始现场正是 x64（`base=00007FF6…`）⇒ 本来就**不该指望** #184 收掉它；「先重测」这一步的价值是把这条期望判死。
  下一轮的抓手（别再靠加大抽样次数）：`c3_crash.txt` 那份现场里 `#10 = 应用帧 ← USER32 ← COMCTL32+0x2CED8 ← COMCTL32+0x2CBD4` ⇒ 关闭期有一次
     消息派发进了应用码，应用码里对 **NULL 解引用写**。同族的 B13（两套子系统共用属性名 `VB6_OrigProc` ⇒ 能把 NULL 写进 `GWLP_WNDPROC`）
     机制不同但同一张桌子：B13 是「把 NULL 存成 wndproc」，本条是「拿着 NULL 写」。⇒ 先做 B13（有确定修法：独立属性名 + 取到 NULL 就不写的守卫），
     再做一次抽样，看这条有没有跟着少一个候选。工件都留着：`.build/b15base/`（基线件副本）、`.build/b23_close.ps1`（这次修好了退出码取法：
     `Start-Process -PassThru` 拿不到 `ExitCode`，改成 `[Diagnostics.Process]::Start($psi)`；顺带记一条——`ProcessStartInfo` 在 .NET Framework 里**没有**
     `RedirectStandardErrorFileName` 这个属性，赋值会抛 PropertyAssignmentException，要落文件只能自己读 `StandardError`）。
· **2026-10-04 再补一行（账 #185 已出 ⇒ 上一条指的「先做 B13」这一步做完了，但没收到本条头上）**：#185 收掉的是「事件层被自绘层整层挤掉」那一族，
  本条的现场是应用码**对 NULL 解引用写**（不是把 NULL 存成 wndproc，也不是没装），机制不同 ⇒ **#185 不构成对本条的解释**。
  下一轮别再抽行了（累计 0/60，基线本人不复现）：要么用**同一次构建**的 map/pdb 把 `rva=0x15a271` 那一格钉成函数名（nearest-symbol 不算，见记忆里那条），
  要么就按「抽样判不了」长期挂起。

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

### B20 `Byte` 数组元素直接进 `If` 比较不成立
出处 §44 L2809–2811。`If buf(3) = 65` 走 Variant 通道（`vb6_VariantFromValue` + `vb6_VarCmpLongEq`）
且**不成立**，中间转一次 `Long` 就对。目前只在 `tests/test_asany_subscript.bas` 的注释里点名，未动。

### B21 两条仍在推进中的族（无 Fix 号，任务列表里是 #9 / #12 / #19 的尾巴）
- VARIANT ↔ typed 转换族（含 160-F 的重估结论）。
- Extender / Ambient 成员访问器的剩余小簇（Fix 161/162/183 之后仍有零星无赋值路径）。

### B24 OLE 拖放的 `hdrop` 旁路日志没人钉
出处 = 账 #175 的跨门工件对形 #300→#301：`oledd_test.txt`(s4) 从 `hdrop FAIL hr=0x1` 变成
`hdrop first=C:\a.txt` —— 两边都不红，因为 `run_tests.ps1` 里 grep 不到 `hdrop`/`oledd`，
它是夹具自己写的日志文件。首拖成败正是这类测试最容易漂的地方 ⇒ 值得挑一轮把它翻成针面。
### B25 账 #179 = 已出（提交 `30a3f4ff`，门 #303）；剩下半条并入 #159
· 已修：容器直调控件公共成员时的宿主上下文 —— 发码在每个 .ctl 实例方法体首 `vb6_UC_PushInstance((void*)me)`、统一出口尾 `vb6_UC_PopInstance()`（前提：`Exit Function/Sub/Property` 早已走 `vb6_proc_exit`，所以配对不漏弹 —— 这也判掉了"路 B 要等 T31-A"那条顾虑）。实测两侧读数由 `mode=3 / mode=1` 变成同值，针面 `U-CTX=True`。
· 剩下半条 = `UserControl.hWnd` 在控件代码里读不回真值，**归 #159**（hwnd 被当对象装箱）。**订正 `30a3f4ff` 提交信息里那句"比较器没问题、值真的是 0"**：`vb6_VarCmpLongNe` 的第一形参是 `vb6_VARIANT*`（vb6rtl.c:593），发码传的是 `&vb6_UserControl_hWnd`（`void*` 全局的地址）⇒ `<>0` 与 `=0` 走同一个被 reinterpret 的槽位，**这对探针判别不了**。
· 那条下一步已跑完（换了探针形态：直接在 `vb6_uc_push` 里打 `r->hwnd` 与全局，两边同值且非 0）⇒ **值是实的，坏的是读法**。**#159 已出**（提交 `ef260daf`，门 #304）：类型 oracle 与那三份成员名单收成一张表 `kHostPseudoRows`，`UserControl.hWnd` 现在发 `(-(vb6_UserControl_hWnd != 0))`而不是 `vb6_VarCmpLongNe(&vb6_UserControl_hWnd, 0)`。本条到此结，剩下的三件表外欠账另立 B26。

### B26 宿主伪成员那张表**外**欠的三件（账 #159 顺带量到）
· `PropertyPage` 一族全局（`vb6_PropertyPage_hwnd` / `_hWnd` / `_ScaleMode` / `_ScaleHeight` / `_Changed`，
定义在 `vb6rtl_com.c:666-670`）**运行期从没有人填值** —— 全是初值 NULL / 1 / 0 / 0，两个 `hwnd`/`hWnd`
拼写并存也只是为了迁就发码。⇒ .pag 代码里 `PropertyPage.hWnd` 恒 0、`Changed` 读写都不落到页上。
**但这一条实测下来不是缺陷, 是产品形态**：driver 把 `.pag` 只当「类模块 + 设计器模块」编进产物（`driver_frontend.cpp:155-170`），RTL 侧**没有任何运行期实例化属性页的人**（`grep PropertyPage src/rtl` 只剩那五行定义），而 VB6 里属性页本来就只由设计器/属性浏览器承载 —— Standard EXE 运行期那些值恒为初值**与 VB6 一致**。⇒ 那张表要保的是它**编得过 + 类型答对**（#174 与 #159 已各自结掉），**不要**去「填」这五个全局。真要做属性浏览器（`PropertyBag` / `IPerPropertyBrowsing`）时再建 .pag 上下文，那时才用得上 #179 那套 Push/Pop。
· 那张表刻意不收的成员 = RTL 根本没声明的人：`vb6_UserControl_Left/Top`（只有 `vb6_Extender_Left/Top`）、
`vb6_PropertyPage_ScaleWidth`（只有 `ScaleHeight`）、`Appearance` / `BorderStyle`（旧 `kNumericHostMembers`
里躺着，永远不可能命中，因为写它们就是 C2065）。要这些名字得先在 RTL 补声明与填值，别在表里挂空名
（哨兵 `ROW-SYMBOL-MISSING` 就是拦这个的）。
· 普查读数：表外手写的宿主符号点 **24 处** —— `cgen_form.cpp:231-245`（按实例 `#undef/#define`，rev20 那套）、
`cgen_base.cpp:61-148`（`Parent.<成员>` 链改写）、`cgen_with.cpp:650-652`（「`With <伪对象>` = 它的 HWND」
这条规则，里面还写死了 `vb6_PropertyPage_hwnd` 那一格拼写）、`cgen_util_type.cpp:824-828`（RTL 宿主**方法**
的形参类型表，与成员值面两回事）、`:864`（认 `vb6_Ambient_DisplayName` 是 BSTR —— 这一处现在表里已答
String 档，下一轮可以试着让 `rewriteByteArrayValue` 改问表）。前四类是**规则**不是成员清单，不动它；
最后一处是真正的第二份口径。

### B27 czUI 的 x64 产物启动期 AV —— **判掉：源码形状限制，不改编译器**
出处 = 账 #180 的真跑面（A/B 已证与那一刀无关：拿改前的编译器编同一份夹具，退出码一模一样
`0xC000041D`）。这一轮把它量到底了，结论是**这条不该由编译器修**：

· 现场（`C3_CRASH_TRACE=1` + `-g` 的 .map 符号化）：AV 落在 `gdiplus.dll+0xF2E1`，读
`0x14FE1F88` —— 一个 32 位量级的地址。栈：`vb6_form_create_frmDemo+0x1027` ←
`vb6_czControl_prop_let_BackColor` ← …，全在 GDI+ 调用上。
· 根因在**工程的 VB 源码**：`tests/czUI-main` 的 Declare 把指针一律写成 `As Long` ——
`GdipCreateFromHDC(ByVal hDC As Long, ByRef graphics As Long)`（被调方把 64 位指针写进
4 字节槽，高半截落到栈上别处）、`GdipDeleteGraphics(ByVal graphics As Long)`（把截断值
递回给 API）、`GdiplusStartup(ByRef token As Long, ByRef inputbuf As Any, …)`。这类声明
**62 行**，是 VB6 只在 32 位跑留下的形状。⇒ 要 x64 就得先把这些声明改成 `LongPtr`
（**改 VB 代码的活**，与 tB 的口径一致：它也是要求源码用 LongPtr，而不是把 Long 偷偷加宽），
不是往 RTL/发码里塞补偿。VB6 工程引用了 32 位 OCX 时同理，别去试 x64。
· 所以本线口径：**czUI 只在 x86 那格钉**（CI 现状 `Test-GuiVbp "czUI" -Arch "x86"` 就是对的），
x64 那档不补用例、不补产物；将来若要把 x64 立成目标，先改夹具源码，再谈别的。
· 顺带留一条**独立**的观察（不在本条结案范围）：崩溃轨迹器 `vb6_CrashTraceVEH`
（`vb6rtl.c:82`）在爆栈现场会**重入** —— 那份 c3_crash.txt 里同一递归栈打了 4 段，第一现场
（gdiplus 那段）被压在最后。诊断工具一重入就把现场盖掉，值得单独挑一轮加个"每进程只记一次"
的闸；但那是诊断面的质量，不影响任何产物行为。

### B22 隐式函数声明（C4013）现在没有**守卫**，只有 census
出处 = 账 #173 / #174。两格都已出（#173 补 `vb6com_internal.h` 的 `extern double vb6_VariantToDouble(VARIANT)`；#174 把裸名 `SelectedControls` 接进 `kPropertyPageHostMembers`），`tests/Charts 2020` 整个构建的 C4013 从 **14 → 0**。但**没有任何东西阻止它再长回来**：
· 为什么必须当缺陷：x86 cdecl 下被隐式声明的函数按 `int` 取返回值，而 `double` 返回值躺在 x87 栈 ST0 上、调用方永不 `fstp` ⇒ 每调一次漏一层栈，八层之后栈满、之后任何浮点取值得 QNaN `0x7FF8...`（#173 的炸法）。x64 走 XMM0，全静默。
· 现成的收口办法：对 RTL 源加 `/we4013`（`src/backend/msvc_driver.cpp` 那一条 `cmd << " /W3"` 旁边）。**前提**是生成码侧也零 C4013 —— 生成码的雷由 #174 那格清了，但只清了这一个名字，未解析裸名的**兜底仍然是发裸名**（`cgen_expr_ident_builtin.inc` 尾部的 Fix 110u 一族），别的工程换个名字就又会漏。
· 所以顺序建议：先量「语料里还有没有别的未解析裸名调用」（`--emit-c` 全语料跑一遍 cl 数 C4013，数法见记忆库「数 cl 的警告必须自己重跑 cl」），再决定是上 `/we4013` 还是在 cgen 侧把未解析裸名**判死**（后者才是单一权威，但要先确认不会把「隐式 Variant 局部」那条兜底一起打掉 —— 它就是 Fix 110u 立着的理由）。
### B28 `run_tests.ps1` 的 PASS/TOTAL 不同源（门的判据没受影响，账面会误导）
出处 = 门 #311 的 11 份 job 日志（副本在 `.build/gate311/`）：vbp 分片 1 / 2 / 4 各打 `PASS=TOTAL+1`（43/42、46/45、43/42），
asm 片打 `PASS=13 SKIP=0 TOTAL=14`（差 1），其余七片自洽。本轮 11 片 `FAIL=0` ⇒ 门是绿的，这条只动**账面**。
后果：任何写成「PASS == TOTAL」的自洽式检查在这里都会假红/假绿 ⇒ 门的判据只看 `FAIL=0` 与工件行。
下一轮动 `tests/` 时顺手把计数收成一处口径（别为它单开一轮门）。

### B29 三件 Charts UC 子工程编不过的根因（2026-10-04 一次真编译量到的，每条都带生成码原文）
出处 = `.build/b187out/x64_*/c3-error.log` 与那份临时生成码（`%TEMP%\C3C\...`）。三件互相独立，别并成一刀。
· **① `ucChartBar` + `ucProgressCircular`：设计期字体块的 Size 被打成非法浮点字面量** —— 生成码原文
  `_vb6_f119->Size = 12f;`（MSVC `error C2059: 语法错误:"数字上的错误后缀"`；Form2.c 里 4 处、Form1.c 里 5 处）。
  发码处 = `src/backend/detail/module/cgen_form_create_controls.inc:116-118`：`snprintf(szBuf, "%.4g", sz)` 之后直接
  `+ "f"` ⇒ **整数值**（12 / 9 / 10）出来就是 `12f` 这种非法 token。同文件 830 行那条路用 `std::to_string(fSize143)`
  所以没事 —— 两条路两套口径，正是这类的常态。正确修法不是就地补小数点：字面量的形状该由**一个**出口负责
  （`cgen_expr.cpp:15` 的 `floatingLiteral` 已经保证「没有 . 或 eE 就补 .0」，把 93 行那句 `+ "f"` 与这里的 `%.4g` 一起收进它，
  再加一条 census 哨兵：`src/backend` 里任何 `+ "f"` 前面必须是 `floatingLiteral` 的结果）。
  **→ ① 已出（提交 `33cb239a` + 收口红 `936124e8`，门 #313（run 37198483318，head `936124e8`，attempt 1）= 11 job 全绿、11 片 `FAIL=0`）**。
  **中间门 #312 红过一次，红因是我这一刀**：我把三处 `std::to_string(v)+"f"` 一起"收进权威"了，而那三处**本来就合法**（to_string 必带 6 位小数）—— 其中设计期 FontSize 那条的字面形状正被 `sl_emitc_native`（vbp#4 片，tests/ctrlslider）钉着 ⇒ 变成 `20.0f` 就照红。
  ⇒ 退回：权威头再加一个 `floatFixed6Literal`（= to_string + 后缀，形状规则仍在同一处），三处改调它 ⇒ 发码字节与 HEAD 一字不差；只有真正坏掉的 `%.4g` 那处走 `floatSingleLiteral(sz, 4)`。
  **工具事实（这次学到的，别再犯）：A/B 的工程清单不能手挑** —— 本仓门禁在发码形状层面的断言面实测有 30 个文件（`.build/b188_surface.txt`，由 `b188_surface.py` 从 run_tests.ps1 的 `Test-EmitcShape`/`Test-CodegenNote` 里推出来）。我第一版手挑 8 个工程 ⇒ 本地全绿、门上一片红。加宽版（37 × 两架构 = 74 次发射）现在只有 ucChartBar 两片被改、数值全等、增删 0 行。

  改法按上面那条口径做: 新增 `src/common/float_literal.hpp` 作为**唯一出口** (`floatingLiteralText` / `floatSingleLiteral`，经典 locale + 「没有 `.` 或 `eE` 就补 `.0」`)，把五处手拼点全接过去 (设计期字体块、设计期 FontSize、属性袋 VT_R4、IFont 袋、语义层 Fix 133z 那段手写 `%.9g`+补点+拼 f)。字体块位数照旧 4 位 ⇒ 8.25 的写法一个字都不变。
  配对读数 (同一份工程、两台编译器、x86): `ucChartBar` 的 C2059 **4 → 0** (只剩本条②的 1 条 C2065)，`ucProgressCircular` 的 C2059 **16 → 0** (剩③的 C2084/C2065/C2198)。
  判据两面: 发码形状针 `fontsize_literal` 钉在**真实工程**上 (Needles `->Size = 12.0f;` + 证人 `->Size = 8.25f;`，Absent `->Size = 12f;` 就是改前的形状)；哨兵 `scripts/check_float_literal_shape.ps1` (R1 手拼后缀=0 / R2 权威头带着补点与经典 locale / R3 调用点>=4) 在缺这刀的树上 R1~R3 全红，R1 当场点出那 6 个手拼点。护栏 `--emit-c` A/B (BASE = 已发布的 `a4e3b574` 冷编) 8 工程 × 两架构 = 14 份逐字节相同，只有 ucChartBar 两片被改，且归一化浮点 token 后逐行相等、数值多重集相等、增删 0 行。

· **② `ucChartBar`：宿主 UC 实例的句柄名发成了没声明的标识符** —— 生成码原文
  `int32_t i_end = (vb6_ComGetIntProp(vb6_hwnd_ucChartBar1, L"Count") - 1);`（`error C2065`）。
  `ucChartBar1` 是**控件数组里那一枚实例**，而实例句柄住在 `vb6_arr_ucChartBar` 里 ⇒ 与账 #157 那条同族
  （拼 `vb6_hwnd_<名>` 而不是走 `ctrlHwndExprForInit` / `vb6_arr_*` 那条既有出口）。修法 = 把这一处也改读那个唯一出口。
  **→ ② 已出（提交 `ea5155af`，门 #314（run 37200966607，head `0efb29b0`，attempt 1）= 11 job 全绿、10 片 `FAIL=0`）**。两条订正，开工前先看这两句：**①根因不是「实例句柄拼错名」** —— 数组实例的句柄表达式本来就走 `vb6_CtrlArr_GetAt(&vb6_arr_<名>, i)`、设计期下发那一路一直是好的；坏的是**整体成员** `Count` 少了一条分支，于是被当成「控件属性」去 COM 兜底，而兜底那一条自己拼了 `vb6_hwnd_<数组名>`（数组压根没有这个变量）。**②「同窗体就编得过」那一半更危险**：同一窗体内 `vb6_hwnd_<名>` 若因为别的原因存在，它编得过、`Count` 恒答 0 —— 只看编不编得过会把这当成修好了。
· **③ `ucProgressCircular`：同一个过程发了两遍体** —— `Form1.c(10)` 与 `Form1.c(13)` 报
  `error C2084: 函数"void vb6_Fo..."已有主体` + `C2198`（实参数不符）⇒ 某个 `Form_*` 事件在两份名单里各发一次。
  这条要先把两份名单找出来（`grep` 发事件 thunk 的那两处），再定哪一份是权威。
· **④ 顺带一条诊断面的事实**（不算缺陷，记着省时间）：`.pag`/`.frm` 报错的**行号不含设计期头块** ——
  `PropPagFMR.pag(594,17)` 实际指向文件第 720 行（720 - 126 = 594，前 126 行是 `VERSION` + Begin/End 控件块）。
  按报的行号去找源码会一无所获 ⇒ 要么按 `文件行 = 报的行 + 头块行数` 折回去，要么改成报真实行号。

· **⑤ 事件臂的处理器函数名按「控件设计期拼写」现拼，而过程定义按 Sub 自己的拼名**（账 #190）—— `ucChartBar` 的 C2065 清掉之后 x86 真编译走到链接期才红：`Form1.obj : error LNK2019 无法解析的外部符号 _vb6_Form1_cboLabelsPositions_Click`、`_vb6_Form1_ChkAxisY_Click`（`fatal LNK1120: 2 个`），物证 `.build/b189new/ucChartBar/c3-error.log`。产物里逐行对上：存在性查询 `symTab_.lookup(ctrl.controlName + "_Click")` **大小写无关**、命中了；发出去的名字却是 `cProcName(ctrl.controlName + "_Click")` —— 拿控件名现拼。而定义那一侧用符号自己的名字。Form1.frm 里恰好两枚不同（控件 `ChkAxisY` / 过程 `ChkAxisy_Click`；控件 `cboLabelsPositions` / 过程 `CboLabelsPositions_Click`）⇒ C 大小写敏感 = 两个符号。普查：`cProcName(ctrl.controlName + "_…")` 那一形 **27 处**（`cgen_form_wndproc_create.inc` 18 / `…_dispatch.inc` 6 / `cgen_form_ctrl_style_apply.inc` 1），而同一文件里 501/523/544 三处已经是**对的**写法（`cProcName(clickSym->name)`）⇒ 口径本来就有，只是没收成一处权威。修法 = 一枚 `eventHandlerFn(ctrlName, suffix)`：lookup 一次，命中就交 `cProcName(sym->name, sym->accessLevel)`，没命中交空串让调用方**不装这条臂**；配 census 哨兵禁掉「现拼」那一形。动手前先按门 #312 那条教训查一遍形状断言面 （`.build/b188_surface.txt`）里有没有针钉的是控件拼写那一版函数名。
  **→ ⑤ 已出（提交 `a27758c3`，门 #315（run 37202837330，head `a159b85d`，attempt 1）= 11 job 全绿、10 片 `FAIL=0`）**。两句留给后人：①普查必须按**形状**（`cProcName(<任意> + "_`）而不是按变量名 —— 我第一遍只扫了两个名字，漏了 `info.ctrlName` 那 16 处（焦点/鼠标/Validate 一整批）和菜单 2 处，"27 处"这个数是错的，真数 43；②这一族的红**只在链接期现形**（`LNK2019`），所以判据里必须有一次**真编译真链接**，`--emit-c` 的形状针 + 语义层的存在性检查都拦不住它。顺带一条好消息：这一刀之后 ucChartBar **第一次产出 exe** ⇒ #187 的升格清单多一件，剩下的独立缺陷是 B29⑥（数组元素的成员访问）。
· **⑥ 控件数组元素的成员访问是另一条通路，且现在是坏的**（账 #191）—— 探针夹具（`.build/b189ve_probe`，ve_units 的副本）x86 真跑 `.build/b189probe.out`：`U-ARR-RAW count=3 lb=0 ub=2`（②那三个数是对的）之后 `U-ARR-M //` —— `uArr(0).SW()`/`uArr(1).SW()`/`uArr(2).SW()` **三枚全空**，下一行读 `uArr(0).Left` **段错误**（rc=139）。发码形状（`vb6_CtrlArr_GetAt(&vb6_arr_uArr, aj)` 直接进 `vb6_ComCall(..., L"SW", …)`）说明运行期把数组元素当成了 IDispatch 兜底，而设计期那一路是 `vb6_UC_InstanceOf(vb6_CtrlArr_GetAt(...))` 再打生成的 `prop_let` —— **少包的就是 `vb6_UC_InstanceOf` 那一层**（待量，别当结论用）。真工程全靠这一形：`ucChartBar1(i).AddSerie …` / `.Move …`。账 #189 的判据**刻意没碰**这一形（只钉整体成员三个数 + 一圈计数），两条缺陷不要互相掩盖。
  **→ ⑥ 已出（提交 `f5b4a03b`，门 #317（run 37208673511，head ea3eeeec，attempt 1）= 11 job 全绿、10 片 FAIL=0）**。修法 = Fix 112 那段从发码点整体抽成 `CCodeGen::emitUcInstanceMemberExpr`（`src/backend/cgen_util_classcall.cpp`），单枚交 `vb6_hwnd_<名>`、数组元素交 `vb6_CtrlArr_GetAt(&vb6_arr_<名>, i)` —— 两条路只差这一个串；元素那条（`cgen_expr_member_precheck.inc`）补上这次调用，放在控件属性表与 COM 兜底**之前**。**A/B 又抓出第二条独立的坑（左值侧，与直发本身不是一件事）**：元素改道之后 `ucChartBar1(i).Font.Size = ...` 落进 `tryRewriteCOMLvalue` 的 Pattern C/D2，而那段是**字符串级**改写 —— 只取 `prop_get_` 那一段、尾巴 `.Size` 整段丢掉 ⇒ 发成 `prop_let_Font(elem, <double>)`，成员名没了、值实参的槽还是 `vb6_ComIface_Font*`，实测 Form2.c 471-477 四条 C2440，把刚编得过的 ucChartBar 打回编不过。修法 = 在折叠**之前**给"带尾巴的 prop_get_ 目标"一条独立落点：取回那枚对象再对它写一层（StdFont 本身是真 IDispatch）⇒ `vb6_ComSetProp(vb6_X_prop_get_Font(直发的实例), L"Size", pack(值))`；认不得的尾巴一律 `return false` 让通用路径发成非法左值、当场红 —— 静默丢成员名比编不过更坏。**判据三面**：`tests/ve_units` 真跑证人 `U-ARRM-methods=3`（x64 与 x86 都跑到 `U-DONE`，既有 `U-ARR=True` 不动）+ 发码形状针 `ucarr_member_call`（Absent=改前那条形）与 `ucobj_chain_write`（钉在**真实工程** ucChartBar 的 Form2.frm:223 上）+ 哨兵 `scripts/check_uc_instance_exit.ps1`（U1 expr/ 手拼 InstanceOf=0 / U2 权威三样齐 / U3 两条路都在调它 / U4 带尾巴的左值必须有独立落点且排在折叠之前）；假 needle 验红做过 —— 把 `(Pattern C-tail)` 改名 ⇒ U4 当场点名，还原回绿。**真编译配对读数（x86）**：ucChartBar **rc=0 且产出 exe**；ucProgressCircular 仍红，但错误类严格是基线的**子集**（16xC2059 + 2xC2065 + 1xC2084 + 25xC2198 -> 1xC2065 + 1xC2084 + 12xC2198；C2059 归 #188，剩下两条正是 B29③④，不是这一刀）。**护栏**：A/B = 40 工程 x 两架构（80 份快照，BASE=`4d1dbf45`）=> 只有 6 份被改（ucChartBar 两片各 8 对、ucProgressCircular 两片各 1 对，ve_units 两片是手改夹具那两份），每条差异行逐行打印归因、三形都属"数组元素的成员访问从 COM 兜底挪到 UC 实例直发"，其余逐字节相同、OFFENDERS 0。**这一刀没修好的那半**：extender 属性（`uArr(i).Left` 那形仍然段错误）另立**账 #193**；另外"单枚那条链"在整面 80 份快照里**没有一处**走带尾巴的写（基线 172 处 Pattern C/D2 全是空尾巴）=> 那条形状针欠着，别以为两边都钉过了。
· **⑦ `ucTreeMaps`：#186 把 parser 那一格清掉之后，红移到了 cl 阶段**（账 #192）—— `--emit-c` 现在零诊断（rc=0），真编译 x86 报 `PropPagFMR.c` 里 **10× C2039 + 2× C2059 + 1× C2198**（物证 `.build/b186tm/c3-error.log`）。两族根因不同，别并成一刀：**①** `hDC` / `CurrentX` / `CurrentY` / `ScaleMode` / `TextHeight` / `Clear` 全部报 `"X" 不是 "HWND__" 的成员` —— 伪成员/方法被发成 HWND 的结构体成员访问，与账 #159（宿主伪成员收成一张表）、账 #174（PropertyPage 的 SelectedControls 裸名调用）**同源**：那张表里 PropertyPage 这一型没登全。开工先读 `scripts/check_host_pseudo_table*.ps1` 两条既有哨兵，把缺的名一次性补进那张表，**别在发码侧再加一层 if**。**②** `C2059 语法错误 ";"`（两处，发空了的语句）+ `C2198 vb6_di_TextOutW 用于调用的参数太少`（Declare 桩参数数不对 —— 记忆库口径：手写一枚 `vb6_di_` 单桩，别重跑 `gen_di_stubs`）。另记一条工具事实：**ucTreeMaps 不在门禁里**（#187 的升格还没做），所以这格的读数只能靠本地真编译，别假设 CI 会跑到它。
  **→ ⑦ 的两条落点已经量到（2026-10-04，x86 真编译，物证 `.build/b192new/ucTreeMaps/c3-error.log` = 11xC2039 + 2xC2059 + 1xC2198），并且 ⑦ 原来那句"再补一张表"要订正**。①**C2039 那一族不是"PropertyPage 伪成员没登记"，是 With 块里那枚控件**：源 = `PropPagFMR.pag:256-266` 的 `With Picture1`（VB.PictureBox，用 `.hDC / .CurrentX / .ScaleMode / .CurrentY / .TextHeight`）与 `pag:618-619` 的 `With lstFonts`（VB.ListBox，`Call .Clear`）。With 前言把 FormControl 那档发成 `HWND _vb6_with_N = (HWND)vb6_hwnd_<控件>`（`cgen_with.cpp:134-143` + `:766`），而成员那一路（`cgen_expr_with.cpp:25-90`）**从不查宿主伪成员表** —— 表在 `cgen_util_com.cpp:469-540`（`kHostPseudoRows`，键 = 宿主种类 usercontrol/propertypage/extender/ambient + 成员名，`hostPseudoFind` :542），它只试 `getControlPropReadFn`（`cgen_util_ctrl.cpp:247`，里面有 scalewidth/scaleheight 但没有 hDC/CurrentX/CurrentY/ScaleMode/TextHeight），全落空就撞上 `cgen_expr_with.cpp:88` 那条兜底 `lastExpr_ = tempVar + "." + cIdent(member)` ⇒ 拿 HWND 当结构体拼成员。`Clear` 同形：零实参方法表 `controlZeroArgMethod`（`cgen_util_ctrl.cpp:1414-1445`）只登记了 setfocus/clearsel。⇒ 这一格真正的问题是「With 里那枚控件的宿主/复刻成员从哪张表读」，与 #159 那张表同源但**键的维度不同**（表按宿主种类，With 这条路按控件类型）=> 修法要把两处收成一处口径，不是在 With 里再加一条 if。②**C2059 那两条是独立的小事**：`Optional ... = 0&` 的默认值文本被原样拼进 C —— `evalOptionalDefault`（`src/semantics/semantic_analyzer_util.cpp:382`）是字面量发码权威 `visit(LiteralExpr)`（`src/backend/expr/cgen_expr.cpp:52-93`，Long 走 `0L`）之外**另写的一份**：Long/Integer 那支直接 return `lit->rawText`（:390-392）=> 词法留在 rawText 里的 VB 类型后缀 `&`/`%` 漏进 C（Double 那支倒是剥了 `!`/`#`，:425）。所有 `_has_*` 补参的消费者（`cgen_call.cpp:544`、`cgen_expr_call_opt_pad.inc:11`、`cgen_util_classcall.cpp:606` 等）共用这一个串 => **一处修，八处齐**。③C2198 `vb6_di_TextOutW` 参数太少 = Declare 单桩那一族（照既有口径手写一枚 `vb6_di_` 桩，**别**重跑 `gen_di_stubs`——它按当次 session 整文件重写会撤掉别人的桩）。
  **→ ⑦ 的开工家底（同一轮只读盘点，file:line 都核过；结论 = 这一格要拆两半，别当一刀做）**。**已有、只差接线**：①`vb6_GetCurrentX/vb6_SetCurrentX/vb6_GetCurrentY/vb6_SetCurrentY` 已在 `src/rtl/core/vb6forms/vb6forms_widget_prop.c:268/279/286/297`（声明 `vb6forms_prop_form.h:79-82`），**按 HWND 存窗口属性 `VB6_CurrentX/Y` ⇒ 任何控件都能用**，可 `src/backend/` 里**零引用** = 全没接；②ScaleMode 的 getter 同在 `vb6forms_widget_prop.c:256`，同样没接；③`vb6_ClearList` 在 `vb6forms_list.c:114`（LB_RESETCONTENT），而 `controlZeroArgMethod`（`cgen_util_ctrl.cpp:1415-1447`）只登记了 setfocus/clearsel 两条 ⇒ **加一行 `clear` 就把 With 那条与语句那条两个头一起接上**（两条头各自 key 在 `knownFormControls_` 上：`stmt/cgen_call.cpp:359-370` 与 `detail/expr/cgen_expr_call_callee_withm.inc:306-355`）。**还不存在、要新造**：④控件的 `.hDC` 没有任何导出口（`_Paint` 那套两层规则已经在 `cgen_form_wndproc_subclass.inc:376-395`：BeginPaint → SetPropW "VB6_PaintDC" → RemoveProp，消费侧 `vb6forms_ctrl.c:674-677` 带 GetDC 兜底 —— 但它现在是 static，`.hDC` 正好复用这一条，不用新发明）；⑤控件/窗体的 `TextHeight`/`TextWidth` 只有 UserControl 版（`vb6rtl_com.c:731/744`、`uc_host.c:882/886`），没有按 HWND 的；⑥带 HWND 参数的 `ScaleX`/`ScaleY` 也没有（只有 `vb6_UserControl_ScaleX/Y`，`vb6rtl_com.c:804/810`，无 HWND），但换算的零件是现成的：`vb6_ScalePxToUser`/`vb6_ScaleUnitsPerPx`/`vb6_WindowScaleModeSelf`（`vb6forms_window.h:58-67`）。**表的缺口**：`kHostPseudoRows` 里没有 picturebox/image/listbox/form 那一档，`CurrentX`/`CurrentY` 在任何一档都没有；`propertypage` 那档只有 changed/hwnd/scaleheight/scalemode/selectedcontrols 五条。⇒ 开工顺序建议：先把「With 那枚控件的接收者认回控件身份」这一条接通（它一处解三形：CurrentX/CurrentY/ScaleMode + Clear），再单独立一格做控件的 DC 与文本量度（那是要往 RTL 加功能的）。
  **→ ⑦ 的一半已出（提交 `a11a7da8`，门 #320（run 37228796120，head 42e4338f，attempt 1）= 11 job 全绿、9 片日志各自 FAIL=0，其中 `[VBP] piccur` / `piccur_x86` / `with_ctrl_cursor_clear` 三行在 CI 上真 PASS）= 账 #192 的接线那半**。读数是自己复算的：`_vb6_with_0 = (HWND)vb6_hwnd_Picture1`、`_vb6_with_11 = (HWND)vb6_hwnd_lstFonts` ⇒ 撞兜底的接收者就是**一枚普通控件被 With 包住**，不是 PropertyPage 宿主（⑦ 原来那条"宿主伪成员"的定性只对了一半：表是按宿主种类建的，而 With 这条路本来就先查 `getControlPropReadFn` 再查 `controlZeroArgMethod` —— 两张表都齐，**只是档位里没有那几行**）。改 = 补表：PictureBox 的 currentx/currenty（读表 + 写表成对补）、`controlZeroArgMethod` 加 clear（ListBox/ComboBox），三条都接在 RTL 早就有的出口上（`vb6_GetCurrentX/Y`、`vb6_SetCurrentX/Y` 按 HWND 存窗口属性；`vb6_ClearList` = LB_RESETCONTENT）。**刻意不给成通用行**：VB6 的 CurrentX/CurrentY 只在画得上去的那几枚上有，给成通用 ⇒ `List1.CurrentX` 也答 0 = 伪造成功。判据：新夹具 `tests/piccur` 两片三头钉（写了读得回 + **另一枚没被写坏**（RTL 按 HWND 存，写成全局一份就露馅）+ 清空前确实是 2，拦住"Clear 什么都不做、本来就 0"那种假绿）；**负控 = `40bbef67` 那台真编同一份夹具** ⇒ PicForm.c 150/151/160 三条 C2039、rc=1。形状针 `with_ctrl_cursor_clear`（Present 直发出口、Absent 是 `tempVar + "." + 成员名` 那一形）。真工程读数（x86, ucTreeMaps）：C2039 **10→4**（剩 hDC / ScaleMode×2 / TextHeight）+ C2198 1；TextHeight 那行在光标通了之后换了个红法（C2039 → C2440 "BSTR→float"），根因同一个。护栏 A/B = 80 份（BASE = `a7524ab7` 那台）⇒ 被改的产物 0 份。剩下的那一半见 §B31/§B32。


### B30 `3%`（Integer 类型后缀）在词法层就被拒（账 #195，2026-10-04 一次顺带测量量到的）

`Public Sub S(Optional ByVal a As Integer = 3%, Optional ByVal b As Long = 0&)` 报
`error VB1005: 十进制数字超出 64 位整数表示范围` 并带一整片级联（VB2004 / VB2001 / VB2002 / VB2003 共 13 条），
而**同一位置**换成长整型后缀就全过 —— 所以这不是"后缀都不支持"，是 `%` 那一支单独断了。读数 =
`.build/p194/m_*.bas`（`C3.exe <file> --syntax-only`，一形一条）：

| 声明里的默认值形状 | 诊断条数 |
|---|---|
| `Optional ByVal a As Long = 12&` | 0 |
| `Optional ByVal a As Long = &H10&` | 0 |
| `Optional ByVal a As Long = 12` | 0 |
| `Optional ByVal a As Integer = 3%`（就这一形）| 13 |

**上一轮的编译器（`ea3eeeec` = 门 #317 那台）读数完全相同** ⇒ 与账 #194 那一刀无关，是存量缺陷。
发现路径也是它带的：#194 的夹具本来要摆三形后缀，`3%` 那条在 parser 就死了，于是夹具改用小写无后缀的 `3`。

落点候选（**还没证**，开工第一步就是证它）：`src/lexer/lexer_number.cpp` 的类型后缀 switch 里
`case `%`: text += advance(); break;` —— 这一支不置任何标志（`&` 置 isLong、`!`/`#` 置 isFloat），
于是带着 `%` 的串落回"无后缀十进制按数值大小定档"那一段。开工前先把两条测过再定范围：
`Dim y As Integer: y = 3%`（普通赋值位置）与 `Print 3%`，
确认是"整条 `%` 都不支持"还是"只在参数默认值里断"。

**→ §B30 已出（提交 `963c5e71`，门 #319 全绿 —— 明细见 §D 那一行）**。落点候选那条**证完了，方向对但差一处**：不只是参数默认值那一段 —— `y = 3%` / `&HFF%` / `&O17%` / `&B101%` / `-4%` / `3% + 1` / `= 3%` 七形在修前**全红**（`.build/p195_*.bas`，`--syntax-only`），而 `12&` / `&H10^` / 无后缀三形一直通（证人：排除"整条后缀机制坏了"那种误读，也排除"`^`/`&` 一起坏"）。根因是 `case ` + pct + `: text += advance(); break;` 这一支**只吃字符不置标志**（`&` 置 isLong、`!`/`#` 置 isFloat、`^` 置 isLongPtr，四条里只有 ` + pct + ` 没置），加上 radixDigits 的剥离表里也没有 ` + pct + ` —— 于是带着 ` + pct + ` 的串落回"无后缀十进制按数值大小定档"那一段，parseIntLit 看见残留的 ` + pct + ` 就报"超出 64 位"。**一条规则抄了四份**（十进制 + &H + &O + &B）再加一张剥离表，漏的就是其中一份 ⇒ 修法四处一起齐，并配哨兵 `scripts/check_int_suffix_sites.ps1`（S1 置标志/无空支、S2 剥离表同时带 ^ 与 %、S3 四个消费点各认一次 %（并同时数 & —— 四份拷贝一分家就报）、S4 显式 % 的 16 位守卫还在）；假 needle 验红：把 % 支改回空支 + 剥离表去掉 % ⇒ S1/S1/S2/S3 四条一起点名。**边界**：显式 % 超出 -32768..32767 报词法错，不按 int32 收下再让 int16 去截（那是把值改错：2147483648 静默回绕成 -2147483648）—— 配一条负例钉住。顺带把 #194 那枚 optdef 夹具先前为绕开本缺陷改成的无后缀两处（`3%` / `5%`）**恢复原样**，x64 与 x86 都真跑到 OD-DONE。护栏 A/B = 80 份（BASE = `40bbef67` 那台）⇒ 被改的产物 0 份：整面存量里一处 % 都没有（有一条就压根编不过），所以它又只能靠自己的夹具响 —— 与 #188/#194 同一课，连着三条了。

### B31 控件的绘图面缺一半：`.hDC` / `TextHeight` / `TextWidth` / 带 HWND 的 `ScaleX`·`ScaleY`（账 #196）

⑦ 剩下的四条 C2039 全在这里。**不是接线问题** —— 后端两张表今天没有可指的名字，因为 RTL 里没有：

- 控件的 `.hDC`：没有任何导出口。可复用的规则已经有了 —— `_Paint` 那套两层（`cgen_form_wndproc_subclass.inc:376-395`：BeginPaint → SetPropW "VB6_PaintDC" → RemoveProp，消费侧 `vb6forms_ctrl.c:674-677` 带 GetDC 兜底），但它现在是 static。晚绑定那一路的 `hDC → GetDC` 在 `uc/detail/uc_hostmodel_getprop.inc:86`。
- 按 HWND 的 `TextHeight`/`TextWidth`：只有 UserControl 版（`vb6rtl_com.c:731/744` + 静态 `vb6_uc_measureText:693`，按实例走 `uc_host.c:882/886`）。签名可以直接照 `vb6_UC_TextHeightOf(void* inst, BSTR text)` 换 HWND。
- 带 HWND 参数的 `ScaleX`/`ScaleY`：只有 `vb6_UserControl_ScaleX/Y`（`vb6rtl_com.c:804/810`，无 HWND），而裸名那条改写还挂在 `isDesignerModule_` 上（`cgen_expr_ident_builtin.inc:215-232`）⇒ `.pag` 里的裸 `ScaleX(...)` 认不出来。换算的零件是现成的：`vb6_ScalePxToUser` / `vb6_ScaleUnitsPerPx` / `vb6_WindowScaleModeSelf`（`vb6forms_window.h:58-67`，见 §B32 那条 ScaleMode 的坑）。

开工顺序建议（2026-10-05 订正）：hDC 与 TextHeight/TextWidth **两条都已出**（门 #323 / #324），ucTreeMaps 的 UnicodePrint 编译面因此全清 —— 但"整条通了"这句要说得更准：它现在停在链接期缺三枚桩（§B36/#201），而页里的控件压根没被创建（§B34/#199）也还没解。剩下的：带 HWND 的 `ScaleX`/`ScaleY`（与 §B32 那条 ScaleMode 同源，那条已通，所以这一条现在做得对了）、#199 的甲/乙口径（页从来没被创建 ⇒ 页里的绘图面至今白画）、新记的 §B37/#202（Frame 标题带按默认字体量）。另：ucTreeMaps 的第一趟真跑已量到 x64 启动期 AV（新账 §B38/#203，x86 是好的）。再一条今天量出来的新账 §B39/#204（**没被写过字体**的控件整张 Font 面读空、文字量按系统默认字体算 —— 读数是 `bName= bfs=0 bpf=0 bth=16`，改前设计期那张 18pt 现在读得到）：它与 #202 同一处出口的覆盖面，开工顺序上 #204 排在 #202 前面（它有产品后果：默认字体那批控件的排版与文字量现在是错的）。

**→ 本轮（2026-10-05）过后订正两句**：① §B31 里那条 `.ScaleMode` 不属于本账，它是 §B32（账 #197）的读表那一半，已随 #197 出掉 —— **ucTreeMaps 的 C2039 实测 4→2**，剩下的两条就是 `hDC` 与 `TextHeight`（+ C2198/C2440 各 1，同根）。② 做 §B32 的时候顺带量出一条**更大的一格**（见 §B34，账 **#199**）：`.pag` 的设计块控件**从来没被创建** —— 同一份 emit 里 PropPagFMR 那一段 `vb6_CreateControl` **0 处**、同工程 Form1 那一段 **14 处**，页里的 `vb6_hwnd_Picture1` 是 `#define ... (*vb6_UC_DesignSlotOf(me, "Picture1"))` 而那个槽位按需新建、初值 NULL（`uc_host.c:641-652`）⇒ 属性页里 `With Picture1` 打的是一枚空句柄。**这条不修，本账剩下的两条做完 UnicodePrint 也还是白画** —— 接下去的开工顺序改成：#199 → #196。

**→ hDC 那一半已出（2026-10-05，提交 `50676a8b`，门 #323 = run 37238929955、head f26f9590、attempt 1，11 job 全绿：10 片各自 FAIL=0（含先前被本机代理拦成 HTML 的 smoke / compile 两片，重下到了）；本轮五条新用例在 CI 上逐行真 PASS —— `[VBP] dcsurf`(vbp#3) / `[VBP] dcsurf_x86`(vbp#4) / `[CODEGEN-NOTE] dcsurf_dc_shape`(syntax) / `[CODEGEN-NOTE] dc_read_real`(syntax) / `[STATIC] control_dc`(compile 片，那一片共 13 行 [STATIC]）**：口径**不新写**。RTL 里「这枚控件的绘图 DC 从哪儿来」早就有一处（`vb6forms_ctrl.c` 的 Print/Cls 在用那两条），这一刀把它抽成 `vb6_ControlDrawDC` 让 `.hDC` 也走它，再补上「句柄交得回 VB 代码」那一半 —— `intptr_t vb6_GetControlHDC(void* hwnd)`：派发期那张直接交出去（既不缓存也不 ReleaseDC），否则按 HWND 缓存进窗口属性 `VB6_ObjectDC`（**VB6 是一个对象一张**，反复读必须读回同一个值；不缓存就是每读一次漏一张），白拿的那张当场 `ReleaseDC`；归还点在 PictureBox/Image 那层自己的 `WM_DESTROY`（`vb6forms_picture_prop.c:307-308`，#185 那套分层槽位）。后端两张表各补一行 `hdc`（PictureBox 与 Form，**刻意不给通用行** —— 与 #192 那条 CurrentX 同一个道理：`List1.hDC` 答一个数就是伪造成功）。RTL 动过 ⇒ touch `c3rtl.rc` 重编 C3.exe（#156 那条旧资源坑）。
判据四面：新夹具 `tests/dcsurf` 两片四头钉（DS01 同一枚反复读 + With 那一形三个数彼此相等且非零 / DS02 两枚互不相等 / DS03 `GetDeviceCaps(hDC, LOGPIXELSX) > 0` 问的是 GDI，证明交回来的是一张**活的 DC** / DS04 `GetPixel` 各自等于**自己那枚**的设计期底色；DS05 只钉前缀 —— dpi 是函数的数，不钉绝对值）；**负控 = 改前那台真编同一份夹具** ⇒ DcForm.c(167) error C2039 "hDC" 不是 "HWND__" 的成员、BUILD rc=1、一条读数都不出；形状针两条（夹具 `dcsurf_dc_shape` + 真工程 `dc_read_real`，Absent 里连**晚绑定那一形** `vb6_ComGetLongPtrProp(X, L"hDC")` 一起钉住）；哨兵 `scripts/check_control_dc.ps1` D1~D5，五条假 needle 全按"人会怎么改坏"植（绕过权威自己 `GetDC` / 权威不再认派发期那张 / 白拿那张不归还 / 销毁时不撤名 / 表里给成通用行）逐条能红，还原后全绿。
**护栏**：A/B 86 份（BASE = 改前那台 `cp`，43 工程 × 两架构）⇒ 被改的产物 8 份、**OFFENDERS 0**，逐行归因 = 14 对 K2（`X.hDC` / `ComGetProp(X, L"hDC")` → `vb6_GetControlHDC(X)`）+ 5 条 WD（`warning VB4001: P17.1: Unknown control property '.'hDC' in With block` 消失 —— 这条没了正是这一刀的目的，属性认识了不该再报不认识）。**顺带一条实测订正（差点按类推写错）**：Charts2020 的 `Proyecto1`/`ucProgressCircular` 里 `Picture2.hDC` 以前走的是晚绑定那条 `vb6_ComGetProp(X, L"hDC")` —— 我原本按 #143/#191 那族"原生控件没 IDispatch ⇒ 恒答 0"去推它，**探针量出来不是**：`.build/b196probe/`（一次性夹具，不进 tests/）用**改前那台**真编真跑裸形 `picA.hDC`，得 `PB01-HDC=671159075`、`PB02-DPI=96` —— 那是一张**活的**窗口 DC（`uc/detail/uc_hostmodel_getprop.inc:86` 里晚绑定那条本来就认 hDC）。所以本刀在裸形上改的不是"答 0"，是**语义**：VB6 是一个对象一张，那条兜底每读一次新取一张、也从不归还；现在两形同归一处出口、反复读回同一个句柄（DS01 钉的就是这个），并在窗口销毁时归还。**真红的那一半是 With 形**（`With Picture1 : TextOut .hDC` = C2039，编不过）。
**真工程配对读数**：ucTreeMaps x86 的 C2039 **2→1**（只剩 `TextHeight`；那条 `TextOutW` 的 C2198 本是 hDC 的级联，跟着一起消失）。
**欠的两条 + 本刀留下的一条口径**：按 HWND 的 `TextHeight`/`TextWidth`（要一枚"一个实参的控件方法"新机制 + 两形码头）、带 HWND 的 `ScaleX`/`ScaleY`（裸名改写仍挂在 `isDesignerModule_` 上，`.pag` 里的裸名认不出来）；另 —— **Form 自己那张缓存 DC 目前没有归还点**（归还只在 PictureBox/Image 那层的 `WM_DESTROY`）。语料里 `Form.hDC` 现存 **0 处**（本轮 86 份产物里没有一份因它而变），窗口销毁漏一张、进程结束由系统收回 ⇒ 不当成本轮的红，但记在这里，等 #199 那条属性页的路一起收。**顺序照旧：#199 → #196 剩下的两条**。

**→ 同账第二条（按 HWND 的 TextHeight/TextWidth）也已出（提交 `060eddca` + 针面订正 `ac3df329`，门 #324 = failure **红在我自己的两条新针**、门 #325 = run 37243854108、head ac3df329、attempt 1 = 11 job 全绿：10 片各自 FAIL=0，本轮七条相关用例在 CI 上逐行真 PASS —— `[VBP] dcsurf` / `dcsurf_x86`、`[CODEGEN-NOTE] dcsurf_dc_shape` / `dc_read_real` / `dcsurf_text_measure` / `text_measure_real`、`[STATIC] control_dc`）**：不需要新机制的"发明" —— 现有 `controlZeroArgMethod`（`cgen_util_ctrl.cpp:1452`）那套**表交名字、码头拼实参**的协议本来就支持带实参：With 形交裸名 + `pendingChainObj_`，由 `cgen_expr_call_com_bind.inc:842` 收下、`cgen_expr_call_opt_pad.inc:74-78` 把句柄**前置**到实参表前面 ⇒ 新表 `controlOneArgMethod` 只登记 `textheight`/`textwidth` × Form/PictureBox 两档（**依旧不给通用行**），两条码头各查一次（With 形 `cgen_expr_with.cpp`、带括号裸形 `cgen_expr_call_com_bind.inc`）；实参签名表补两行 `{"void*","BSTR"}`（Fix 113 那一味：漏了就等于把 vb6_VARIANT 裸喂给 `GetTextExtentPoint32W`）。RTL 那头新增 `vb6_ControlMeasureTextPx`（DC 走**同一处** `vb6_ControlDrawDC`、字体走 `WM_GETFONT` = 与 Print 同口径）+ 两个出口 `float vb6_ControlTextWidth/Height`（float = VB6 的 Single，别让生成 C 去做 double→float 收窄），**交回的单位过 `vb6_ScalePxToUser` + `vb6_WindowScaleModeSelf`**（#175/#197 那一份单位表，缇型对象上交像素就是 #177 那一味）。
判据：`tests/dcsurf` 加长 —— TH01 两形逐数相等且非零（With 与裸形不同归一处就是一头 0）、TH02 **比值**判据（同字体的缇框 vs 像素框 >4 倍；实测 16px vs 240 = 15 倍，不钉绝对数所以 DPI 变了不假红）、TH04 宽度随文字变（拦住"恒答一个常数"）；TH05/TH06 是只钉前缀的读数行。负控 = 改前那台真编同一份夹具 ⇒ rc=1、无 exe。哨兵 `check_control_dc.ps1` 加了 D6~D9（五条假 needle 逐条能红：单位不折算 / With 那条码头断 / 表少一档 / 出口返回档改回 double / 别处手拼发码 ⇒ 全红，还原回绿）。**顺手抓到哨兵自己一条假绿**：D9 第一版把 `vb6_ControlTextWidth` 与 `vb6_ControlTextHeight` 当**一段**正则来抓，改坏宽度那半时高度里的 `vb6_ScalePxToUser` 还在 ⇒ 整段照样过；改成"一个一个函数各自取身体"后那条 needle 才真的红（同 #197 的"注释行不算"、"计数要打印实测值"，是同一类自欺）。
护栏：A/B 86 份（BASE = 这两刀之前那台）⇒ 被改 8 份、**OFFENDERS 0**；逐行归因 = 32 对读法替换（hDC 与 TextHeight/TextWidth 都算 K2，配对正确性用**整行**判 —— 公共前缀会把 `Co` 这种片段折掉，残段里搜全名搜不到，这是本轮第二课）+ 8 条 WD（VB4001 那三条诊断消失）。真工程配对：ucTreeMaps 两台（x64/x86）的 C2039/C2440 **全部消失**，`VB4001 Unknown control property` 只剩 1 条（`ScaleX/ScaleY` 那一族另计），红点从"编不过"推进到 **LNK2019 ×3**（`AddFontMemResourceEx` / `GdipNewPrivateFontCollection` / `GdipPrivateAddMemoryFont` 三枚 DI 桩没登记，见新账 §B36/#201）。
**本刀没接的两处（记下不装绿）**：① 不带括号那条语句形（`picA.TextHeight "x"`，`cgen_call.cpp` 那一支）刻意没接 —— 语料 0 处，接它要先证明那条路上实参表怎么交；② 窗口字体那条判据今天**当不了判据**（见新账 §B35/#200）。

### B32 `VB6_ScaleMode` 在整个 RTL 里没有任何人写它（账 #197）

`grep -rn "VB6_ScaleMode" src/rtl src/backend` 的全部命中只有 setter 自己（`vb6forms_widget_prop.c:265` 的 SetPropW）与两条读点，**0 个调用者**。⇒ 谁去读控件或窗体的 `ScaleMode` 都只会拿到缺省 1，跟 .frm/.pag 里写的设计值无关；所有按单位换算的路径（ScaleX/ScaleY、缇/像素互转）因此都建立在一个恒为 1 的数上。这与 #154/#160 同族（设计期属性从没下发到窗口），但影响面更大：它决定的是**量出来的数对不对**，不是某一枚控件的外观。
开工第一步：先量"设计期 ScaleMode 有没有进过 me->properties 表"（`.frm` 里 `ScaleMode = 1` 那行是 Form 级的，控件级只有 PictureBox 一类容器有），再决定下发点落在创建那两路的哪一处（同 #83/#151 那条"两条创建路都要打"）。

**→ 已出（提交 `9565bdbb`，门 #322（run 37235573381，head ada93533，attempt 1）= 11 job 全绿、11 片日志**全部下到**（这轮代理没拦）：10 片各自 FAIL=0（第 11 片是 Build C3.exe，不打这个计数），本轮五条新用例在 CI 上逐行真 PASS —— `[VBP] scalemode` / `scalemode_x86` / `[CODEGEN-NOTE] scalemode_design_write` / [CODEGEN-NOTE] scalemode_read_real` / `[STATIC] scalemode_writers`）**。读数与做法：
- **RTL 一个字节没动** —— `vb6_GetScaleMode` / `vb6_SetScaleMode` 早就在（`vb6forms_widget_prop.c:256/263`，声明 `vb6forms_prop_form.h:77/78`，由 `vb6forms.h` 传递可见），缺省 1=缇。这格从头到尾是**发码侧没人调用**，不是运行时缺出口 ⇒ 不 touch `c3rtl.rc`，也就绕开账 #156 那条「RTL 嵌在 C3.exe 资源里，探针测的是旧 RTL」的坑。
- **普查**（`.build/b197_census.py`，93 份 .frm/.pag/.ctl 设计块）：19 处 ScaleMode —— UserControl 10 处（9×3 + 1×1，**已由 .ctl 注册那条路接走**，`cgen_form.cpp:393-436` 把它交给 `vb6_UC_WindowScaleMode`）、PictureBox 6 处（全 3）、Form 3 处（1×1 + 1×3）。⇒ 缺的就是 PictureBox 与 Form 这两档，而 VB6 的设计器**只在这三类块里写这一行**，所以口径收成「设计块写了就发」：不按值筛（声明 1 与没声明在产物里要分得开），也不再另开一张控件型白名单（Frame 那一类根本不写这行，写了就是给人读的）。
- **一处权威 × 三条落点**：新增 `CCodeGen::emitDesignerScaleModeProp`（`cgen_util_ctrl.cpp`，紧挨 #154/#160 那两个同型出口），三条路各调一次 —— 顶层 `cgen_form_ctrl_style_apply.inc`、容器子控件 `cgen_form_frame_menu.inc`（账 #83/#151 那条「两条创建路都要打」）、窗体自己那档 `cgen_form_wndproc_create.inc` 的 P20-40 块。
- **读写成对**：两张表各补 `scalemode` 两行（PictureBox 与 Form）—— 读给 **`vb6_WindowScaleModeSelf`**，也就是 #175 那张单位表的同一处，程序读到的数与几何换算用的数**不可能分家**；写给 `vb6_SetScaleMode`。以前 `Picture1.ScaleMode` 走的是 COM 兜底（把 HWND 当 IDispatch 问属性 ⇒ 交回 Empty）。
- **判据四面**（新夹具 `tests/scalemode`，x64+x86 两片）：SM01 三处设计值各自读回（`SM01-MODE=3/3/1`）；SM02/SM03 **问窗口** —— 同尺寸的缇框与像素框，ScaleWidth 与框内按钮 Left 必须差一个单位比（本地实测 1170 vs 78、120 vs 8 = 15 倍 @96dpi；针面只取 `>3` 这个**比值**，不钉绝对数，DPI 变了不假红）；SM04 运行期切档后两枚**逐数相等**（证明这数是活的，不是创建时烘死的）；SM05 另一枚没被写坏（RTL 按 HWND 存，全局一份就露馅，同 #192 双向钉）；SM06/SM07 **第二条创建路**（Frame 里那枚 PictureBox）也发了这一句。**负控 = 改前那台真编同一份夹具**：`SmForm.c(166)/(182)` 两条 C2039 "ScaleMode" 不是 "HWND__" 的成员 + C2198，rc=1、一条读数都不出（物证 `.build/b197out/b64/c3-error.log`）。
- **形状针两条**：`scalemode_design_write`（夹具：Present 三句 SetScaleMode 与 `vb6_WindowScaleModeSelf(_vb6_with_`，Absent 三形 HWND 取成员）+ `scalemode_read_real`（钉在**真工程** ucTreeMaps 那一行上）。两条都在改前那台上当场红（缺 4/缺 1 + 漏 1），改后绿 —— 见 `.build/b197_needle.txt`。
- **哨兵** `scripts/check_scalemode_writers.ps1`（W1 手拼发码点=1 / W2 权威一次定义且仍读设计块那条属性 / **W3 三条落点各≥1 且注释行不算** / W4 读写成对≥2 each / W5 RTL 里写 `VB6_ScaleMode` 这个属性名=1、读≥1）；假 needle 验红：**把容器那条落点注释掉** ⇒ W3 点名、exit 1。**这轮自己踩的工具坑两条**：① 第一版 W3 用整篇正则，把调用点注释掉**照样绿** —— 植针要按「人会怎么改坏」植（注释掉/删掉），我先前植的是改个 `XX` 前缀，`Contains` 当子串照样命中，等于没验；② PASS 消息里写死 `W1=1 W3=3` 会掩盖真数，改成打印算出来的计数。
- **护栏 A/B** = 82 份（41 工程 × 两架构，BASE = 改前那台，`.build/b197_base/C3.exe` 用 cp 不重建）⇒ **12 份被改，OFFENDERS 0**：10 份各多 1 行 K1（ctrltabindex / czUI / piccur / tabwalk / ve_units，就是普查里那 5 个声明了 ScaleMode 的工程）；2 份各改 2 行 K2（VBFlexGridDemo 两架构）—— 那是 `Me.ScaleMode` 以前发成 `vb6_ComGetProp(vb6_hwnd_MainForm, L"ScaleMode")`（**把 HWND 当 IDispatch 问属性 ⇒ 交回 Empty**），现在答得出数。逐行归因见 `.build/b197_ab.txt`。
- **真工程读数**：ucTreeMaps x86 的 C2039 **4→2**（剩 hDC / TextHeight = #196）。**czUI-main**（语料里唯一在 **Form** 级声明 3=Pixel 的工程，几何读数真的会被这一档挪动）真编真起窗：`CZUI_BUILD=0` / `CZUI_ALIVE`（`.build/b197_gui.bat`）。
- **留给下一轮**（这一行写于 #197 收线时，2026-10-05 当天 #196 的 hDC 那一半已随门 #323 出掉 ⇒ 剩下改成）：#196 欠按 HWND 的 TextHeight/TextWidth（要一枚"一个实参的控件方法"机制：现有那张 `controlZeroArgMethod`（`cgen_util_ctrl.cpp:1452`）有三条码头 —— With 形 `cgen_expr_with.cpp:72`、语句形 `cgen_call.cpp:400`、带括号裸形 `cgen_expr_call_com_bind.inc:80`，新机制同样要三条都接）+ 裸 `ScaleX/ScaleY`（那条改写仍挂在 `isDesignerModule_` 上，`.pag` 里的裸名认不出来）；以及 §B34 那条**新账 #199**（甲/乙口径待拍）。

### B34 `.pag` 的设计块控件从来没被创建（账 #199）

做 §B32 的时候顺带量出来的，比 §B31 剩下的两条更底层：同一份 `--emit-c` 里 **PropPagFMR 那一段 `vb6_CreateControl` 0 处，同工程 Form1 那一段 14 处**。页里那枚 `Picture1` 的句柄是
`#define vb6_hwnd_Picture1 (*vb6_UC_DesignSlotOf(me, "Picture1"))`（`cgen_form.cpp:207-212` 那条 UserControl 内嵌控件的路数），而 `vb6_uc_designSlotIn`（`uc_host.c:641-652`）是**按需新建、初值 NULL** —— 没有任何人往里写。⇒ 属性页里 `With Picture1 : .hDC / .CurrentX / .ScaleMode / .TextHeight` 全打在 **NULL 句柄**上：编得过（#197 之后连 C2039 都不报了）、发码对、运行期一句不响。
**这条不修，#196 做完 ucTreeMaps 的 UnicodePrint 也还是白画** —— 所以控件线的顺序改成 #199 → #196。
**已量到（2026-10-05，读码，未动产品）**：那三条"先量"的问题一次答完 ——
① `.pag` **解析没问题**：`driver_frontend.cpp:172` 对 `.pag` 与 `.frm` 走同一个 `FrmParser::parse`，`frm_parser.cpp:485-487` 认根块 `Begin VB.PropertyPage`、`:371-372` 递归收子控件 ⇒ 页里的 `Picture1` **在 `frmDesc.formControl.children` 里躺着**（那条 `#define` 本身的存在就是证据）。
② 分岔在**发码的模块种类那一问**：`cgen_base_generate_decl_pass.inc:39-46` —— `if (module.isFormModule && frmDesc) emitFormFramework(...); else if (frmDesc) emitDesignerControlDecls(...);`。而 `isFormModule` 只对 `.frm` 置真（`driver_frontend.cpp:161-163`，`.pag` 只被折进 `isClassModule`），`isDesignerModule_ = frmDesc && !isFormModule`（`cgen_base_generate_prologue.inc:15`）。`vb6_CreateControl` **只在 `emitFormFramework` 的片段里出现**（`cgen_form_create_controls.inc` → `cgen_form_ctrl_style_apply.inc:339`，容器子控件在 `cgen_form_frame_menu.inc:132`）⇒ 页压根没进那条路，`emitDesignerControlDecls`（`cgen_form.cpp:170-213`，注释自己写着"不发射窗体窗口框架 … 子控件句柄保持 NULL"）只发槽位宏与名字登记。
③ `.vbp` 侧另有第三档：`VbpSourceType::PropertyPage`（`vbp_parser.cpp:67`）。
⇒ 定性：**这是 (b) 一条模块种类的发码分支，落成 (c) 一个没接完的架构决定** —— 那句"槽位由宿主窗体写"对 `.ctl`（被窗体的 CreateControls 填）成立，对**独立一页**不成立，而属性页的宿主/装页 runtime 本仓没有。两条候选路摆在这里：**甲** = 页也走 `emitFormFramework`（跟 .frm 一样发创建 + 句柄赋值，代价是页要有一枚真窗口/承载体）；**乙** = 保留槽位语义，补一个"页装好时把创建出的句柄写进槽"的宿主（改动小、语义正，但要先回答"谁创建页的窗口"）。语料侧的事实：`PropPagFMR` 在整个 VbEclipse 里**只被 .vbp 列了名，没有任何一处实例化** ⇒ 这一格不做完，页里的绘图面永远是白画，但它也不挡别的工程。开工前先拍甲/乙这条口径。判据照 #83/#151/#160 那条纪律：**两头钉**（新格有没有真的发创建 + 旧格有没有被抢），别只看编译过没过 —— 这一格"编得过"今天就已经是绿的样子了。

### B35 控件窗口字体换了，文字量跟着不动（账 #200，**已出**）

做 #196 第二条时撞出来的，**归因还没做**，所以那条判据今天当不了判据（夹具里只留读数 TH06）。读数：`tests/dcsurf` 里 `picB.FontSize = 20`（发码确实是 `vb6_SetControlFontSize(vb6_hwnd_picB, 20)`）之后再量 `picB.TextHeight("Xg")`，**量回来还是 16**；设计期写 18pt 的 picA 也量 16，而 `picA.FontSize` 读回 18（自存的那份读得回来，问窗口的没有）。两头都哑：`picA.FontPixelHeight`（RTL 里 `vb6_ControlFontPixelHeight` = WM_GETFONT + GetObject，#153 那套"问窗口的证人"）今天**整条 Debug.Print 一行都没打出来**——发码是齐的（`vb6_ControlFontPixelHeight(vb6_hwnd_picA)`），运行期那句却像被吃掉，这本身又是一条要查的。
三种可能分不开，按顺序问：① `vb6_SetControlFontSize` 的 WM_SETFONT 对 PictureBox（自绘 + 分层子类那层，见 #185）到底发没发出去；② 发了的话，设计期那条 pass 有没有被后面的创建/装子类 pass **再设回默认**（同 #185"同名槽位被后装那层挤掉"那一族的另一味）；③ `FontPixelHeight` 那句为什么不打（夹具的语句被谁吞了）。
**为什么值得单开一账**：#177/#129 两轮的结论都是"文字量必须按控件自己的字体与单位"，单位这一半这轮接通并钉住了（TH02 实测 15 倍），字体这一半现在只能靠代码口径（`WM_GETFONT`，与 Print 同源）自证，**运行期没人验证过**。真工程里 Charts 2020 的图例/标题全走这条路，字号一歪就是整张图的排版歪。
**→ 已出（2026-10-05）**：归因 = ①②③ 三条猜的**都不是**，真身在窗口类本身。一次性探针（`.build/b200probe/fontprobe.c`，裸 STATIC、谁也没子类化过）实测 `WM_SETFONT` 之后 `WM_GETFONT` 回 NULL，`STM_SETFONT`/`STM_GETFONT` 那一对同样回 NULL —— STATIC 这个类**压根不记字体**。于是原来那三条读法（`.FontName/.FontSize` 背后的 `vb6_GetControlLogFont`、Print 落笔前的选字体、#196 的 `vb6_ControlMeasureTextPx`）在这类窗口上永远拿不到用户设的那张，只能拿 DC 的默认字体画；而 `.FontSize` 读回来还是设计值（那是另一份自存的属性）⇒ 两头都不报错，只有量出来的数不动。③ 那条 `FontPixelHeight` 整行不打也归到同一条因：它自己就是 `WM_GETFONT + GetObject`，拿到 NULL 就直接 `return 0`。

**改**：一处出口 + 一份自存。新增 `static HFONT vb6_ControlFont(HWND)`（先问窗口，回 NULL 再读窗口属性 `VB6_CtrlFont`），setter `vb6_SetControlFontFromLogFont` 把自己 `CreateFontIndirectW` 出来的那张存进这个**新名字**（#185「一层一个属性名」那条纪律），三条读法全改走这一处。旧字体的找法刻意改成「先读自存、找不到才问窗口」—— 顺序反了会对真记字体的那几类（EDIT/BUTTON）**双删**（`WM_GETFONT` 回来的正是我们上一轮存进去的那张，两边都当成旧字体）。**顺带修掉一处 GDI 泄漏**：改前 STATIC 每写一次字体就漏一张，因为那句 `DeleteObject` 依赖的 `WM_GETFONT` 恒回 NULL ⇒ 旧字体永远找不到；现在找得到、也删得掉。已知边界（记在这里不装绿）：窗口销毁时最后一张字体不被 `DeleteObject`（Windows 回收属性表、但不认识 GDI 对象），一枚控件至多一张 —— 普通控件没有统一的 WM_DESTROY 挂钩（只有 picture/image 那层有，就是 #185/#196 归还 DC 的那一站），要补这一头得先造机制。

**运行期读数**（真跑，x64 与 x86 **逐行相同**）：`TH06-FONTRAW a=29 b=16 b2=27 fsA=18 pfA=24 pfB=27` —— 设计期 18pt 的 picA 从 16 → **29**；运行期改 20pt 的 picB 从「改前改后都是 16」→ **16 / 27**；证人 `FontPixelHeight` 两台都答 **24 / 27**（改前那一句整行都不打）。TH03 因此从「只留读数」升回**真判据**：`ok7 = (tA > tB) And (tB2 > tB) And (pfA >= 18)`，`TH03-FONT=True` 进 `$dcSurfExpected`（两台各一条）。真工程配对：ucTreeMaps 两台仍 rc=0、诊断面 0 error、warning 面 109 VB3001 + 6 VB3003 + 1 VB4001（与 #201 那轮同一条），exe 659,968 / 562,688 **与 #201 那轮两个数字一模一样** —— 同尺寸一开始被我读成「这台没真的重新链接、拿的是旧产物」的形状，所以把两台产物目录**删空重跑**（09:31 / 09:32 新写的文件）：照样同尺寸 ⇒ 那是 PE 段对齐把这点增长吸收了，不是旧产物。**这一枪打在自己身上是有价值的**：判据「产物逐字节相同」本来就当不了判据（记忆里那条冷编两次 md5 就不同），但**反过来**"尺寸没变"也不能当成"没重编"的判据 —— 要问就删空目录再问一次。

**护栏**：哨兵 D10 四条 —— 出口在 `vb6forms_ctrl.c` 恰好定义一次 / 文件里带 `WM_GETFONT` 的那一行恰好 1 且必须落在出口体内（第 dl+1..dl+8 行）/ `VB6_CtrlFont` 写者恰好 1 / 读者 >= 1。**假 needle 真红过**：把 measure 那一处换回裸 `SendMessageW(hw, WM_GETFONT, 0, 0)` ⇒ `FAIL D10 问窗口字体的那一行 = 2 处 -> vb6forms_ctrl.c:325 | :733`，换回来即绿。**这一刀自己的第一枪红得不该怪产品**：D10 落地那一次报的是 3 处，多出来那两条是我给两行**行尾注释**写了 `WM_GETFONT` —— 哨兵跳行首 `//`、不跳行尾注释，于是「注释把形状写出来」就自己造了红；措辞改成不嵌那个 token 才对上（同「计数要打印实测值」那一类自欺，只是这回是自欺的方式换了个方向）。**A/B** 86 份（BASE = #201 那份 emit 快照、NEW = 现在这台）⇒ changed=2、**OFFENDERS=0**；那 2 份就是 dcsurf 两台，逐行归因全是夹具自己新增的那几行（pfA/pfB 两条声明与两次 `vb6_ControlFontPixelHeight` 读、ok7、TH03 那条 Debug.Print、TH06 那串 concat 变长）—— 后端一字节未动，这正是「只动 RTL 的一刀在发码面应当一字不变」这条立论的钉法。

**剩下的同族**：探针这一轮又补了一枚裸 `BUTTON`(BS_GROUPBOX)，**同样回 NULL** ⇒ `vb6forms.c` 里 Frame 标题带那处读法一并定了罪，另立新账 §B37/#202（写法已经现成，缺的是把出口跨文件递过去）。

### B36 ucTreeMaps 现在只差三枚 DI 桩（账 #201，**已出**）

#196 两条接通之后，`tests/Charts 2020/ucTreeMaps/Proyecto1.vbp` 两台的诊断面从 C2039/C2440 **全清**，红点推到链接期：`LNK2019` 三条 + `LNK1120` —— 缺的符号是 `_vb6_di_AddFontMemResourceEx@16`、`_vb6_di_GdipNewPrivateFontCollection@4`、`_vb6_di_GdipPrivateAddMemoryFont@12`，调用者是工程自己的 `vb6_FontMemRes_AddMemFonts`（把字体从内存资源装进私有 FontCollection，GDI+）。
口径照记忆里那条：**手写一枚 `vb6_di_*` 单桩，别重跑 `gen_di_stubs`**（它按当次 session 整文件重写，会撤掉别人的桩），并同步 `scripts/di_stubs_manifest.txt` 与 `check_di_stubs.ps1` 的普查，不然哨兵会说谎。这三枚桩是**真实现**（AddFontMemResourceEx 要真的把字节交给 GDI、GdipPrivateAddMemoryFont 要真的建私有集合）而不是空桩，否则 FontMemRes 这条链只是从"编不过"变成"跑起来没字"——那是 #196 一开始就要躲开的"静默空转"。做完这一格，ucTreeMaps 就是第四枚能进 `Test-VbpBuild` 清单的 UC 子工程（#187 那三条之外的第一条新增）。
**→ 已出（2026-10-05）**：三枚桩手写进 `src/rtl/core/di/vb6_di_stubs.c`，清单基线 620→624（`check_di_stubs.ps1 -Update`），ucTreeMaps 两台第一次真编真链出 exe（x64 659,968 / x86 562,688，诊断面 0 error），并已升格进 `Test-VbpBuild` 清单。细节与那条"pdv 为什么交 NULL"的取舍在 §D 的账 #201 那一行。

### B37 Frame 的标题带是按默认字体量的（账 #202，开着）

做 #200 时把那颗一次性探针补了一枚裸 `BUTTON` / `BS_GROUPBOX`：`WM_SETFONT` 之后 `WM_GETFONT` **同样回 NULL**（读数 `C plain BUTTON : WM_GETFONT=0000000000000000 want=... match=0`）。于是 `src/rtl/core/vb6forms/vb6forms.c` 里 Frame 标题带那一处（接管绘制那趟的 `HFONT hf = (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0);`）拿到的是 NULL ⇒ `SelectObject` 整步跳过，带高 `th` 与带宽 `sz.cx` 都按 DC 的默认字体算，而组框自己画标题用的是我们 `WM_SETFONT` 给过去的那张 —— 帧字体一改，那条白带要么盖不住标题、要么盖过头。今天不红是因为语料里没人改 Frame 的字体（设计期默认 8.25pt 与系统默认算出同一个数），这正是「编得过、跑了、什么都没发生」那一族的另一味。

**修法在 #200 里已经造好了**，缺的是把它跨文件递过去：那一行改读 `vb6_ControlFont` ⇒ 要先把出口从 `static` 提出来、进内部头；同时 `VB6_CtrlFont` 的写侧要覆盖到 Frame 这一档（现在唯一那条写者是 `vb6_SetControlFontFromLogFont`）。哨兵 D10 今天刻意只圈 `vb6forms_ctrl.c`（规则注释里写着原因），跨出来那一天应当把普查范围一起放开成整个 `src/rtl` —— 别留一份「两处口径、只守一处」的表。


### B38 ucTreeMaps 的 exe 第一次真跑：x86 起窗、x64 启动期 AV（账 #203，开着）

#201 让这台工程第一次真编真链出 exe，本条是那份产物的**第一趟真跑**读数（`.build/b200_tmrun.ps1` 收 stdout/stderr 与退出码，`.build/b182_winprobe.ps1` 数窗口）：

- **x86** = `alive=True mainhwnd=0x1606BC`，顶层窗口 6 条，其中 `cls=VB6_Form_Form1 txt=Form1` 是可见的 ⇒ **窗体真起来了**（另五条是 GDI+ Hook Window、ComboLBox、MSCTFIME 与两枚 Default IME，都是系统的）。
- **x64** = 8 秒内自己退出，`code=-1073740771`（0xC0000409 fail-fast），而 crash trace 打的是 `code=0xc0000005 at rva=0xce8a7463` + `av read target=0x48777bc3`（那是个野值），24 帧里只有 5 帧落在 exe 内（rva=0x2602c / 0x35317 / 0x2194e / 0x21a4c / 0x21f74），stdout 0 字节、Form1 那一枚窗口根本没出现。

⇒ 这一格从「编得过」推进到 VB6 那一侧的「能不能跑」；x64 起不来是**新缺陷**，不是 #201 那三枚桩的余波（那三枚在 x86 那条路上同样被调到，窗体照样起来）。**下一步（还没做）**：把 x64 那条 AV 归因 —— 顺序照 #182 那一味先拿带符号的产物把那几个 rva 落成函数名（**没符号化的 rva 名单不构成结论**），再分岔问「是产品发码把指针按 32 位存了」还是「工程自己的 Declare 把指针写成 `As Long`」，后者是改 VB 源码、不动编译器（#163/#175 那一族早已立过口径）。注意 x86 这一侧今天是**好的**，所以任何「回归坏了」的判据都不该把它算进去；反过来说，门禁今天只对这台工程断言「编得过」（Test-VbpBuild），跑得起来这件事还没进任何判据。

### B39 创建期下发的那张默认字体没人存：没被写过字体的控件整张 Font 面读空（账 #204，开着）

#200 收了"改过字体读不到"那一半，这半是**没改过字体**的那一半。读数（`.build/b204probe` = dcsurf 的一份**临时拷贝**，`.build/b204_run.bat` 真跑，仓里那份夹具一字节未动）：一枚从头到尾没人写过字体的 PictureBox 答 `bName= bfs=0 bpf=0 bth=16` —— `.FontName` **空串**、`.FontSize` **0**、证人 `FontPixelHeight` **0**、`TextHeight("Xg")` **16**；同一趟里设计期给了 18pt 的那枚答 `aName=MS Sans Serif afs=18 apf=24`（这条是 #200 刚接通的那一路，读得到）。**对照 VB6**：picB 该答 MS Sans Serif / 8.25 / 约 13-14 像素，而 16 那个数是Segoe UI 9pt 的高度 —— 也就是**默认字体那批控件的文字量与 Print 现在全按系统默认字体算**，不只是读空。

根因是 #200 那一处出口的**覆盖面**，不是读法：`vb6forms.c` 的创建路（Fix 181 那一站，`vb6_Vb6DefaultGuiFont()` 每枚控件新建一张 MS Sans Serif 8.25 再 `WM_SETFONT` 过去）与控件数组那条创建路（`vb6forms_ctrlarr.c` 跟着把模板的字体 `WM_SETFONT` 给新窗口）都**只发不存** —— 而 STATIC 那一类窗口不答 `WM_GETFONT`（#200 已用探针钉死），于是 `vb6_ControlFont` 两问皆空。顺带这一处还留着一张没人认领的 GDI 对象：Fix 181 造的那张字体，改前改后都只有等某次字体赋值才可能被删，而删它靠的正是"读得到旧字体"。

修法照 #200 那一条口径走到底：**创建路把刚发给窗口的那张也存进同一个槽位**（`VB6_CtrlFont`），两条创建路都要给（#83/#151/#160 那条"两条创建路读同一个数"的纪律 ⇒ 判据两头钉）。**护栏的形状要先想清楚再动**：哨兵 D10 今天写的是「`VB6_CtrlFont` 的写者恰好 1」，这一刀会把写者变成 3（setter + 两条创建路），所以那条 census 要**按理由改数**、并把每个写者是谁写进规则注释 —— 不是把上限放宽就完事。判据建议照 #153 那套"两头钉"：`bName` 等于 `MS Sans Serif` 且 `bfs` 约 8.25（自存往返）+ `bpf` 在 12..16 之间（问窗口的证人）+ `bth` 从 16 掉到那个区间（**文字量真的跟着换了字体**，这一条才是本账的产品后果）。

为什么单开一账而不是并回 #200：#200 的症状是"改了没生效"，本账的症状是"从来没生效过"，两者的修法在不同文件、判据也不同一头；并在一起会把 D10 那条 census 的理由写得说不清。

## C. 仍在生效的口径与工具事实（与本文档等长的一半价值在这里；完整版见记忆库）

- **子类化分层的槽位口径（账 #185 起）**：RTL 里**每一层**窗口子类用**自己**的窗口属性名存它下面那层的 wndproc ——
  `VB6_OrigProc` = 发码的事件层、`VB6_ImageOrigProc` = PictureBox/Image 的自绘层、`VB6_GBox_OrigProc` / `VB6_GfxBtn_OrigProc` /
  `VB6_SSTab_OrigProc` 各自一层，而「这层装过没有」那一问**只看自己那层的名字**。两层同名 = 后装的那层静默不装，
  症状是「处理器编得过、消息臂发得对、一次也不响」—— 这类缺陷只有运行期看得见，所以判据必须带一枚**没人跟它抢的证人**
  （本线用的是同窗体上的 Label：同为 STATIC，只是样式不含 SS_BITMAP）。画的序也收成一条：最外层 BeginPaint/EndPaint 一次，
  DC 经 `VB6_PaintDC` 交给下面那层画表面，再抬用户的 `_Paint`。哨兵 `scripts/check_subclass_slots.ps1`（S1 一名一文件）拦的就是同名。

- **`AddressOf` 的调用约定口径（账 #184 起）**：VB6 的 `AddressOf` 交出去的是 **`__stdcall`
  调用桩的地址**，不是本体地址；C3 现在按它在**定义模块**里发桩（口径一处：
  `CCodeGen::addressOfTargetCName`），本体保持 `__cdecl`。判这类刀时记三条：
  ① 小夹具不算红判据——MSVC 写的调用方有 EBP 帧，`leave` 会把 ESP 拉回来，所以
  `EnumWindows(AddressOf cb)` 在改前也读数全对（实测 HEAD~1 与 HEAD 两份产物逐字相同）；
  真判据要用**不自我吸收的调用链**（comctl32 的 `SetWindowSubclass` thunk）或发码形状针。
  ② 页堆只能把「谁在读已释放堆块」钉成确定现场，钉不出「谁按 stdcall 调它」——
  把归因钉死的是**一次「关掉一条路」的对照实验**（拷一份工程出去给 `FlexSetSubclass`
  体首加 `Exit Sub`：崩溃 3/4 ⇒ 0/6）。
  ③ A/B 分类器要允许「纯改名」这一类差异（判据 = 两边都把 `aoThunk_` 去掉再比），
  并把 `C3:` 开头的诊断行滤掉——`--emit-c` 的 stdout 与 stderr 混流，census 行会伪装成
  「多了一行」。

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

| 账 #173（提交 `332f6363` = <vbeclipse> rev38，门 #299） | x86 上 `vb6_VariantToDouble` 无原型 → x87 栈泄漏 → Charts 2020 启动即 error 6「Overflow」；补真实原型 + 同族 census （C4013 14→2，剩 B22 那两处） | 已提交并过门 |
| 账 #174（提交 `3684b1d7` = <vbeclipse> rev39，门 #300） | 属性页裸名 `SelectedControls(i)` 改走宿主内建裸名表，发射成 RTL 既有出口 `vb6_PropertyPage_SelectedControls`；新用例 `pp_selctrl_hostmember`，语料 A/B 118/120 一字不动 | 已提交并过门 |

| 账 #175（提交 `e4bf2b17` = <vbeclipse> rev40，门 #301） | 控件坐标的**单位**收成「容器的 ScaleMode」一处权威（`vb6_ScalePxToUser` / `vb6_ScaleUserToPx` + `vb6_ContainerScaleMode` / `vb6_WindowScaleModeSelf`），替掉散在 12+ 处的写死缇；Charts 2020 的饼/柱/面积/矩形不再整幅画在画布外（图体空白），czUI 运行期 `Move` 的字面量恢复像素语义。判据 = `Test-GuiVbp -DumpMinColors`（数 `C3_UC_DUMPDIR` 每控件绘制缓冲的不同颜色；BASE 4..10 / NEW 63..512）+ 结构哨兵 `scripts/check_uc_scale_units.ps1`（BASE 树 20 处红）。同轮量到 #176（见 B23）、TextWidth 量纲（见 #177） |
| 账 #177 + #178（提交 `d5f3e190` = <vbeclipse> rev41，门 #302） | 文字量纲（`UserControl.TextWidth/.TextHeight`）跟着容器声明的 ScaleMode 走，单位表收成一份（`vb6_ScaleUnitsPerPx`，`ScaleX/ScaleY` 转调；顺手订正 6=毫米/7=厘米 抄反）；「控件代码运行在自己的上下文里」补了两处出口 —— 早绑定走发码（`.ctl` 里把 `UserControl.TextWidth(t)` 重定向为 `vb6_UC_TextWidthOf((void*)me,t)`；选宏而不做 enter/leave 配对，因为 `Exit Function` 漏帧是已知欠账），晚绑定走 `OwnPropGet/OwnPropSet/OwnMethodCall` 三处 push/pop。新夹具 `tests/ve_units`（判据写成「缇型 = 像素型 × TwipsPerPixelX」⇒ 与 DPI 无关，CI 两片读数与本地逐字节相同）+ 哨兵两条新规则（出现第二张单位表即红）。红侧实测：#175 产物上 `U-TW/U-TH=False`；像素型控件读数全落在同一产物跑两遍的 ±5% 自抖区间内 |
| 账 #179（提交 `30a3f4ff` = <vbeclipse> rev42，门 #303） | 控件代码不管被谁调都跑在自己的宿主上下文里：发码在 .ctl 实例方法体首 `vb6_UC_PushInstance((void*)me)`、统一出口尾 `vb6_UC_PopInstance()`（谓词 `ucCtxScoped()` 一处，三个发射点共用；能配对的前提是 `Exit Function` 早已走 `vb6_proc_exit`），替掉 #178 那种逐个成员补按实例出口的做法。不变式：charts/czui/flex/ve_units 逐函数 push=pop，无 .ctl 的工程 0/0。针面 `U-CTX`（只取 ScaleMode 一位 ⇒ 与 DPI 无关）。顺带把自己一条错结论订正回 #159（见 B25） |
| 账 #159（提交 `ef260daf` = <vbeclipse> rev43，门 #304） | 宿主伪成员（UserControl / PropertyPage / Extender / Ambient）的「叫什么 / 是什么类型 / 能不能裸写 / 赋值要不要拆」收成一张表 `kHostPseudoRows`（55 行、35 标量），四个消费点 + 拼写规范化都改读它（此前是五份互不相交的清单）。**根因在读法不在值**：类型 oracle 答 Variant ⇒ 发码把 `&vb6_UserControl_hWnd`（8 字节 `void*` 的地址）当 `vb6_VARIANT*`（16 字节）递给 `vb6_VarCmpLongNe` ⇒ `<>0` 恒假；`CStr` 落进 `_Generic` 的 `default: vb6_VariantObject` ⇒ 空串。红→绿：同一夹具 BASE 发 `vb6_VarCmpLongNe(&…,0)`、NEW 发 `(-(… != 0))`；真跑 `U-HW=True`（x64 + x86 两片同值）。--emit-c A/B 六工程只有 charts +12/-12、flex +2/-2、ve_units +8/-8 变，逐行归因全在这一家，czui/ve_list/iface_wrap 零差异、六工程行数未增删。哨兵 `scripts/check_host_pseudo_table.ps1`（逐行对 `vb6rtl_userctl.h` 的 extern 类型 + 禁五份旧清单回潮，两条假针都能红）进 `[STATIC] host_pseudo_table_census`。顺带：`.ctl` 里裸写 `hWnd` 现在能解析。表外欠账见 B26 |
| 账 #180（提交 `6533bae4` = <vbeclipse> rev44，门 #305） | §B 的 B19 落地：`vb6_UserControl_ContainerHwnd` 从 `int32_t` 换成 `void*`（声明 / 定义 / push-pop 快照字段 / 写入点四处一起；x64 上原来那句 `(int32_t)(intptr_t)r->parent` 把 HWND 高 32 位当场丢掉），#159 那张表里对应行跟着答 `LongPtr` ⇒ 哨兵从此钉住这个宽度（改回 int32_t 就 `ROW-TYPE-MISMATCH` 红）。顺带修掉类型答对后露出的一格：`CStr(句柄)` 落进 `_Generic` 的 `default: vb6_VariantObject` ⇒ 打空串，现在在 CStr 分诊处按 `LongPtr` 走 `vb6_CStrLongLong((int64_t)(intptr_t)(x))`（**不往 `_Generic` 补 `void* :`** —— 那会把真对象引用一律打成数字）。判据 `U-CNT` 三头（容器非零 / 容器≠自己 / CStr 非空串），红侧实测：只改宽度不加 CStr 支路 ⇒ `cnt=` 空、`U-CNT=False`。--emit-c A/B：五工程逐行相同（宽度住在 RTL 里），六工程行数未增删；真跑 ve_units x64+x86 两片绿、charts/flex/flex_x86/czui_x86 无崩。另立 B27（czUI x64 退出码 0xC000041D，A/B 证为预存）|
| 账 #161（提交 `a0b4c67a` = C29-RT-e，门 #306） | **读数推翻本账前提**：一次赋值（`Text` 或 `SelText`）聚焦/不聚焦都**恰好 1 条** `_Change`，连做三次 1/1/1；「只动选区」`chg=0` ⇒ 当年猜的「`_Change` 挂 EN_UPDATE、选区变也发」当场否掉。唯一出 2 的形状是**焦点刚落到这枚控件之后的第一格** —— RichEdit 补发的通知落进下一个泵窗口，被算进相邻那一格（与 #162 同族）⇒ 判据污染，不是产品缺陷。**只改判据**：`RT84` 从 `>= 1` 回到钉死（排空后 `= 1` 且再泵一轮不加发），新增 `RT91`（只动选区 ⇒ Change 0 条，只问 Change，因自发 SelChange 条数天生 2..7 抖）与 `RT92`（一次赋值恰好 1 条）。产品侧一行未动；本地 x64×4 / x86×3 稳定，门里两片三格全 Y |
| 账 #181（提交 `e16e42cd` = C29-CH-f，门 #307） | 崩溃轨迹每进程只记一次：新增零依赖头 `vb6rtl_crash.h` 声明 `vb6_CrashTraceClaim(int slot)`（定义在 vb6rtl.c，4 槽 InterlockedCompareExchange），三个出口（stderr / c3_crash.txt 的 AV 段 / 末段 EXCEPTION）共用这一处口径；**槽位各记一次**而非全局一次（全局一次会让 stderr 那份抢掉文件那份）。为什么必须：记轨迹本身会再触发异常（栈溢出时 fprintf 拿 CRT 锁、栈扫描再读同一批页）⇒ 处理器套处理器 —— 实测 czUI x64 那份 c3_crash.txt 把同一递归栈写了 4 遍、第一现场（gdiplus AV）被压到最后。改后 **6 段 → 2 段且第一段就是 FAULT gdiplus.dll+0xF2E1**。产品行为零改动（三个出口只在 C3_CRASH_TRACE 下装；不带 env 不写文件、退出码不变）。两条工具事实：诊断面的头不该依赖别的 RTL 头（塞进 vb6rtl_runtime.h 会让 vb6forms.c 炸在 vb6_SafeArray1D 未声明）；新增 RTL 头要三处登记（CMakeLists / c3rtl.rc 的 RCDATA 号 / rtl_embedded 的枚举与号→名表），少一处就是 C1083 |
| 账 #182（提交 `0b13dcf6` = C29-CH-g，门 #308） | x64 的崩溃轨迹以前**一条应用帧都没有**，三处各自把路堵死：① `vb6forms.c` 那段栈扫描整段包在「故障地址属于哪个模块」那一问里 ⇒ `call` 跳飞（rip=0x1）时这一问直接失败、一个候选都不打；② `vb6rtl.c` 的栈指针线性扫描只在 `#ifdef _M_IX86` 里编；③ 扫描的 x64 分支用 `wsprintfA("0x%016llX")` —— 用户态 wsprintf **不认 `ll`**，一直打成 `0xlX -> <一串字节>+0xlX`（这段以前从没被执行到，①改完才露出来）。改法：扫描目标一律换成**主 exe 镜像**（生成代码都在 exe 里，扫系统模块没用）、扩到 `_M_X64`（取 `Rsp`、按 `ULONG_PTR` 步长）、偏移改打 32 位 RVA；顺手把不在本模块镜像里的帧照实标 `(outside exe)`（以前把 ntdll 的地址减掉自己的基址打成「rva=0x4376...」，看着像自己的符号），文件侧 AV 头把 `ExceptionInformation[0]==8` 认成 EXECUTE(DEP)（以前一律写 WRITE，与 vb6rtl.c 里 Fix 187 同口径）。读数：同一夹具同一 env，改前只有 5 帧派发链 + 零条扫描行；改后 `st+0 rva=0x2581ae` 落进 `vb6_ComCall`，`st+37/st+40/st+48` 落在工程自己的 `VTableHandle`（`IOleIPAO_EnableModeless` / `GetVTableIPAO` / `ActivateIPAO`）上 —— **账 #183 就靠这几行归的因**；B23/B15 那两条以前归不了因，根因也在这条工具哑火上 |
| 账 #183（提交 `d9e34590` = C29-CH-h，门 #308） | Fix 191 那条「按名 `AddRef`/`Release` 直发槽位」**少绕了一层 vtable**：`((void**)disp)[1]` 读的是 `disp+8`（对象自己的第二个字段），槽位要先从对象首字读出 vtable 再取；同文件里 `vb6_ComIsDispatchable` 自己是两级读法 ⇒ 同一件事两套口径。在 `VTableHandle.bas` 手搭的伪 `IOleInPlaceActiveObject` 上，`VTableIPAODataStruct` 的第二字段恰好是 `RefCount As Long` ⇒ 读出来是 1 ⇒ `call 1`（AV EXECUTE(DEP) target=0x1）。触发条件量到是**确定的、不是偶发**：向 FlexGrid 子窗发**一条 WM_LBUTTONDOWN**（只这一条；`WM_LBUTTONUP` / 右键 / 滚轮都不发）再对主窗发 WM_CLOSE ⇒ x64 `-g` 与 x64 无 `-g` 各 16/16 复现。改成 `lpVtbl->AddRef/Release`（与 `vb6com.c` 里 `vb6_ReleaseObject`/`vb6_ComAddRefDispatch` 同形）后两台各 12 次关窗干净退出、零条崩溃现场；负控（改前那台）同条件 2/2 仍崩。census：`grep -E "\(\(void\s*\*\*\)" src/rtl` = **0** ⇒ 全 RTL 再无手写槽位读法，按 vtable 调一律 `->lpVtbl->`（218 处）。顺带把 `vb6_ComGetProp` 那条 `fallback to IDispatch` 从字体/Extender/宿主三个岔口**之前**挪到之后（它对根本不走 IDispatch 的调用也照打，本轮差点据此把嫌疑引向没执行过的路径）。四条判据面事实进 memory：崩溃后**退出码仍是 0**（判据只能看 `c3_crash.txt` / `[C3_CRASH]`）；`c3_crash.txt` 写在**被测进程当前目录**（相对路径）；`Start-Process -PassThru` 的 `.ExitCode` 在碰过 `.MainWindowHandle` 后拿到 `$null`；**产物架构读 PE 头别看目录名**（本轮一次漏传 `--arch x86`，目录名 `b182gx86n` 的产物其实是 x64，整条「x86 侧读数」当场作废）。x86 那台**起窗就 0xC0000374** 与本刀无关（把 #183 退回单验仍崩）⇒ B23 继续开着 |
| 账 #172（提交 `3e7231c7`，门 #309（run 37165123051，head `3e7231c7`，attempt 1）= 11 job 全绿、非绿 0） | **真相不是「Date 默认值偶尔发垃圾」，是日期字面量 `#...#` 从来没有值**：parser 建 `LiteralExpr` 时只挂原文（`case TokenKind::DateLiteral` 一句 return），发码侧 Date 档照 `node.doubleValue` 打 —— 而构造函数只写了 `intValue(0)`，清的是 4 个字节，8 字节槽的高半从没人写过 ⇒ Ninja/Debug 恰好读到 0.0、VS 生成器/Release 读到 -6.277e+66。**所以改前 Debug 那台也不是对的**（VB6 里 `#1/1/1900#` 是 2.0），只是错得稳定。复现不需要另一台机器：同源码同生成器、只加 `/RTCu` 冷编一台 C3.exe，`--emit-c` 一比就把 4 处垃圾点钉出来（flex 的 ComboCalendar Min/MaxDate 的 ret 赋值 + 各自 `Select Case x To y` 折出的区间边界）；改后两台**逐行零差异**。修法：① 一处出口 `foldDateLiteralToOADate()`（定义 parser_helpers.cpp、声明 parser.hpp）—— 斜杠 M/D/Y、连字符 D-M-Y、带 `H:N[:S]` 与 AM/PM、两位年份 <50→2000s / ≥50→1900s、闰年与真日历校验、OLE epoch 1899-12-30=0.0 用 daysFromCivil 无循环算；② 构造函数改整体清零，把「只清半个联合体」这一类堵住。认不出的形状**照旧留 0、不发新诊断**（宁可不许把现在编得过的工程编红）。判据：新夹具 `tests/test_datelit.bas` 进 bas 队列 —— 新编译器 14 条读数全对（门工件 `test-logs-bas-1/job8/test_datelit.out` 原样可查、`.err` 0 字节）；负控 = 同一份测试喂改前的编译器：10 条变 False 且 `L-serial=-6.27743597849989e+66`。A/B 护栏（BASE=临时回退四份源文件重编、NEW=修复后，同配置同生成器，六工程 --emit-c）：charts/czui/ve_list/iface_wrap/ve_units **0 行变化**，flex 4 行且分类器要求「只有数字变」，新值只有 {2.0, 2958465.0, 2958465.999988426} 三个 OLE 序列 |
| 账 #184（提交 `ab45e2d3`，门 #310（run 37172942961，head `ab45e2d3`，attempt 1）= 11 job 全绿、非绿 0） | **B23 的根因 = `AddressOf` 把本体的裸地址交给了 OS**：x86 上本体是 `__cdecl`（`ret` 不弹参），而 Win32/COM 回调是 `__stdcall` ⇒ 每回调一次把调用方的 ESP 少弹 N*4 字节。VBFlexGrid 每枚网格都经 comctl32 的 `SetWindowSubclass` 装了 6 形参的 SUBCLASSPROC ⇒ 每条消息少弹 24 字节 ⇒ 起窗期堆损坏 `0xC0000374`（改前本地 3/4~5/6 崩、窗口从来出不来；页堆之下现场 6/6 钉在创建 `tooltips_class32` 那一刀，因为 comctl32 正是那条消息链上第一个读到被踩坏的栈的人）。改法按 VB6 口径：被取址的标准模块过程在**定义模块**里另发一枚 `__stdcall` 转发桩（参数表逐字复制 `makeProcSignature` ⇒ 个数/宽度/顺序与本体一致，体内原样转调；Private→`static` 桩，Public→非 static 且原型进自家 `.h`），取址点交桩地址；**本体一个字不改** ⇒ 直接调用那条路与 RTL 那批 cdecl 登记面（Timer / Form_Resize / Winsock / OLE 拖放 / `vb6_di_qsort` 的 cmp）全不牵连。口径只在一处 `addressOfTargetCName`，与委托桩（a15c40b）共用同一套`splitProcSignature` / `thunkArgsFromParams`；标记趟 = stage 3.5c（必须晚于 3.5 跨模块链接才认得归属模块），绑定到 `Delegate` 的 `AddressOf` 一步都不碰。**FlexGridX86 从此挂进门禁**（B23 记的「这份 demo 完全在回归之外」就是它藏这么多轮的原因）。判据四面：`test_addrof_cb` 真跑 x64+x86（六条读数全 Y，CI 两片同值 136 枚窗口，判据写成自洽式 ⇒ 不依赖桌面有几枚窗口）+ `addrof_cb_shape` 发码形状针（两条 `Absent` 就是负控：HEAD~1 的发码里一个 `aoThunk_` 都没有、取址点写的是 `(void*)vb6_CountWin`）+ 静态哨兵 `scripts/check_addressof_thunk_sites.ps1`（同一份脚本在 HEAD~1 的树上 R1~R4 全红）+ A/B `--emit-c` 六工程两架构：**除 flex 外五份逐行相同**，flex `+81/−0` 且 40 处差异全是「取址点改交桩地址」，删除 0 行。另：本轮顺手把 `Test-GuiVbp` 的窗口标题读法从 ANSI 改回 Unicode（`CharSet.Ansi` 一直把类名/标题截成 `V|V`）。遗留：census 那句 `AddressOf callback procs: 49` 数的是**符号副本**（定义模块一份 + 每个引用模块一份），不是过程数（实际 36 枚桩），下轮有别的源码刀时顺手去重，不为它单开一轮门。 |
| 账 #185（提交 `a4e3b574` = C29-PB-a，门 #311（run 37177647595，head `a4e3b574`，attempt 1）= 11 job 全绿、11 片 `FAIL=0`） | **B13 那条「同名属性」的真形状是「装了但没装」**：RTL 建窗时给 STATIC+SS_BITMAP（PictureBox/Image）装自绘子类 `vb6_InstallImageSubclass`，发码为带事件的控件装事件子类 `vb6_InstallControlSubclass`，两层把「下面那层的 wndproc」存在**同一个属性名** `VB6_OrigProc` 上、又都写「属性已存在就不装」⇒ 后装的事件层被整层丢掉。夹具实测：PictureBox 的 Paint/MouseDown/MouseUp/Click 与 Image 的 Click **五条全 0**（x86 与 x64 同形），同一窗体上的 Label（也是 STATIC，样式不含 SS_BITMAP ⇒ 没人抢）一直响 —— 判别力就在这条对比里。**订正 §B 旧 B13 那句「可把 NULL 写进 GWLP_WNDPROC」**：四处还原点（widget.c、picture_prop.c、shape.c:474、forms.c:513）全有 `if (orig)` 守卫，NULL 写不进去；照旧措辞开工会被引向一条不存在的通路。改法两条口径：① 分层槽位（自绘层换 `VB6_ImageOrigProc`，与既有 GBox/GfxBtn/SSTab 同规矩）；② 画的序收成一条 —— 最外层那一臂 BeginPaint/EndPaint 一次，DC 经 `VB6_PaintDC` 交下去让自绘层画表面（BackColor+Picture），再抬用户的 `_Paint` ⇒ 用户笔画在表面之上（VB6 口径）；自绘层已有 DC 就只画、不再第二次 BeginPaint（RTL 那句 trace 新增 `shared=` 一位，实测 `shared=1` = 这条走通）。判据四面：`tests/pbsub`（自驱 Timer 往三枚子窗 PostMessage，两头钉 —— 五条事件针 + PB01 那颗像素必须等于设计期 BackColor 的红）进门禁 x64+x86 两片；**负控 = HEAD 的编译器编同一份夹具**，两边都只剩 PB00/PB06 且 `mdown=0 mup=0 click=0 img=0 lb=1 paint_ok=0`；哨兵 `scripts/check_subclass_slots.ps1` 四条规则（S1 一名一文件 / S2 自绘层不碰裸名 / S3 共享 DC 两头 / S4 向下转调必须在抬处理器之前）同一份脚本在 HEAD 的树上 6 行全红；`--emit-c` A/B 7 工程 × 两架构（BASE 冷编注意 VS 生成器把产物放在 `.build/Debug/C3.exe`）**12 份逐行相同、flex +2/-0 正是那两句新增、删除 0 行、OFFENDERS 0**。顺带三条读数：census 显示 RTL 里拿窗口属性当「已装」旗标的只有那两趟子类化（另三条是重排队列旗标）⇒ 同名碰撞到此为止；frmevents 全套 EV 针齐 + rc=0；FlexGrid x86 起窗 4/4 无崩。 |
| 账 #188（提交 `33cb239a` + 收口 `936124e8` = B29①，门 #313（run 37198483318，head `936124e8`，attempt 1）= 11 job 全绿、11 片 `FAIL=0`；上一轮门 #312 红一条，红因见 B29① 那段的订正） | **C 里的 `12f` 是非法 token**（MSVC C2059 "bad suffix on number"），整数值的 Single 必须写 `12.0f`。这条规则在编译器里散着三份，其中「.frm 设计期字体块」那处（`snprintf("%.4g")` 后直接拼 f）抄漏了补小数点那步 ⇒ 设计期字号写成整数的工程**直接编不过**（实测 Charts 2020 的 ucChartBar 4 条 C2059、ucProgressCircular 16 条）；VB6 默认字号 8.25 带小数点 ⇒ 门禁里所有夹具都恰好躲过，这条潜伏了不知多少轮。同一个形状在语义层 Fix 133z 早被修过一次（`Optional ... As Single = 1!`），当时只在那一处补的小数点 ⇒ 规则没收口。**改**：新增 `src/common/float_literal.hpp` 一处权威（`floatingLiteralText` / `floatSingleLiteral` / `floatFixed6Literal`，经典 locale + 「没有 . 或 eE 就补 .0」），五处发射全接过去；位数各按原样（字体块 4 位、to_string 那三处保持 6 位固定形状 ⇒ 发码字节不动）。**判据**：`fontsize_literal` 发码形状针钉在**真实工程** ucChartBar 上（Needles `->Size = 12.0f;` + 证人 `->Size = 8.25f;`，Absent 是改前的 `->Size = 12f;`）；哨兵 `scripts/check_float_literal_shape.ps1`（R1 手拼后缀=0 / R2 三个函数各定义一次且带着补点与经典 locale / R3 调用点>=4）在缺这刀的树上 R1~R3 全红、R1 当场点出那 6 个手拼点。**配对读数**（同一份工程 × 两台编译器 × x86）：ucChartBar C2059 4→0（只剩 B29②的一条 C2065）、ucProgressCircular 16→0（剩 B29③）。**护栏**：A/B 从门禁形状断言面取清单（37 文件 × 两架构）= 只有 ucChartBar 两片被改，归一化浮点 token 后逐行相等、数值多重集相等、增删 0 行、OFFENDERS 0。 |
| 账 #189（提交 `ea5155af` = B29②，门 #314（run 37200966607，head `0efb29b0`，attempt 1）= 11 job 全绿、10 片 `FAIL=0`） | **VB6 里 `arr.Count` 问的是数组本身，不是某一枚控件的属性** —— 发码侧以前只有 `LBound`/`UBound` 在成员读取那一路各写了一条 if，`Count` 漏了 ⇒ 掉进 COM 兜底，发成 `vb6_ComGetIntProp(vb6_hwnd_<数组名>, L"Count")`。两种红法各一条实测：**跨窗体**引用（Charts 2020/ucChartBar 的 Form2 引用 Form1 那枚 static）= `error C2065 未声明的标识符`；**同窗体** = 编得过、恒答 0（`For i = 2 To arr.Count - 1` 一格也不走，这类是静默错值那一族）。与账 #157 同族：句柄类表达式必须走 `vb6_arr_*`，不许凭空拼 `vb6_hwnd_`。**改**：新增唯一出口 `CCodeGen::ctrlArrayMetaMemberExpr()`（`src/backend/cgen_util_ctrl.cpp`，声明在 `detail/util/cgen_helpers.inc`），成员读取那一路把它放在**问控件属性之前**（`cgen_expr_member_form_builtin.inc`）—— 这三条永远不是控件属性，顺序本身就是口径；旧代码那两条 if 还额外挂着 `readFn.empty()` 当前提，等于把「属性表里恰好没有 LBound」当成了条件，一并撤掉。**判据三面**：①`tests/ve_units` 塞一枚 UC 控件数组 `uArr(0..2)`，两头钉 —— `U-ARR-RAW count=3 lb=0 ub=2`（三个数各对上）+ `U-ARR=True`（内含「拿 Count 当上界**真的**圈了三圈」，只钉前头那条的话 循环不走也绿）；**负控 = `a4e3b574` 的编译器编同一份夹具** ⇒ `frmUnits.c(214)/(220): error C2065 "vb6_hwnd_uArr"`、BUILD rc=1。②发码形状针 `ctrlarr_member`（Needles 三枚 `vb6_CtrlArr_*(&vb6_arr_uArr)`，Absent = 改前那条形；两台编译器实测 2/0 ⇒ 这枚针真能红）。③哨兵 `scripts/check_ctrl_array_members.ps1`（A1 三个 RTL 出口只许出现在权威那一个文件里 / A2 权威三条分支齐且留着空串出口 / A3 `memLower == "lbound"` 那一形归零 + 权威至少一个调用点），假 needle 验红做过：临时文件里多写一处 `vb6_CtrlArr_GetCount(` ⇒ A1 当场点名，删掉回绿。**护栏**：A/B 仍从门禁形状断言面取清单（38 工程 × 两架构 = 76 次发射；BASE = `a4e3b574`，所以 #188 那 4 对 `12f`→`12.0f` 也在账上、已逐行归因）= 只有 ucChartBar 与 ve_units 四片被改、行数一字不差、增=删、其余逐行相同、OFFENDERS 0。**顺带两条新账**（都在 ucChartBar 真编译通下去之后才现形，见 §B29 ⑤⑥）：**⑤** 那两枚控件的事件臂函数名按控件设计期拼写现拼、过程定义按 Sub 自己的拼名 ⇒ 大小写不一致就是两个符号，链接期 `LNK2019` ×2；**⑥** 控件数组**元素的成员访问**（`uArr(i).SW()` 返空、`uArr(i).Left` 直接崩）另是一条独立通路。 |
| 账 #190（提交 `a27758c3` = B29⑤，门 #315（run 37202837330，head `a159b85d`，attempt 1）= 11 job 全绿、10 片 `FAIL=0`） | **事件臂调用的函数名以前是按「控件的设计期拼写」现拼的，而过程定义发的是 Sub 自己的拼写** —— VB6 标识符大小写不敏感、C 敏感，于是"改了控件名没改过程名"（VB6 完全合法，两枚照样配一对）在产物里就是**引用一个没人定义的函数**：链接期 `LNK2019` + `fatal LNK1120`。这条红**只在链接期现形**：存在性那一步 `symTab_.lookup(控件名 + "_Click")` 本来大小写无关、命中了，语义层与 `--emit-c` 的形状都看不出坏了 ⇒ 缺的只是"命中之后按谁的名字发"。实测 ucChartBar 的 Form1 恰好两枚（控件 `ChkAxisY` / 过程 `ChkAxisy_Click`:435，控件 `cboLabelsPositions` / 过程 `CboLabelsPositions_Click`:439）⇒ 正好 2 个外部符号。**改**：新增唯一出口 `CCodeGen::eventHandlerFn(ctrlName, suffix)`（存在性与名字出自**同一次** lookup，命中交 `cProcName(sym->name)`，没命中交空串让调用方**不装这条臂** —— "没处理器就不装"从此是结构性事实，不是每条 if 各自记得查）。43 处现拼全改读它：create.inc 18 / subclass.inc 16 / dispatch.inc 6 / cgen_form_menu.cpp 2 / ctrl_style_apply.inc 1。AccessLevel 不用跟着改：`extern void f();` 之后再声明同名 static 是合法 C（早先那条 static 胜出）⇒ 只有**拼写**要紧。**普查教训（比修本身值钱）**：第一遍只按 `ctrl.controlName` / `ctrlName` 两个变量名扫 ⇒ **少了 18 处**（`info.ctrlName` 那一族 = 焦点/鼠标/Validate 一整批 + 菜单两枚），是第二、三趟按剩余量逼出来的。⇒ census 要按**形状**写（`cProcName(<任意> + "_`），别枚举变量名。**判据三面**：①新夹具 `tests/evtcase`（x64+x86 两片）摆三枚控件 —— `cmdRun`/`CmdRun_Click`、`chkOpt`/`ChkOpt_Click` 只差大小写，再加拼写一致的 `cmdSame`/`cmdSame_Click` 当**证人**（排除"BM_CLICK 自己没驱动起来"那种假红，口径同账 #185 那枚 Label）；驱动 = Timer 第一拍对三枚发 `BM_CLICK`、第二拍打 `EC-CNT run=1 chk=1 same=1` 自退。**负控 = `a27758c3` 那台编译器真编同一份夹具**：`CaseForm.obj : error LNK2019 无法解析的外部符号 _vb6_cmdRun_Click / _vb6_chkOpt_Click` ⇒ **恰好两枚、证人那枚没有第三条**（物证 `.build/b190pre/c3-error.log`）；修复后 x64 与 x86 都是 `run=1 chk=1 same=1`。②真工程配对读数：ucChartBar x86 的 LNK2019 **2 → 0**，并且**这个工程第一次产出 exe**（590,336 字节，`.build/b190chart/Proyecto1.exe`）⇒ B29 那张表里它从此归入"编得过"，#187 的升格候选多一件。③哨兵 `scripts/check_event_handler_names.ps1`（E1 现拼=0 / E2 权威必须"从符号取名"且留着空串出口 / E3 调用点>=40）；假 needle 验红做过 —— 临时塞一枚现拼 ⇒ E1 当场点出文件与行号，删掉回绿。**护栏**：A/B 沿用 #188 那份从门禁形状断言面推出来的清单 + evtcase/pbsub（39 工程 × 两架构 = 78 次发射，两台编译器之间只差这一刀）⇒ 只有 4 份产物被改（ucChartBar 两片、evtcase 两片）各 2 行，每行都验过"整行只换一个标识符"（`old.replace(旧名,新名) == new`），其余 74 份逐字节相同、OFFENDERS 0。 |
| 账 #186（提交 `4d1dbf45`，门 #316（run 37205289964，head `4d1dbf45`，attempt 1）= 11 job 全绿、10 片 `FAIL=0`） | **`Erase m_tvFiles(lIndex).bvData` 这一形以前在 parser 就被拒**（VB2001 "Erase 不支持带下标的形式"），而它是 VB6 的合法写法、真工程在用（`tests/Charts 2020/ucTreeMaps/FontMemRes/PropPagFMR.pag:720` —— 报的行号 594 是折掉 126 行设计期头块之后的，与 B29④ 那条对得上）⇒ 整个工程卡在最前面。**改**：`EraseStmt` 加 `targets`（与 `ReDimStmt.targetExpr` 同一套机制，Fix 100），`varNames` 仍存**去下标的点链名** ⇒ Variant 成员那一问（`isVariantArrayTarget`，Fix 090p）继续只有一处判据；点链→表达式树收成 `Parser::buildDottedNameExpr`，ReDim 那趟改为转调它（**别再抄第二份**）；发码侧 `emitExpr` 出 `VB6_SA_AT(vb6_type_TElem, m_items, 1).bvData` ⇒ 销毁它并置 NULL。1D/ND 那一问不需要新表 —— UDT 成员的动态数组**恒为一维**（结构体发码单点 `cgen_decl.cpp:506`）。**边界刻意守住**：`Erase arr(i)`（无成员）继续报诊断 —— VB6 只在元素是 Variant 时允许，本仓没那条通路，静默降级成"销毁整个数组"比编不过更坏；配一条负例 `erase_neg_indexed` 钉住"不许过头"。**自己撞出来的一条（值钱）**：空括号的次序是**先吃完整条点链、再吃那组括号**（Fix 082 的另一半）。第一版我在基名后就直接判括号，于是 `Erase m_bag.MaxWidths()` / `Erase .MaxWidths()`（VBFlexGrid 6590 就是这形）残留一个裸 `(` ⇒ VB2003/VB2002。**是 A/B 逐字节比抓到的**：78 份快照里 flex 两片从 5.3 MB 掉成 1.3 KB ⇒ 快照**体量**差比"差异行数"更早报警 —— 行数那种判据对"整片塌掉"反而钝。**判据四面**（`tests/erase_sub` 两片 + 一负例 + 一形状针）：①`EA01-KEEP s0=30 s2=50`（只擦那一格，拦"销毁整个数组"）；②`EA02-RECYCLE s1=20 len=1`（那一格可再分配并用 —— ReDim 自己会先销毁旧数组 ⇒ 没置 NULL 就是**双释放**、现场直接崩；反过来没真销毁会读到残留的 40）；x64 与 x86 都 `keep_ok=True recycle_ok=True`。**负控 = `4d1dbf45` 那台编同一份夹具** ⇒ VB2001 + VB2003、BUILD rc=1、一条读数都不出现。③`erase_neg_indexed`（`Test-CompileFail`，针取消息里的**英文片段** —— GBK 控制台下中文会被折行/转码，既有那批负例也是这个口径）；④`erase_member_shape`（`Test-CodegenNote`）钉住"销毁 + 置 NULL"那一整行，Absent 是退回整数组那一形。**护栏**：A/B = 40 工程 × 两架构（80 次发射，`a159b85d` 那台 vs 现在这台）⇒ 生成的 C **逐字节相同 78/78**（新夹具两个旧快照里没有，不算）。**顺带把下一格量出来了**：ucTreeMaps 现在零诊断过 parser、真编译推进到 cl 才红（10×C2039 / 2×C2059 / 1×C2198）⇒ 见 B29⑦ 与新账 #192。 |
| 账 #191（提交 `f5b4a03b` = B29⑥，门 #317（run 37208673511，head ea3eeeec，attempt 1）= 11 job 全绿、10 片 FAIL=0） | **工程内 UserControl 的数组元素访问以前不走直发那条出口** —— Fix 112 只给"单枚控件"接了 `vb6_UC_InstanceOf` 那一层，数组元素另有一条值上下文通路（`cgen_expr_member_precheck.inc`）不认识工程内的 .ctl => 整条落进 `vb6_ComCall` 兜底，而宿主值不是 IDispatch ⇒ 交回来是**空值**（编得过、跑得起来、就是静默不干活；探针 `b189probe.out` 实测 `uArr(0..2).SW()` 三枚全空）。**改**：那段 40 行从发码点抽成唯一出口 `emitUcInstanceMemberExpr`，两条路各交自己的宿主串（单枚 `vb6_hwnd_<名>` / 元素 `vb6_CtrlArr_GetAt(&vb6_arr_<名>, i)`），顺序放在控件属性表与 COM 兜底之前。**连带第二条（左值侧，独立缺陷）**：改道之后 `ucChartBar1(i).Font.Size = ...` 落进 `tryRewriteCOMLvalue` 的字符串级折叠，它只取 `prop_get_` 那一段、把尾巴 `.Size` **整段丢掉** => `prop_let_Font(elem, <double>)`（值槽是 `vb6_ComIface_Font*`）= Form2.c 四条 C2440，工程从"编得过"退回编不过；修 = 折叠之前加一条"带尾巴的 prop_get_ 目标"落点 —— 取回那枚对象再写一层（StdFont 本身是真 IDispatch），认不得的尾巴 `return false` 让它当场红。**判据三面**：真跑证人 `U-ARRM-methods=3`（x64+x86）+ 发码针 `ucarr_member_call`与 `ucobj_chain_write`（后者钉真实工程）+ 哨兵 U1~U4（假 needle 验红：改 `(Pattern C-tail)` 一名 => U4 点名）。**真编译配对读数（x86）**：ucChartBar rc=0 且出 exe；ucProgressCircular 错误类严格是基线子集（C2059 归 #188，剩 B29③④ 两条）。**护栏**：A/B 80 份 => 只有 6 份被改、逐行归因、OFFENDERS 0。**留给下一轮**：①extender 属性（`uArr(i).Left` 仍 AV）= 新账 **#193**；②"单枚那条链"没有夹具（整面 80 份快照里带尾巴的写只有元素这一形）；③任何把访问从兜底挪到直发的刀，都要连带问一句"左值侧谁在按字符串改写它" —— 基线上这条链根本到不了 Pattern C/D2。 |
| 账 #194（提交 `40bbef67` = B29⑦ 的第二条，门 #318（run 37211897524，head 9bb2d08b，attempt 1）= 11 job 全绿；能下到的 3 片日志各自 FAIL=0，剩下 8 片与工件zip被本机网络代理拦成 HTML 页（预签名 URL 全拿不到），所以 optdef 两片的 CI 逐条 PASS 行**没有物证** —— 由 job 结论 + 与 evtcase 同款接线 + 本地两台真跑推定，下一轮补读） | **VB 的整数类型后缀是词法记号，不该出现在生成 C 里** —— 发码侧 `visit(LiteralExpr)` 本来就按**数值**重打（Integer 无后缀 / Long 带 L / 超 32 位带 LL），但语义层那份 `evalOptionalDefault` 是**另写的一份**：Integer·Long 两支直接 `return lit->rawText` ⇒ `Optional ByVal FontIndex As Long = 0&` 发成 `if (!_has_FontIndex) (*FontIndex) = 0&;` = C2059 "bad suffix on number"（真工程物证：ucTreeMaps 的 PropPagFMR.pag:740/886 两条，`.build/b192new/ucTreeMaps/c3-error.log`）。**改**：新增 `src/common/int_literal.hpp` 一处权威 `intLiteralText(数值, 是否 Long 档)`（LL 那一步的 32 位界限判定照搬：MSVC 的 long 是 32 位，`2147483648L` 会退成 unsigned long）；发码侧两支、语义侧两支、`cgen_util_ctrl.cpp` 里 Slider 设计期那两处手拼 `+ "L"` 全改读它（Slider 那两处换完**字节不变**）。**判据三面**：①新夹具 `tests/optdef` 两片两头钉 —— `OD-VAL=True`（四枚默认值 12/3/16/0 各按声明落地，十六进制那枚顺带证明"按数值重打"没改错数）+ `OD-EXPL=True`（显式实参照样赢；只钉前头那条的话"恒取默认值"也绿）；**负控 = `ea3eeeec` 那台真编同一份夹具** ⇒ OptForm.c 116/118/119 三条 C2059 + C2065 "H10"、rc=1、一条读数都不出。②发码形状针 `optdef_default_shape`（Absent 是抄源码那三形）与 `optdef_default_shape_real`（钉在**真工程** ucTreeMaps 那一行上 —— 该工程今天还因 #192 红着，但发码面已过，所以这枚针现在就能红能绿）。③哨兵 `scripts/check_int_literal_shape.ps1`（I1 手拼整数后缀=0 / I2 权威一次定义且留着 LL 界限与无后缀支路 / I3 调用点>=3 且两侧各有 / I4 语义侧不许再 `return lit->rawText`）；假 needle 验红：把语义侧那一支改回 rawText ⇒ I4 当场点名，还原回绿。**真编译配对读数（x86, ucTreeMaps）**：C2059 2→0，C2039 10 与 C2198 1 原样（属 #192）。**护栏**：A/B = 40 工程 x 两架构 = 80 份（BASE = `ea3eeeec`）⇒ **被改的产物 0 份**、OFFENDERS 0 —— 这条读数本身是信息：整面存量里没有一处带后缀的 Optional 默认值 ⇒ 它只在真工程里响，**夹具必须自己带一份**，否则规则又会被"全绿"掩盖（与账 #188 的 `12f` 同一课）。**顺带量出的独立缺陷另立 §B30（账 #195）**：`3%` 在词法层就被拒。 |
| 账 #195（提交 `963c5e71` = §B30，门 #319（run 37215210897，head a7524ab7，attempt 1）= 11 job 全绿；可读到的 4 片各自 FAIL=0，里面 `[SYNTAX] int_suffix_forms` / `[SYNTAX-FAIL] int_suffix_neg_range` / `int_suffix_shape` 三条都 PASS，而且顺带把 #318 欠的那条物证补上了 —— `[VBP] optdef` 与 `optdef_x86` 两片在 CI 上真跑真 PASS。静态哨兵所在那一片的日志仍被本机网络代理拦成 HTML 页 ⇒ `int_suffix_sites` / `int_literal_shape` 的 CI 行还是没有物证（本地两台全绿 + 假 needle 验红做过）） | **VB 的 Integer 后缀 % 在词法层就被拒** —— 类型后缀的消费在词法器里**抄了四份**（十进制 + &H + &O + &B）外加一张 radixDigits 剥离表，而 `case %:` 那一支只吃字符不置标志（`&`/`!`/`#`/`^` 都置），于是 `3%` 落回"无后缀十进制按数值大小定档"那一段，parseIntLit 看见残留的 % 就报"十进制数字超出 64 位整数表示范围"，一条合法语句级联 9-13 条诊断、整个工程卡在最前面。**读数**：七形（十进制 / 三种进制 / 负数 / 表达式 / 参数默认值）修前全红，`12&` 与 `&H10^` 与无后缀三形一直通 = 证人；上一轮那台（ea3eeeec）读数完全相同 ⇒ 存量，与 #194 那条"把 0& 抄进生成 C"不是一件事（那条过了 parser、这条压根过不去）。**改**：四处一起认下 % 并置 isInteger，十进制那支发 IntegerLiteral（按数值），剥离表补 %；显式 % 超出 -32768..32767 **报词法错**而不是按 int32 收下再让 int16 截（那是静默把 2147483648 变成 -2147483648）。**判据四面**：正例夹具 `tests/intsuffix`（Test-SyntaxMulti）+ 边界负例 `tests/intsuffix_neg`（针取 ASCII 片段 "(-32768..32767)")+ 形状针 `int_suffix_shape`（三种进制各按数值落地 `vb6_ChkInt(255)/(15)/(5)`，参数默认值那形顺带钉住 #194 的出口）+ 哨兵 `check_int_suffix_sites.ps1`（假 needle 验红：改回空支 + 剥离表去 % ⇒ 四条一起点名）。另把 #194 的 optdef 夹具先前为绕开本缺陷改成的无后缀两处恢复原样，x64+x86 真跑到 OD-DONE。**护栏**：A/B 80 份 ⇒ 被改的产物 0 份（存量里一处 % 都没有 ⇒ 又只能靠夹具响，与 #188/#194 同一课）。 |
| 账 #197（提交 `9565bdbb` = §B32，门 #322（run 37235573381，head ada93533，attempt 1）= 11 job 全绿、11 片日志**全部下到**（这轮代理没拦）：10 片各自 FAIL=0（第 11 片是 Build C3.exe，不打这个计数），本轮五条新用例在 CI 上逐行真 PASS —— `[VBP] scalemode` / `scalemode_x86` / `[CODEGEN-NOTE] scalemode_design_write` / [CODEGEN-NOTE] scalemode_read_real` / `[STATIC] scalemode_writers`） | **`VB6_ScaleMode` 这个窗口属性全仓 0 个写者** —— 读的一侧早就齐了（`vb6_GetScaleMode` / `vb6_WindowScaleModeSelf` / `vb6_ContainerScaleMode`，缺省 1=缇），写的一侧 `vb6_SetScaleMode` 一个调用点都没有 ⇒ 谁问 ScaleMode 都答缺省。这一档决定的是**量出来的数对不对**（ScaleWidth/ScaleHeight、控件几何 #175、文字量纲 #177 全按它折算），不是某一枚控件的外观，所以症状是"处处差 15 倍"而不是"某处坏"。**改**：一处权威 `emitDesignerScaleModeProp` × 三条落点（顶层创建路 / 容器子控件路 / 窗体 WM_CREATE 的 P20-40 块）+ 读写两张表成对补 `scalemode`（读给**同一个** `vb6_WindowScaleModeSelf`，程序读到的数与换算用的数不可能再分家）。RTL 一字节未动 ⇒ 不碰 `c3rtl.rc`，绕开 #156 那条旧资源坑。普查 93 份设计块 19 处（UC 10 已通 / PictureBox 6 / Form 3）⇒ 口径「设计块写了就发」，不按值筛也不开控件型白名单。**判据四面**：夹具 `tests/scalemode` 两片（存设计值 + 问窗口的**比值**判据不钉绝对数 + 运行期切档逐数相等 + 另一枚没被写坏 + Frame 里那枚钉第二条创建路）；负控=改前那台真编 ⇒ SmForm.c 166/182 两条 C2039、rc=1；形状针两条（夹具 + 真工程 ucTreeMaps）在改前那台当场红；哨兵 W1~W5（假 needle：注释掉容器那条落点 ⇒ W3 点名 exit 1；**工具教训**：植针要按人会怎么改坏植——先前改成 `XX` 前缀，`Contains` 当子串照样命中，等于没验）。**护栏**：A/B 82 份 ⇒ 12 份被改、OFFENDERS 0（10 份各 1 行新增 SetScaleMode；2 份 = VBFlexGridDemo 各 2 行，`Me.ScaleMode` 从 `vb6_ComGetProp` 兜底换成真读数）。**真工程配对**：ucTreeMaps x86 C2039 **4→2**；czUI-main（唯一 Form 级声明 3=Pixel 的工程）真编真起窗。**顺带新账 #199**：`.pag` 的设计块控件从来没被创建（PropPagFMR 段 CreateControl 0 处 vs 同工程 Form1 段 14 处，页里的 `vb6_hwnd_Picture1` 是按需新建、初值 NULL 的槽位）⇒ 属性页里 `With Picture1` 全打在空句柄上，#196 做完 UnicodePrint 也还是白画，控件线顺序改成 #199 → #196。 |
| 账 #196（提交 `50676a8b` = §B31 的 hDC 那一半，门 #323（run 37238929955，head f26f9590，attempt 1）= 11 job 全绿、10 片各自 FAIL=0（smoke / compile 两片这轮重下到了），本轮五条新用例逐行真 PASS：`[VBP] dcsurf` 与 `dcsurf_x86`、`[CODEGEN-NOTE] dcsurf_dc_shape` 与 `dc_read_real`、`[STATIC] control_dc`） | **「这枚控件的绘图 DC 从哪儿来」在 RTL 里早就有一处口径，但句柄交不回 VB 代码** —— Print/Cls 走的那条（派发期用外层 BeginPaint 挂上的 `VB6_PaintDC`，否则回落 `GetDC`）一直是 static 且只在内圈用，`.hDC` 没有出口 ⇒ 真工程那一形 `With Picture1 : TextOut .hDC, ...`（Charts 2020/ucTreeMaps 的 PropPagFMR.pag:258）只能撞 `cgen_expr_with.cpp` 那条 "hwnd.成员" 兜底 = C2039。**改**：把那条口径抽成 `vb6_ControlDrawDC`（三头共用：Cls / Print / 新出口），新出口 `vb6_GetControlHDC(void* hwnd)` 按 **VB6 的"一个对象一张"** 语义把派发期那张直接交出（不缓存不释放）、否则把回落那张按 HWND 缓存进 `VB6_ObjectDC` 并归还白拿的那张，归还点在 PictureBox/Image 那层自己的 `WM_DESTROY`（#185 那套分层槽位）；后端读写两张表各补一行 `hdc`，**只登记 PictureBox 与 Form**（通用行 = `List1.hDC` 也答一个数 = 伪造成功，同 #192）。RTL 动过 ⇒ touch `c3rtl.rc` 重编 C3.exe。**判据四面**：新夹具 `tests/dcsurf` 两片四头钉（DS01 反复读+With 三个数相等非零 / DS02 两枚互不相等 / DS03 `GetDeviceCaps>0` 证明是一张**活的 DC** / DS04 `GetPixel` 各自等于**自己那枚**的设计期底色；DS05 只钉前缀，dpi 不钉绝对值）；**负控 = 改前那台真编同一份夹具** ⇒ DcForm.c(167) C2039 "hDC" 不是 "HWND__" 的成员、rc=1；形状针两条（夹具 + 真工程，Absent 连晚绑定那一形 `vb6_ComGetLongPtrProp(X, L"hDC")` 一起钉）；哨兵 `check_control_dc.ps1` D1~D5（五条假 needle 按"人会怎么改坏"植：自己抢 DC / 权威不认 PaintDC / 不归还 / 销毁不撤名 / 表给成通用行 ⇒ 逐条能红，还原回绿）。**护栏**：A/B 86 份 ⇒ 被改 8 份、**OFFENDERS 0**，14 对 K2 + 5 条 WD（`VB4001 Unknown control property '.'hDC'` 那条诊断消失是这一刀的目的）逐行归因。**真工程配对**：ucTreeMaps x86 C2039 **2→1**（只剩 TextHeight；TextOutW 的 C2198 是它的级联，一起没了）。**顺带一条实测订正**：Charts2020 `Proyecto1`/`ucProgressCircular` 的 `Picture2.hDC` 以前走晚绑定 `vb6_ComGetProp`，探针（`.build/b196probe`，改前那台真编真跑裸形）量到 `HDC=671159075 / DPI=96` ⇒ 那是一张**活的** DC，不是"恒答 0"那一族（`uc_hostmodel_getprop.inc:86` 本来就认 hDC）；本刀在裸形上改的是**语义**（每读一次新取一张、从不归还 → VB6 的一个对象一张 + 销毁归还），真红的那一半是 With 形（C2039）。**留下的口径**：Form 自己那张缓存 DC 暂无归还点（语料 `Form.hDC` 现存 0 处、进程结束由系统收回，记下不装红）；本账欠按 HWND 的 `TextHeight`/`TextWidth` 与带 HWND 的 `ScaleX`/`ScaleY` 两条，顺序 #199 → #196。 |
| 账 #196 第二条（提交 `060eddca` + 针面订正 `ac3df329` = §B31 的 TextHeight/TextWidth，门 #324（run 37242648474，head 6db5203b）= **failure，逐 job 归因：产品侧一片绿**（Build success、其余 9 片各自 FAIL=0、dcsurf / dcsurf_x86 / dcsurf_dc_shape / dc_read_real / control_dc 五条真 PASS），红的只有 Tests (syntax) 里我新加的两条 [CODEGEN-NOTE]：`missing: vb6_ControlTextHeight((void*)_vb6_with_0` —— 我本地拿**前缀** `_vb6_with_` 验过就登记成 `_vb6_with_0`（那个 Sub 里两个 With picA：hDC 那条是 _vb6_with_0、文字量那条是 _vb6_with_1），另一条又在结尾多写一个 `)`（产物那位置是逗号）⇒ 登记的根本不是产物里的串；按 emit 逐字重验后改针（两台 present 1/2/1 与 1/1、absent 全 0），产品一字节未动。门 #325（run 37243854108，head ac3df329，attempt 1）= 11 job 全绿、10 片各自 FAIL=0，七条相关用例逐行真 PASS） | **按 HWND 的文字量缺半个出口** —— `With Picture1 : .CurrentY + .TextHeight(Text)`（ucTreeMaps 的 PropPagFMR.pag:265）是 #196 收完 hDC 之后该工程仅剩的那条 C2039。**改**：不发明新机制 —— 现有 `controlZeroArgMethod` 那套「表交名字、码头拼实参」本来就支持带实参（With 形交裸名 + `pendingChainObj_`，调用点把句柄**前置**进实参表），新表 `controlOneArgMethod` 只登记 textheight/textwidth × Form/PictureBox 两档（**不给通用行**，同 #192/#196 那条口径），两条码头各查一次；实参签名表补两行 `{"void*","BSTR"}`（漏了就是把 vb6_VARIANT 裸喂给 GetTextExtentPoint32W = Fix 113）。RTL 新增 `vb6_ControlMeasureTextPx`（DC 走**同一处** `vb6_ControlDrawDC`、字体走 `WM_GETFONT` = 与 Print 同源）+ `float vb6_ControlTextWidth/Height`，**单位过 `vb6_ScalePxToUser` + `vb6_WindowScaleModeSelf`**（#175/#197 那一份表）。判据：夹具加长三头 TH01 两形逐数相等非零 / TH02 缇框 vs 像素框 **>4 倍**（实测 16 vs 240=15 倍，比值判据不钉绝对数）/ TH04 宽度随文字变；负控 = 改前那台 rc=1 无 exe；哨兵 D6~D9 五条假 needle 逐条能红。**顺手抓到哨兵一条假绿**：D9 第一版把两个函数当一段正则抓，改坏宽度那半时高度里的换算还在 ⇒ 整段照过；改成"一个一个函数各自取身体"才红（与"注释行不算""计数要打印实测值"同一类自欺）。**A/B** 86 份 ⇒ 被改 8 份、OFFENDERS 0（32 对读法替换 + 8 条 WD；配对正确性要用**整行**判 —— 公共前缀会把 `Co` 折掉，残段里搜全名搜不到）。真工程配对：ucTreeMaps 两台 C2039/C2440 **全清**，红点推进到 **LNK2019 ×3**（三枚 GDI+/字体内存桩没登记 ⇒ 新账 #201）；`VB4001 Unknown control property` 只剩 1 条。**没接的**：不带括号的语句形（语料 0 处）；窗口字体那条判据今天当不了判据（改了字号量回来不动、`FontPixelHeight` 那句整行不打 ⇒ 新账 #200，夹具里只留读数 TH06）。 |
| 账 #201（提交 `4ced55cc` + 记账 `871a627f` = §B36 的三枚 DI 桩，门 #326（run 37247978838，head 871a627f，attempt 1）= 11 job 全绿、10 片各自 FAIL=0；CI 自己量的两条新用例真 PASS —— `[VBP-BUILD] charts_ucTreeMaps ... PASS (679,424 bytes)` 与 `charts_ucTreeMaps_x86 ... PASS (585,728 bytes)`，顺带 `[STATIC] di_stubs_census`、`[VBP] dcsurf`、`[STATIC] control_dc` 三行还在原样绿） | ucTreeMaps 编译面全清之后卡在链接期三条 LNK2019。**改**：三枚桩手写进 `src/rtl/core/di/vb6_di_stubs.c`（就是"手写那一档"，gen_di_stubs 不动它），`check_di_stubs.ps1 -Update` 把基线从 620 冻到 624（少一个就红，正是这道哨兵存在的理由）。读数说清楚为什么生成器没发它们：`AddFontMemResourceEx` 那一形第三参在真源码里写的是 `ByRef DESIGNVECTOR`（无 `As` ⇒ Variant），落在生成器"形状不定就跳过"那一档；GDI+ 那两枚名字里带 Font 会被路由到 text 族，但当时那次会话没引用到 ⇒ 定义集里就没有。**一个刻意的取舍**：`AddFontMemResourceEx` 的 pdv 交 **NULL** 而不是把那个 `vb6_VARIANT*` 转手给 GDI —— VB6 那句 `AddFontMemResourceEx(.bvData(0), n, 0&, cnt)` 里 `0&` 的意思是"没有 design vector"，而 Win32 对这一意的拼法就是 NULL；把 variant 的地址当 `PDWORD` 交进去，GDI 读到的是 VARIANT 头两个字，那既不是 NULL 也不是 design vector，是伪造。gdiplus 那两枚走 `LoadLibrary`+`GetProcAddress`（`gdiplus.h` 是 C++ only，且本仓的 Declare 一律不要求导入库），解析不到回 `GpStatus` 的 2，与 gdiplus 族生成桩同形；GDI+ 的 `GdiplusStartup` 桩早就有（工程自己调）。**结果**：ucTreeMaps 两台（x64/x86）**第一次真编真链出 exe** = 659,968 / 562,688 字节，诊断面 **0 error**、116 条 VB 警告（`VB3001 未声明的标识符: Ambient / Extender / Is / PropertyPage` 那一族 = 属性页与 extender 面还欠着，见 §B34/#199 与 #193）。**护栏**：这一刀只动 RTL 与清单 ⇒ 发码面该一字不变，A/B 86 份（BASE = 改前那台的 emit 快照）**changed=0、offenders=0**。**升格**：`Test-VbpBuild charts_ucTreeMaps` + `_x86` 两条进清单（#187 那条口径的正解 —— 列进来这一次就是"从此不许退回编不过"），run_tests.ps1 里那条"刻意不列"的注释同步改成只剩 ucProgressCircular。**下一步**（这一格解开的）：把工程真跑起来的观测面 —— 属性页那条路（#199 的甲/乙）与 extender/`VB3001` 那一族；以及 #200 那条窗口字体的归因（文字量唯一还没被运行期验证的一头）。 |
| 账 #187（提交 `a51e84aa` = B14 后半，门 #321（run 37230353501，head 59811f14，attempt 1）= 11 job 全绿、每片 FAIL=0） | **门禁里以前没有"真工程必须编得过"这一格正面积** —— 三个 helper 各管两头：`Test-Vbp` 要跑起来校验输出（第三方真工程没有自退出口 ⇒ 进不去），`Test-VbpBuildFail` 只钉"必须红"，中间那格"必须绿"没人管。后果这两轮反复撞到：#188 的 `12f`、#190 的 LNK2019、#191 的数组元素、#192 的 With 光标全落在 Charts 2020 的 UC 子工程上，而它们在清单里**压根不存在**，"编不过"在门禁里连红都算不上。**改**：新增 `Test-VbpBuild`（真编译真链接 + 断本轮新出的 exe 在，不跑），三枚 UC 子工程 × 两架构六条进清单（ucChartBar / ucChartArea / ucPieChart；实测 x64 696,320 / 576,000 / 576,000，x86 590,336 / 496,128 / 498,176 字节）。两处刻意的写法：①**输出目录按用例隔离** —— 这四份 .vbp 的 ExeName32 全写 `Proyecto1.exe`，共用 $OutDir 会互相盖掉（#166 那条的姊妹坑：名字修对了还会串味）；②**先删干净再编、rc 才是判据** —— 只 `Test-Path` 会命中上一轮的旧 exe。ucProgressCircular / ucTreeMaps 刻意不列（今天还红着：B29③④ 与 #192/#196），列进来就是把已知的红当基线，等编过之后再加那一次才算"不许退回编不过"。**能红性**：同一份 helper 指 ucTreeMaps ⇒ `[VBP-BUILD] FAIL rc=1 exe=False`、计数器 total=3 pass=2 fail=1；指三枚绿的 ⇒ 全 PASS。分片走 `Enter-VbpShard` 轮转。 |
| 账 #200（提交 `77ce2b65` = §B35 的控件窗口字体，门 **待回填**） | **控件窗口字体换了、文字量跟着不动** —— 做 #196 第二条时撞见的，那时只留了读数 TH06。**归因靠探针、不靠猜**：`.build/b200probe/fontprobe.c` 拿一枚**裸 STATIC**（谁也没子类化过）实测 `WM_SETFONT` 之后 `WM_GETFONT` 回 NULL、`STM_SETFONT`/`STM_GETFONT` 那一对同样回 NULL ⇒ 这个窗口类**本身不记字体**。于是原来那三条读法（`vb6_GetControlLogFont` = .FontName/.FontSize 背后那一条、Print 落笔前的选字体、#196 的 `vb6_ControlMeasureTextPx`）在 PictureBox/Label 那一类窗口上永远拿不到用户设的那张，只能按 DC 的默认字体画；而 `picA.FontSize` 读回来还是 18（那是另一份自存的属性）⇒ 两头都不报错，只有量出来的数不动（设计期 18pt 与运行期改 20pt 都答 16）。证人 `FontPixelHeight` 那句整行不打也归到同一条因：它自己就是 `WM_GETFONT + GetObject`，拿到 NULL 直接 `return 0`。**改**：一处出口 + 一份自存 —— 新增 `static HFONT vb6_ControlFont(HWND)`（先问窗口、回 NULL 再读窗口属性 `VB6_CtrlFont`），setter 把自己 `CreateFontIndirectW` 出来的那张存进这个**新名字**（#185「一层一个属性名」同纪律），三条读法全改走这一处；旧字体的找法刻意是「先读自存、找不到才问窗口」，顺序反了会对真记字体的 EDIT/BUTTON **双删**（`WM_GETFONT` 回来的正是我们上一轮存进去的那张）。**顺带修掉一处 GDI 泄漏**：改前 STATIC 每写一次字体漏一张，因为那句 `DeleteObject` 依赖的 `WM_GETFONT` 恒回 NULL、旧字体永远找不到。已知边界（写在 §B35，不装绿）：窗口销毁时最后一张字体不被 `DeleteObject`（Windows 回收属性表、不认识 GDI 对象），普通控件没有统一的 WM_DESTROY 挂钩。**判据**：TH03 从「只留读数」升回真判据 `ok7 = (tA > tB) And (tB2 > tB) And (pfA >= 18)`，`TH03-FONT=True` 进 `$dcSurfExpected`（两台各一条）；真跑读数两台**逐行相同** = `TH06-FONTRAW a=29 b=16 b2=27 fsA=18 pfA=24 pfB=27`。**护栏**：哨兵 D10 四条 + 假 needle 真红过（把 measure 那一处换回裸 `SendMessageW(hw, WM_GETFONT, 0, 0)` ⇒ `FAIL D10 问窗口字体的那一行 = 2 处 -> vb6forms_ctrl.c:325 | :733`，换回即绿）；**D10 自己第一枪红得不该怪产品** —— 它报的是 3 处，多出来那两条是我给两行的**行尾注释**写了 `WM_GETFONT`：哨兵跳行首 `//`、不跳行尾注释，于是「注释把形状写出来」就自造了红 ⇒ 措辞改成不嵌那个 token。**A/B** 86 份（BASE = #201 那份 emit 快照、NEW = 现在这台）⇒ changed=2、**OFFENDERS=0**，那两份就是 dcsurf 两台、逐行归因全是夹具自己新增的那几行；注释定稿后又单独重发两台 emit 与快照**逐字节相同**（VB 注释不进发码）。真工程配对：ucTreeMaps 两台仍 rc=0、诊断面 0 error、exe 659,968 / 562,688 与 #201 那轮同尺寸。**新账 #202（§B37）**：探针这一轮补了一枚裸 `BUTTON`(BS_GROUPBOX)，**同样回 NULL** ⇒ `vb6forms.c` 里 Frame 标题带那处读法一并定了罪，修法就是把 #200 这一处出口跨文件递过去。 |
