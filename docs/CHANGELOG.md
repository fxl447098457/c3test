# 变更日志

> 项目：**C3 — 现代化 VB6 编译器**（官网 <https://c3.vb6.pro> ｜ 仓库 <https://gitcode.com/woeoio/c3.vb6.pro>）
> 把 VB6 工程（`.vbp` / `.bas` / `.cls` / `.frm` / `.frx`）直接编译为原生 Windows x64 / x86 可执行文件或 ActiveX DLL，源码无需修改、无运行时 DLL 依赖。
> 实现：C++17 手写前端（词法 LL(2) → 递归下降 + Pratt → 两遍语义）+ 生成 C 代码后 shell-out 给 MSVC `cl.exe`/`link.exe`；运行时静态链接。
> 当前版本：`0.10.1`（见根目录 `VERSION`）
> 组织方式：本项目**按「里程碑 M1–M33」+「阶段 P0–P31」推进**，不是严格 semver。因此本文按「版本/里程碑 + 日期」分节，节内按类别归类。
> 详细台账：[`ai/004-进度表.md`](../ai/004-进度表.md)（阶段与里程碑）、[`ai/开发历程/`](../ai/开发历程/)（逐次修复日志）、[`ai/022-源码拆分进度表.md`](../ai/022-源码拆分进度表.md)（源码拆分专题）。
> 维护约定：每个里程碑收口、或每个重要变更批次落地时，在本文顶部追加一节。日期取该批次的收口日。

---

## 0.10.1（未发布）— 2026-09-14 ~ 2026-09-17

### 重构

- **源码拆分专题**（台账 `ai/022-源码拆分进度表.md`）：把「函数多、单个函数不巨型」的巨型文件按**家族**拆到约 500 行/文件，全程**纯搬移、零行为改动**，每批次都做代码行多重集校验。
  - RTL 侧（各 `.c` 是独立编译单元，需处理跨文件 `static` 可见性 → 提升为外部链接 + 新增 `<模块>_internal.h`）：
    - `src/rtl/core/vb6comserver.c` 956 → 5 文件 + 内部头（`beffe9f`，首个 RTL 拆分）
    - `src/rtl/core/vb6com.c` 1608 → 6 文件 + 内部头（`6caa32a`）
    - `src/rtl/core/vb6forms.c` 3749 → 11 文件 + 内部头（`a7bf82b`，RTL 最大文件）
  - C++ 侧（`CCodeGen::` 成员方法 + `namespace` 内自由函数）：
    - `src/backend/cgen_expr.cpp` 6948 → 7 文件（`0cda42c`）
    - `src/backend/cgen_stmt.cpp` → 4 文件（`5314e58`），再细分至约 500 行粒度（`f08c834`）
    - `src/backend/cgen_util.cpp` 3244 → 8 文件（`e34cc7d`）
    - `src/semantics/semantic_analyzer.cpp` 2814 → 6 文件 + 内部头（`ef4d5f8`）
    - `src/com/typelib_parser.cpp` 1155 / `src/preprocessor/preprocessor.cpp` 653 / `src/parser/parser_stmt.cpp` 1561 / `src/backend/decl/cgen_decl.cpp` 1604（`cd14766`）
    - `lexer` / `msvc_driver` / `parser` 三个大文件（`60e4b9a`）
  - 目录重组：`src/backend` 按家族归入 `expr/`、`stmt/`、`decl/`、`module/`（`57e4716`）
  - 未纳入本专题：`vb6rtl.c`(5328)、`cgen_form.cpp`(2222)、`cgen_expr_call.cpp`(3627) 等「单函数巨型」文件，需函数级抽取，属另一量级任务
- `RTL` 回归源码级编译，移除预编译 `.lib` 嵌入方案（`78df0b3`）

### 清理

