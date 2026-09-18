# Enum 语句

**Enum 语句**

       

定义枚举类型。

**语法**

 \[**Public** \| **Private**\] **Enum** *name*

*membername* \[= *constantexpression*\]

*membername* \[= *constantexpression*\]

**. . .**

**End Enum**

**Enum** 语句包含下面部分：

|  |  |
|----|----|
| **部分** | **描述** |
| **Public** | 可选的。表示该 **Enum** 类型在整个工程中都是可见的。**Enum** 类型的缺省情况是 **Public**。 |
| **Private** | 可选的。表示该 **Enum** 类型只在所声明的模块中是可见的。 |
| *name* | 必需的。该 **Enum** 类型的名称。*name* 必须是一个合法的 Visual Basic 标识符，在定义该 **Enum** 类型的变量或参数时用该名称来指定类型。 |
| *membername* | 必需的。用于指定该 **Enum** 类型的组成元素名称的合法 Visual Basic 标识符。 |
| *constantexpression* | 可选的。元素的值（为 **Long** 类型）。可以是别的 **Enum** 类型。如果没有指定 *constantexpression*，则所赋给的值或者是 0（如果该元素是第一个 *membername*），或者比其直接前驱的值大 1。 |

  

**说明**

所谓枚举变量，就是指用 **Enum** 类型定义的变量。变量和参数都可以定义为 **Enum** 类型。**Enum** 类型中的元素被初始化为 **Enum** 语句中指定的常数值。所赋给的值可以包括正数和负数，且在运行时不能改变。例如：

    Enum SecurityLevel
       IllegalEntry = -1
       SecurityLevel1 = 0
       SecurityLevel2 = 1
    End Enum

**Enum** 语句只能在模块级别中出现。定义 **Enum** 类型后，就可以用它来定义变量，参数或返回该类型的过程。不能用模块名来限定 **Enum** 类型。类模块中的 **Public** **Enum** 类型并不是该类的成员；只不过它们也被写入到类型库中。在标准模块中定义的 **Enum** 类型则不写到类型库中。具有相同名字的 **Public Enum** 类型不能既在标准模块中定义，又在类模块中定义，因为它们共享相同的命名空间。若不同的类型库中有两个 **Enum** 类型的名字相同，但成员不同，则对这种类型的变量的引用，将取决于哪一个类型库具有更高的**引用**优先级。

不能在 **With** 块中使用 **Enum** 类型作为目标。
