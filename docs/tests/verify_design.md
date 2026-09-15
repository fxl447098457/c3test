# C3 验证落地方案（本机细节）

> 分层总纲见 `docs/自验证.md`（L1-L5）。本文是本机（C 盘新机）落地实施细节：前提资产、关键语义约束、Test-FormRun 设计、差分测试设计、首例规格。
> 日期：2026-09-15

## 一、本机资产前提

| 资产 | 状态 |
|------|------|
| C3.exe | `.build\C3.exe`，全量回归基线 81 PASS / 0 FAIL / 1 SKIP |
| VB6 编译器 | `C:\Program Files (x86)\VB6Mini\bin\VB6.EXE`，官方 6.00.9782（SP6），bin 下 C2.EXE / LINK.EXE / MSPDB60.DLL 完整，`/make` 命令行编译具备条件 |
| PowerShell | 5.1（Add-Type P/Invoke 可用） |
| 桌面交互会话 | 满足（窗体 run 类用例可跑） |

## 二、关键语义约束（差分设计的前提）

**Debug.Print 的语义分歧**：VB6 原生 exe 中 Debug.Print 只进 IDE 立即窗口，独立运行零输出；C3 把它映射到 stdout。

推论：**差分用例的反馈谓词必须走「写文件」**——用例源码内自带反馈信号（Open ... For Output + Print #h 写结果文件），双编译双运行后对比文件内容。stdout 对比仅适用于 C3 自身的 run 类用例，不适用于跨编译器差分。

## 三、Test-FormRun 设计（L1 冒烟 + L2 控件树）

流程：编译 → 启动 → 轮询 5s 找类名前缀 `VB6_Form_` 的主窗口（cgen_form.cpp:62）→ EnumChildWindows dump 控件树 → 与金样比对 → WM_CLOSE → 查退出码。

- 默认比对列（DPI 无关）：`ClassName|Text|CtrlID|Visible`；位置/尺寸列仅记录不比对（Win11 缩放致坐标跨机漂移）
- 金样 `-UpdateGolden` 生成后**必须人工对照 .frm 控件定义确认**，防固化既有 bug
- 兜底：WM_CLOSE 3s 未退 → Kill + FAIL(no-exit)（挂起本身就是缺陷）
- x86 产物无 WOW64 顾虑（窗口枚举走全局命名空间）

## 四、差分测试设计（L4，本机 VB6Mini）

**硬规则**：

1. 位数对齐：VB6 产物必为 32 位，C3 侧用 `--arch x86`
2. /make 三坑（既有实战教训）：PowerShell `Start-Process -Wait` 启动（Git Bash 直接 exec 报 Permission denied）；IDE 占用时静默失败仅 ExitCode 1（跑前确认无 VB6 进程）；路径必须绝对路径
3. 对比谓词：结果文件逐行比对（首选）+ 控件树 dump（窗体用例）+ 退出码；窗口类名归一化（`ThunderRT6FormDC` 系 ↔ `VB6_Form_*`）后再比

**差分对象优先级**：tests 的 .vbp 工程（直接对象）→ 窗体用例包 .vbp → VBMAN 大工程终验。

## 五、首例规格：diff_smoke

最小反馈用例（纯 ASCII，GBK 兼容）：Sub Main 计算整数和 / 字符串拼接 / 浮点数组 / 分支，结果写 `App.Path\result.txt`。

预期输出（双编译器必须一致）：

```
SUM=5050
STR=BCDEF
FLT=12.00
BRANCH=OK
```

双跑目录隔离（`output\diff_vb6\` 与 `output\diff_c3\`），防止 result.txt 互相覆盖。
