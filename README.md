# C3 — 现代化 VB6 编译器

> 官网：<https://c3.vb6.pro> ｜ 仓库：<https://gitcode.com/woeoio/c3.vb6.pro>（本地路径 `D:\code\vi\c3.vb6.pro`）
>
> C3 把 VB6 工程（`.vbp` / `.bas` / `.cls` / `.frm` / `.frx`）直接编译为**原生 Windows x64 / x86 可执行文件或 ActiveX DLL**，
> 源码无需任何修改，无运行时 DLL 依赖。

---

## 一、这是什么

C3 是一个用 C++17 从零手写的 VB6 编译器前端 + C 代码生成后端：

```
VB6 源码 (.bas/.cls/.frm/.frx + .vbp)
   ↓ 编码检测 (GBK / UTF-8 / UTF-16)
   ↓ 预处理   #Const / #If / #ElseIf / #Else
   ↓ 词法     LL(2) + Unicode
   ↓ 语法     递归下降 + Pratt（14 级优先级）
   ↓ 语义     两遍扫描 + 符号表 + 类型系统 + TypeLib 导入
   ↓ 代码生成 VB6 → C
   ↓ 编译链接 shell-out 到 MSVC 的 cl.exe / link.exe
原生 EXE / DLL（静态链接 VB6 运行时）
```

**核心信条：不支持的语法必然报错，绝不静默误编。** 错编译比不编译更危险。

### 产品定位

| 维度 | 说明 |
|------|------|
| 定位 | 旧 VB6 项目迁移工具 + 用 VB6 语法写新项目的工具 |
| 形态 | 编译器 + 工具链内核（CLI），**不做 IDE** |
| 开源 | 是 |
| 与 twinBASIC 的关系 | 不竞争。twinBASIC 是闭源一体化 IDE，C3 做开源底层，对接任意前端 |
| 目标用户 | 需要迁移旧项目、偏好开源的开发者 |

---

## 二、当前状态（2026-09-14）

| 项目 | 状态 |
|------|------|
| 版本 | `C3 --version` → `0.10.0` |
| 里程碑 | M1 ~ M33 全部达成 |
| 最新基线 tag | `v0.1.0-m33-vbman-dll`（提交 `5f4bed1`） |
| 编译期 | 大型真实工程（VBMAN，125 模块含 4 个窗体）**error C = 0**，链接期 LNK2019 / LNK2005 / LNK1104 均已清零 |
| 产出验证 | VBMAN.dll 2,566,144 字节，可 `regsvr32` 注册（运行期行为仍在对齐中） |
| 内置函数覆盖 | 143 个 VB6 内置函数中已实现 126 个（88%），详见 `ai/021-VB6语法成员覆盖率统计.md` |
| 回归测试 | `tests/` 下 98 个 `.bas` + 13 个 `.vbp`，由 `scripts\test.bat` 驱动 |
| 代码规模 | 编译器 C++ 约 4.0 万行，RTL（C）约 1.8 万行 |

> 权威进度以 `ai/004-进度表.md` 为准；每次会话恢复修复上下文请读 `ai/C3_FIX_HANDOFF.md`。

---

## 三、快速开始

### 3.1 环境要求

| 工具 | 版本 | 说明 |
|------|------|------|
| Visual Studio 2022 Community | MSVC 14.4x | 提供 `cl.exe` / `link.exe` / `rc.exe` |
| CMake | ≥ 3.20（实测 3.31.6） | 需先加载 vcvarsall 才在 PATH |
| Ninja | 1.12+ | 生成器 |
| Windows SDK | 10.0.22621+ | 窗体/COM 相关头文件 |

`vcvarsall.bat` 默认路径：
`C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat`

### 3.2 构建编译器

```bat
scripts\build              REM 增量构建
scripts\build clean        REM 清理 .build 后完整构建
```

