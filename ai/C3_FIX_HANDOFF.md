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
· **② `ucChartBar`：宿主 UC 实例的句柄名发成了没声明的标识符** —— 生成码原文
  `int32_t i_end = (vb6_ComGetIntProp(vb6_hwnd_ucChartBar1, L"Count") - 1);`（`error C2065`）。
  `ucChartBar1` 是**控件数组里那一枚实例**，而实例句柄住在 `vb6_arr_ucChartBar` 里 ⇒ 与账 #157 那条同族
  （拼 `vb6_hwnd_<名>` 而不是走 `ctrlHwndExprForInit` / `vb6_arr_*` 那条既有出口）。修法 = 把这一处也改读那个唯一出口。
· **③ `ucProgressCircular`：同一个过程发了两遍体** —— `Form1.c(10)` 与 `Form1.c(13)` 报
  `error C2084: 函数"void vb6_Fo..."已有主体` + `C2198`（实参数不符）⇒ 某个 `Form_*` 事件在两份名单里各发一次。
  这条要先把两份名单找出来（`grep` 发事件 thunk 的那两处），再定哪一份是权威。
· **④ 顺带一条诊断面的事实**（不算缺陷，记着省时间）：`.pag`/`.frm` 报错的**行号不含设计期头块** ——
  `PropPagFMR.pag(594,17)` 实际指向文件第 720 行（720 - 126 = 594，前 126 行是 `VERSION` + Begin/End 控件块）。
  按报的行号去找源码会一无所获 ⇒ 要么按 `文件行 = 报的行 + 头块行数` 折回去，要么改成报真实行号。


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