- 删除 `src/rtl/` 下 **20 个纯占位空文件**：`src/rtl/core/` 16 个空类壳（`variant`/`string_funcs`/`safearray`/`math_funcs`/`fileio`/`error_handler`/`datetime`/`conversion` 的 `.cpp`+`.hpp`）+ `src/rtl/platform/win32/` 4 个单行注释文件；空目录 `src/rtl/platform/` 一并移除（`0b1ae74`）
- 顶层目录名统一英文，清理失效文件与大目录（`06887b8`）
- 清除全项目 AIGC 水印并撤销防注入机制（`5353843`）
- 根目录文档归入 `ai/`，清理测试与编译残留（`a5ffc92`）

### 新增

- 版本文件 `VERSION`（`d98f666`）
- 无参数运行 `c3` 时显示帮助（等价 `-h`），不再直接报错（`dfbfc08`）；`--help` 补全「性能/杂项选项」段并对齐排版（`5d002fc`）
- 冒烟测试：`smoke.bas` 用例接入 `smoke` 分类（`43a923d`）+ 差分冒烟测试框架（`b69db59`）
- `publish/` 发布目录：构建成功后自动复制 `C3.exe`/`.pdb`（`7a66e1a`）

### 构建与工具链

- 脚本可在任意机器 / CI 运行：`vswhere` 自动定位 VS 工具链（`7c16f7b`）、环境变量配置支持（`7e9ed0d`）、`dev.ps1` 路径与 `vcvarsall` 改为环境变量 + vswhere（`44a01b2`）、测试脚本路径按脚本位置推导（`8a7fb52`）
- 合并 `ferock/0.10.1` 分支：融合环境变量机制与 vswhere 自动探测（`1b4191b`）

### 修复

- 中文路径参数导致无法打开源文件（`363a15e`）
- 控制台输出 UTF-8 中文乱码：启动时切 65001、退出恢复（`c5379ec`）
- UTF-8 路径构造与 WithEvents 事件槽 `C2223`（`48b0b5c`）
- 项目类实例被误按 COM 后期绑定处理（`140b387`）
- VB6 → C3 的多个语法解析问题（函数调用、成员访问）（`0925bd1`、`5ad1f05`）
- COM 事件生成与类型解析（`24841c7`）
- 测试产物不再落进仓库根目录（`5323e6b`）；外部 COM 依赖缺失时跳过而非判失败（`45798f0`）
- `hello.bas` 示例改用 `Debug.Print` 替代 `MsgBox`，避免无人值守运行卡死（`57ecebf`）

### 文档

- 新增根目录 `README.md`；仓库地址更新为 gitcode（`a40e8b8`、`99bfdf8`）
- 开源参考来源清单更新至 README 与官网，新增 RustASP / aspgo / twinBASIC / VisualBasic6.g4（`2772595`、`a1fab37`）
- VB6 原生 GUI 兼容度盘点，确立**内置控件 67% → 100%** 为主方向（`62153e7`）
- 存档 VB6 隐藏 COM 接口实现机制：`VBLoader.Load/Unload`(004)、`VBPrint.Print`(003)、`IEnumVARIANT` 的 `For Each`(005)
- `todo.md` 改为全员任务索引 + `todo/` 目录 + Issues 双轨制（`bf90d72`、`7a57ff4`）
- 修正绘图 `PSet`/`Line`/`Circle`/`Print` 为**关键字语句**而非方法调用（`4269548`）

---

## v0.1.0-m33-vbman-dll — 2026-09-11

首个**可注册的 `VBMAN.dll`**（2,566,144 字节，`regsvr32` 可注册；运行期功能尚待对齐）。这一天把链接期错误从「有」清到「零」。

### 里程碑

