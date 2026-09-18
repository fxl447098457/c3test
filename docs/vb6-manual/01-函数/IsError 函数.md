# IsError 函数

返回 **Boolean** 值，指出表达式是否为一个错误值。

**语法**

**IsError(***expression***)**

必需的 *expression* 参数，可以是任何有效表达式。

**说明**

利用 **CVErr** 函数将实数转换成错误值就会建立错误值。**IsError** 函数被用来确定一个数值表达式是否表示一个错误。如果 *expression* 参数表示一个错误，则 **IsError** 返回 **True**；否则返回 **False**。
