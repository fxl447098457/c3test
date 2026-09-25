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
  EXE 保留组内那半 —— 反过来说，在 DLL 工程里这一位是真开关：它决定组名档 ProgID 会不会进
  COM 服务器表，见下"实现状态"）；**契约聚合**（块列的接口，实现类含祖先必须满足）报 `VB3012`/`VB3017`；
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
**对外可用**（类工厂、注册、类型库、外部进程 `CreateObject`）走 B13–B17，`ai/022` D56 量现状、D57/D58 记落地。
先说不是从零开始的那半：`Type=DLL` 工程今天**能编能链**（`DllGetClassObject`/`DllRegisterServer` 那一族、
IDispatch 成员表、内嵌 `.tlb` 都在发），RTL 侧的 `QueryInterface`/`AddRef`/`Release` 也是**真实现**
（原子计数、归零销毁实例并在那里触发 `Class_Terminate`）。回归里有五条用例钉住这条线
（`tests\test_activex_dll\` 两份 + `tests\cc_dll\`，助手 `Test-VbpDll`，外加一条读 `.tlb` 的
`Test-TlbIdentitySingleSource`）。

**B13b/B13c 已经接上的**（`ai/022` D57、D58）：
- **块名那一档 ProgID 会注册了**，但**要显式表态** —— `[ComCreatable(True)]` 是唯一开关：写了它，
  COM 服务器表里就多出一行 `<工程名>.<块名>`（同 CLSID、同类工厂），`g_vb6_coclassCount` 从 1 变 2；
  不写它，组名对外仍然不存在。`<工程名>.<类模块名>` 那一档**保留**，存量 DLL 客户不受影响。
- **表里的身份与语义层同源**：CLSID 走唯一出口（`[CoClassId]` > vbp 三段式 > 确定性派生），
  块内 `[Default]` 接口的 IID 也出自同一张表。
- **一个接口在一次编译里只有一枚 GUID**（B13c）：接口自己的 IID 也搬进了唯一出口
  （`[InterfaceId]` > 按 `<工程名>` + 接口名的确定性派生），三条通道 —— COM 服务器表、
  实现类自己的接口 vtable 的 `QueryInterface`、类型库 —— 一律读它，不再各算一份。
  回归里 `cc_dll_tlb_matches_table` 用 `tests\tools\tlbprobe.cpp`（`LoadTypeLib` 探针）
  钉住这一点，改动前的实测是同一个 `IProbe` 有**四枚**值。
- **对外广告的那个视图 == 服务器应答的那个视图**（B13e）：类型库里 coclass 的 DEFAULT 引用、
  COM 服务器表里的"默认接口 IID"，一律是 `<_类名>` 那一档 —— 也就是类的**公有成员**，
  正是 `IDispatch` 的 `GetIDsOfNames`/`Invoke` 真正查的那张表。`[Default] Interface IProbe`
  管的是**语言层**的默认视图（`As PG` 拿到哪份成员面、虚表怎么排），不改对外那一档。
  B13c 曾把类型库的 DEFAULT 改成跟随块，实测是把"广告"与"应答"劈开：客户端按库里的
  默认接口去点，服务器那头的成员面却是另一份，两头都点不到 ⇒ 本批回退。
- 边界要说清：① **折算记录不享受以上各条**（VB6 头属性折来的身份继续走原路，改了就是把存量
  DLL 工程的注册身份换掉）；② 表里没有的接口（不是新式接口）照旧走各自的老派生。
- **口径（B13c 定）**：`[Default]` 指向新式接口的类，**契约成员不会变成对外可点的 disp id**。
  契约成员按 VB6 惯例是 `Private`，把它们发进 IDispatch 表等于换语义；而这个接口的成员对外
  可调用要靠"真接口"那条路（类型库 `TKIND_INTERFACE` 已由 B15 发出，**成员**进库排在 B16）。
  今天对外能调用的是**类的公有成员**那一档 —— 这条已经有真客户端钉住：回归里的
  `ax_dll_dispatch_invoke` 用 `tests\tools\disp_probe.c`（`LoadLibrary` + `DllGetClassObject`，
  不查注册表）把编出来的 DLL 真的按 `IDispatch` 调通，`cc_dll_dispatch_iface_only` 则钉住反面：
  只满足新式接口的类，`GetIDsOfNames` 一个名字都不认。
- **接口模块在类型库里就是真接口**（B15）：`Interface IProbe` 的宿主模块在库里那一档是
  `TKIND_INTERFACE`，GUID 用唯一出口那枚，行名就是接口自己的名字。改之前它是**两行假广告**：
  一行 `coclass IProbe`（带一枚谁也不认的 CLSID，还宣称可创建）+ 一张 0 成员的 `_IProbe`
  dispinterface。回归里 `cc_dll_tlb_matches_table` 的通道 3 跟着换了读法，并加两条**反面**断言
  （库里再出现那两行就判红）。
- **接口成员在库里如实发契约**（B16）：那一档的 `cFuncs` = 契约成员数，每个成员带
  `oVft=(3+槽号)*指针宽` / `callconv=stdcall` / 原生返回 vt（`ByRef` 建 `VT_PTR` 链），
  类型库的位数 flag 跟 `--arch` 走。薄面（槽表 + 实现函数）在 x86 下一律 `__stdcall`；
  接口 IID 的 `QueryInterface` 交**薄指针**（前 3 槽就是 IUnknown，客户端可当 `IUnknown*` 收尾）。
- **外部激活真跑通**（B17）：`DllRegisterServer` 写注册表之后 ——
  - 按 CLSID / ProgID `CoCreateInstance` 拿到 `IDispatch`（= `CreateObject` 走的那条路），
    按名调用**类的公有成员**得值；
  - 按**接口 IID** 激活拿到薄指针，客户端按库里的 `oVft` **直调契约槽**得值（x86 与 x64 各一遍）；
  - `DllUnregisterServer` 之后 CLSID / ProgID / TypeLib 三类键**都不留**（用例可重复跑、不脏机器）。
  契约成员是 `Private`，所以类的**默认面上按名点不到**它们 —— 这是 VB6 语义的应有行为，不是缺陷；
  接口成员走上面那条早绑定路。回归里 `cc_dll_external_activate`（x64）、
  `cc_dll_external_activate_x86` 钉住这些读数，`cc_dll_late_client` 则是把"另一个进程才是客户"
  那半接上：C3 编译的外部客户 EXE（不引用 DLL）`CreateObject` 后按名调用。
  这一格顺带修掉两条真缺陷：① `rc.exe` 的发现面太窄 —— SDK 不在默认盘时**静默不嵌**类型库资源，
  于是注册表里根本没有 TypeLib 项、外部客户按 LIBID 找不到库；② 反注册的 `UnRegisterTypeLib`
  实参顺序写反（`lcid`/`syskind` 互换）⇒ 每次反注册都静默漏掉整棵 TypeLib 键。
- 仍开：**canonical 返回形状**（`HRESULT` + `[out, retval]`）按 B17 的实测**不做** ——
  真按库里 `oVft` 直调的客户已经能用，它只有"把接口当 dual 自动化接口"才需要；
  跨"进程内薄指针"与"COM 包装器"两个世界的 IUnknown 身份目前仍是两个值（外部客户拿到的是包装器指针）。

**另见**

[Interface 语句](Interface%20语句.md)、[Implements 语句](Implements%20语句.md)

**端到端示例**：`tests\cc_demo\` —— 同一份源集合编成 EXE（语言侧 12 条断言）与 DLL（注册后由外部客户
`CreateObject` 激活）两种形态；把接口、`Implements`（含 `Via` 委托）、`Inherits`/`Overrides`/`Protected`/
`MyBase`、CoClass 块与组内激活一次用全。分工与边界见 `ai/027-接口继承CoClass实施收口.md`。