- **M31 链接期 `LNK1104` 清零**：`msvbvm60`（10 模块 14 处 `Declare`）与 `cryptdlg` 均无可用导入库 → 对 VB6/VBA 运行时库与 `cryptdlg` 停止生成 `#pragma comment(lib,…)`（`Declare` 早已走 `extern vb6_di_<name>` + RTL 桩）；RTL 补 4 个原生桩（`VarPtr` 恒等 / `__vbaObjSetAddref` / 序号 644 经 `dumpbin` 反查即 `VarPtr` / `CertSelectCertificateW` 动态加载）。另修 `c3rtl.rc` 嵌入依赖缺失（致 `C3.exe` 仍嵌旧库，症状极具误导性）。
- **M32 链接期 `LNK2019`/`LNK2005` 清零**：未解析符号 18 → 0（`driver --dll` 补链 `vb6rtl_gui.lib`；cgen 成员名规范化 / 事件分派 / 公有字段 / `InStrB`；RTL 补 `SavePicture` 与 `ord_413` 桩）；随后 23 个 `LNK2005` 重复定义清零（事件包装函数加模块前缀；跨模块同名模块级变量由 driver 预扫描后转 `static`）。
- **M33 `PictureBox`/`Frame` 的 `.Align` 停靠**（清除最后 2 条 `VB4001`）：新增 RTL `vb6_SetControlAlign`/`vb6_GetControlAlign`（贴父客户区边缘、停靠轴撑满），并在控件属性表登记 `align`。

### 修复

- 链式默认属性访问 `X(a)(b)` 走后期绑定（`ac69fc0`）
- `Debug.Assert` 条件的顶层 `=` 不再被当作赋值（`f799e55`）
- 模块限定调用以限定模块名为前缀，不再被全局同名过程的 `sourceModule` 覆盖（`bad7b27`）；模块限定调用前缀兜底取工程模块规范名（`ec7fa10`）
- COM 接口字段的链式成员访问走后期绑定（`e0df780`）
- UDT 数组成员元素类型 + `VarPtr` 左值宏白名单（`b6ff125`）

### 工具

- 新增 `C3_CRASH_TRACE` 崩溃栈追踪（`dbghelp`）与 `bisect -Forms` 开关（`97eb89c`）

---

## 2026-09-01 ~ 2026-09-10 — VBMAN 编译期错误清零攻坚

以真实工程 `vbman`（120+ 模块、含 125 个窗体）为靶子，把 MSVC 编译期错误从 **777 条清到 0**，全程用 `bisect` 子集 + 错误数增量跟踪。这是本项目单阶段最长的一次连续修复。

### 修复（按簇）

- **`C2440`（Variant/COM 打包解包簇，最大簇）**：`Variant` 在 `int32_t` 上下文自动转 `Long`；具体类型值自动 `vb6_VariantFromValue`；`ByVal Variant` 形参收 `Object` 实参；类字段 `Variant` 判定；COM 结果前缀判定改用 `find(==0)`；`cExprIsVariant` 名单补 `vb6_LoadResData`；顶层 COM 结果实参先 `vb6_VariantFromComResult` 再提取
- **`C2198`（参数太少）**：值上下文无括号类方法引用按形参表补默认参数；`As New` 类字段；无参类方法 `Optional` padding；只写属性 LHS 去掉被 pad 的 `value` 实参
- **`C2039`（成员不存在）**：类字段名规范化回声明名（VB6 大小写不敏感）；`With` 多级成员路径逐段展开；嵌套 `With` 目标表达式延后入栈；`For Each` 遍历 `ParamArray` 用 `SAFEARRAY*` 专用 API
- **`With` 块成员链**：嵌套类 / COM 字段解析、`With` 对象引用为 `Variant` 时改用 `VariantToObjectVal`、类字段写消费 COM 标记、`With void*` 目标恢复 COM 分派
- **`For Each`**：数组源（`Split`/`Filter`/`Array`）走数组迭代并物化；COM 集合源为 `vb6_ComCall` 对象指针时不再按 `Variant` 提取；类元素解包
- **事件 / COM**：事件回调传入 handler 对象；类工厂指针字段 NULL 初始化；`vtable` COM event sink 携带宿主实例；注册 ADODB 枚举别名；`Friend` 成员编译为 `extern` 而非 `static`
- **字符串 / BSTR**：`String + BSTR返回调用` 视为拼接；移除 `vb6_VariantFromComResult` 误判前缀；`wrapToBSTR` 识别项目 `Variant` 返回函数；`StrConv(LoadResData(…))` 子串误判
- **声明 / 作用域**：模块级 `Dim As New` 变量跨函数持久注册（`moduleNewVars_`）；局部声明提升；模块级 `Type`/`Enum` 预扫描 + 用户符号覆盖内置；`VB6 无 As` 类型声明成员默认 `Variant`；`Date` 类型类字段归入浮点成员集
- **常量**：局部常量按字面量定类型 + 模块常量位运算折叠
- **参数**：`ParamArray` 元素赋值改写 `vb6_PA_SetXxx`；`UBound` 不重复提取；`ByRef Variant` 索引免取址；`IsMissing` 语义/表项；命名实参 `ByVal Variant` 打包；`ByRef` 数组形参复合字面量用 `SafeArray1D**`；`NULL`/`nullptr` 实参改用复合字面量取址
- 其它：`As Collection`/`Forms` 等 VB6 内置对象类型识别为 `Object`；数组下标为 `Variant` 时转 `Long`；`String*N` 定长字符串 + `LSet`

