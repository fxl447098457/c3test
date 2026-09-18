# Select Case 语句

根据表达式的值，来决定执行几组语句中的其中之一。

**语法**

**Select Case** *testexpression*  
\[**Case** *expressionlist-n*  
\[*statements-n*\]\] `...`  
\[**Case Else**  
\[*elsestatements*\]\]

**End Select**

**Select Case** 语句的语法具有以下几个部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *testexpression* | 必要参数。任何数值表达式或字符串表达式。 |
| *expressionlist-n* | 如果有 **Case** 出现，则为必要参数。其形式为 *expression*，*expression* **To** *expression*，**Is** *comparisonoperator* *expression*的一个或多个组成的分界列表*。***To** 关键字可用来指定一个数值范围。如果使用 **To** 关键字，则较小的数值要出现在 **To** 之前。使用 **Is** 关键字时，则可以配合比较运算符（除 **Is** 和 **Like** 之外**）**来指定一个数值范围。如果没有提供，则 **Is** 关键字会被自动插入。 |
| *statements-n* | 可选参数。一条或多条语句，当 *testexpression* 匹配*expressionlist-n*中的任何部分时*执行。* |
| *elsestatements* | 可选参数。一条或多条语句，当 *testexpression* 不匹配 **Case** 子句的任何部分时执行。 |

  

**说明**

如果 *testexpression* 匹配某个 **Case** *expressionlist* 表达式， 则在 **Case** 子句之后，直到下一个 **Case** 子句的 *statements* 会被执行；如果是最后一个子句，则会执行到 **End Select**。然后控制权会转移到 **End Select** 之后的语句。如果 *testexpression* 匹配一个以上的 **Case** 子句中的 *expressionlist* 表达式，则只有第一个匹配后面的语句会被执行。

**Case Else** 子句用于指明 *elsestatements*，当 *testexpression* 和所有的 **Case** 子句中的 *expressionlist* 都不匹配时，则会执行这些语句。虽然不是必要的，但是在 **Select Case** 区块中，最好还是加上 **Case Else** 语句来处理不可预见的 *testexpression* 值。如果没有 **Case** *expressionlist* 匹配 *testexpression*，而且也没有 **Case Else** 语句，则程序会从 **End Select** 之后的语句继续执行。

可以在每个 **Case** 子句中使用多重表达式或使用范围，例如，下面的语句是正确的：

    Case 1 To 4, 7 To 9, 11, 13, Is > MaxNumber

**注意**   **Is** 比较运算符和使用在 **Select Case** 语句中的 **Is** 关键字并不相同。

也可以针对字符串指定范围和多重表达式。在下面的例子中，**Case** 所匹配的字符串为：等于 `everything`、按英文字母顺序落入从 `nuts` 到 `soup` 之间的字符串、以及 `TestItem` 所代表的当前值。

    Case "everything", "nuts" To "soup", TestItem

**Select Case** 语句也可以是嵌套的。但每个嵌套的 **Select Case** 语句必须要有相应的 **End Select** 语句。
