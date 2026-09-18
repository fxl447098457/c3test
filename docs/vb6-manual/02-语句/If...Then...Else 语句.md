# If...Then...Else 语句

根据表达式的值有条件地执行一组语句。

**语法**

**If** *condition* **Then** \[*statements*\]\[**Else** *elsestatements*\]

或者，可以使用块形式的语法：

**If** *condition* **Then**  
\[*statements*\]

\[**ElseIf** *condition-n* **Then**  
\[*elseifstatements*\] `...`

\[**Else**  
\[*elsestatements*\]\]

**End If**

**If...Then...Else** 语句的语法具有以下几个部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *condition* | 必要参数。一个或多个具有下面两种类型的表达式： |
|   | 数值表达式或字符串表达式，其运算结果为 **True** 或 **False**。如果 *condition* 为 Null，则 *condition* 会视为 **False。** |
|   | **TypeOf** *objectname* **Is** *objecttype* 形式的表达式。其中的 *objectname* 是任何对象的引用，而 *objecttype* 则是任何有效的对象类型。如果 *objectname* 是 *objecttype* 所指定的一种对象类型，则表达式为 **True**，否则为**False**。 |
| *statements* | 在块形式中是可选参数；但是在单行形式中，且没有 **Else** 子句时，则为必要参数。一条或多条以冒号分开的语句，它们在 *condition* 为 **True** 时执行。 |
| *condition-n* | 可选参数。与 *condition* 同。 |
| *elseifstatements* | 可选参数。一条或多条语句，它们在相关的 *condition-n* 为 **True** 时执行。 |
| *elsestatements* | 可选参数。一条或多条语句，它们在前面的 *condition* 或 *condition-n* 都不为 **True** 时执行。 |

  

**说明**

可以使用单行形式（第一种语法）来做短小简单的测试。但是，块形式（第二种语法）则提供了更强的结构化与适应性，并且通常也是比较容易阅读、维护及调试的。

**注意** 在单行形式中，按照 **If...Then** 判断的结果也可以执行多条语句。所有语句必须在同一行上并且以冒号分开，如下面语句所示：

    If A > 10 Then A = A + 1 : B = B + A : C = C + B

在块形式中，**If** 语句必须是第一行语句。其中的 **Else**、 **ElseIf**，和 **End If** 部分可以只在之前加上行号或行标签。**If** 块必须以一个 **End If** 语句结束。

要决定某个语句是否为一个 **If** 块，可检查 **Then** 关键字之后是什么。如果在 **Then** 同一行之后，还有其它非注释的内容，则此语句就是单行形式的 **If** 语句。

**Else** 和 **ElseIf** 子句都是可选的。在 **If** 块中，可以放置任意多个 **ElseIf** 子句，但是都必须在 **Else** 子句之前。**If** 块也可以是嵌套的。

当程序运行到一个 **If** 块（第二种语法）时，*condition* 将被测试。如果 *condition*为 **True**，则在 **Then** 之后的语句会被执行。如果 *condition* 为 **False**，则每个 **ElseIf** 部分的条件式（如果有的话）会依次计算并加以测试。如果找到某个为 **True** 的条件时，则其紧接在相关的 **Then** 之后的语句会被执行。如果没有一个 **ElseIf** 条件式为 **True**（或是根本就没有 **ElseIf** 子句），则程序会执行 **Else** 部分的语句。而在执行完 **Then** 或 **Else** 之后的语句后，会从 **End If** 之后的语句继续执行。

**提示** 根据单一表达式来执行多种可能的动作时，**Select Case** 更为有用。不过，TypeOf objectname Is objecttype 子句不能在 Select Case 语句中使用。

**注意 TypeOf** 不能与诸如 Long、Integer 以及其他不是 Object 的固定数据类型一起使用。
