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

- CoClass 块是**模块级声明**，可出现在 `.bas` 与 `.cls` 中，块名在工程内唯一（模块名、接口名、
  CoClass 名共用同一套命名空间）。**例外**：块名与**它自己所在的宿主模块**同名是 VB6 的惯用写法
  （`Widget.cls` 里写 `CoClass Widget`，实现就是宿主类本身），不算撞车。
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
- 块列出的每个接口都是一份**要核对的契约**（`ai/022` B11/C03b）：`[Implementation]` 那个类
  **连同它的祖先**必须满足每一槽，缺槽报 `VB3012`、签名不符报 `VB3017` —— 与 `Implements`
  用的同一套槽键与签名口径（属性按 get/put/putref 分三席，`ByVal`/`ByRef` 算签名的一部分）。
  实现类写了 `Implements I Via m_holder` 时整份契约由持有对象满足，逐槽"未实现"照旧免报。

**注意**

- **已交付校验**（编译期错误，`ai/022` B11/C03a + C03b）：块名重复、块名撞别的模块名或接口名
  （`VB3032`）；契约条目引用不存在的接口、把**类模块**当接口列进集合、同一条目写两遍、
  标了不止一条 `[Default]`（`VB3031`）；`[Implementation]` 指向不存在或不是类模块、
  EXE 工程里写 `[ComCreatable(True)]`（`VB3033`，只有 ActiveX DLL 才注册 COM 服务器，
  EXE 保留组内那半）；**契约聚合**（块列的接口，实现类含祖先必须满足）报 `VB3012`/`VB3017`；
  `Inherits` 一个 CoClass 块名报 `VB3020`，话已改成指名"那是组契约的块，没有成员表可继承"。
- **组内已激活**（`ai/022` B11/C05）：块名可以当类型用。写在**类型位置**上的块名 —— `Dim c As Circle`、
  形参 `Sub Use(c As Circle)`、返回值 `Function F() As Circle`、模块级字段、UDT 成员
  （`Private Type THeld : c As Circle : End Type`，实测 `h.c.Move 6` 跑得通）、`New Circle` ——
  在编译期就地改写成 `[Implementation]`
  那个**类名**，此后走的就是"工程类"那条已经跑通的路：变量是类结构体指针、`New` 直调类工厂
  （不经注册表）、成员调用按实现类的虚表派发（派生类 `Overrides` 的那份会答话，不是静默绑根）。
  `Set v = CreateObject("<工程名>.<块名>")` 里 ProgID 命中本工程某个块的，同样在编译期换成
  `New <实现类>`；命中不到的一概照旧走注册表，外部组件不受影响。真发生了改写，stderr 有一行
  `C3: CoClass 'Circle' activated in-project: type name -> class 'ShapeAct' (...)`，没用到就一个字不多。
- **`ReDim a(1) As Circle` 现在编得过**（Fix 192，2026-09-25 修）。这条以前记的是"会编不过，
  但不是 CoClass 的事" —— 前半句已不成立，后半句的**判断是对的，范围记窄了**：撞 `C2224` 的
  不是 `ReDim … As <类名>` 这一个形状，而是**元素类型为项目类的数组的成员访问**本身。
  `Dim s(1) As ShapeAct` 这种静态数组一样撞（`s(0).Move` 发成
  `VB6_SA_AT(void*, s, 0).Move(...)`，void* 取成员）。根因在**访问侧推断**：
  `resolveArrayElemType` 把 `As 某类` 压成 `Vb6Type::Object`（枚举装不下"哪个类"），
  `inferClassTypeOfExpr` 又只认"方法调用返回类"，于是 `arr(i)` 推断不出类。
  修法 = 登记元素类名（`arrayClassElemTypes_`，`Dim`/`ReDim As` 两侧都登记）+ 推断侧认它，
  之后照常走早绑定 `vb6_ShapeAct_Move(VB6_SA_AT(void*, a, 0), …)`。
  覆盖：`Dim s(1) As C`、`Dim a() As C` + `ReDim a(n)`（不带 As）、`ReDim a(n) As C`、
  `ReDim Preserve`、模块级数组、CoClass 块名当元素类型（`ReDim a(1) As Circle`）。
  夹具 `tests/arr_cls`（AC1–AC6），基线实测 28 条 `C2224` → 修后 0，x64 + x86 都跑。
