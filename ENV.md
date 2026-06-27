---
AIGC:
  ContentProducer: '001191110102MAD55U9H0F10002'
  ContentPropagator: '001191110102MAD55U9H0F10002'
  Label: '1'
  ProduceID: '96ea9b49-400d-4383-8347-626f1da41601'
  PropagateID: '96ea9b49-400d-4383-8347-626f1da41601'
  ReservedCode1: 'ec4d4d59-44ce-4cf1-bcd1-d758d33a49e3'
  ReservedCode2: 'ec4d4d59-44ce-4cf1-bcd1-d758d33a49e3'
---

---
AIGC:
  ContentProducer: '001191110102MAD55U9H0F10002'
  ContentPropagator: '001191110102MAD55U9H0F10002'
  Label: '1'
  ProduceID: '7b035d93-441f-4307-8cb1-7fc76f374ccb'
  PropagateID: '7b035d93-441f-4307-8cb1-7fc76f374ccb'
  ReservedCode1: 'ca1dc0a7-ba29-4396-8e9c-c0a71e991560'
  ReservedCode2: 'ca1dc0a7-ba29-4396-8e9c-c0a71e991560'
---

---
AIGC:
  ContentProducer: '001191110102MAD55U9H0F10002'
  ContentPropagator: '001191110102MAD55U9H0F10002'
  Label: '1'
  ProduceID: '5e968878-1d7b-4c43-8145-f4d5e8d3c68e'
  PropagateID: '5e968878-1d7b-4c43-8145-f4d5e8d3c68e'
  ReservedCode1: 'bd11dbed-fe9f-4042-8784-624f5e14a5e8'
  ReservedCode2: 'bd11dbed-fe9f-4042-8784-624f5e14a5e8'
---

# VB6编译器（C3）开发环境速查

> 本文件是 AI Agent 每次会话的环境参考，避免重复试错。

## 工作目录

| 项目 | 路径 |
|------|------|
| 项目根目录 | `D:\开源项目\BASIC家族\vb6.pro` |
| Junction 快捷路径 | `D:\vb6pro` → 项目根目录（两个路径等价） |

## 构建工具链

| 工具 | 版本 | 备注 |
|------|------|------|
| MSVC | VS2022 Community | 需通过 vcvarsall.bat 加载环境 |
| CMake | 3.31.6 | 需 vcvarsall 后才在 PATH |
| Ninja | 1.12.1 | 需 vcvarsall 后才在 PATH |

## 关键路径

