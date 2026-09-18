# Do...Loop 语句

当条件为 **True** 时，或直到条件变为 **True** 时，重复执行一个语句块中的命令。

**语法**

**Do** \[{**While** \| **Until**} *condition*\]  
\[*statements*\]  
\[**Exit Do**\]  
\[*statements*\]

**Loop**

或者可以使用下面这种语法：

**Do**  
\[*statements*\]  
\[**Exit Do**\]  
\[*statements*\]

**Loop** \[{**While** \| **Until**} *condition*\]

**Do Loop** 语句的语法具有以下几个部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *condition* | 可选参数。数值表达式或字符串表达式，其值为 **True** 或 **False**。如果 *condition* 是 Null，则 *condition* 会被当作 **False**。 |
| *statements* | 一条或多条命令，它们将被重复当或直到 *condition* 为 **True**。 |

  

**说明**

在**Do…Loop** 中可以在任何位置放置任意个数的 **Exit Do** 语句，随时跳出**Do匧oop** 循环。**Exit Do** 通常用于条件判断之后，例如**If匱hen**，在这种情况下，**Exit Do** 语句将控制权转移到紧接在 **Loop** 命令之后的语句。

如果 **Exit Do** 使用在嵌套的**Do…Loop** 语句中，则 **Exit Do** 会将控制权转移到 **Exit Do** 所在位置的外层循环。
