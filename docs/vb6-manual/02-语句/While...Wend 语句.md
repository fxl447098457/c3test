# While...Wend 语句

只要指定的条件为 **True**，则会重复执行一系列的语句。

**语法**

**While** *condition*  
\[*statements*\]

**Wend**

**While...Wend** 语句的语法具有以下几个部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *condition* | 必要参数。数值表达式或字符串表达式，其计算结果为 **True** 或 **False**。如果 *condition* 为 Null，则 *condition* 会视为 **False**。 |
| *statements* | 可选参数。一条或多条语句，当条件为 **True** 时执行。 |

  

**说明**

如果 *condition* 为 **True**，则所有的 *statements* 都会执行，一直执行到 **Wend** 语句。然后再回到 **While** 语句，并再一次检查 *condition*，如果 *condition* 还是为 **True**，则重复执行。如果不为 **True**，则程序会从 **Wend** 语句之后的语句继续执行。

**While...Wend** 循环也可以是多层的嵌套结构。每个 **Wend** 匹配最近的 **While** 语句。

**提示**   **Do...Loop** 语句提供了一种结构化与适应性更强的方法来执行循环。
