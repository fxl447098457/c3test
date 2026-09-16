# scripts 目录说明

构建 / 测试 / 编译辅助脚本的集合。脚本清单总览见项目根 [README.md](../README.md) 第五节「脚本工具（`scripts/`）」。

## 可移植环境变量与自动探测

脚本历史上硬编码了特定开发机的路径（vcvarsall 位置、项目根目录），现已改为**环境变量优先、缺省自动探测**：

| 环境变量 | 作用 | 未设置时的默认行为 |
|----------|------|------------------|
| `C3_VCVARSALL` | `vcvarsall.bat` 的完整路径（想锁定特定 VS 版本时设置） | 通过 `vswhere` 自动探测本机 VS 实例（兼容 Community / Professional / Enterprise / BuildTools 及多实例共存），探测不到则报错退出 |
| `C3_PROJECT_DIR` | 项目根目录（含 `CMakeLists.txt` 的目录） | 脚本所在目录的上一级（`%~dp0..` / `Split-Path -Parent $PSScriptRoot`），一般无需设置 |

> 一般情况下**无需设置任何环境变量**：`build.bat` / `run_tests.ps1` / `env.ps1` 会用 `vswhere` 自动定位本机 Visual Studio，在任意开发机 / CI 环境零配置可用。环境变量仅用于显式锁定非默认工具链。

### 使用示例（锁定特定 VS 实例时才需要）

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

`build.bat` 已内置兜底：`cmake` / `ninja` 不在 `PATH` 时，自动遍历本机所有 VS 实例，把完整 IDE 捆绑的 CMake / Ninja（「C++ CMake tools for Windows」组件）追加到 `PATH`。**无需手动处理。**

若本机只装了 VS Build Tools（不含捆绑 CMake）且 CMake 不在 PATH，可独立安装 CMake ≥ 3.20 与 Ninja ≥ 1.12 并加入 PATH，或设置环境变量后手动追加：

```bat
set "PATH=%PATH%;C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
```
