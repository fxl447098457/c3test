# 005 IEnumVARIANT 接口——For Each 语句的真正实现机制（群友补充存档，2026-09-15）

> **来源**：群聊（dxvbgraph 作者续补，002/003/004 同系列第四篇）
> **日期**：2026-09-15
> **背景**：群友继续补全 VB6 关键字语句的隐藏派发机制系列——这次是 **`For Each`**，接口为 `{00020404-0000-0000-C000-000000000046}`。**注意：这个 IID 就是标准 `IEnumVARIANT` 的官方 IID**（系统 oleaut32 定义），群友截图里的定义名叫 `EnumVariant`，是他加入自改版 msvbvm60.tlb 的接口定义——目的和 003/004 一样：**让 VB6 类模块可以 `Implements` 它，从而让自定义类的对象可被 `For Each` 枚举**。
> **速览**：
> 1. `For Each x In obj` 的消费侧机制：对 `obj` 调 `_NewEnum`（DISPID **-4**，DISPID_NEWENUM）拿到 `IEnumVARIANT`，再循环 `Next` 取元素——这是 Automation 标准协议，C3 **消费侧已实现**（P22-11，详见文末现状核对）。
> 2. 本篇群友补充的增量在**生产侧**：接口定义（IID 与标准 IEnumVARIANT 相同、定义名 EnumVariant）可供 VB6/C3 类模块 `Implements`，实现 `Next/Skip/Reset/Clone` 后其对象即可被 `For Each`。
> 3. 截图签名与标准 IEnumVARIANT 有细节差异（定义名、Clone 为 propget、long 而非 ULONG），按 IID 匹配故二进制兼容，但 C3 做 Implements 时应**以标准 IEnumVARIANT 签名为准**。
> 4. C3 的缺口：`Implements EnumVariant/IEnumVARIANT` 支持 + 类模块 `_NewEnum` 成员（dispid -4）的生成 + `--dll` typelib 输出。

---

## 目录

