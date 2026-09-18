# Mid 语句

在一 **Variant** (**String**) 变量中以另一个字符串中的字符替换其中指定数量的字符。

**语法**

**Mid**(*stringvar*, *start*\[, *length*\]) **=** *string*

**Mid** 语句的语法具有下面几个部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *stringvar* | 必要参数。被更改的字符串变量名。 |
| *start* | 必要参数；**Variant** (**Long**)。*stringvar* 中被替换的字符开头位置。 |
| *length* | 可选参数；**Variant** (**Long**)。被替换的字符数。如果省略，*string*将全部用上。 |
| *string* | 必要参数。字符串表达式，替换部分 *stringvar* 的字符串。 |

  

**说明**

被替换的字符数量总是小于或等于 *stringvar* 的字符数。

**注意** **MidB** 语句作用于包含在字符串中的字节数据。在 **MidB** 语句中，*start* 指定 *stringvar* 中被替换的字节开头位置，而 *length* 为替换的字节数。
