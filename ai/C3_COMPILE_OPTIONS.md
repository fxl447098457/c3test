# C3 编译器完整编译参数参考

> 本文档由 `C3 --help` 及 `src/driver/driver.cpp` 参数解析逻辑整理而成，
> 包含全部**公开参数**与**隐藏调试参数**，供日常编译与调试参考。

## 基本用法

```
C3 [选项] <源文件...>
```

- 源文件可为 `.bas` / `.vbp` / `.frm` / `.cls` 等
- 编译 `vbman` 等 ActiveX DLL 工程时**必须在 MSVC 环境中运行**
  （先执行 `vcvarsall.bat x64`，见 scripts/build.bat）

## 基本选项

| 参数 | 说明 |
|---|---|
| `-o <文件>` | 输出文件路径（DLL/EXE） |
| `--output-dir <目录>` | 输出目录（默认: 源文件所在目录） |
| `--target <平台>` | 目标平台: `win-x86` / `win-x64` / `linux-x64` / `macos-arm64` |
| `--arch <架构>` | 目标架构: `x64`（默认）或 `x86`（用于 32 位 COM 组件） |
| `--gui <模式>` | GUI 模式: `native` / `webview` / `none` |
| `--syntax-only` | 只做语法检查 |
| `-d, --define <N=V>` | 定义条件编译常量（如 `-d:DEBUG=-1`） |
| `-O <级别>` | 优化级别 0-3 |
| `-g, --debug` | 生成调试信息 |
| `--dll` | 编译为 ActiveX DLL（自动生成 TypeLib .tlb） |
| `-v, --verbose` | 详细输出 |
| `-h, --help` | 显示帮助 |
| `-V, --version` | 显示版本 |

## 调试/转储选项（原隐藏参数，已全部公开）

| 参数 | 说明 |
|---|---|
| `--dump-tokens` | 输出词法分析后的 Token 流 |
| `--dump-ast` | 输出抽象语法树 |
| `--dump-symbols` | 输出符号表 |
| `--dump-preprocess` | 输出预处理后的源码 |
| `--dump-ir` | 输出中间表示 |
| `--dump-frm` | 输出 `.frm` 窗体描述 |
| `--emit-c` | 仅生成 C 代码，不编译链接 |
| `--emit-llvm` | 生成 LLVM IR |
| `--keep-for-debug` | **保留中间文件便于调试（不清理）** |
| `--compat-check` | 兼容性检查模式 |

### --keep-for-debug 说明（调试中间 C 代码的关键参数）

- 默认情况下 C3 编译结束后会**清理中间文件**
- 加 `--keep-for-debug` 后，生成的中间 C 文件（如 `xxx.c`、`dll_entry.c`、`test_prop.c`）
  及 RTL 头文件会保留在临时目录：`%TEMP%\C3C\<会话ID>\`
- 链接失败（LNK2019 等）时用该参数可检查生成代码与实际符号名是否一致

### --emit-c 说明

- 仅做 VB6 → C 的代码生成，不做 cl/link，速度最快，适合快速定位前端问题
- C 文件同样写入 `%TEMP%\C3C\<会话ID>\`

## TypeLib/COM 选项

| 参数 | 说明 |
|---|---|
| `--typelib <文件>` | 显式引用 TypeLib |
| `--no-auto-typelib` | 禁用自动 TypeLib 加载 |
| `--progid <前缀>` | ActiveX DLL 的 ProgID 前缀 |
| `--libid <字符串>` | 显式指定 TypeLib 的 LibID |

## 常用示例

```bat
REM 编译 EXE 工程
C3 工程.vbp --output-dir <dir>

REM 编译 ActiveX DLL 工程（vbman 等，须在 vcvarsall x64 环境）
C3 --dll --arch x64 -o <输出目录>\xxx.dll 工程.vbp

REM 保留中间 C 文件以便调试链接错误
C3 --dll --arch x64 --keep-for-debug -o xxx.dll 工程.vbp

REM 仅生成 C 代码（快速验证，不链接）
C3 --emit-c --output-dir <dir> 工程.vbp

REM 转储中间产物
C3 --dump-tokens --dump-ast 工程.vbp
```

## 中间文件位置

- 中间 C 文件目录：`%TEMP%\C3C\<会话ID>\`
- `--output-dir` 仅用于最终 DLL/EXE 及错误日志（`c3-error.log`）
