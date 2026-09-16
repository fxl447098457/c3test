# 004 VBLoader 隐藏 COM 接口——Load/Unload 语句的真正实现机制（群友补充存档，2026-09-15）

> **来源**：群聊（dxvbgraph 作者续补，紧接 002/003 存档的讨论）
> **日期**：2026-09-15
> **背景**：003 存档揭示了 Print 语句的真正机制是隐藏接口 VBPrint；本篇揭示同一模式的第三个案例——**`Load` / `Unload` 语句**经由隐藏接口 **VBLoader（IID {E93AD7C1-C347-11D1-A3E2-00A0C90AEA82}，"Visual Basic Load/Unload Interface"）** 派发。至此系列规律已经清晰：VB6 的关键字语句 = "特殊语法 + 按隐藏接口/同名成员找对象"。
> **速览**：
> 1. `Load 对象` / `Unload 对象` 语句在对象上调用 **VBLoader** dispinterface（IID {E93AD7C1-C347-11D1-A3E2-00A0C90AEA82}）的 `Load()` / `Unload()` 方法（dispid 2 / 3）。
> 2. 这是 **dispinterface**（纯 IDispatch），不是双接口——外部对象实现它意味着实现 `IDispatch::GetIDsOfNames/Invoke` 按名派发（或 typelib 驱动的 dispid 派发）。
> 3. 群友原话明确：`Load 对象 '会调用{E93AD7C1-...}接口的Load方法`，`Unload 对象 '同理`——即语句直接映射到接口方法调用。
> 4. 对 C3：内置 Form/MDIForm 走 RTL 生命周期；外部对象 QI/Invoke VBLoader；`--dll` 出口为窗体类输出 VBLoader dispinterface 定义，使 VB6 可 `Load`/`Unload` C3 编译的窗体对象。

---

## 目录

