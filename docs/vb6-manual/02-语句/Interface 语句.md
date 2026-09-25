# Interface 语句

> **本项目扩展**（twinBASIC 风格适配），非微软 VB6 原生语句。规范与实施进度见仓库内 `ai/018`、`ai/022`。

**Interface 语句**

指定一个只含成员签名、不含实现体的接口契约块。

**语法**

**Interface** *InterfaceName* \[**Extends** *ParentInterface*\]
&nbsp;&nbsp;&nbsp;&nbsp;\[*成员签名*\]
**End Interface**

*成员签名* 只能是下面几种声明的**签名**（不得带 `End Sub` 一类的实现体）：

- **Sub** *name*\[(*参数列表*)\]
- **Function** *name*\[(*参数列表*)\] **As** *类型*
- **Property Get** | **Let** | **Set** *name*\[(*参数列表*)\] \[**As** *类型*\]

声明行上方可写方括号属性行，如 `[InterfaceId("{00000000-0000-0000-C000-000000000046}")]`、
`[Description("...")]`、成员级 `[DispId(3)]`。

**说明**

- 接口块是**模块级声明**，可出现在 `.bas` 与 `.cls` 中；接口名在工程内唯一。
- 类模块用 **Implements** *InterfaceName* 声明实现该接口，编译器逐项做**错误级**契约检查：
  缺成员、多成员、签名（参数个数、`ByVal`/`ByRef`、`Optional`、`ParamArray`、返回类型）不符都会报错。
- 实现成员可用尾子句 **Implements** *接口名*.*成员名* 显式绑定，逗号列表可同时绑定多个接口槽：

  ```vb
  Public Sub WriteText(ByVal s As String) Implements ILog.Write, IAudit.Record
  ```

  未写尾子句时按成员名与接口槽名**同名隐式匹配**（大小写不敏感）。写了子句的成员只按子句入座。
- 属性按 COM 惯例拆三槽：`Property Get X` → `get_X`、`Property Let X` → `put_X`、
  `Property Set X` → `putref_X`。
- `Extends` 链的槽序为**父先己后**、同层按声明序；COM 虚表没有遮蔽，故接口内不允许同名重载，
  链上槽名冲突会报错。
- 一文件一接口的宿主写法：`.cls` 文件（如 `IWriter.cls`，`Attribute VB_Name = "IWriter"`）内
  只含一个与该模块同名的 `Interface` 块时，模块名与接口名相同不算冲突；该宿主文件不得再放其它声明。

**注意**

- 接口块内不能出现字段、常量、枚举、事件、`Declare`，也不能带 `Public`/`Private` 等访问修饰符
  （契约成员一律公开）。
- 与 VB6 旧式用法的分界：`Implements` 一个**类模块**（VB6 把 `.cls` 当接口用的习惯）仍走旧路径，
  只做命名约定（*接口名*\_*成员名*）的告警式检查；`Implements` 一个 **Interface 语句**声明的接口
  才走上述严格检查。
- 标准模块里不能写成员级 `Implements` 子句；泛型类模板（`Class X(Of T)`）内不能声明接口。

**实现状态**

已交付（`ai/022` B01–B06b）：语法与契约检查、接口类型变量、虚表发码与派发、引用计数生命周期、
`QueryInterface`/跨接口 `Set`/`TypeOf … Is <接口>` 与上下行转换。**规范 IUnknown**（B13d）：一个类
实现多个接口时，从**任何**一个接口问 `IUnknown` 拿到的都是同一个指针（本类实现序里第一个接口的指针，
`QueryInterface` 的三个槽都真实可用），所以 `Set` 赋值与"同一个对象"的比较不会因入口接口而异而分裂。

对外那一半（B13–B16 已交付的部分）：`IDispatch` 四件套被真客户端点通（B14），**接口在类型库里占的
那一档现在是真接口**（B15：`TKIND_INTERFACE`，GUID 用唯一出口那枚；改之前那一档是"一行假 coclass
（带一枚谁也不认的 CLSID、还宣称可创建）+ 一张 0 成员的 `_<接口>` dispinterface"），且**如实发契约
成员**（B16：`cFuncs` = 契约成员数，每个成员带自己的 vtable 槽偏移 `oVft=(3+槽号)*指针宽`、
`callconv=stdcall` 与原生返回类型；类型库的位数 flag 跟 `--arch` 走 ⇒ x86 客户端按 4 字节一步读到的
槽偏移与 x64 的各按自己的指针宽）。生成的**槽表与实现函数在 x86 下一律 `__stdcall`**（x64 单一 ABI 下
MSVC 忽略该修饰）—— 这就是 COM 规范要求的调用约定，探针按库里那一形状直调槽并拿到返回值（x86/x64
双验）。**外部激活也真跑通了**（B17）：`DllRegisterServer` 写注册表之后，按接口 IID
`CoCreateInstance` 拿到的就是薄指针，外部客户端按库里那一形状直调契约槽得值（x86/x64 各一遍），
`DllUnregisterServer` 之后 CLSID / ProgID / TypeLib 三类键都不留。仍未交付的：接口值作实参 / 进
`Variant`；**canonical COM 的返回形状**（成员返回 `HRESULT`、结果走 `[out, retval]`）按 B17 的实测
**不做** —— 真按库里 `oVft` 直调的客户已经能用，它只有"把接口当 dual 自动化接口"才需要。

跨"进程内薄指针"与"COM 包装器"两个世界的身份：接口 IID 的 `QueryInterface` 现在**交薄指针**（B16
前交的是 COM 包装器胖指针），且包装器把底座引用交还给最后一个薄引用 —— `IClassFactory::CreateInstance`
那种"QI 完就 Release 包装器"的规范姿势下，交出去的接口指针不会悬空。同一次 QI 问 `IID_IUnknown`
拿到的仍是包装器指针（与薄指针是两个值）—— 这一条仍未接成同一个值：外部客户（`CreateObject`）
拿到的是包装器，进程内薄指针是另一个表示。

**另见**

[Implements 语句](Implements%20语句.md)
