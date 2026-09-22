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

当前版本交付的是语法与编译期契约检查。接口类型变量、虚表发码、`New`/`CreateObject` 激活与
COM 注册（IUnknown/IDispatch、类型库导出）按 `ai/022` 进度表在后续批次交付。

**另见**

[Implements 语句](Implements%20语句.md)
