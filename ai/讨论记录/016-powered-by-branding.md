---
AIGC:
  ContentProducer: '001191110102MAD55U9H0F10002'
  ContentPropagator: '001191110102MAD55U9H0F10002'
  Label: '1'
  ProduceID: 'c2e7a921-82ee-453d-8e26-f5f30bd04672'
  PropagateID: 'c2e7a921-82ee-453d-8e26-f5f30bd04672'
  ReservedCode1: '44cb6b27-e85a-448c-a1b9-3c8c17efde66'
  ReservedCode2: '44cb6b27-e85a-448c-a1b9-3c8c17efde66'
---

---
AIGC:
  ContentProducer: '001191110102MAD55U9H0F10002'
  ContentPropagator: '001191110102MAD55U9H0F10002'
  Label: '1'
  ProduceID: 'fd55fcb5-9699-4833-b9d0-460689a65e5d'
  PropagateID: 'fd55fcb5-9699-4833-b9d0-460689a65e5d'
  ReservedCode1: 'fbdbaf81-36c7-41ad-8a3b-d6527871228a'
  ReservedCode2: 'fbdbaf81-36c7-41ad-8a3b-d6527871228a'
---

---
AIGC:
  ContentProducer: '001191110102MAD55U9H0F10002'
  ContentPropagator: '001191110102MAD55U9H0F10002'
  Label: '1'
  ProduceID: '9f1fcde6-5400-407d-9087-6d78a144b091'
  PropagateID: '9f1fcde6-5400-407d-9087-6d78a144b091'
  ReservedCode1: '6daea697-eeda-4ed9-838c-13a8f15713d3'
  ReservedCode2: '6daea697-eeda-4ed9-838c-13a8f15713d3'
---

# 016: 免费版编译产物品牌标识（Powered by）

> 日期: 2026-07-01
> 状态: 待实施
> 影响范围: driver.cpp 编译流程 + 新增 .rc 生成

## 需求

免费版用户编译出的 EXE/DLL 文件，在文件属性中显示 "Powered by https://c3.vb6.pro"。
付费用户可去除该标识。

这是商业编译器的经典做法：
- **Delphi**: 免费版编译的 EXE 在运行时显示启动画面（splash screen），付费版可去除
- **Inno Setup**: 免费版安装程序底部显示 "This installation was built with Inno Setup"，付费版可去除
- **Unity**: 免费版启动画面强制显示 "Made with Unity" 水印，Pro 版去除
- **Godot**: 纯开源无此限制的典型反例

## 技术方案

### 原理：Windows VERSIONINFO 资源

在编译产物中嵌入 `.rc` 资源脚本，链接进 EXE/DLL。用户右键→属性→详细信息 可见。

### 生成 app_version.rc

c3 在生成 `.c/.h` 的同时，额外生成 `app_version.rc`：

```rc
#include <windows.h>

VS_VERSION_INFO VERSIONINFO
FILEVERSION 1,0,0,0
PRODUCTVERSION 1,0,0,0
BEGIN
  BLOCK "StringFileInfo"
  BEGIN
    BLOCK "080404B0"
    BEGIN
      VALUE "CompanyName", "c3.vb6.pro"
      VALUE "FileDescription", "VB6 Application"
      VALUE "FileVersion", "1.0.0.0"
      VALUE "ProductName", "工程1"
      VALUE "Comments", "Powered by https://c3.vb6.pro"
    END
  END
  BLOCK "VarFileInfo"
  BEGIN
    VALUE "Translation", 0x0804, 1200
  END
END
```

### 编译流程变更

1. `driver.cpp` 的 `runCodeGeneration()` 中，额外生成 `app_version.rc`
2. `rc.exe` 编译 `.rc` → `.res`（已有 rc.exe 查找逻辑，P10.15 已实现）
3. `.res` 作为附加链接输入传给 `msvc_driver.cpp`
4. `MsvcDriverOptions` 新增 `resFile` 字段
5. `cl.exe` 命令行末尾追加 `.res` 文件

### 免费版 vs 付费版控制

```
免费版:  c3 project.vbp                     → 自动附加 "Powered by" 资源
付费版:  c3 project.vbp --no-brand           → 不附加
         c3 project.vbp --pro-key XXXX       → 验证后不附加（远期）
```

短期用 `--no-brand` 开关，后续商业化接 license key 验证。

### 字段来源

| VERSIONINFO 字段 | 来源 | 示例 |
|-----------------|------|------|
| ProductName | VBP `Name=` 或文件名 | 工程1 |
| FileDescription | 固定 "VB6 Application" 或 VBP 描述 | VB6 Application |
| FileVersion | VBP `MajorVer.MinorVer.RevisionVer` | 1.0.0 |
| CompanyName | 固定 "c3.vb6.pro" | c3.vb6.pro |
| Comments | 免费版 "Powered by https://c3.vb6.pro" | Powered by... |

## 工作量评估

约半天：
- `driver.cpp`: 生成 `.rc` 文件 + 编译流程集成 (~2h)
- `msvc_driver.cpp`: 链接 `.res` 支持 (~0.5h)
- 测试验证 + 回归 (~1h)

## 优先级

非紧急，P17+ 可实施。商业化启动时作为免费/付费分层的关键差异化特性。

## 讨论记录：是否在运行时强制校验品牌标识

### 提议

在 RTL 的 WinMain 启动时读取自身 EXE 的 VERSIONINFO，校验 Comments 字段。
免费版编译的程序如果没有 "Powered by" 标识（被用户手动篡改/剥离），就 ExitProcess 强制退出。

### 结论：强烈不建议

**这会被视为恶意行为。** 用户的项目程序运行时突然闪退，排除代码层面原因后发现是编译器植入了校验逻辑——与恶意软件的行为模式一致。

**业界零先例：**

| 产品 | 免费版限制 | 是否阻止运行 |
|------|-----------|-------------|
| Delphi | 启动画面（splash） | 否，可点掉 |
| Inno Setup | 安装界面底部文字 | 否，正常安装 |
| Unity | 闪屏 "Made with Unity" | 否，几秒后自动消失 |
| VB6 自身 | 无 | 否 |

没有任何主流编译器在用户程序中植入运行时自毁逻辑。这条线一旦越过，性质就变了。

**实际后果：**
- 社区一旦发现，口碑立即崩塌——"这个编译器在你程序里种后门"
- 技术人员几个反汇编就能绕过，防住的恰恰是最不该得罪的小白用户
- 反弹声量会远超标识本身的价值

### 建议方案：可见但不碍事

品牌标识做到"静默可见"的程度，不做任何运行时干预：

1. **VERSIONINFO 写入**——右键属性可见，静默存在（已规划）
2. **控制台程序** stderr 输出一行 `Powered by c3.vb6.pro`，不影响程序功能
3. **GUI 程序**可选闪屏（splash），1-2秒自动消失，或首次显示关于框

免费/付费的区分放在**付费版的正向价值**上（优化、VBA保护、去标识、优先支持），而不是惩罚免费用户。

> AI生成

> AI生成

> AI生成