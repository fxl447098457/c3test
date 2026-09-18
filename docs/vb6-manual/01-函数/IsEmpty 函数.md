# IsEmpty 函数

返回 **Boolean** 值，指出变量是否已经初始化。

**语法**

**IsEmpty(***expression***)**

必要的 *expression* 参数是一个 Variant，包含一个数值或字符串表达式。但是，因为 **IsEmpty** 被用来确定个别变量是否已初始化，所以 *expression* 参数通常是单一变量名。

**说明**

如果变量未初始化或已明确设置为 Empty，则 **IsEmpty** 返回 **True**；否则返回 **False**。如果 *expression* 含有多个变量，则 **IsEmpty** 总是返回 **False**。**IsEmpty** 只返回对 variant 表达式有意义的信息。