产物在 **`.build\C3.exe`**（注意：不在 `.build\Release\` 下）。

PowerShell 一键构建 + 测试（Agent 会话推荐）：

```powershell
scripts\dev.ps1                  # 构建 + 测试
scripts\dev.ps1 -SkipTest        # 只构建
scripts\dev.ps1 -SkipBuild       # 只测试
```

### 3.3 编译一个 VB6 程序

```bat
.build\C3.exe tests\hello.bas -o output\hello.exe
.build\C3.exe 工程.vbp --output-dir output
.build\C3.exe --dll --arch x64 -o output\VBMAN.dll 工程.vbp
```

### 3.4 运行测试

```bat
scripts\test               REM 全部
scripts\test run           REM 仅运行测试
scripts\test compile       REM 仅编译测试
scripts\test syntax        REM 仅语法检查
```

```bat
scripts\compile tests\hello.bas        REM 单文件编译（自动带 MSVC + RTL 环境）
scripts\run hello                      REM 运行 output\hello.exe
```

---

## 四、命令行参数

完整参数见 `ai/C3_COMPILE_OPTIONS.md`，常用项：

| 参数 | 说明 |
|------|------|
| `-o <文件>` | 输出文件路径 |
| `--output-dir <目录>` | 输出目录（默认源文件所在目录） |
| `--arch <x64\|x86>` | 目标架构（默认 x64；32 位 COM 组件用 x86） |
| `--target <平台>` | win-x86 / win-x64 / linux-x64 / macos-arm64 |
| `--gui <模式>` | native / webview / none |
| `--dll` | 编译为 ActiveX DLL（自动生成并嵌入 TypeLib） |
| `--syntax-only` | 只做语法检查 |
| `-d, --define <N=V>` | 定义条件编译常量，如 `-d:DEBUG=-1` |
| `-O <0-3>` / `-g` | 优化级别 / 调试信息 |
| `--typelib <文件>` / `--no-auto-typelib` | TypeLib 显式引用 / 禁用自动加载 |
| `--progid` / `--libid` | ActiveX DLL 的 ProgID 前缀 / TypeLib LibID |
| `--dump-tokens / --dump-ast / --dump-symbols / --dump-preprocess / --dump-ir / --dump-frm` | 各阶段中间产物转储 |
| `--emit-c` | 只生成 C 代码，不调用 cl/link |
| `--keep-for-debug` | 保留中间文件（默认清理） |
| `--compat-check` | 兼容性检查模式 |
| `-v` / `-h` / `-V` | 详细输出 / 帮助 / 版本 |

中间文件目录：`%TEMP%\C3C\<会话ID>\`。调试链接错误（LNK2019 等）时加 `--keep-for-debug` 检查生成的 C 与符号名是否一致。

编译 ActiveX DLL 等场景**必须在 MSVC 环境中运行** C3（先 `vcvarsall.bat x64`）。

---

## 五、目录结构

```
c3.vb6.pro/
├── src/                   编译器源码（C++17）与 VB6 运行时（C）
│   ├── common/            诊断、源码管理、编码转换（GBK↔UTF-8）
│   ├── lexer/             词法分析（LL(2) + Unicode）
│   ├── preprocessor/      条件编译 #Const / #If
│   ├── parser/            递归下降 + Pratt 表达式（分 expr/stmt/decl 三个文件）
│   ├── ast/               AST 节点、Visitor、打印器
│   ├── semantics/         符号表、类型系统、两遍语义分析
│   ├── project/           .vbp / .frm / .frx 解析
│   ├── com/               TypeLib 解析（前期绑定）
│   ├── typelib/           CreateTypeLib2 内建生成 .tlb
│   ├── backend/           C 代码生成（cgen_expr/stmt/decl/com/form/util）+ MSVC 驱动
│   ├── driver/            CLI 入口、编译流水线编排、RTL 资源解包
│   ├── rtl/               VB6 运行时：core/*.h|*.c + lib/ 预编译静态库（x64/x86）
│   └── ir/                历史遗留空壳（已停用，不参与构建产物）
├── tests/                 回归测试：98 个 .bas + 13 个 .vbp + 多模块/COM/窗体用例
├── scripts/               构建、测试、编译、运行脚本（见下）
├── ai/                    全部设计文档、进度表、开发历程（99 篇）
├── publish/               发布包：C3.exe + 内置迷你 MSVC 工具链 + 右键菜单 + demos
├── www/                   官网 c3.vb6.pro 静态站点源码 + scp 部署脚本
├── archive/vbman/         VBMAN 大型 VB6 库（真实工程回归靶子）+ 编译问题日志
├── output/                测试编译产物（exe/out/err），git 忽略
├── .build/                CMake + Ninja 构建目录，可随时删除重建
├── _res/ logo/ pic/       品牌素材、favicon、演示动图
└── reference/             预留给外部参考项目 clone（当前为空）
```

### 脚本工具（`scripts/`）

| 脚本 | 用法 | 说明 |
|------|------|------|
| `build.bat` | `build [clean]` | 构建 C3.exe |
| `test.bat` | `test [all\|run\|compile\|syntax] [verbose]` | 回归测试 |
| `compile.bat` | `compile <source> [outdir]` | 编译单个 .bas/.frm/.vbp |
| `run.bat` | `run <exename> [timeout]` | 运行 output/ 下的 EXE |
| `dev.ps1` | `dev [-SkipBuild] [-SkipTest]` | 一键构建 + 测试 |
| `env.ps1` | `. .\scripts\env.ps1` | dot-source 加载 MSVC 环境 |
| `build_rtl_libs.bat` | — | 重建 RTL 静态库并嵌入 C3.exe（改了 RTL 后必跑） |

---

## 六、架构要点

**决策 006：后端走 C 代码生成 + MSVC，不走 LLVM。**
`src/backend/llvm_backend.*`、`pe_writer.*`、`src/ir/*` 均为 2 行空壳，CMake 中 LLVM 后端默认 `OFF`。
CLI 仍保留 `--dump-ir` / `--emit-llvm` 开关，但没有消费方。

**决策 008：库优先（library-first）。**

| 产物 | 内容 |
|------|------|
| `vb6c3-core.lib` | 词法 / 语法 / 语义 / 符号 / 类型 / 诊断 |
| `vb6c3-cgen.lib` | C 代码生成后端 |
| `C3.exe` | 薄 CLI 驱动（链接上述两个库） |

**RTL 内嵌（P10）。** 4 个 RTL 头文件 + 6 个预编译静态库（x64/x86 各 3 个）以 `RCDATA` 资源嵌入 `C3.exe`
（见 `src/driver/c3rtl.rc`），运行时由 `rtl_embedded.cpp` 解包到 `%TEMP%\C3C\<会话ID>\rtl\` 再交给 `cl.exe`。
因此**用户机器上不需要安装任何 VB6 运行时**，产物静态链接、零依赖。
改动 `src/rtl/` 后必须执行 `scripts\build_rtl_libs.bat`，否则 C3.exe 里嵌的仍是旧库。

**RTL 组成**（`src/rtl/core/`）：

| 文件 | 职责 |
|------|------|
| `vb6rtl.h/.c` | BSTR 字符串、VARIANT、SAFEARRAY、数学、转换、日期、文件 I/O、Err 对象、Format |
| `vb6forms.h/.c` | Win32 窗体与控件实现（缇↔像素 1/15）、消息循环、菜单 |
| `vb6com.h/.c` | COM 客户端：CreateObject / GetObject / IDispatch 后期绑定 / VARIANT 封送 |
| `vb6comserver.h/.c` | COM 服务端：类工厂、DllGetClassObject / DllRegisterServer |
| `vb6_di_*.c` | Win32 API 转发桩（解决 x64 下 msvbvm60 等无导入库的问题） |

---

## 七、已支持与未支持

### 已支持

- **语言**：模块 / 类模块 / UDT / Enum / 常量、Pratt 14 级表达式、软关键字、条件编译（含 `Win64` / `VBA7` 等内置常量）、
  GoSub/Return、With、For Each、On Error / Resume / Err 对象、默认属性解析、ParamArray / Optional / 命名参数
- **COM**：CreateObject / GetObject、前期绑定（TypeLib 导入）与后期绑定（IDispatch）、Implements、WithEvents 事件
  （内部类 / 窗体控件 / 外部 COM 三条路径）、IEnumVARIANT 集合枚举、ActiveX DLL 产出（内建生成 .tlb）
- **窗体**：`.frm` + `.frx` 二进制资源（图片 / 图标 / ImageList / 文本）、控件数组、MDI、菜单、Timer
- **控件**：Form / MDIForm、CommandButton、TextBox、Label、CheckBox、OptionButton、ListBox、ComboBox、
  Frame、PictureBox、HScrollBar / VScrollBar、Image、Timer、Menu、WebBrowser（WebView2）
- **运行时**：126 个 VB6 内置函数（字符串 / 数学 / 日期 / 转换 / 文件 I/O / 数组 / 财务 等），静态链接

### 尚未支持

- 控件：Shape / Line、Data / OLE、DriveListBox / DirListBox / FileListBox、SSTab、Toolbar / StatusBar、CommonDialog
- 控件数组之外的容器控件嵌套
- 多接口 `Implements IFoo, IBar`（已决策跳过）
- 部分内置函数：注册表 4 函数（GetSetting / SaveSetting / GetAllSettings / DeleteSetting）、
  FormatDateTime、CVErr、GetAttr / SetAttr、Erl / Tab / Spc 等，清单见 `ai/021`
- LSP / DAP / 格式化器 / Linter / 窗体设计器 —— 见 `ai/013-工具链与生态建设规划.md`，均**尚未实现代码**

> 遇到不支持的语法，C3 会明确报错，不会静默生成错误代码。

---

## 八、文档导航（`ai/`）

| 文档 | 用途 |
|------|------|
| `003-开发计划.md` | 分阶段计划与里程碑定义 |
| `004-进度表.md` | **权威进度**：P0~P33 全部阶段与任务状态 |
| `009-产品定位与最终成果展望.md` | 定位、可行性分级、VB6 语言边界情况清单 |
| `011-参考项目综合分析与技术借鉴.md` | 对 FreeBASIC / vb6parse 等的参考分析 |
| `012-第二期VBA-VBS-ASP规划.md` | 第二期方言扩展（VBA / VBS / ASP） |
| `013-工具链与生态建设规划.md` | LSP / DAP / 格式化器 / 包管理器 等长期规划 |
| `020-P20遗留项任务清单.md` | 遗留缺陷清单 |
| `021-VB6语法成员覆盖率统计.md` | 内置函数覆盖率与缺口优先级 |
| `C3_COMPILE_OPTIONS.md` | **完整命令行参数参考** |
| `C3_FIX_HANDOFF.md` | **修复任务交接**：新会话直接读它继续修 bug |
| `开发历程/` | 99 篇按阶段记录的修复与决策过程 |
| `ENV.md`（根目录） | 开发环境速查：路径、脚本、编码约定 |

VBMAN 编译过程的问题日志见 `archive/vbman/c3log/001.md ~ 054.md`。

---

## 九、开发约定

| 规则 | 说明 |
|------|------|
| 换行符 | 全项目统一 **CRLF** |
| VB6 工程文件 | `.frm` / `.bas` / `.cls` / `.vbp` 必须 **GBK**，否则 VB6 IDE 打开乱码 |
| C/C++ 源码 | **UTF-8**（与 CMake / MSVC 默认一致），CMake 已加 `/utf-8` |
| 部分 `.md` 带 UTF-8 BOM | 编辑时保持原样 |
| 构建目录 | `.build\` 可随时删除重建；遇 release-only 崩溃先 `--clean-first` 全量重建再复现 |
| 崩溃追踪 | 设环境变量 `C3_CRASH_TRACE=1` 启用 dbghelp 栈追踪 |

---

## 十、路线图（摘要）

1. **VBMAN.dll 运行期行为对齐** —— 当前编译 / 链接 / 注册全通，下一步是运行期功能差异
2. **内置函数补齐** —— DoEvents、FormatDateTime、CVErr、注册表 4 函数、GetAttr / SetAttr
3. **控件补齐** —— SSTab、Toolbar / StatusBar、CommonDialog、Shape / Line
4. **c3-lsp 语言服务器 + VS Code 插件** —— P0 级生态组件（`ai/013`）
5. **c3-dap 调试适配器** —— 依赖代码生成阶段输出 VB6 行号 ↔ C 行号映射
6. **第二期方言** —— VBA / VBS / ASP（`ai/012`）

详细规划与工期估算见 `ai/013-工具链与生态建设规划.md`。

---

## 十一、参考来源

C3 在语法规则核对、测试语料与实现思路上参考了以下开源项目。源码副本放在 `reference/`（该目录已被 `.gitignore` 忽略，不入库）：

| 项目 | 版本 | 许可 | 参考内容 |
|------|------|------|----------|
| [vb6parse](https://github.com/scriptandcompile/vb6parse) | 1.0.1 | MIT | Rust 实现的 VB6 解析器，覆盖工程 / 窗体 / 模块 / 控件，附 30+ 个真实 VB6 工程语料 |
| [proleap-vb6](https://github.com/uwol/proleap-vb6-parser) | — | AGPL-3.0 | 基于 ANTLR4 的 VB6 分析器与转换器，提供完整 `VisualBasic6.g4` 语法 |
| [AvaloniaVisualBasic6](https://github.com/BAndysc/AvaloniaVisualBasic6) | — | MIT | C# + Avalonia 复刻的 VB6 IDE 与语言，含可视化设计器与 VB6 兼容工程格式 |
| [FreeBASIC (fbc)](https://github.com/freebasic/fbc) | 1.20.0 | 编译器 GPL-2.0+ / RTL LGPL-2.1+ | 成熟 BASIC 编译器源码，RTL 组织与代码生成策略 |
| vb6grammarfuzz | 0.1.0 | — | 基于 `VisualBasic6.g4` 的 VB6 语法模糊测试器（仓库内本地工具，依赖 vb6parse，无公开地址） |

> `reference/vba-gobasic-aspgo/gobasic/` 与 `reference/vba-gobasic-aspgo/ai/` 当前为空目录，无来源信息，暂未列入。
