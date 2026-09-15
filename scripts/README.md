# scripts 目录说明

构建 / 测试 / 编译辅助脚本的集合。脚本清单总览见项目根 [README.md](../README.md) 第五节「脚本工具（`scripts/`）」。

## 可移植环境变量

脚本历史上硬编码了特定开发机的路径（vcvarsall 位置、项目根目录），现已改为**环境变量优先、缺省自动探测**：

| 环境变量 | 作用 | 未设置时的默认值 |
|----------|------|------------------|
| `C3_VCVARSALL` | `vcvarsall.bat` 的完整路径（VS 安装位置与版本因机器而异） | `build.bat` / `tests\run_tests.ps1` 取 VS2022 Community 路径；`env.ps1` 取 VS2022 BuildTools 路径 |
| `C3_PROJECT_DIR` | 项目根目录（含 `CMakeLists.txt` 的目录） | 脚本所在目录的上一级（`%~dp0..` / `Split-Path -Parent $PSScriptRoot`），一般无需设置 |

### 使用示例

**cmd（当前会话临时生效）：**

```bat
set "C3_VCVARSALL=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
scripts\build
```

**PowerShell（当前会话临时生效）：**

```powershell
$env:C3_VCVARSALL = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
scripts\dev.ps1 -SkipTest
```

**持久化（用户级，写入注册表，新开终端生效）：**

```bat
setx C3_VCVARSALL "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
```

> 上例路径为 **Build Tools 版** VS2022 的 vcvarsall 位置；完整 IDE（Community / Professional / Enterprise）对应 `C:\Program Files\Microsoft Visual Studio\2022\<Edition>\VC\Auxiliary\Build\vcvarsall.bat`。

### 附注：CMake / Ninja 不在 PATH 时

`build.bat` 依赖 `cmake` / `ninja` 命令。VS2022 安装了「C++ CMake tools for Windows」组件时它们位于 VS 内部目录，部分安装形态下 `vcvarsall` 不会将其加入 `PATH`，可手动追加：

```bat
set "PATH=%PATH%;C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
```

（或独立安装 CMake ≥ 3.20 与 Ninja ≥ 1.12 并加入 PATH。）