### 性能 / 编译优化（opt1–opt4）

- `opt1`：`cl.exe` 加 `/MP` 并行编译，加速大型项目（`665eb4f`）
- `opt2`：新增 `--no-warn` 静默指定 ID 警告（`vbman` 构建抑制 `VB3001`/`VB3003`）（`8bdb221`）
- `opt3`：`--incremental` 增量编译，基于内容哈希的 obj 级缓存（`54c8f70`）
- `opt4`：`--trim-includes` 裁剪未实际引用的跨模块 `include`，降低 `cl` 预处理量（`8fe7b6c`）
- 实测报告与 A-B 数据：`ai/编译效率优化/`；新增 `optbench` 120 模块 demo 与生成脚本（`24b9844`、`635ad32`）

### 工具

- 新增 `scripts/fix_tool.ps1`：极简 `.vbp` 编译器，用于快速迭代验证 C3 修复（`6b296a7`）

---

## 2026-08-31 — x64 / `LongPtr` 支持与 Fix 081 系列

### 新增

- x64 支持：`LongPtr`/`LongLong` 类型 + 指针安全运行时（`e713613`）

### 修复

- 代码生成：UDT 数组 / 属性访问 / 对象表达式 / `ByRef`（`5f60481`）
- 语义：模块级 `Type`/`Enum` 预扫描 + 用户符号覆盖内置（`d701dcc`）
- 窗体：控件初始文本延迟设置 + 窗口创建修复（`2a8c545`）
- TypeLib：`dispinterface` `ByRef` 参数标记 + `SetFuncAndParamNames` 错误诊断（`9243c33`）

### 工具 / 文档

- 编译选项文档、帮助文本、编译测试脚本（`da8aa95`）

---

## 2026-07-18 — M29 / M30

### 修复