| 项目 | 路径 |
|------|------|
| vcvarsall.bat | `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat` |
| C3.exe | `.build\C3.exe`（项目根目录下，构建后直接在 .build 根目录） |
| 构建目录 | `.build\`（CMake + Ninja 生成） |
| 输出目录 | `output\`（编译生成的 .h/.c/.exe 中间产物） |
| 测试目录 | `tests\`（57个.bas + 4个.vbp 测试文件） |
| 文档目录 | `ai\`（进度表、开发历程、规划文档） |
| text-writer MCP | `D:\code\_bin\text-writer-mcp\index.js` |

## 构建命令

### AIGC 水印 hook 防御（关键！）

AIGC 水印 hook 会在 **两次 tool call 之间** 对工作目录根目录文件注入 ~10KB 零宽字符。
CMakeLists.txt 被注入后 CMake 无法解析。`scripts\build.bat` 也会触发（因为写入和编译不在同一个 cmd 进程）。

**唯一可靠的构建方式：一次性 bat 脚本（copy + configure + build 同进程）**

将干净 CMakeLists.txt 备份在 `.temp\CMakeLists_clean.txt`，构建时 bat 脚本先 copy 再 cmake，hook 无法在中间注入：

```bat
REM .temp\full_build.bat — 推荐，每次构建都用这个
@echo off
copy /Y "D:\vb6pro\.temp\CMakeLists_clean.txt" "D:\vb6pro\CMakeLists.txt" >nul
if exist "D:\vb6pro\.build" rd /s /q "D:\vb6pro\.build"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
cd /d D:\vb6pro
cmake -G Ninja -B .build -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (echo CMAKE_CONFIGURE_FAILED & exit /b 1)
cmake --build .build --config Release
if errorlevel 1 (echo CMAKE_BUILD_FAILED & exit /b 1)
echo BUILD_SUCCESS
```

**运行方式（PowerShell）：**
```powershell
Start-Process -FilePath "cmd.exe" -ArgumentList '/c D:\vb6pro\.temp\full_build.bat > D:\vb6pro\.temp\build_log.txt 2>&1' -NoNewWindow -Wait
# 然后读取日志：text-writer_read_text D:\vb6pro\.temp\build_log.txt
```

### 何时更新 CMakeLists_clean.txt 备份

每次修改 CMakeLists.txt 后，必须用 text-writer_write_text 写入 `.temp\CMakeLists_clean.txt`，
然后从 `.temp\` 用 bat 的 `copy /Y` 复制到项目根目录。
**绝不能直接写项目根目录的 CMakeLists.txt**（hook 会在下一次 tool call 前注入零宽字符）。

### 增量构建（已 configure 过，CMakeLists.txt 未变）

```bat
cmd /c "call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1 && cd /d D:\vb6pro && cmake --build .build --config Release 2>&1"
```

### 运行 C3.exe

```bat
D:\vb6pro\.build\C3.exe <参数>
```

**C3.exe 就在 .build 根目录，不在 .build\Release\ 下！**

### 运行自动化测试

```powershell
# 需先构建，然后：
D:\vb6pro\tests\run_tests.ps1
```

## 文件写入纪律（红线）

| 规则 | 说明 |
|------|------|
| **禁止使用内置 write 工具** | 会在文件头注入 ~10KB 零宽字符（U+200B/U+200D），导致 CMake 解析失败 |
| **禁止使用内置 edit 工具** | edit 也走内置写入路径，同样会注入零宽字符水印 |
| **必须使用 text-writer_edit_text** | 无水印、自动编码检测与保持、支持 replaceAll |
| **必须使用 text-writer_write_text** | 无水印、自动 CRLF、支持 GBK/UTF-8 |
| **必须使用 text-writer_read_text** | 自动编码检测，无水印注入 |
| **VB6 项目文件用 GBK 编码** | .frm/.bas/.cls/.vbp 必须指定 encoding=gbk |
| **CMakeLists.txt 写入 .temp/ 再 copy** | hook 会在 tool call 间隙注入零宽字符，必须先写 .temp/ 再同进程 copy |

### 零宽字符污染修复

如果 .build 目录被污染（CMake 报错奇怪字符）：
1. 删除 `.build` 目录
2. 确保 `.temp\CMakeLists_clean.txt` 是最新版本
3. 用 `.temp\full_build.bat` 重新构建（copy + configure + build 同进程）

## 脚本工具 (scripts/)

| 脚本 | 用法 | 说明 |
|------|------|------|
| build.bat | build [clean] | 构建 C3.exe（clean=清理后完整构建）**注意：会被hook注入，推荐用 .temp\full_build.bat** |
| test.bat | test [all\|run\|compile\|syntax] [verbose] | 运行回归测试 |
| compile.bat | compile <source> [outdir] | 编译 .bas/.frm/.vbp（自动加载 MSVC+VB6RTL） |
| run.bat | run <exename> [timeout] | 运行 output/ 下的 EXE |
| dev.ps1 | dev [-SkipBuild] [-SkipTest] | 一键构建+测试（PowerShell，Agent 会话用） |
| env.ps1 | . .\scripts\env.ps1 | 加载 MSVC 环境（dot-source） |

**命令行示例:**
```bat
scripts\build              REM 增量构建
scripts\build clean        REM 清理后构建
scripts\test               REM 全部测试
scripts\test run           REM 仅运行测试
scripts\compile tests\hello.bas
```

## 源码结构

```
src/
├── lexer/          # 词法器 LL(2) + Unicode
├── parser/         # 解析器 递归下降 + Pratt
├── preprocessor/   # 条件编译 #Const/#If
├── semantics/      # 语义分析 两遍扫描
├── backend/        # C代码生成器
├── driver/         # CLI + MSVC驱动
├── ir/             # 中间表示
├── rtl/            # VB6运行时 (vb6rtl.h/c)
├── com/            # COM客户端+服务端运行时
├── typelib/        # TypeLib内建生成器 (CreateTypeLib2)
├── common/         # 公共工具
├── array/          # 数组支持
├── project/        # VBP工程解析
└── (其他)
```

## 当前开发阶段

- **P10 RTL内嵌与编译流程封闭** 基本完成
- P9 已完成（72个测试零失败），M9达成
- P10 核心完成：RTL 内嵌 RC 资源 + 会话目录 + c3-error.log
- .temp\full_build.bat 是唯一推荐的构建方式