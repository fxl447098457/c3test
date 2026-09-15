# 003 VBPrint 隐藏 COM 接口——Print 语句的真正实现机制（群友补充存档，2026-09-15）

> **来源**：群聊（dxvbgraph 作者续补，紧接 002 存档的讨论）
> **日期**：2026-09-15
> **背景**：002 存档确认了 PSet/Line/Circle/Print 关键字语句的目标对象可以是外部类库对象；本篇是作者对 **Print** 机制的进一步揭示——Print 走的不是"同名成员派发"，而是一个 **VB6 隐藏 COM 接口 `VBPrint`（IID {000204F0-0000-0000-C000-000000000046}）**。这是 C3 实现 Print 语句与"任意对象可 Print"生态的关键依据。
> **速览**：
> 1. Print 语句的真正机制 = 隐藏接口 **VBPrint**：`Output(BSTR Text)` 方法负责文字绘制，`Cursor` 属性负责光标位置（**字符位置单位**，与 CurrentX/CurrentY 的**绘图坐标单位**完全不是一回事）。
> 2. VB6 自带对象的 Print（Form/PictureBox/Printer）内部即经由该接口；外部对象 Implements VBPrint 后即可用 `obj.Print ...` 关键字写法。
> 3. 股票 msvbvm60.tlb 里没有暴露这个接口；作者把接口定义**加入了他自改的新版 msvbvm60.tlb**，VB6 类模块即可 `Implements VBPrint`。
> 4. 作者已验证用例：`Implements VBPrint` 实现控制台类（`con.Print` 打字符串到控制台）；TextBox 版 Print 也"没有任何问题"。
> 5. 对 C3 的实现清单见文末：内部对象走 RTL、外部对象 QI IID_VBPrint 优先 / 同名成员兜底、`--dll` 生成 .tlb 时输出 VBPrint 接口、支持 `Implements VBPrint`。

---

## 目录

