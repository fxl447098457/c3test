# 87 - C3Fix 092z-2：无导入库 Lib 重定向内置 RTL + 揪出「C3 嵌旧库」陷阱

> 日期: 2026-09-11
> 提交: `03de92f`（重定向）、`460e621`（构建系统修复）
> 相关: 86（链接期问题首次暴露）、`C3_FIX_HANDOFF.md` 第 10 节

## 0. 本轮目标

86 号把含窗体回归的 `error C` 打到 0 之后，链接期随即暴露两类问题：`LNK2005` 重定义 + `LNK1104: 无法打开文件 msvbvm60.lib`。后者是 **fatal**，会挡在后面真正的「未解析外部符号」清单之前，所以先处理它。

用户提供线索：`_res\lib\msvbvm60.lib`（127,850 字节，2017-10-28）—— 先判断这个文件能否使用。

## 1. 那个 `msvbvm60.lib` 用不了：x86，且符号名对不上

- `dumpbin /headers _res\lib\msvbvm60.lib`：成员全是 `14C machine (x86)` → x64 链接会 `LNK1112`，直接不可用。
- 更根本的是**符号名对不上**。Fix 076 之后 Declare 一律生成 `extern <ret> __stdcall vb6_di_<name>(...)` + `#define <VB名> vb6_di_<name>`，生成物里 `__declspec(dllimport)` 出现 **0 次** —— 导入库这条路径对 Declare 早已废弃。链接器要的是 `vb6_di_VarPtr` / `vb6_di_vb6___vbaObjSetAddref` / `vb6_di_ord_644` 这类 **C3 内部名**，而 MSVBVM60.DLL 导出的是 `VarPtr` / `__vbaObjSetAddref` / 序号。补上真库也只能消掉 `LNK1104`，随即变成 `LNK2019`。
- 完整证据链（本机全盘无此文件、VB6 未安装、SDK/VS 不携带、x64 无 32 位运行时）见 handoff 第 10 节 2.0。该文件只对「序号 → 名称」对照有旁证价值。

## 2. 序号 644 反查 = `VarPtr`（本轮最有用的一次 dumpbin）

```
dumpbin /exports C:\Windows\SysWOW64\msvbvm60.dll
  644  4A 000EEC2C VarPtr
  350  ...          __vbaObjSetAddref
```

于是 `Alias "#644"`（源码名 `SplitLongToBytes`）其实就是 `VarPtr`：x86 下 VarPtr 把入参原样放回 EAX，而 VB6 对 4 字节 UDT 返回值同样取 EAX → 这一声明成了「Long → 4 字节小端位重解释」的技巧。`Alias "VarPtr"`（源码名 `ArrPtr`）与它同源。

`ArrPtr` 的语义也据此核对清楚：VBMAN 的调用点是 `CopyMemory(ArrPtr(a), ArrPtr(b), 4)`（**交换两个 `SAFEARRAY*`**）与 `CopyMemory(lBufPtr, ByVal ArrPtr(a), LenB(...))`（**读出 `SAFEARRAY*`，未分配时为 0 据此判断**），两者都要求返回「数组变量地址」——而 C3 对 ByRef 数组传入的实参**已经是**数组变量地址（生成声明 `vb6_SafeArray1D**`），故桩只需恒等返回。

## 3. 实施（提交 `03de92f`）

1. **生成器**（`src/backend/cgen_decl.cpp`）：对 VB6/VBA 运行时库（`msvbvm60`/`msvbvm50`/`vbe7`/`vbe6`/`vba7`/`vba6`）与 `cryptdlg` **不再生成** `#pragma comment(lib, ...)`。
2. **RTL**（`src/rtl/core/vb6_di_stubs.c`）新增 4 个原生桩：
   - `vb6_di_VarPtr`（`ArrPtr`）：`return (intptr_t)Ptr;`（见上节语义核对）。
   - `vb6_di_vb6___vbaObjSetAddref`：`if (src) src->AddRef(); *(void**)dst = src;`，**不** Release 旧值以对齐原版语义（避免误 Release 非持有引用）。
   - `vb6_di_ord_644`（`SplitLongToBytes`，序号别名）：按小端拆字节返回 4 字节结构；本地定义同布局 `vb6_di_longByteType`（x64 走 EAX、x86 走 EAX，与生成类型 ABI 一致）。
   - `vb6_di_CertSelectCertificateW`：`cryptdlg.dll` **没有导入库**（SDK `10.0.26100.0\um\x64` 实测无 `cryptdlg.lib`），故走 `GetModuleHandleW`/`LoadLibraryW` + `GetProcAddress`（导出序号 15）动态加载。
