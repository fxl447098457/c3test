# Property Let 语句

**Property Let 语句**

       

声明 **Property Let** 过程的名称，参数以及构成其主体的代码，该过程给一个属性赋值。

**语法**

\[**Public** \| **Private** \| **Friend**\] \[**Static**\] **Property** **Let** *name* **(**\[*arglist***,**\] *value***)**  
\[*statements*\]  
\[**Exit Property**\]  
\[*statements*\]

**End Property**

**Property Let** 语句的语法包含下面部分：

|  |  |
|----|----|
| **部分** | **描述** |
| **Public** | 可选的。表示所有模块的所有其它过程都可访问该 **Property Let** 过程。如果在包含 **Option Private** 的模块中使用，则这个过程在该工程外是不可使用的。 |
| **Private** | 可选的。表示只有在包含其声明的模块的其它过程中可以访问该 **Property** **Let** 过程。 |
| **Friend** | 可选的。只能在类模块中使用。表示该 **Property Let** 过程在整个工程中都是可见的，但对于对象实例的控制者是不可见的。 |
| **Static** | 可选的。表示在调用之间将保留 **Property Let** 过程的局部变量的值。**Static** 属性对在该 **Property Let** 过程外声明的变量不会产生影响，即使过程中也使用了这些变量。 |
| *name* | 必需的。**Property Let** 过程的名称；遵循标准的变量命名约定，但不能与同一模块中的 **Property** **Get** 或 **Property Set** 过程同名。 |
| *arglist* | 可选的。代表在调用时要传递给 **Property Let** 过程的参数的变量列表。多个变量则用逗号隔开。**Property** **Let** 过程中的每个参数的名称和数据类型必须与 **Property** **Get** 过程中的相应参数一致。 |
| *value* | 必需的。指用于给属性赋值的变量。当调用该过程时，这个参数出现在调用表达式的右边。*value* 的数据类型必须和相应的 **Property Get** 过程的返回值类型一致。 |
| *statements* | 可选的。**Property Let** 过程中执行的任何语句组。 |

  

其中的 *arglist* 参数的语法和语法的各个部分如下：

\[**Optional**\] \[**ByVal** \| **ByRef**\] \[**ParamArray**\] *varname*\[**( )**\] \[**As** *type*\] \[**=** *defaultvalue*\]

|  |  |
|----|----|
| **部分** | **描述** |
| **Optional** | 可选的。表示参数不是必需的。如果使用了该选项，则 *arglist* 中的后续参数都必须是可选的，而且必须都使用 **Optional** 关键字声明。注意，一个 **Property Let** 表达式的右边是不可能为 **Optional** 的。 |
| **ByVal** | 可选的。表示该参数按值传递。 |
| **ByRef** | 可选的。表示该参数按地址传递。**ByRef** 是 Visual Basic 的缺省选项。 |
| **ParamArray** | 可选的。只用于 *arglist* 的最后一个参数，指明最后这个参数是一个 **Variant** 元素的 **Optional** 数组。使用 **ParamArray** 关键字可以提供任意数目的参数。**ParamArray** 关键字不能与 **ByVal、ByRef** 或 **Optional** 一起使用。 |
| *varname* | 必需的。代表参数变量的名称；遵循标准的变量命名约定。 |
| *type* | 可选的。传递给该过程的参数的数据类型；可以是 Byte、Boolean、Integer、Long、Currency、Single、Double、Decimal（目前尚不支持）、Date、String（只支持变长）、Object 或 Variant。如果参数不是 **Optional**，则也可以是用户定义类型，或对象类型。 |
| *defaultvalue* | 可选的。任何常数或常数表达式。只在 **Optional** 参数时是合法的。如果类型为 **Object**，则显式的缺省值只能是 **Nothing**。 |

  

**注意** 每个 **Property Let** 语句必须为其所定义的过程定义至少一个参数。当这个 **Property Let** 语句所定义的过程被调用时，该参数（如果有多个参数则指最后一个）就包含了将赋给属性的实际值。该参数就是前述语法中的 *value*。

**说明**

如果没有使用 **Public、Private** 或 **Friend** 显式指定，则 **Property** 过程缺省为公用。如果没有使用 **Static**，则不会在调用之后保留局部变量的值。**Friend** 关键字只能在类模块中使用。不过 **Friend** 过程可以被工程中的任何模块的过程访问。**Friend** 过程不会在其父类的类型库中出现，且 **Friend** 过程不能被后期绑定。

所有的可执行代码都必须属于某个过程。不能在别的 **Property、Sub** 或 **Function** 过程中定义 **Property Let** 过程。

**Exit Property** 语句使执行立即从一个 **Property Let** 过程中退出。程序接着从调用该 **Property Let** 过程的语句下一条语句开始执行。在 **Property Let** 过程的任何位置都可以有 **Exit Property** 语句。

**Property Let** 过程与 **Function** 和 **Property Get** 过程的相似之处是：**Property Let** 过程是一个可以获取参数，执行一系列语句，以及改变其参数值的独立过程。而与 **Function** 和 **Property Get** 过程不同的是：这两个过程都有返回值，而 **Property Let** 过程只能用于属性表达式或 **Let** 语句的左边。
