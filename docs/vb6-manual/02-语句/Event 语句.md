# Event 语句

**Event 语句**

       

定义用户自定义的事件。

**语法**

\[**Public**\] **Event** *procedurename* \[**(***arglist***)**\]

**Event** 语句包含下面部分：

|  |  |
|----|----|
| **部分** | **描述** |
| **Public** | 可选的。指定该 **Event** 在整个工程中都是可见的。缺省情况下 **Events** 类型是 **Public**。应注意，事件只能在所声明的模块中产生。 |
| *procedurename* | 必需的。事件的名称；遵循标准的变量命名约定。 |

  

*arglist* 参数的语法及语法的各个部分如下：

\[**ByVal** \| **ByRef**\] *varname*\[**( )**\] \[**As** *type*\]

|  |  |
|----|----|
| **部分** | **描述** |
| **ByVal** | 可选的。表示该参数是按值传递的。 |
| **ByRef** | 可选的。表示该参数是按地址传递的。**ByRef** 是 Visual Basic 的缺省设置。 |
| *varname* | 必需的。代表要传递给过程的参数变量的名称；遵循标准的变量命名约定。 |
| *type* | 可选的。指传递给过程的参数的数据类型；可以是 Byte、布尔、Integer、Long、Currency、Single、Double、Decimal（目前尚不支持）、Date、String（只支持变长）、Object、Variant、用户定义类型或对象类型。 |

  

**说明**

事件被声明之后，就可以使用 **RaiseEvent** 语句来产生该事件。如果在标准模块中出现 **Event** 声明，就会产生语法错误。不能声明带返回值的事件。在下面的代码段中，给出了声明事件和产生事件的典型事件：

    '在类模块的模块级中声明一个事件

    Event LogonCompleted (UserName as String)

    Sub
       RaiseEvent LogonCompleted("AntoineJan")
    End Sub

**注意** 可以象声明过程的参数一样来声明事件的参数，但有以下不同：事件不能有带命名参数、**Optional** 参数、或者 **ParamArray** 参数。事件没有返回值。
