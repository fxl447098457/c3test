# 贡献指南 (CONTRIBUTING)

欢迎参与 C3 开发。本文约定协作流程与代码规范，提交 PR 前请先通读一遍。

> **核心信条：不支持的语法必然报错，绝不静默误编。** 错编译比不编译更危险。
> 任何 PR 若引入"静默通过但语义错误"的行为，一律不予合并。

---

## 一、开发环境

| 工具 | 版本 | 说明 |
|------|------|------|
| Visual Studio 2022 | 任意版本 (Community 即可) | 需勾选「使用 C++ 的桌面开发」工作负载 |
| CMake | ≥ 3.20 | |
| Ninja | 1.12+ | 生成器 |
| Windows SDK | 10.0.22621+ | 窗体/COM 相关头文件 |

构建脚本会通过 `vswhere` 自动定位本机 Visual Studio，不依赖固定安装路径。

```bat
scripts\build              REM 增量构建 (产物 .build\C3.exe)
scripts\build clean        REM 清理后完整构建
scripts\test               REM 全部回归测试 (98 个 .bas + 13 个 .vbp)
scripts\test smoke         REM 仅冒烟测试 (最快，提交前至少跑这个)
```

**PR 提交前，`scripts\test` 必须全绿 (PASS 且 FAIL=0)。**

---

## 二、协作流程

### 2.1 两种贡献者

| 身份 | 方式 |
|------|------|
| 核心开发者 (有仓库写权限) | 在本仓库开分支 → 提 PR → 审核 → 合并 |
| 外部贡献者 | fork 本仓库 → 在自己 fork 里开分支 → 向 main 发 PR |

### 2.2 分支命名

```
feat/<模块>-<简述>     例: feat/gui-shape-line
fix/<问题简述>         例: fix/declare-udt-align
doc/<内容>             例: doc/rtl-api
```

### 2.3 PR 要求

1. **小而聚焦**：一个 PR 只做一件事。巨型混合 PR 会被要求拆分。
2. **CI 必须绿**：PR 会自动触发流水线 (构建 + 回归测试)，红色不予合并。
3. **必须带回归测试**：新功能 / 修 bug 都要新增或修改 `tests\` 下的用例 (见第三节)。
4. **描述清楚动机**：PR 说明里写清"解决什么问题、如何复现、如何验证"。修 bug 的 PR 优先先提 Issue 并在描述中关联。
5. **main 分支受保护**：禁止直接 push，只能通过 PR 合并，且需至少一人审核。

### 2.4 分工建议

动手前先在仓库 Issues 里认领任务，避免两人同时改同一模块；个人想法草稿放 `todo/<用户名>.md`（一人一文件，动手前转为 Issue，约定见根目录 [todo.md](todo.md)）。模块地图：

| 模块 | 路径 | 说明 |
|------|------|------|
| 词法器 | `src/lexer/` | LL(2) + Unicode |
| 解析器 | `src/parser/` | 递归下降 + Pratt |
| 预处理器 | `src/preprocessor/` | #Const / #If |
| 语义分析 | `src/semantics/` | 符号表 + 类型系统 |
| C 代码生成 | `src/backend/cgen_*.cpp` | VB6 → C |
| MSVC 驱动 | `src/backend/msvc_driver.cpp` | shell-out cl.exe / link.exe |
| RTL 运行时 | `src/rtl/core/` | C 运行时 (vb6rtl / vb6forms / vb6com) |
| 工程解析 | `src/project/` | .vbp / .frm / .frx |
| CLI 驱动 | `src/driver/` | main + RTL 内嵌 |

---

## 三、测试规范

1. **新功能**：在 `tests\` 新增 `test_<功能名>.bas`，能编译运行的用 Test-Run 风格 (编译 + 运行 + 校验输出)，并在 `tests\run_tests.ps1` 中登记。
2. **修 bug**：新增一个能复现原 bug 的最小用例，修复前后该用例分别 FAIL / PASS。
3. **用例写法约定**：
   - 输出统一走 `Print`，校验点格式建议 `XXX-N:OK`，便于输出匹配。
   - 禁止 `MsgBox` 等阻塞语句 (见 `tests\smoke.bas` 文件头约束)。
   - 写文件的测试只用相对路径 (工作目录为 `output\`)。
   - 依赖外部 COM 组件的用例，登记时加 `-RequiresCom`，组件未注册时自动 SKIP 而非 FAIL。
4. **GUI/窗体类**：当前只验证编译通过 (Test-Compile 风格)。

---

## 四、代码规范

- C++17，风格与现有代码保持一致 (命名、缩进、注释语言跟随所在文件)。
- C++ 源码统一 **UTF-8 (无 BOM)** + **CRLF** 换行。
- 源码里中文注释欢迎，但注意 MSVC `/utf-8` 已启用，不要引入 GBK 编码的 .cpp/.h。
- 不引入新的第三方依赖，除非在 Issue/PR 中先行讨论。

### 文件编码对照表 (严格遵守)

| 文件类型 | 编码 | 换行 |
|----------|------|------|
| C/C++ 源码 (.cpp/.h/.c) | UTF-8 无 BOM | CRLF |
| CMake / 脚本 (.txt/.bat/.ps1) | 跟随现有文件 (bat/ps1 为 GBK) | CRLF |
| VB6 测试文件 (.bas/.frm/.cls/.vbp) | **GBK** (VB6 IDE 要求) | CRLF |
| 文档 (.md) | UTF-8 | CRLF |

> 提交前可用 `git diff --check` 检查混入的换行符问题。

---

## 五、提交信息

```text
feat(gui): 打通 Shape/Line 控件最后一公里
fix(declare): UDT 成员 Long 在 x64 下不再错误加宽
test(rtl): 补充 DateAdd 边界用例
doc(readme): 更新控件覆盖率
```

格式：`类型(范围): 一句话描述`。类型取 feat / fix / test / doc / refactor / chore。

---

## 六、行为红线 (审核最高准则)

以下 PR 直接拒绝，不接受讨论：

1. 引入静默误编 (应报错的语法被静默编译通过)。
2. 破坏现有回归测试且无正当理由。
3. 引入平台锁定依赖 (C3 目标是 Windows x64/x86 原生，但编译器源码本身不依赖特定 IDE)。
4. 大规模无说明的重构 / 格式化提交 (淹没真实改动，无法审核)。
