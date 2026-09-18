# IsNull 函数

返回 **Boolean** 值，指出表达式是否不包含任何有效数据 (Null)。

**语法**

**IsNull(***expression***)**

必要的 *expression* 参数是一个 Variant，其中包含数值表达式或字符串表达式。

**说明**

如果 *expression* 为 **Null，**则 **IsNull** 返回 **True**；否则 **IsNull** 返回 **False**。如果 *expression* 由多个变量组成，则表达式的任何作为变量组成成分的 **Null** 都会使整个表达式返回 **True**。

**Null** 值指出 **Variant** 不包含有效数据。**Null** 与 Empty 不同，后者指出变量尚未初始化。**Null** 与长度为零的字符串 (““) 也不同，长度为零的字符串指的是空串。

**重要** 使用 **IsNull** 函数是为了确定表达式是否包含 **Null** 值的。在某些情况下，希望表达式取值为 **True**，比如希望 `If Var = Null` 和 `If Var <> Null `取值为 **True**，而它们总取值为 **False**。这是因为任何包含 **Null** 的表达式本身就是 **Null，**所以为 **False**。
