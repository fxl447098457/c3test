# 012 - 第二期规划：VBA / VBS / ASP 分支

> 日期: 2026-06-26
> 状态: 规划中（第二期，VB6编译器完成后启动）
> 前置: VB6编译器P6 COM+ + P7 窗体控件全部完成

---

## 一、定位

VB6编译器（c3.exe）是第一期核心产品，目标是100%兼容VB6语法，无需修改源码即可编译。

第二期将c3.exe扩展为**BASIC家族多方言编译器**，支持：
- **VBA**（Visual Basic for Applications）：Excel/Access/Word宏、Office自动化
- **VBS**（VBScript）：WSH脚本、ASP经典页面、Windows管理脚本
- **ASP**（Active Server Pages经典）：服务端Web脚本

三者都是VB6的子集或变体，共享80%+的语法基础设施。

---

## 二、各方言与VB6的关系

### VBA vs VB6

| 维度 | VB6 | VBA | 差异程度 |
|------|-----|-----|---------|
| 语法核心 | 完整 | 几乎相同 | 极低 |
| 类型系统 | 完整 | 相同 | 无 |
| 内置函数 | VB6 RTL | VB6 RTL + 宿主对象 | 中 |
| 窗体/控件 | 原生Win32 | 宿主内嵌(UserForm) | 中 |
| COM互操作 | CreateObject/引用 | 相同 + 宿主Application | 中 |
| 文件扩展名 | .bas/.cls/.frm | .bas/.cls(模块内) | 低 |
| 关键差异 | 独立EXE | 宿主进程内运行 | 高 |

关键：VBA不需要独立EXE输出，而是编译为宿主进程内执行的p-code或native code。
但作为编译器工具，我们可以将VBA代码编译为独立EXE（使用COM自动化宿主对象）。
这个后期再讨论，因为需要增加EXE之外的另外一个形态：DLL。
DLL形态下，是作为COM服务器嵌入到VBA宿主调用的，目的是为了封装VBA，保护源码。
这个形态下也许不需要转换宿主对象，而是给一个初始化函数，让开发者把宿主专有对象传入进来。
比如 Application 这些。那么我们编译器其实就可以编译期加入这些对象。

### VBS vs VB6

| 维度 | VB6 | VBS | 差异程度 |
|------|-----|-----|---------|
| 类型系统 | 强类型+Variant | 仅Variant | 高 |
| 变量声明 | Dim As Type | Dim（无As，全是Variant） | 中 |
| 类/UDT | 完整 | 仅Class（无UDT/Enum） | 中 |
| 错误处理 | On Error GoTo/Resume | On Error Resume Next（有限） | 低 |
| 内置对象 | VB6 RTL | WScript/Shell/FSO/RegExp等 | 高 |
| 文件扩展名 | .bas/.cls | .vbs/.wsf | 中 |

关键：VBS是VB6的无类型子集，编译器只需放宽类型检查+提供WSH运行时桩。

### ASP vs VBS

| 维度 | VBS | ASP | 差异程度 |
|------|-----|-----|---------|
| 语言 | VBScript | VBScript | 无 |
| 运行模式 | WSH/cscript | IIS服务端 | 中 |
| 内置对象 | WScript等 | Request/Response/Session/Application/Server | 高 |
| 输出 | Console/对话框 | HTML流 | 中 |
| 文件扩展名 | .vbs | .asp | 低 |

关键：ASP是VBS在IIS服务端的封装，只需要替换内置对象集。

---

## 三、技术路线

### 3.1 方言切换机制

```
c3 hello.bas              → VB6模式（默认）
c3 --dialect=vba macro.bas → VBA模式
c3 --dialect=vbs script.vbs → VBS模式
c3 --dialect=asp page.asp → ASP模式
```

方言影响：
1. **词法器**：VBS无行续行符，VBA有特殊模块头
2. **解析器**：VBS无Type/Enum/Implements，VBA有特殊语句
3. **语义分析**：VBS全部Variant，VB6强类型
4. **代码生成**：不同内置对象集
5. **运行时**：不同宿主桩
6. **输出格式**：VB6→EXE，VBA→DLL(可选)，VBS/ASP→解释执行或EXE

### 3.2 共享基础设施

| 基础设施 | VB6 | VBA | VBS | ASP |
|---------|-----|-----|-----|-----|
| 词法器 | ✅ 完整 | ✅ 复用 | ⚠️ 简化 | ⚠️ 简化+ASP标记 |
| 解析器 | ✅ 完整 | ✅ 复用 | ⚠️ 子集 | ⚠️ 子集 |
| 语义分析 | ✅ 完整 | ✅ 复用 | ⚠️ Variant-only | ⚠️ 同VBS |
| C代码生成 | ✅ 完整 | ✅ 复用 | ✅ 复用 | ✅ 复用 |
| RTL核心 | ✅ 完整 | ✅ 复用 | ⚠️ 子集 | ⚠️ 子集 |
| 宿主对象桩 | - | Office对象 | WSH/FSO | Request/Response等 |

### 3.3 实施顺序（第二期）

| 阶段 | 内容 | 预计工期 |
|------|------|---------|
| Q2.1 | VBS方言定义 + --dialect CLI | 1周 |
| Q2.2 | VBS解析器子集（禁用Type/Enum/Implements等） | 1周 |
| Q2.3 | VBS运行时桩（WScript/Shell/FSO/RegExp） | 2周 |
| Q2.4 | ASP方言扩展（内置对象+HTML混合解析） | 2周 |
| Q2.5 | VBA方言定义+模块头解析 | 1周 |
| Q2.6 | VBA宿主对象桩（Application/Workbook/Worksheet等） | 3周 |
| Q2.7 | VBA/VBS/ASP端到端验证 | 1周 |

---

## 四、决策记录

1. **优先级**：VB6编译器100%完成是第一优先级，第二期不干扰第一期进度
2. **架构**：方言切换通过--dialect CLI参数，不创建独立可执行文件
3. **VBS输出**：VBS代码编译为独立EXE（内嵌WSH运行时模拟层），不是生成.vbs脚本
4. **ASP输出**：ASP代码编译为独立EXE（内嵌HTTP服务器或CGI模式），不是.aspx
5. **VBA输出**：VBA代码编译为独立EXE（通过COM自动化驱动宿主），或DLL供宿主加载
6. **向后兼容**：所有方言共享同一c3.exe，--dialect省略时默认VB6

---

## 五、风险

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| VBS Variant-only语义与VB6强类型冲突 | 中 | 架构调整 | 方言层封装类型策略 |
| ASP HTML混合解析复杂度 | 中 | 延期 | MVP先支持纯<% %>块 |
| VBA宿主对象API巨大 | 高 | 延期 | 按使用频率分批，先用IDispatch后期绑定 |
| COM自动化可靠性 | 中 | 调试困难 | 充分利用第一期COM基础设施 |

---

## 六、待展开

本文档为规划概述，待VB6编译器第一期满完成后，再展开详细设计。