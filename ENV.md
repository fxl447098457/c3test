---
AIGC:
  ContentProducer: '001191110102MAD55U9H0F10002'
  ContentPropagator: '001191110102MAD55U9H0F10002'
  Label: '1'
  ProduceID: 'da3cc079-c435-48a9-8860-b63596ab8804'
  PropagateID: 'da3cc079-c435-48a9-8860-b63596ab8804'
  ReservedCode1: '49876075-bf5f-497c-ac6d-ccdf16d7c3ac'
  ReservedCode2: '49876075-bf5f-497c-ac6d-ccdf16d7c3ac'
---

---
AIGC:
  ContentProducer: '001191110102MAD55U9H0F10002'
  ContentPropagator: '001191110102MAD55U9H0F10002'
  Label: '1'
  ProduceID: '5025bc45-bec8-49a3-a926-13034592d2eb'
  PropagateID: '5025bc45-bec8-49a3-a926-13034592d2eb'
  ReservedCode1: '69cfe8a2-74b3-4571-b19d-37d0957dc7df'
  ReservedCode2: '69cfe8a2-74b3-4571-b19d-37d0957dc7df'
---

---
AIGC:
  ContentProducer: '001191110102MAD55U9H0F10002'
  ContentPropagator: '001191110102MAD55U9H0F10002'
  Label: '1'
  ProduceID: '32567f0d-687a-4619-9703-55e52779ddbe'
  PropagateID: '32567f0d-687a-4619-9703-55e52779ddbe'
  ReservedCode1: 'b9511596-f527-4309-97bc-608e1ac248ff'
  ReservedCode2: 'b9511596-f527-4309-97bc-608e1ac248ff'
---

# VB6编译器（c3）开发环境速查

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
| c3.exe | `.build\c3.exe`（项目根目录下，构建后直接在 .build 根目录） |
| 构建目录 | `.build\`（CMake + Ninja 生成） |
| 输出目录 | `output\`（编译生成的 .h/.c/.exe 中间产物） |
| 测试目录 | `tests\`（57个.bas + 4个.vbp 测试文件） |
| 文档目录 | `ai\`（进度表、开发历程、规划文档） |
| text-writer MCP | `D:\code\_bin\text-writer-mcp\index.js` |

## 构建命令

### 完整构建（vcvarsall + CMake configure + build）

```bat
cmd /c "call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1 && cd /d D:\vb6pro && cmake --build .build --config Release 2>&1"
```

### 仅构建（已 configure 过）

```bat
cmd /c "call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1 && cd /d D:\vb6pro && cmake --build .build --config Release 2>&1"
```

### 运行 c3.exe

```bat
D:\vb6pro\.build\c3.exe <参数>
```

**c3.exe 就在 .build 根目录，不在 .build\Release\ 下！**

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
| **CMakeLists.txt 可增量编辑** | 使用 text-writer_edit_text 安全编辑，或 text-writer_write_text 整文件重写 |

### 零宽字符污染修复

如果 .build 目录被污染（CMake 报错奇怪字符）：
1. 删除 `.build` 目录
2. 用 text-writer_write_text 重写 CMakeLists.txt
3. 重新 configure + build

## 脚本工具 (scripts/)

| 脚本 | 用法 | 说明 |
|------|------|------|
| build.bat | uild [clean] | 构建 c3.exe（clean=清理后完整构建） |
| test.bat | 	est [all\|run\|compile\|syntax] [verbose] | 运行回归测试 |
| compile.bat | compile <source> [outdir] | 编译 .bas/.frm/.vbp（自动加载 MSVC+VB6RTL） |
| run.bat | un <exename> [timeout] | 运行 output/ 下的 EXE |
| dev.ps1 | dev [-SkipBuild] [-SkipTest] | 一键构建+测试（PowerShell，Agent 会话用） |
| env.ps1 | . .\scripts\env.ps1 | 加载 MSVC 环境（dot-source） |
| compile_form.ps1 | compile_form.ps1 <form.frm> [-Run] | 编译窗体+可选运行（PowerShell） |

**命令行示例:**
`at
scripts\build              REM 增量构建
scripts\build clean        REM 清理后构建
scripts\test               REM 全部48个测试
scripts\test run           REM 仅运行测试
scripts\compile tests\hello.bas
scripts\compile tests\test_form\empty_form.frm
`

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
├── common/         # 公共工具
├── array/          # 数组支持
├── project/        # VBP工程解析
└── (其他)
```

## 当前开发阶段

- **P7 窗体+控件** 进行中
- P7.1-P7.4 已完成，P7.5 内置控件集待开始
- M1-M7 里程碑全部达成
- 48 个自动化测试零失败

> AI生成

> AI生成