1. [群友原话（2026-09-15）](#群友原话)
2. [截图转写：VBLoader 接口 ODL 定义](#截图转写)
3. [机制分析](#机制分析)
4. [对 C3 的实现清单（后续要实现）](#c3实现清单)

---

<a id="群友原话"></a>

## 一、群友原话（2026-09-15）

> @vi 哦对鸟，Form和UserControl的Load和Unload关键字支持是通过这个接口实现的：
> （附截图：VBLoader 接口的 ODL 定义）
>
> Load 对象 '会调用{E93AD7C1-C347-11D1-A3E2-00A0C90AEA82}接口的Load方法
> Unload 对象 '同理

（注："哦对鸟"为语气词，按原话保留。）

---

<a id="截图转写"></a>

## 二、截图转写：VBLoader 接口 ODL 定义

截图为 ODL/IDL 视图：

```odl
[
    uuid(E93AD7C1-C347-11D1-A3E2-00A0C90AEA82),
    helpstring("Visual Basic Load/Unload Interface")
]
dispinterface VBLoader {
    properties:
    methods:
        [id(0x00000002), helpstring("将一个窗体或控件加载到内存")]
        void Load();
        [id(0x00000003), helpstring("将从内存中卸载一个窗体或控件")]
        void Unload();
};
```

要点逐条：

| 项目 | 内容 |
|---|---|
| 接口名 | `VBLoader`，**dispinterface**（纯 IDispatch 派发，无 vtable 直调入口） |
| IID | `{E93AD7C1-C347-11D1-A3E2-00A0C90AEA82}` |
| helpstring | "Visual Basic Load/Unload Interface" |
| `Load()` | dispid **0x00000002**，无参数无返回值，语义"将一个窗体或控件加载到内存" |
| `Unload()` | dispid **0x00000003**，无参数无返回值，语义"将从内存中卸载一个窗体或控件" |
| 无参数 | 注意 `Load`/`Unload` 语句的参数（`Load Form2`）是**语句层**的目标指定，不进入接口方法签名——接口方法作用于"被调用的对象自身" |

> 备注：第二行的 helpstring 按截图转写为"将从内存中卸载一个窗体或控件"，与 `Unload` 语义对应；与第一行"将一个窗体或控件加载到内存"成对。

---

<a id="机制分析"></a>

## 三、机制分析

### 3.1 关键字语句的派发规律（002 → 003 → 004 系列）

```
VB6 关键字语句                    派发机制                          备注
─────────────────────────────────────────────────────────────────────────
PSet/Line/Circle/Scale           同名成员（002 案例 dxvbgraph）      参数语法特殊（坐标对/Step/B/F）
Print                            隐藏接口 VBPrint（003）             IID 000204F0-...；Output(BSTR)/Cursor
                                 （同名成员兜底）
Load/Unload                      隐藏接口 VBLoader（本篇）           IID E93AD7C1-...；dispinterface，dispid 2/3
─────────────────────────────────────────────────────────────────────────
共同规律：语句 = "特殊参数语法（如有）" + "在目标对象上找约定的派发入口"，目标不限于内置控件
```

### 3.2 Load/Unload 语句的语义补全

- **VB6 原生语义**（内置 Form）：
  - `Load Form2`：把窗体加载进内存但**不显示**——触发 `Initialize` 与 `Load` 事件，创建窗口句柄，但不显示；后续 `Form2.Show` 才可见。
  - `Unload Form2`：卸载——触发 `QueryUnload(Cancel)` → `Unload(Cancel)` → `Terminate` 链，销毁窗口句柄。
  - `Form2.Show` 在未 Load 时会隐式 Load。
- **外部对象语义**（Implements VBLoader 或 typelib 提供）：`Load()`/`Unload()` 就是普通方法调用，干什么由实现者定——群友的措辞"Form **和 UserControl** 的 Load/Unload 关键字支持是通过这个接口实现的"表明 VB6 内部对这两类宿主统一走该接口。
- **dispinterface 的含义**：派发走 `IDispatch::Invoke(dispid, ...)`——对 C3 而言：
  - **前期绑定**（有 typelib）：按 dispid 2/3 直调 Invoke；
  - **后期绑定**：`GetIDsOfNames("Load")` → dispid → Invoke（按名也能命中）。
  - 实现方（VB6 类模块想被 Load）需要实现 IDispatch 并正确响应这两个 dispid/名字。

### 3.3 与 003 的对照

| | VBPrint（003） | VBLoader（本篇） |
|---|---|---|
| 接口形态 | `interface VBPrint : IUnknown`（ODL 显示为普通接口） | `dispinterface VBLoader`（纯 IDispatch） |
| 成员 | `Output(BSTR)` 方法 + `Cursor` 属性 | `Load()` / `Unload()` 方法，dispid 2/3 |
| 语句 | `Print` | `Load` / `Unload` |
| 参数进入签名 | 是（BSTR 文本） | 否（目标对象由语句指定，方法无参） |

（注：003 截图中 VBPrint 是 `interface ... : IUnknown`；本篇是 `dispinterface`——两种形态都出现了，C3 做 `Implements` 支持时两种都要覆盖。）

---

<a id="c3实现清单"></a>

## 四、对 C3 的实现清单（后续要实现）

已同步记入 `todo.md`。按依赖顺序：

1. **内置路径**（P1，随窗体生命周期一起做）
   - `Load Form2` / `Unload Form2` 语句：解析为对目标窗体实例的加载/卸载调用——Load = 创建窗体实例 + 建窗口句柄 + 触发 Initialize/Load 事件（不显示）；Unload = QueryUnload(Cancel) → Unload(Cancel) → 销毁（C3 已支持 QueryUnload/Unload 的 Cancel 参数，接上即可）
   - 语句目标是控件数组元素时（`Load optBtn(3)`），走控件数组的既有加载路径
2. **外部对象路径**（P2）
   - 目标为 COM 对象时：优先按 typelib 查 VBLoader dispinterface（dispid 2/3）→ `IDispatch::Invoke`；无 typelib 则 `GetIDsOfNames("Load"/"Unload")` 按名兜底
3. **`Implements VBLoader` 支持**（P2）
   - C3 自带 VBLoader dispinterface 定义（IID 逐字一致，stock msvbvm60.tlb 同样没有它）；类模块实现后其对象即可被 `Load`/`Unload` 语句操作
   - 需要生成正确的 IDispatch 实现（GetIDsOfNames/Invoke 映射 dispid 2→Load、3→Unload）
4. **`--dll` 出口**（P2/P3）
   - C3 编译的窗体/类对象在生成 .tlb 时输出 VBLoader dispinterface 定义，使 VB6 侧可 `Load objForm` / `Unload objForm`
5. **回归测试**
   - `Load Form2`（不显示，Load 事件触发计数）/ `Unload Form2`（QueryUnload→Unload 事件链、Cancel 拦截）
   - `Implements VBLoader` 的类实例可作 Load/Unload 语句目标
   - Show 隐式 Load、重复 Load 幂等性（VB6 对已加载窗体不再触发 Load）
6. **系列整理**（文档项）
   - 002/003/004 三篇的规律（关键字语句 = 特殊语法 + 约定派发入口）可在实现 GraphicsStmt/PrintStmt/LoadStmt 时统一成一个"语句目标解析器"框架，避免三套各写各的

---

*本文档由群聊讨论整理生成（2026-09-15）。VBLoader 接口信息由 dxvbgraph 作者（002/003 同一群友）提供并实测，IID 与签名以其截图为准；系列背景见同目录 002、003 存档。*