3. **`scripts/build_rtl_libs.bat` 入库**：原脚本只在被 `.gitignore` 忽略的 `.temp\` 里，而 C3 链接依赖这些 `.lib`（经 `c3rtl.rc` 嵌入 C3.exe），必须可复现构建。

> 附带的坑：该 `.bat` **必须保持纯 ASCII**。第一版写了中文注释，cp936 控制台把 `REM` 行切成碎片并当命令执行（`'vb6rtl.obj' is not recognized ...`），重建结果不可信；改纯 ASCII 后干净通过。

## 4. 陷阱：C3.exe 一直嵌着旧库（提交 `460e621`）

改完、重建库、重建 C3、跑回归 —— **链接期仍报 `vb6_di_VarPtr` 未解析**，看起来像桩没生效。

真凶在时间戳链上：

| 证据 | 值 |
| --- | --- |
| `.build\CMakeFiles\c3.dir\src\driver\c3rtl.rc.res` | **12:17**（当天早些时候） |
| 新建的 `src\rtl\lib\vb6rtl.lib` | **19:45** |
| C3 会话目录提取出的 `rtl\vb6rtl.lib` | 467,468 字节 = **HEAD 版库大小** |
| 新建库实际大小 | 470,076 字节 |

`c3rtl.rc` 以 RCDATA 嵌入 10 个文件（4 头 + 3×x64 库 + 3×x86 库），但 **CMake 只把 `.rc` 自身当依赖** → 重建 `.lib` 后 `.res` 不会重编，C3.exe 继续嵌旧库。

修法：`CMakeLists.txt` 把这 10 个文件显式声明为 `c3rtl.rc` 的 `OBJECT_DEPENDS`（CMake 3.31 + Ninja 实测有效：重建库后 `.res` 时间戳刷新，C3.exe 体积 3,354,624 → 3,359,744）。

## 5. 验证

含窗体全量 125 模块、`--dll`、x64 链接（`scripts/_tmp_bisect.ps1 -Forms`）：

| 指标 | 修复前 | 修复后 |
| --- | --- | --- |
| `error C` | 0 | 0 |
| `LNK1104`（缺库） | 2（msvbvm60、cryptdlg） | **0** |
| 本次新增 4 符号的未解析条数 | — | **0** |
| 未解析外部符号总数 | 451 | **443** |
| `LNK2005` 重定义 | 27 | 27（另一项，未动） |

日志：`vbman/src/_fix/bisect/outfrm/c3-error.log`。

## 6. 教训

1. **「补一个文件」之前先问「这条路径还活着吗」**：`msvbvm60.lib` 的第一反应是找一个库补上，但导入库路径早已被 Fix 076 废弃，补库连符号名都对不上。先确认机制是否仍在用，再决定是补料还是改道。
2. **「改了却像没改」先查产物时间戳链**：RTL 库、C3.exe、`c3rtl.rc.res`、会话目录提取出的库 —— 四个时间戳一对就真相大白；不要先怀疑自己刚写的代码。
3. **`.bat` 里不要写中文注释**：cp936 下会切碎命令行，且失败方式很迷惑（既报错又"成功"）。
4. **度量口径要跟上**：链接期（`LNK1104`/`LNK2005`/未解析符号）要像编译期那样纳入固定基线，否则「全绿」是假象。

## 7. 下一步

1. 按新基线批量补 `vb6_di_*` 桩：从 `c3-error.log` 用 `Select-String -Pattern 'LNK(2019|2001)'` + 正则 `vb6_[A-Za-z0-9_]+` 去重导出（当前 443 条）。
2. 确认 27 条 `LNK2005` 重定义（模块级私有变量未按模块 mangle、事件包装函数重复定义）在 `vbman/build_vbman.bat` 全量链接下是否重现，再决定修法。
