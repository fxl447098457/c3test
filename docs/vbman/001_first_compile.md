# 001 首次编译验证（2026-09-16）

## 结论

vbman 真实工程（`src\VBMAN.vbp`，129 模块，Type=OleDll）用 C3 编译：**前端（词法/语法/语义）129 模块零错误全量通过**，C 代码生成成功，失败在 cl.exe 编译生成 C 阶段——仅 **21 个 cl 错误**（6 类模式、7 个文件），距全量编译通过很近。这是本机（C3 版本 ferock/0.10.1 @ 44a01b2）的首次 vbman 差分基线。

## 环境

- 机器：Windows 11 本机（MSVC 19.44.35228，BuildTools）
- C3：`ferock/0.10.1` @ `44a01b2`（含 main 合并 4c0a461）
- 工程参数：`--dll --arch x86`（对齐 VB6 原生 32 位 OleDll）

## 工程选择辨析（重要）

vbman `src\` 下有两个 .vbp：

| 工程文件 | 状态 | 结论 |
| --- | --- | --- |
| `VBMAN.vbp` | 129 条目路径全部有效，`build-vbman.bat`（vbman 官方构建脚本）用它调 VB6 `/make` 产出 VBMAN.dll | **C3 编译目标，权威工程** |
| `ASPMAN.vbp` | 98 条目中 8 条断链（如 `Tools\Json\cJson.cls` 实际在 `Json\cJson.cls`，`StaticClass\Tools\cToolsStream.cls` 实际在 `Tools\Fso\cToolsStream.cls`），VB6 下同样无法打开 | 过时工程文件，不用 |

ASPMAN.vbp 的断链文件本身都存在，只是 vbp 里目录层级写错，疑似目录重组后未同步 vbp。

## 用法

```powershell
cd docs\vbman
.\build_vbman.ps1                  # 完整编译 (x86)，自动汇总统计
.\build_vbman.ps1 -SyntaxOnly      # 仅前端检查（秒级，不生成 C/不调 cl）
.\build_vbman.ps1 -Arch x64        # 换架构
.\build_vbman.ps1 -LogName xxx.log # 自定义日志名
```

日志归档在本目录，cl 错误明细在 `output\c3-error.log`。

## 结果数据（三次编译完全稳定）

| 编译器 | 结果 | 产物 |
| --- | --- | --- |
| VB6Mini 6.00.9782 SP6（权威基准） | ✅ 编译成功，ExitCode 0 | `dist\DLL\VBMAN.dll` 2,306,048 字节（32 位） |
| C3 | ❌ cl 阶段失败 | 前端 0 error，927 warning，21 cl error |

- C3 warning：927（VB3001 未声明标识符 626、VB3003 隐式转换 301，均不阻断）
- cl error：21（exit code 2）

### VB6 基线对照（2026-09-16 补充）

本机 VB6Mini（官方 6.00.9782 SP6，安装路径见项目 README）命令行编译成功：

```
VB6.EXE /make <绝对路径>\tests\vbman\src\VBMAN.vbp /out <绝对路径>\docs\vbman\vb6_make_out.log
```

- 启动方式：PowerShell `Start-Process -Wait`（差分硬规则）
- 编译日志 `vb6_make_out.log`：「'VBMAN.dll' 编译成功。」
- **pitfall**：VB6 `/make` 会改写 `src\VBMAN.vbp`——把 6 条 COM Reference 的绝对路径重写为 `..\..\..\` 相对路径形态，并覆盖 `dist\DLL\` 下的 dll/exp/lib 产物，导致子模块工作区变脏。**每次 VB6 对照编译后需执行** `git -C tests\vbman checkout -- .` 恢复
- 结论：同一份源码 VB6 全量通过、C3 差 21 个 cl 错误，差距完全落在 C3 的 cgen/事件 sink/Variant 转换生成层，与 vbman 源码无关。VB6 产物 VBMAN.dll（2.3MB）可作为后续 C3 修复完成后的体积/接口对照基准。

## 21 个 cl 错误明细与模式归类

### 模式 A：跨类调用实参缺失（C2198 用于调用的参数太少，15 条）

被调函数均带实例首参（`vb6_cls_cWebSocketUtils *` 等），调用点未生成实例参数。集中在 WebSocket 家族：

- cWebSocketClient.c：7 条（81/121/209/329/334/449/511 行，调 GenerateWebSocketKey、GetCloseCodeDescription、StringToUTF8、GetHeaderValue、ComputeAcceptKey、UTF8ToString）
- cWebSocketServer.c：4 条（416/422/549/614 行）
- cWebSocketFrame.c：2 条（332/358 行）
- ToolsJs.c：1 条（45 行，vb6_ToolsJs_NewArr）
- Demo.c：1 条（714 行，vb6_cJson_Decode）

修复方向参考 handoff 教训：实参包装路径问题先在内建识别分支 vs 用户函数实参路径定位；泛用实参改动易大面积回归，先小集合验证。

### 模式 B：事件 sink 声明缺失（C2065 ×2 + C2099 ×1）

cHttpClient.c(682,684,685)：`vb6_vsink_inst_OnResponseStart`、`vb6_vsink_inst_OnResponseDataAvailable` 未声明（WinHTTP 事件回调），初始化表 C2099 与之同源。方向：cgen 的 WithEvents/事件接口 sink 生成。

### 模式 C：散点（3 条）

- Dictionary.c(190) C2440：`vb6_VARIANT` 直接赋 `int32_t`，缺 Variant 取值转换
- ToolsJs.c(45) C2106：返回值赋给非左值（与模式 A 同行）
- Demo.c(714) C2039：`Rs 不是 vb6_cls_cJson 的成员`（与模式 A 同行，疑似链式调用/默认属性生成）

## 后续

- 按模式 A → B → C 顺序修，每修一类跑 `.\build_vbman.ps1` 观察 21 → 递减
- 与权威机 `archive\vbman\c3log\001.md~054.md` 的历史问题对照，避免重复踩坑
- vbman 仓库 `ASPMAN.vbp` 的 8 条断链可反馈上游修正（本仓库不代改子模块内容）
