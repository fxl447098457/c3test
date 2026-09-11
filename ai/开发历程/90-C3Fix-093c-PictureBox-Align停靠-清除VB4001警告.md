# 90 - C3Fix 093c：PictureBox/Frame `.Align` 停靠支持，清除最后 2 条 VB4001

> 日期: 2026-09-11
> 提交: `72acccb`
> 相关: 89（LNK2005 清零后，全量回归只剩这 2 条告警）

## 0. 症状

`FIX_bisectfrm.vbp` 全量回归（125 模块含窗体）编译链接均通过，但 `c3-error.log`
的 `=== C3 Diagnostics (linking) ===` 段残留 2 条相同告警：

```
(1,1): warning VB4001: P7.5: Unknown control property write 'Picture1.Align' for control
type, generating struct field access (may not compile)
```

来源：`vbman/src/Toast/FToastDrawer.frm:228,231`

```vb
If IsLeft(PosVal) = True Then Picture1.Align = 4      ' vbAlignRight
If IsRight(PosVal) = True Then Picture1.Align = 3     ' vbAlignLeft
```

（气泡提示框贴在屏幕左侧时，把图标 Picture1 停靠到窗体右边缘，反之亦然。）

## 1. 根因

`CCodeGen::getControlPropWriteFn()` 的控件属性表里没有登记 `Align`（全工程只有
`Alignment`，那是文本对齐，语义完全不同）。属性查不到 → 走告警 + 回退分支，回退把
控件 HWND 当 COM 对象发出：

```c
vb6_ComSetProp(vb6_hwnd_Picture1, L"Align", vb6_ComPackInt(4));  /* COM SetProp */
```

能编译、能链接，但运行期对 HWND 做 `IDispatch::Invoke`，等于**静默失效**。

## 2. 修法

### 2.1 生成端（`src/backend/cgen_util.cpp`）

`FrmControlType::PictureBox` 与 `FrmControlType::Frame` 的**读/写**两张表各补一条
（VB6 中 `Align` 只属于 PictureBox / Frame 这类可停靠控件）：

```cpp
if (propLower == "align") return "vb6_SetControlAlign";   // 写表
if (propLower == "align") return "vb6_GetControlAlign";   // 读表
```

### 2.2 RTL（`src/rtl/core/vb6forms.{c,h}`）

新增 `vb6_SetControlAlign` / `vb6_GetControlAlign`，按 VB6 语义实现停靠：

| Align | 含义 | 行为 |
| --- | --- | --- |
| 0 | vbAlignNone | 仅记录，不动位置 |
| 1 | vbAlignTop | 贴父客户区顶边，**宽**撑满 |
| 2 | vbAlignBottom | 贴底边，宽撑满 |
| 3 | vbAlignLeft | 贴左边，**高**撑满 |
| 4 | vbAlignRight | 贴右边，高撑满 |

- 停靠轴撑满、固定轴保持原尺寸（`GetWindowRect` / `GetClientRect(parent)` +
  `SetWindowPos(SWP_NOZORDER | SWP_NOACTIVATE)`）。
- 用窗口属性 `VB6_Align` 记住取值，供 `vb6_GetControlAlign` 读回（与 `VB6_Tag` 同一套路）。
- **已知简化**：VB6 停靠时还会“顶开”其它控件并重排，本实现不重排其它控件（只做本控件
  的贴边与撑满）。

## 3. 验证

```
powershell -File scripts/dev.ps1 -SkipTest          # 重建 C3
cmd /c scripts/build_rtl_libs.bat                   # 重建 RTL 静态库 (x64/x86)
powershell -File scripts/_tmp_bisect.ps1 -Forms     # 含窗体全量回归
```

| 项 | 结果 |
| --- | --- |
| C3 exit code | **0**（`[BISECT] C3 exit=0`） |
| `c3-error.log` | **未生成**（本轮零诊断；上一轮由失败时写出的旧日志残留 2 条 VB4001） |
| 生成代码 | `vb6_SetControlAlign(vb6_hwnd_Picture1, 4);` / `(..., 3);`（COM 回退消失） |
| `VBMAN.dll` | 重建成功 2,566,144 字节（+512，新增 RTL 函数 + 调用点） |

## 4. 本轮踩坑

1. **验证前必须确认二进制真的重建了**：我第一次用 `cmd.exe /c 'scripts\dev.ps1 -SkipTest'`
   调 PowerShell 脚本，cmd 直接静默失败（`$LASTEXITCODE` 还是 0、日志 0 行），
   `.build\C3.exe` 时间戳没变 → 后续“验证”其实是拿旧二进制跑的，结论全错。
   正确姿势：`powershell -NoProfile -ExecutionPolicy Bypass -File scripts/dev.ps1 -SkipTest`，
   并核对 `.build\C3.exe` 的 LastWriteTime。
2. **`--emit-c`（不链接）不会输出诊断**：`=== C3 Diagnostics (linking) ===` 是链接阶段
   收集后写 `c3-error.log` 的，所以“`--emit-c` 里 VB4001=0”不能作为告警已清的证据；
   证据应是「生成的 C 代码不再走回退分支」+「全量回归 exit=0 且不再产生日志」。