1. [群友原话（2026-09-15）](#群友原话)
2. [截图转写：VBPrint 接口 ODL 定义](#截图转写)
3. [机制分析](#机制分析)
4. [对 C3 的实现清单（后续要实现）](#c3实现清单)

---

<a id="群友原话"></a>

## 一、群友原话（2026-09-15）

> 我的Print是可以打印Unicode字符出来
> （附截图：VBPrint 接口的 ODL 定义，uuid 处红框标注）
> 要支持Print则是需要实现GUID为{000204F0-0000-0000-C000-000000000046}的COM接口，我在新版本msvbvm60.tlb中加入了这个接口（VB6自己写类模块可以Implements这个接口来实现）。
>
> Output 方法是文字绘制实现
> Cursor 属性是光标位置设置/获取的实现（注意：和CurrentX/CurrentY完全不是一回事哦，CurrentX/CurrentY的单位是绘图坐标单位，VBPrint接口的Cursor属性是字符位置的单位）。
>
> @vi 我之前还试过用这个 Implements VBPrint 来实现控制台类，可以完美实现 con.? 打印字符串到控制台
>
> 你想实现一个TextBox版的Print也是没有任何问题的

（注："con.?" 为群友笔误/简写，指 `con.Print`——VB6 IDE 中 `Print` 可缩写为 `?`。）

---

<a id="截图转写"></a>

## 二、截图转写：VBPrint 接口 ODL 定义

截图为 ODL/IDL 视图（应为 OLE/COM 浏览器或 tlb 反编译工具的输出）：

```odl
[
    odl,
    uuid(000204F0-0000-0000-C000-000000000046),      ← 截图红框标注
    helpstring("Visual Basic Print Interface")
]
interface VBPrint : IUnknown {
    [helpstring("输出文本")]
    HRESULT _stdcall Output([in] BSTR Text);
    [propput, helpstring("返回/设置光标位置")]
    HRESULT _stdcall Cursor([out, retval] int* Value);
    [propget, helpstring("返回/设置光标位置")]
    HRESULT _stdcall Cursor([out, retval] int* Value);
};
```

要点逐条：

| 成员 | 签名 | 语义 |
|---|---|---|
| `Output` | `HRESULT _stdcall Output([in] BSTR Text)` | 文字绘制实现；参数是 **BSTR** → 天然支持 **Unicode**（群友第一句"我的Print是可以打印Unicode字符出来"的依据） |
| `Cursor`（propput） | `HRESULT _stdcall Cursor([in] int Value)`（截图标注 `out,retval` 疑为工具显示误差，put 语义应为传入） | 设置光标位置，**字符位置单位**（第几列/行） |
| `Cursor`（propget） | `HRESULT _stdcall Cursor([out, retval] int* Value)` | 获取光标位置，字符位置单位 |
| 接口名 | `VBPrint`，继承 `IUnknown` | helpstring："Visual Basic Print Interface" |
| IID | `{000204F0-0000-0000-C000-000000000046}` | 在微软保留 IID 段（000204xx 系，与 ITypeInfo/ITypeLib 等标准接口同段）——**官方隐藏接口，文档从未公开** |

> 注意：截图里 propget/propput 两行的参数标注均显示 `[out, retval] int* Value`，propput 行按 COM 惯例应为 `[in] int Value`（put 传值入），疑为截图工具的显示误差；转写按截图保留并在此注明。实现 C3 时以 COM 属性存取器标准语义为准（get 取值 / put 传值）。

---

<a id="机制分析"></a>

## 三、机制分析

### 3.1 Print 语句的完整分发链（综合 002 + 003）

```
Print 语句 / obj.Print ...
        │
        ├─ 缺省对象 → 当前窗体（Me）
        │
        └─ 前缀对象 obj：
             1) obj 实现了 VBPrint 接口（QI IID {000204F0-...} 成功）
                  → 调 Output(BSTR) 输出文本、put_Cursor/get_Cursor 管光标
                  （VB6 自带 Form/PictureBox/Printer 的 Print 内部即走此接口；
                    外部类库 Implements VBPrint 后同样命中）
             2) 未实现 VBPrint（后期绑定对象）
                  → 按名字找成员派发（002 案例的同名成员机制，运行时 IDispatch::Invoke）
```

（注：1、2 两条路径的先后/互斥关系为合理推断，未经反汇编实证；但"实现 VBPrint 即可用 Print"是群友实测结论，"同名成员可命中"是 002 案例 dxvbgraph 的实测结论。C3 实现时建议 QI 优先、同名兜底，两种实测路径都覆盖。）

### 3.2 两套"位置"概念的区分（易踩坑）

| 概念 | 单位 | 归属 | 用途 |
|---|---|---|---|
| `CurrentX` / `CurrentY` | **绘图坐标单位**（随 ScaleMode，默认缇） | 画布（Form/PictureBox） | PSet/Line/Circle/Print 的落笔定位（`Step` 相对坐标的基准） |
| `VBPrint.Cursor` | **字符位置单位**（第几列/行，类似控制台光标） | VBPrint 接口 | Print 文本输出的字符级光标管理 |

C3 实现内部 Print 状态时必须**同时维护两套位置**，不能混用：`CurrentX/CurrentY` 影响文本绘制起点（像素/缇级），`Cursor` 影响字符列/行推进（`;`、`,`、`Spc()`、`Tab()` 的定位语义落在字符单位上）。

### 3.3 该接口的生态意义

- **官方未公开但有实锤**：IID 在微软保留段、helpstring 是官方口吻，且"VB6 自带对象的 Print 经由它"与群友实测自洽——这是 Print 语句从 VB2/3 时代一路继承下来的派发协议。
- **可定制输出后端**：Implements VBPrint = 给任意对象装一个"Print 语句插座"。群友已验证两个后端：
  - **控制台类**（`Implements VBPrint` → Output 写 stdout）→ `con.Print "..."` 完美工作；
  - **TextBox 版 Print**（Output 追加文本到 TextBox）→ 老式 BASIC 风格的滚动输出界面，零改造。
- **对 C3 的双面价值**：既是"Print 语句对外部对象的正确派发协议"（兼容层），又是"C3 产出的类可被 VB6 Print"的出口（生态层）。

---

<a id="c3实现清单"></a>

## 四、对 C3 的实现清单（后续要实现）

已同步记入 `todo.md`「下一步主方向」第 3 条。按依赖顺序：

1. **RTL 内置路径**（P1，随绘图语句一起做）
   - `vb6_Print(obj, ...)`：Form/PictureBox 上按 `CurrentX/CurrentY`（缇/像素）绘制文本；内部维护字符级 Cursor 状态支撑 `;`/`,`/`Spc()`/`Tab()` 定位语义
   - Text 输出统一走 Unicode（BSTR/W 版 API），对齐群友"Print 可打印 Unicode"的基线
2. **外部对象路径**（P1）
   - Print 语句目标为 COM 对象时：先 `QueryInterface(IID {000204F0-0000-0000-C000-000000000046})`，成功则调 `Output(BSTR)` / `put_Cursor / get_Cursor`
   - QI 失败再退回同名成员派发（002 机制，后期绑定 IDispatch::Invoke）
3. **`Implements VBPrint` 支持**（P2）
   - 允许 C3 编译的 VB6 类模块 `Implements VBPrint`（接口定义注入：内置到 C3 的 TypeLib 支持层，或随 `--typelib` 导入——注意 stock msvbvm60.tlb 没有它，C3 需自带该接口定义，IID 必须逐字一致）
   - Implements 派发链：`Print 语句 → QI → obj 的 Output/Cursor 实现`
4. **`--dll` 出口**（P2，招牌 demo 素材）
   - C3 生成 .tlb 时，为可被 Print 的类输出 VBPrint 接口定义（uuid 照抄），使 C3 编译出的 ActiveX DLL 对象在 VB6 里直接 `obj.Print ...`
   - Demo 素材：控制台 Print 类（对齐群友 `con.Print` 用例）、TextBox 版 Print 类
5. **回归测试**
   - `Implements VBPrint` 的类实例可作 Print 语句目标（QI 命中路径）
   - dxvbgraph 式同名成员对象作 Print 目标（兜底路径）
   - Unicode 字符经 Print 输出不乱码（BSTR 全程）
   - Cursor（字符单位）与 CurrentX/CurrentY（绘图单位）互不干扰
6. **顺带确认项**（低优先）
   - 反汇编/资料佐证 Form/PictureBox/Printer 的 Print 是否确实 QI 该接口（决定内置路径是否也统一走 VBPrint 抽象，便于内外一致）

---

*本文档由群聊讨论整理生成（2026-09-15）。VBPrint 接口信息由 dxvbgraph 作者（002 存档同一群友）提供并实测，IID 与签名以其截图为准；相关讨论背景见同目录 002 存档。*