- **M29 `COM DLL DISPID` 不一致**（`Calc.Add` 找不到成员）：三层根因 —— TypeLib 全局 `DISPID` vs `dll_entry.c` 按类重置、跨符号表 `Symbol*` 不共享、`Property Let/Set` 跨表查找缺失。5 处修复：新增 `Symbol.comDispid` 字段、driver 写回、`Property Let/Set` 跨表 fallback、`comDispid` 跨表同步、cgen 读取 `comDispid`。9/9 COM 验证 + 82/82 回归零失败
- **M30 x86 交叉编译路径拼接**（`findVcvarsallBat` 缺反斜杠）：`install_msvc.bat` 永久写入 `VCINSTALLDIR` 无尾随 `\`，`--arch x86` 时路径拼接错位致 `findVcvarsallBat` 返回空、`cl.exe` 以 x64 环境编 x86 报 `LNK1112`。仅在 C++ 层归一化 `base` 尾随 `\`（兼容两种 `VCINSTALLDIR` 形态），不改 `install_msvc.bat` 以规避 `reg.exe` 反斜杠转义陷阱

---

## 2026-07-14 — M27 / M28

### 修复

- **M28 VB6 字符串语义 + `Format` 完整实现**：`vbCrLf` 换行 `0D0D0A` 修复、双引号 `""` 转义修复、`Format` 函数约 600 行重写（命名/自定义/字符串格式）；含 KIMI 8 项代码优化（共享代码 / IOCP / `SCRIPTITEM_ISSOURCE` / `lock_guard`）。82/82 回归零失败
- **M27 第三方 COM 事件 SourceIID**：事件 Sink 响应 source IID + RTL 库重建（`/MT`），VBMAN / WinHTTP 事件验证通过。82/82 回归零失败

---

## 2026-07-06 ~ 2026-07-10 — P24 / P25

### 新增 / 修复

- **P24 COM 优化专题**（后期绑定 + 集合枚举 + 服务质量）：P24-01 ~ P24-12 + Bug1~Bug2 全部完成。82/82 回归零失败
- **P25 `frxParse` 端到端运行修复**：COM 参数化属性 `PUT` / `MsgBox` `Variant` / `CStr` COM 适配 / late-bound `VARIANT` / `For Each` 跳空 + `Count` 安全上限。`frxParse` 完整运行，82/82 回归零失败

---

## 2026-07-02 ~ 2026-07-05 — M12 / M13 系列

### 新增

- **M13 双架构支持**：x64 + x86（`2026-07-03`）

### 修复

- **M12 `FRX` 二进制资源读取**：`FrxReader` + `.frx` 解析 + `vb6_LoadPictureFromMemory` + GBK 文本 + `ListBox List/ItemData`（2 字节前缀修复）+ `TextBox MultiLine/ScrollBars`。后续 FIX1~FIX4 依次解决背景渲染 / `Form.Picture` / `.ico` 直接解析 / Graphical 按钮子类化绘制 / 闪烁消除 / `ImageList.Picture` 链式访问
- **M13-FIX `COM ProgID`**：用 `ProgIDFromCLSID` 反查真实 `ProgID`（替代 coclass 短名 fallback）+ cgen 运算符优先级 bug（`_progId` 局部变量）
- **M13-FIX2 `COM Item PROPERTYGET`**（`dic.Item` 返回空）
- **M13-FIX3 `Variant` 数组索引**（`a(i)` 其中 `a` 是 `Variant`）
- **M13-FIX4 定长字符串 `String*N` + `LSet`**

---

## 2026-07-01 — M22 真实 VB6 项目编译

首个**真实 VB6 工程**（`realVBP` + 窗体 demo）编译链接成功。

### 修复

- 6 个运行时问题 + Issue A/B/C 三项深度修复
- `std::filesystem::path` 双重编码全面修复
- `isVarName2` 作用域修复
- `Declare ANSI` 无泄露
- `MsgBox` 空参数

74/74 回归零失败。

---

## 2026-06-30 — P17 ~ P21，74/74 基线达成

### 里程碑

- **M14：74/74 回归零失败基线达成**，全阶段 P0–P17 完成，编译器核心特性全覆盖

### 新增（兼容性填平）

- `With` 默认属性、`Picture` 完善、多维 `ReDim Preserve`、`GoSub` 覆盖（P17.1–P17.4）
- Batch A：`Format$` / `MsgBox` / `Mid$` 语句 / `On GoTo` / `CCur` / `CDec` / `RGB` / `QBColor` / `FileDateTime` / `FileLen` / `SendKeys` / `AppActivate`
- Batch B：`KeyUp` / `Mouse` / `QueryUnload` / `DblClick` / `Activate` / `Deactivate` / `GotFocus` / `LostFocus`（含按钮类）
- Batch C：`Clipboard` / `Screen` / `Printer` / `Forms` 集合 + `Option Compare Text/Binary`（含 `gdi32.lib` 链接修复）
- Batch D：`AscW` / `ChrW` / `AscB` / `ChrB` / `Timer` / `StrConv` / `Filter` / `VarPtr` / `StrPtr` / `ObjPtr` / `LSet` / `RSet` / `WeekdayName` / `MonthName` / `FormatCurrency` / `FormatNumber` / `FormatPercent`
- Batch E：`Choose` / `Switch` + 10 个财务函数 + `Lock`/`Unlock` + `Reset` + `Partition`
- Batch F：控件子类化基础设施 + `PictureBox` `GotFocus`/`LostFocus` + `MouseEnter`/`MouseLeave` + 控件级鼠标/键盘事件

---

## 2026-06-29 — P14 ~ P16 / M11 / M12

### 里程碑

- **M11 产物治理 + 控件属性 + 兼容性深化综合验证**：P11–P15 全部完成，控件属性 13 类 + COM 动态调用 + Bug 修复 6 项 + 兼容性深化 14 项。74/74 回归
- **M12 ActiveX DLL 完整能力验证**：TypeLib 内建生成 + WithEvents 控件事件绑定 + Error 430/459 双杀。74/74 回归

### 修复

- **P15.1 `RaiseEvent` `VARIANT` 类型修复**
- **P16 WithEvents 控件事件**：HWND 匹配 WndProc 分发 + 属性读写 + `Set HWND` 提取
- **P6.6 VBA 早绑定 + WithEvents 事件**：Error 430 / 459 双杀 + 6 个边界 bug（BUG-1~6）

---

## 2026-06-27 — P8 ~ P10 / M8.5 / M9 / M10

### 里程碑

- **M8 窗体控件综合验证**：综合窗体（控件 + 数组 + 菜单 + WebView）+ MDI 测试全通过，66 回归零失败
- **M9 TypeLib 内建生成验证**：72/72 回归 + TypeLib 生成 + DLL 编译 + MIDL 彻底移除（替代方案见 P9）
- **M10 RTL 内嵌零配置编译验证**：71/72 回归 + 零配置 E2E + 会话目录清理

### 新增

- **P9 TypeLib 内建生成**：用 `CreateTypeLib2` 内建生成 `.tlb`，彻底删除 MIDL 代码路径
- **P10 RTL 内嵌与编译流程封闭**：RC 资源嵌入 + 会话目录 + `c3-error.log` + dist 更新
- **P8 已知限制修复**：7 项全部完成，72/72 回归零失败（M8.5）
- **P7 窗体控件**：P7.1–P7.10 全部完成，含 WebView 宿主、菜单系统、`TypeLib` 嵌入 DLL（`MIDL` + RC 自动化）
- **P6.8/P6.9**：`.vbp` 指定 `CLSID` + 清理死代码
- **P6.6 BSTR OLE 兼容修复**

---

## 2026-06-26 — P5 ~ P7 / M6 / M7 / M8

### 里程碑

- **M6 多模块 VB6 工程编译 + 综合验证**：`M6Test.vbp` 通过，6 个 bug 修复（递归 / `ByRef` / UDT / `Enum` / `InStr` / 类型转换）
- **M7 COM + 对象模型综合验证**：`M7Test.vbp` 4/4 PASS（`CreateObject` + 后期绑定 + 类模块 + `WithEvents`），`Property` 同名共存修复，48 回归零失败
- **M8 窗体控件综合验证**（收口于 06-27）

---

## 2026-06-24 ~ 2026-06-25 — P0 ~ P4 / M1 ~ M5

项目从零到「能编译并运行 VB6 程序」的四个里程碑，四天完成。

### 里程碑

- **M1 词法器正确扫描 VB6 源文件**（2026-06-24）：词法器 + Unicode + LLVM tool mode
- **M2 解析器生成完整 AST**（2026-06-24）：约 2900 行解析器，软关键字 + 注释跳过，多测试用例零错误
- **M3 语义分析完成类型检查**（2026-06-25）：两遍扫描 + 120+ 内置符号；修复 Pratt 非运算符 token 丢失表达式 bug；36 测试零错误
- **M4 编译出 `hello.exe`**（2026-06-25）：C 代码生成 + MSVC 驱动 + 最小 RTL；20/36 端到端通过（16 个缺 `main` 属预期）
- **M5 运行含数组 / 文件 I/O 的程序**（2026-06-25）：数组 + 文件 I/O + 错误处理全通过，5/5 回归；RTL 约 1076 行 + 366 行头

### 关键决策

- 后端选择「生成 C 代码 + shell-out MSVC」而非直接 LLVM（LLVM backend 后续置为可选、默认关闭）
- 产物**静态链接运行时、零依赖**，无 DLL 形式
- RTL 最终走 **C（`/std:c11`）** 路线，早期规划的 C++（`.hpp`/`.cpp`）方案由 C 侧完全取代
