# #If...Then...#Else 指令

条件编译已选择的 Visual Basic 代码块。

**语法**

**\#If** *expression* **Then**

*statements*

\[**\#ElseIf** *expression-n* **Then**

\[*elseifstatements*\]\]

\[**\#Else**

\[*elsestatements*\]\]

**\#End If**

**\#If...Then...#Else** 指令的语法具有以下几个部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *expression* | 必要。包含一个或多个条件编译常数、文字与运算符的任何表达式，其值为 **True** 或 **False**。 |
| *statements* | 必要。Visual Basic 程序行或编译指令，如果关联的表达式为 **True**，则运行它们。 |
| *expression-n* | 可选。由一或多个条件编译常数、文字和运算符组成的任何一个表达式，其值为 **True** 或 **False**。 |
| *elseifstatements* | 可选。一个或多个程序行或编译命令，如果 *expression-n* 为 **True**，则运行它们。 |
| *elsestatements* | 可选。一个或多个程序行或编译命令，如果以前的*expression* 或 *expression-n* 中没有一个为 **True**，则运行它们。 |

  

**说明**

**\#If...Then...#Else** 指令的作用与 **If...Then...Else** 语句相同，其差异在于 **\#If**、**\#Else**、**\#ElseIf**，及 **\#End If** 指令没有单独成行的形式，也就是说，在指令所在的那一行，不能有其他代码出现。条件编译通常用来编译不同平台上的同一个程序。也可以用来避免调试程序代码出现在可执行程序中。条件编译时被排除的程序代码在最后的可执行文件中被完全略去，所以不会对程序的大小或功能有任何影响。

无论结果如何，都要计算所有表达式。所以，在表达式中用到的所有常数都必须加以定义— 任何未定义的常数都会被当作 Empty 来计算取值。

**注意**   **Option Compare** 语句不会影响 **\#If** 及 **\#ElseIf** 语句中的表达式。条件编译指令中的表达式总是用 **Option Compare Text** 计算值。
