# 89 - C3Fix 093b：LNK2005 重复定义清零，125 模块链接出 VBMAN.dll

> 日期: 2026-09-11
> 提交: `2185e12`
> 相关: 88（LNK2019 清零）、87（LNK1104 清零）

## 0. 结果

`FIX_bisectfrm.vbp`（125 模块、含窗体、ActiveX DLL）**首次全链路成功**：

| 项 | 值 |
| --- | --- |
| C3 exit code | **0** |
| 日志 `LNK` 行数 | **0**（LNK2019/2001/2005/1169 全清） |
| 产出 | `outfrm\VBMAN.dll` 2,565,632 字节 + `VBMAN.lib` / `VBMAN.exp` |

## 1. 症状

88 号把 LNK2019 清成 0 后，链接器报出 **23 个 LNK2005 重复定义符号** + `fatal error
LNK1169: 找到一个或多个多重定义的符号`。分三族：

| 族 | 符号 | 定义处（举例） |
| --- | --- | --- |
| A | `vb6_evt_wrap_m_listensocket_*` / `vb6_evt_wrap_m_socket_*` / `vb6_evt_wrap_m_osocket_*`（15 个） | `cModbusSlave.c`、`cModbusTransportTCP.c`、`cWebSocketServer.c` … |
| B | 窗体级变量 `Sad` `m_Theme` `m_State` `PosVal` `TopVal` `m_ParentToast` `m_TagName`（7 个） | `FToastDrawer.c` 与 `FToastCenter.c` |
| C | `Delay` | `Tools.c` 与 `mDelay.c` |

## 2. 根因

### A. 事件包装函数名缺模块前缀

各模块都有**同名** `WithEvents` 变量（`m_ListenSocket` / `m_socket` / `m_osocket`），
包装函数名此前只由「变量名 + 事件名」拼成 → 不同模块生成**同名**非静态函数 → LNK2005。

### B / C. 模块级变量名未去重

VB6 **允许**不同模块各自声明同名模块级变量（模块内引用绑定自身副本；跨模块引用必须写
`模块名.变量名`），但 C 语言下两个模块会生成**同名全局符号**：

- `Tools.bas:22  Public Delay As New cDelay` 与 `Delay/mDelay.bas:13  Public Delay As New cDelay`
- `Toast/FToastDrawer.frm:96-106` 与 `Toast/FToastCenter.frm` 各有一组同名 `Dim`

> 注：`Delay As New cDelay` 在 VB6 里确实允许重名（各自一个实例），不是源码笔误。

## 3. 修法

### 3.1 事件包装函数加模块前缀（族 A）

新增两个统一命名助手（`cgen.hpp`，私有内联）：

```cpp
std::string evtWrapperName(varLower, evtName) {    // vb6_evt_wrap_<模块>_<变量>_<事件>
std::string comEvtWrapperName(varLower, evtName) { // vb6_com_evt_<模块>_<变量>_<事件>
```

6 处命名点全部改走助手（保证「声明 / 定义 / 引用」三处必然一致）：

- `cgen_base.cpp`：`.h` 前向声明 ×2（内部类 + COM 源接口）、`.c` 实现 ×2
- `cgen_stmt.cpp`：`Set obj = ...` 处的 sink 表回调赋值 ×2

包装函数只在**本模块**内被引用（sink 是 `Set` 处的函数内 `static` 局部变量），加前缀
既消除撞名又不改变链接属性。

### 3.2 撞名模块级变量转 `static` 内部链接（族 B / C）

- **driver 预扫描**（`src/driver/driver.cpp`，在主生成循环前）：统计每个模块级
  `VariableDecl` 的名字 → 模块集合，**出现在 ≥2 个模块**的名字进 `dupModuleVarNames`。
- **生成端**（`cgen.hpp` 新增 `isPublicModuleDecl()` 两个重载 + `setStaticModuleVarNames()`）：
  命中名字的模块级变量不再生成「.h `extern` + .c 全局」，改为 `.c` 内 `static`。
  变量/常量分别重载：`ConstDecl` 生成的是 `#define` 宏，不产生符号，不受影响。
- `cgen_decl.cpp` 里 9 处 `node.access == AccessLevel::Public` 判定统一改走该函数。

语义等价性：VB6 下模块内引用绑定本模块副本，跨模块引用必须模块限定（不会落到裸名），
故 `static` 不改变行为，只是各模块保留自己的副本。

## 4. 踩坑：类模块成员被误判为「撞名」

首版扫描遍历了**所有**模块的 `VariableDecl`，结果 `StaticClass/cVBMAN.cls` 里的
`Public ToolsUtf8 As New cToolsUtf8`（类字段）与 `Tools/Tools.bas` 的同名 `Public`
被判为撞名 → **Tools.bas 的全局被误转 `static`** → 所有跨模块引用崩：

```
99 × error C2065: “ToolsHttp”/“ToolsFso”/“ToolsStr”/“ToolsArray”/“ToolsUtf8”: 未声明的标识符
 6 × error C2198 / 6 × error C2223（连带）
```

修法：扫描 **跳过 `module->isClassModule`** —— 类成员是结构体字段（`me->x`），
从不产生全局符号，不能参与撞名判定。

## 5. 验证与日志判读（重要）

- 判据要看 **C3 exit code** 与 `outfrm\` 目录时间戳，**不要**只看 `[BISECT] errors=`。
  `c3-error.log` 只在**失败时**才写；链接成功后日志仍是上一轮的旧文件，本轮就因此
  误读了陈旧的 `errors=111`（实际 exit=0、`LNK` 行数 0、DLL 时间戳为最新）。
- 回归命令：

```
powershell -ExecutionPolicy Bypass -File scripts/dev.ps1 -SkipTest
C3 --vbp vbman\src\FIX_bisectfrm.vbp --forms --out vbman\src\_fix\bisect\outfrm
```
