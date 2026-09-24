# CoClass 语句

> **本项目扩展**（twinBASIC 风格适配），非微软 VB6 原生语句。VB6 只有等价的 attribute 写法
> （`Attribute VB_Creatable`、`Instancing` 等），设计取舍与实施进度见仓库内 `ai/026`、`ai/022`。

**CoClass 语句**

指定一个把"对外契约"与"实现类"聚在一起的 CoClass 声明块。

**语法**

**CoClass** *CoClassName*
&nbsp;&nbsp;&nbsp;&nbsp;\[*属性行*\]
&nbsp;&nbsp;&nbsp;&nbsp;\[**\[Default\]**\] **Interface** *InterfaceName*
**End CoClass**

块内只允许两类行：

- **属性行**（方括号形式，可多行）：块级的 `[CoClassId("{GUID}")]`、`[ProgId("...")]`、
  `[ComCreatable(True)]`、`[Implementation("实现类名")]`，条目级的 `[Default]`；
- **契约条目** **Interface** *InterfaceName* —— 这是对**已声明接口**的引用，不是内联定义，
  因此条目后不跟成员签名、也没有 `End Interface`。

**说明**

- CoClass 块是**模块级声明**，可出现在 `.bas` 与 `.cls` 中，块名按设计在工程内唯一（该唯一性
  与下面各条校验一样目前尚未实现，见"注意"）。
- 属性行的归属按"名字 + 位置"判定：`[Default]` 永远属于紧随其后的那条契约条目（可与它同行，
  也可单独一行）；其余属性行写在**首个契约条目之前**时属于块本身，写在条目之后则属于下一条目。
- 属性行的实参可以是字符串、整数或 `True`/`False`。
- `[Default]` 标出的接口是该 CoClass 的**默认接口**，它必须同时出现在契约集合里。
- 块的身份（CLSID / IID / ProgID）由**唯一一处**规则求解，三档优先级：

  ```
  CLSID:  [CoClassId("{...}")]  >  vbp 三段式 Class=Name; x.cls; {CLSID}  >  确定性派生
  IID:    默认接口的 [InterfaceId("{...}")]                              >  确定性派生
  ProgID: [ProgId("...")]                                               >  <工程名>.<CoClass名>
  ```

  确定性派生以工程名与块名为种子做哈希，**不含随机数**：同一份输入每次构建得到同一个 GUID。
  编译时会为每个块向标准错误输出一行
  `C3: CoClass 'X' identity: CLSID=... (档位) IID=... (档位) ProgID=... (档位) impl='...' comCreatable=...`，
  用来核对"这个块到底拿到了哪个身份、由哪一档给的"。
- vbp 三段式挂在**类模块**上，所以 CLSID 查表的次序是：先按 `[Implementation]` 指的模块名，
  查不到再按 CoClass 块名。
- `[Implementation("...")]` 是绑定实现类的**唯一**形式：不做同名隐式匹配，也不反查 `Implements`。
  一个 CoClass 绑一个实现类。
- 一个 CoClass 对外只有块里列出的那些接口；实现类自己的其它成员（含 `Private` 成员）不属于契约。
  这正是显式块相对 VB6 attribute 写法的意义。

**注意**

- 目前交付的是**语法、语法树与身份求解**这三层：把块名当类型用（`As` *CoClassName*、
  `New` *CoClassName*、`CreateObject` *ProgID* 的编译期改写）与契约完整性校验还没有实现，
  按进度表 `ai/022` 的 B11 后续格（C03/C05）与 B12 交付。块名与引用的接口名**当前不做存在性
  检查**，写错不会报错，只会没有效果；身份今天只求出来、还没有任何一端去用它。
- `CoClass` 是软关键字，存量代码里同名变量、同名成员照旧可用。
- VB6 存量的 `Attribute VB_Creatable` / `Instancing` / `VB_Exposed` 一类属性行**今天不参与**任何
  COM 身份求解（只有 `Attribute VB_Name` 被消费）；把它们折算成 CoClass 记录是独立的一格（`ai/026` C04）。
- 事件源形态的 `[Default, Source]` 逗号并列写法目前不认（属性名按整个方括号内文本比对）。

**实现状态**

`ai/022` B11/C01：语法与语法树（块、属性行、契约条目、`[Default]` 标记）；B11/C02：三档身份
（CLSID/IID/ProgID）的唯一求解函数与可复现性。两者对不用该语法的工程都**逐字节不变**。
**组内可用**（契约聚合校验 + `As`/`New`/`CreateObject`）要到 B11 的 C03/C05 与 B12 点亮；
**对外可用**（类工厂、注册、类型库、外部进程 `CreateObject`）要到 B13–B17。

**另见**

[Interface 语句](Interface%20语句.md)、[Implements 语句](Implements%20语句.md)
