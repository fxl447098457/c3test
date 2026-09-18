# Exit 语句

退出 **Do...Loop**、**For...Next**、**Function**、**Sub** 或 **Property** 代码块。

**语法**

**Exit** **Do**

**Exit For**

**Exit Function**

**Exit Property**

**Exit Sub**

**Exit** 语句的语法有以下几种形式：

|  |  |
|----|----|
| **语句** | **描述** |
| **Exit Do** | 提供一种退出 **Do...Loop** 循环的方法，并且只能在 **Do...Loop** 循环中使用。**Exit Do** 会将控制权转移到 **Loop** 语句之后的语句。当 **Exit Do** 用在嵌套的 **Do...Loop** 循环中时，**Exit Do** 会将控制权转移到 **Exit Do** 所在位置的外层循环。 |
| **Exit For** | 提供一种退出 **For** 循环的方法，并且只能在 **For...Next** 或 **For** **Each...Next** 循环中使用。**Exit For** 会将控制权转移到 **Next** 之后的语句。当 **Exit For** 用在嵌套的 **For** 循环中时，**Exit For** 将控制权转移到 **Exit For** 所在位置的外层循环。 |
| **Exit Function** | 立即从包含该语句的 **Function** 过程中退出。程序会从调用 **Function** 的语句之后的语句继续执行。 |
| **Exit Property** | 立即从包含该语句的 **Property** 过程中退出。程序会从调用 **Property** 过程的语句之后的语句继续执行。 |
| **Exit Sub** | 立即从包含该语句的 **Sub** 过程中退出。程序会从调用 **Sub** 过程的语句之后的语句继续执行。 |

  

**说明**

不要将 **Exit** 语句与 **End** 语句搞混了。**Exit** 并不说明一个结构的终止。
