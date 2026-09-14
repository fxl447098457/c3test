# VB6编译器（C3）开发环境速查

> 本文件是 AI Agent 每次会话的环境参考，避免重复试错。

## 工作目录

| 项目 | 路径 |
|------|------|
| 项目根目录 | `D:\code\vi\c3.vb6.pro` |

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
| C3.exe | `.build\C3.exe`（构建后直接在 .build 根目录，**不在** `.build\Release\` 下） |
| 构建目录 | `.build\`（CMake + Ninja 生成，可随时删除重建） |
| 输出目录 | `output\`（编译生成的 .exe/.err/.out） |
| 测试目录 | `tests\`（98 个 .bas + 13 个 .vbp） |
| 文档目录 | `ai\`（进度表、开发历程、规划文档） |
| 临时目录 | `.temp\`（一次性排查脚本与日志，不是源码） |

## 构建

```bat
scripts\build            REM 增量构建
scripts\build clean      REM 清理 .build 后完整构建
```

```powershell
# Agent 会话内一键构建 + 测试
D:\code\vi\c3.vb6.pro\scripts\dev.ps1
D:\code\vi\c3.vb6.pro\scripts\dev.ps1 -SkipTest     # 只构建
D:\code\vi\c3.vb6.pro\scripts\dev.ps1 -SkipBuild    # 只跑测试
```

CMakeLists.txt 由 git 管理，直接改直接提交，不需要额外副本或中转。

## 运行 C3.exe

```bat
.build\C3.exe <参数>
```

## 运行自动化测试

```powershell
D:\code\vi\c3.vb6.pro\tests\run_tests.ps1
```

## 文件编码与换行符约定

| 规则 | 说明 |
|------|------|
| 统一 CRLF | Windows 项目，避免出现混合换行符的文件 |
| VB6 工程文件用 GBK | .frm/.bas/.cls/.vbp 必须 GBK，否则 VB6 IDE 显示乱码 |
| C/C++ 源码用 UTF-8 | 与 CMake/MSVC 默认一致 |
| 保留既有 BOM | 部分 .md 带 UTF-8 BOM，编辑时保持原样 |

## 脚本工具 (scripts/)

| 脚本 | 用法 | 说明 |
|------|------|------|
| build.bat | build [clean] | 构建 C3.exe（clean=清理后完整构建） |
| test.bat | test [all\|run\|compile\|syntax] [verbose] | 运行回归测试 |
| compile.bat | compile <source> [outdir] | 编译 .bas/.frm/.vbp（自动加载 MSVC+VB6RTL） |
| run.bat | run <exename> [timeout] | 运行 output/ 下的 EXE |
| dev.ps1 | dev [-SkipBuild] [-SkipTest] | 一键构建+测试（PowerShell，Agent 会话用） |
| env.ps1 | . .\scripts\env.ps1 | 加载 MSVC 环境（dot-source） |
| build_rtl_libs.bat | — | 生成 RTL .lib 并嵌入 C3.exe（链接依赖，勿删） |

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
