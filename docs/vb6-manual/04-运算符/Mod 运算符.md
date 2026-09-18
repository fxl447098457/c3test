# Mod 运算符

用来对两个数作除法并且只返回余数。

**语法**

*result* **=** *number1* **Mod** *number2*

**Mod** 的语法具有以下几个部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *result* | 必需的；任何数值变量。 |
| *number1* | 必需的；任何数值表达式。 |
| *number2* | 必需的；任何数值表达式。 |

  

**说明**

在进行 **Mod** 运算或求余数运算时，该运算符将 *number1* 用 *number2* 除（将浮点数字四舍五入成整数），并把余数作为 *result* 的值返回。例如，在下列表达式中，A (*result*) 等于 5。

    A = 19 Mod 6.7

一般说来，不管 *result* 是否为一个整数，*result* 的数据类型为 Byte，**Byte** 变体、Integer、**Integer** 变体、Long 或一个包含 **Long** 的 Variant。任何小数部分都被删除。但是，如果任何一个 Null，类型的表达式出现时，*result* 都将是 **Null**。任何 Empty 类型表达式都作为 0 处理。