1. [群友原话（2026-09-15）](#群友原话)
2. [截图转写：EnumVariant 接口 ODL 定义](#截图转写)
3. [与标准 IEnumVARIANT 的对照](#标准对照)
4. [C3 现状核对（消费侧已完成）](#c3现状)
5. [对 C3 的实现清单（后续要实现）](#c3实现清单)

---

<a id="群友原话"></a>

## 一、群友原话（2026-09-15）

> @vi 对鸟，还有For Each支持需要实现{00020404-0000-0000-C000-000000000046}这个接口：
> （附截图：EnumVariant 接口的 ODL 定义）

（本条无文字补充，纯接口定义截图。）

---

<a id="截图转写"></a>

## 二、截图转写：EnumVariant 接口 ODL 定义

```odl
[
    odl,
    uuid(00020404-0000-0000-C000-000000000046)
]
interface EnumVariant : IUnknown {
    HRESULT _stdcall Next(
                    [in] long Celt,
                    [out] VARIANT* Fetched,
                    [out, retval] long* CeltFetched);
    HRESULT _stdcall Skip([in] long Celt);
    HRESULT _stdcall Reset();
    [propget]
    HRESULT _stdcall Clone([out, retval] EnumVariant** Copies);
};
```

| 成员 | 签名（截图） | 语义 |
|---|---|---|
| `Next` | `HRESULT Next([in] long Celt, [out] VARIANT* Fetched, [out, retval] long* CeltFetched)` | 取至多 Celt 个元素到 Fetched，返回实际取到数；返回 S_FALSE 表示枚举结束 |
| `Skip` | `HRESULT Skip([in] long Celt)` | 跳过 Celt 个元素 |
| `Reset` | `HRESULT Reset()` | 重置到开头 |
| `Clone` | `[propget] HRESULT Clone([out, retval] EnumVariant** Copies)` | 克隆当前枚举器（**截图里是 propget 属性形态**） |
| 接口名 | `EnumVariant`（继承 IUnknown） | IID = **标准 IEnumVARIANT 的官方 IID** |

---

<a id="标准对照"></a>

## 三、与标准 IEnumVARIANT 的对照

标准 IEnumVARIANT（oleaut32 / stdole2.tlb）签名：

```odl
interface IEnumVARIANT : IUnknown {
    HRESULT Next([in] ULONG celt, [in] VARIANT rgvar, [out] ULONG* pceltFetched);
    HRESULT Skip([in] ULONG celt);
    HRESULT Reset();
    HRESULT Clone([out] IEnumVARIANT** ppEnum);
}
```

| 差异点 | 群友截图（EnumVariant） | 标准 IEnumVARIANT | 判断 |
|---|---|---|---|
| IID | 00020404-0000-0000-C000-000000000046 | 相同 | **二进制兼容，QI 按 IID 匹配，名字无所谓** |
| 定义名 | `EnumVariant` | `IEnumVARIANT` | 疑为避开 stdole2.tlb 中已存在的同名接口（VB6 Implements 引用解析冲突），或纯命名习惯 |
| 计数类型 | `long`（32 位有符号） | `ULONG`（32 位无符号） | 位宽相同，实际无差 |
| `Next` 参数形态 | `VARIANT* Fetched` 单元素指针 | `VARIANT rgvar` 数组（按 celt 个） | 语义等价；For Each 每次只取 1 个（celt=1）时两者二进制布局一致 |
| `Clone` | **propget 属性**（retval） | 普通方法（out 参数） | 调用约定不同——C3 做 Implements 时以标准方法形态为准，兼容时注意甄别 |

**结论**：这是同一协议的"民间 typelib 转写版"，IID 相同所以运行时无差别；C3 实现时接口定义应照抄标准 IEnumVARIANT（名字用 `IEnumVARIANT`），Implements 引用名可同时接受 `EnumVariant` / `IEnumVARIANT`。

---

<a id="c3现状"></a>

## 四、C3 现状核对（消费侧已完成）

**结论：For Each 对 COM 集合的枚举（消费侧）C3 已实现，无需重做**，源码位置：

| 位置 | 内容 |
|---|---|
| `src/backend/cgen_stmt.cpp:2400`（`visit(ForEachStmt)`） | For Each 双路径：数组走 SafeArray 迭代；否则走 COM 路径（2570 起） |
| `src/backend/cgen_stmt.cpp:2570-2589` | P22-11：生成 `vb6_ForEach_Init / vb6_ForEach_Next / vb6_ForEach_Release` 循环 |
| `src/rtl/core/vb6rtl.c:4700-4727`（`vb6_ComNewEnum`） | `GetIDsOfNames("_NewEnum")` → `Invoke(DISPID -4)` → 对返回的 IUnknown/IDispatch `QI(IID_IEnumVARIANT)` |
| `src/rtl/core/vb6com.c:1469-1601` | P22-11 全套：`vb6_ForEach_Init`（含读 Count 属性做安全上限）、`vb6_ForEach_Next`、`vb6_ForEach_Release` |
| `src/rtl/core/vb6com.c` P25-fix4 | 防呆：buggy IEnumVARIANT 永不终止时用 Count 限次兜底 |

小事项：`cgen_stmt.cpp:2406` 的注释"暂不支持: COM集合(IEnumVARIANT)迭代"已过时（被 2570 起的 COM 路径取代），可顺手清理。

**真正缺的是生产侧**——C3 编译的类（VB6 类模块 / C3 自产 COM 对象）目前**不能被** For Each 枚举：没有 Implements IEnumVARIANT 的通道，也没有 `_NewEnum` 成员（dispid -4）的生成路径。

---

<a id="c3实现清单"></a>

## 五、对 C3 的实现清单（后续要实现）

已同步记入 `todo.md`。按依赖顺序：

1. **类模块实现 IEnumVARIANT 通道**（P2）
   - 支持 `Implements IEnumVARIANT`（引用名同时接受 `EnumVariant`——群友 tlb 版本的定义名；IID 逐字用 {00020404-0000-0000-C000-000000000046}）
   - 类内实现 `Next/Skip/Reset/Clone`（Clone 按标准方法形态），其对象即可被 `For Each x In obj` 枚举
2. **`_NewEnum` 约定成员**（P2）
   - 类模块可用隐藏成员 `_NewEnum`（dispid **-4**）返回枚举器；C3 消费侧已在调 dispid -4，生产侧补齐成员生成即可闭环
   - VB6 经典套路：`Private Sub Class_Initialize` 里建 Collection + `Public Property Get NewEnum() As IUnknown` 标 dispid -4 —— C3 需支持给成员指定/生成 dispid -4（typelib 层）
3. **`--dll` 出口**（P2/P3）
   - C3 生成 .tlb 时：为实现了枚举协议的类输出 `_NewEnum` 成员（dispid -4）+ IEnumVARIANT 接口引用，使 VB6 侧可 `For Each x In C3对象`
4. **回归测试**
   - C3 类 `Implements IEnumVARIANT` 后可被 For Each（Next/Reset/Skip 序列正确、S_FALSE 正常终止）
   - C3 自产 ActiveX DLL 的集合类在 VB6 里被 For Each（dispid -4 闭环）
   - 既有消费侧回归保持（数组路径 / COM 路径 / buggy 枚举器 Count 兜底）
5. **系列归并**（与 004 的建议一致）
   - 002（同名成员）/ 003（VBPrint）/ 004（VBLoader）/ 本篇（IEnumVARIANT + dispid -4）统一进"语句目标解析器 + COM 协议注入"框架设计；IEnumVARIANT 消费侧已存在，重点是生产侧通道与 typelib 出口

---

*本文档由群聊讨论整理生成（2026-09-15）。接口信息由 dxvbgraph 作者（002/003/004 同一群友）提供，截图为其自改版 msvbvm60.tlb 中的定义；标准 IEnumVARIANT 签名以 Windows SDK 为准。系列背景见同目录 002、003、004 存档。*