- **`TypeOf c Is Circle` 现在答得对**（Fix 193，2026-09-25 修）。这条以前记的是"今天仍然答否，
  而且与 CoClass 无关" —— **前半句的"与 CoClass 无关"是对的，但"答否"是既有缺口这一层没查到底**：
  `TypeOf … Is <类名>` 落进的是 `vb6_TypeOf`，那是 `vb6rtl_conv.c` 里一个**恒返 0 的桩**
  （注释自己写着"简化版，始终返回 False"）。所以错的**不是** `Is Circle` 这一支，而是
  `Is <项目类>` 整支 —— 拿一个从没写过 CoClass 的工程实测 `TypeOf raw Is ShapeAct` 也答 False，
  而 `raw` 就声明成 `ShapeAct`。只有接口名那条路（`TypeOf iv Is IShapeAct`，走
  `vb6_IfaceSupports` 真 QueryInterface）一直是好的。
  修法 = 编译期按**声明类 + 祖先链**判定（`inferClassTypeOfExpr` 取声明类，`ClassChainView::chain`
  自根到叶含祖先）：目标类是声明类或其祖先 → 真（`Nothing` 仍要判空，不能发常量 1）；否则假。
  CoClass 那条路原样受益 —— 块名在类型位置已改写成实现类名，于是 `TypeOf c Is Circle` 与
  `TypeOf c Is ShapeAct` 都答"是"。
  覆盖：自身 / 直接祖先 / 链根 / 兄弟类（False）/ 无关类（False）/ `Nothing` 与 `Set … = Nothing`
  （False）/ 无继承无虚槽的普通类 / 类数组元素（与 Fix 192 联动）。
  夹具 `tests/typeof`（TOF1–TOF12），基线实测 **8 FAIL / 4 OK** —— 那 4 个 OK 只是"本该 False"
  被恒假蒙对；修后 12/12，x64 + x86 都跑。
  **残留边界（登记，不静默）**：判定按的是**声明**类型，不是运行时实际类型。
  `Dim b As InhBase : Set b = New InhDerived` 之后 `TypeOf b Is InhDerived`，VB6 按实际类型答"是"，
  这里答"否"。这是**假阴性，与改前恒假同向**，不会把原本对的翻成错的；要修得给类加运行时类型标记
  （`__cvtbl` 只在**有虚槽**的类上生成，当通用 RTTI 覆盖面不均；给所有类加字段则动结构体布局，
  022 线有逐字节护栏），代价与收益不成比例，不在这批里做。
- **一处口径偏差，记清楚别当 bug 找**：`As Circle` 能摸到的成员面是**实现类的公开成员**，比默认接口宽。
  要"只有默认接口那一份"就写 `As IShape`（B02/B03 的接口视图，比对更严）。为什么 v1 不拿接口视图当
  `As <块名>` 的默认：契约按 `ai/026` 五-1 只要求"实现类**连同祖先满足**这些槽"，实现类完全可以不写
  `Implements`（上面 `p08` 那种形状就是合法的），那时候对象身上没有那份接口槽表可指。
- 两条边界：`CreateObject` 的改写只认"整个右值就是这一枚调用"（`Set c = CreateObject("p.c")`），
  嵌在更大表达式里的（`Foo(CreateObject("p.c"))`）照旧走注册表；块名与一个**同名模块**并存时
  类/模块赢 —— `Widget.cls` 里写 `CoClass Widget` 是 VB6 惯用写法，`As Widget` 的含义不许被一块
  新语法改掉；折算记录（下条）一律不进类型表，同一个理由。
- 没写 `[Implementation]` 的块**不判契约**（无从判起），块本身照样合法；但把它的名字当类型用会报
  `VB3039` —— 那块没有可绑的实现类，而这一位在改前是"未知类型名被当 `Variant` 吞掉、成员调用变成
  空指针上的晚绑定"，编译器既然已经知道块没有实现类，就没有理由让用户去猜。
- `CoClass` 是软关键字，存量代码里同名变量、同名成员照旧可用。
- VB6 存量写法：类模块头部那几行 `Attribute VB_Creatable` / `VB_Exposed` / `VB_PredeclaredId` /
  `VB_GlobalNameSpace` **会被折算成一条 CoClass 记录**（`ai/022` B11/C04），于是老工程的 CLSID
  仍然走 `.vbp` 的三段式 `Class=` 那档、身份求解只有一处。折算**只记信息、不判死**：
  折算来的记录不参与上面任何一条新校验（`VB3031`/`VB3032`/`VB3033`、契约聚合），因为
  "EXE 工程 + `VB_Creatable = True`"是 VB6 存量工程的正常形状，拿新规矩打它就是打死能编的代码。
  同一个模块里**又写了手写块**时以手写块为准，属性行不折算（会报一行信息说明）。
  带点的成员级属性行（`Attribute m_X.VB_Description` 一类）**不是**头属性，一概不折。
- 事件源形态的 `[Default, Source]` 逗号并列写法目前不认（属性名按整个方括号内文本比对）。

**实现状态**

`ai/022` B11/C01：语法与语法树（块、属性行、契约条目、`[Default]` 标记）；B11/C02：三档身份
（CLSID/IID/ProgID）的唯一求解函数与可复现性；B11/C03a：形状与名字校验（见上"注意"清单）；
B11/C03b：契约聚合校验（`VB3012`/`VB3017`）；B11/C04：存量头属性的只读折算；
**B11/C05（= 进度表里的 B12）：组内激活** —— `As`/`New`/`CreateObject` 绑到 `[Implementation]` 类。
前五格对不用该语法的工程**逐字节不变**；C05 只在"块名被当类型用"或 ProgID 命中本工程时才动发码，
不这么写的工程照旧逐字节不变（`ai/022` D55 的 16 文件 `--emit-c` 护栏里连 `cc_id` 这种"声明了块、
从不把块名当类型"的工程也多不出一个 stderr 字符）。
**对外可用**（类工厂、注册、类型库、外部进程 `CreateObject`）要到 B13–B17。

**另见**

[Interface 语句](Interface%20语句.md)、[Implements 语句](Implements%20语句.md)
