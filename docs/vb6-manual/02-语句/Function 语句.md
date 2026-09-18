# Function 语句

**Function 语句**

       

声明 **Function** 过程的名称，参数以及构成其主体的代码。

**语法**

\[**Public** \| **Private \| Friend**\] \[**Static**\] **Function** *name* \[**(***arglist***)**\] \[**As** *type*\]  
\[*statements*\]  
\[*name* **=** *expression*\]  
\[**Exit Function**\]  
\[*statements*\]  
\[*name* **=** *expression*\]

**End Function**

**Function** 语句的语法包含下面部分：

|  |  |
|----|----|
| **部分** | **描述** |
| **Public** | 可选的。表示所有模块的所有其它过程都可访问这个 **Function** 过程。如果是在包含 **Option Private** 的模块中使用，则这个过程在该工程外是不可使用的。 |
| **Private** | 可选的。表示只有包含其声明的模块的其它过程可以访问该 **Function** 过程。 |
| **Friend** | 可选的。只能在类模块中使用。表示该 **Function** 过程在整个工程中都是可见的，但对于对象实例的控制者是不可见的。 |
| **Static** | 可选的。表示在调用之间将保留 **Function** 过程的局部变量值。**Static** 属性对在该 **Function** 外声明的变量不会产生影响，即使过程中也使用了这些变量。 |
| *name* | 必需的。**Function** 的名称；遵循标准的变量命名约定。 |
| *arglist* | 可选的。代表在调用时要传递给 **Function** 过程的参数变量列表。多个变量应用逗号隔开。 |
| *type* | 可选的。**Function** 过程的返回值的数据类型，可以是 Byte、布尔、Integer、Long、Currency、Single、Double、Decimal（目前尚不支持）、Date、String（除定长）、Object、Variant或任何用户定义类型。 |
| *statements* | 可选的。在 **Function** 过程中执行的任何语句组。 |
| *expression* | 可选的。**Function** 的返回值。 |

  

其中的 *arglist* 参数的语法以及语法各个部分如下：

\[**Optional**\] \[**ByVal** \| **ByRef**\] \[**ParamArray**\] *varname*\[**( )**\] \[**As** *type*\] \[**=** *defaultvalue*\]

|  |  |
|----|----|
| **部分** | **描述** |
| **Optional** | 可选的。表示参数不是必需的。如果使用了该选项，则 *arglist* 中的后续参数都必须是可选的，而且必须都使用 **Optional** 关键字声明。如果使用了 **ParamArray**，则任何参数都不能使用 **Optional** 声明。 |
| **ByVal** | 可选的。表示该参数按值传递。 |
| **ByRef** | 可选的。表示该参数按地址传递。**ByRef** 是 Visual Basic 的缺省选项。 |
| **ParamArray** | 可选的。只用于 *arglist* 的最后一个参数，指明最后这个参数是一个 **Variant** 元素的 **Optional** 数组。使用 **ParamArray** 关键字可以提供任意数目的参数。**ParamArray** 关键字不能与 **ByVal**，**ByRef**，或 **Optional** 一起使用。 |
| *varname* | 必需的。代表参数的变量的名称；遵循标准的变量命名约定。 |
| *type* | 可选的。传递给该过程的参数的数据类型；可以是 **Byte、Boolean、Integer、Long、Currency、Single、Double、Decimal**（目前尚不支持）、**Date、String**（只支持变长）、**Object** 或 **Variant**。如果参数不是 **Optional**，则也可以是用户定义类型，或对象类型。 |
| *defaultvalue* | 可选的。任何常数或常数表达式。只对于 **Optional** 参数时是合法的。如果类型为 **Object**，则显式缺省值只能是 **Nothing**。 |

  

**说明**

如果没有使用 **Public、Private** 或 **Friend** 显式指定，则 **Function** 过程缺省为公用。如果没有使用 **Static**，则局部变量的值在调用之后不会保留。**Friend** 关键字只能在类模块中使用。但 **Friend** 过程可以被工程的任何模块中的过程访问。**Friend** 过程不会在其父类的类型库中出现，且 **Friend** 过程不能被后期绑定。

**小心** **Function** 过程可以是递归的；也就是说，该过程可以调用自己来完成某个特定的任务。不过，递归可能会导致堆栈上溢。通常 **Static** 关键字和递归的 **Function** 过程不在一起使用。

所有的可执行代码都必须属于某个过程。不能在另外的 **Function、Sub** 或 **Property** 过程中定义 **Function** 过程。

**Exit Function** 语句使执行立即从一个 **Function** 过程中退出。程序接着从调用该 **Function** 过程的语句之后的语句执行。在 **Function** 过程的任何位置都可以有 **Exit Function** 语句。

 **Function** 过程与 **Sub** 过程的相似之处是： **Function** 过程是一个可以获取参数，执行一系列语句，以及改变其参数值的独立过程，而与**子**过程不同的是：当要使用该函数的返回值时，可以在表达式的右边使用 **Function** 过程，这与内部函数，诸如 **Sqr、Cos** 或 **Chr** 的使用方式一样。

在表达式中，可以通过使用函数名，并在其后用圆括号给出相应的参数列表来调用一个 **Function** 过程。请参阅 **Call** 语句关于如何调用 **Function** 过程的详细说明。

要从函数返回一个值，只需将该值赋给函数名。在过程的任意位置都可以出现这种赋值。如果没有对 *name* 赋值，则过程将返回一个缺省值：数值函数返回 0，字符串函数返回一个零长度字符串 ("")，**Variant** 函数则返回 Empty。如果在返回对象引用的 **Function** 过程中没有将对象引用赋给 *name* （通过 **Set**），则函数返回 **Nothing**。

下面的示例说明如何给一个名为 `BinarySearch` 的函数赋返回值。在这个示例中，将 **False** 赋给了该函数名，表示没有找到某个值。

    Function BinarySearch(. . .) As Boolean
    . . .
       '值未找到，返回一个 False 值。
       If lower > upper Then
          BinarySearch = False
          Exit Function
       End If
    . . .
    End Function

在 **Function** 过程中使用的变量分为两类：一类是在过程内显式声明的，另一类则不是。在过程内显式声明的变量（使用 **Dim** 或等效方法）都是局部变量。对于那些没有在过程中显式声明的变量，除非它们在该过程外更高级别的位置有显示地声明，否则也是局部的。

**小心** 过程可以使用没有在过程内显式声明的变量，但只要有任何在模块级别中定义的名称与之相同，就会产生名称冲突。如果过程中使用的未声明的变量与另一个过程，常数，或变量的名称相同，则会认为过程使用的是模块级的名称。显式声明变量就可以避免这类冲突。可以使用 **Option Explicit** 语句来强制显式声明变量。

**小心** Visual Basic 可能会重新安排数学表达式以提高内部效率。若 **Function** 过程会改变某个数学表达式中变量的值，则应避免在此表达式中使用该函数。
