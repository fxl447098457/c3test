# IsMissing 函数

返回 **Boolean** 值，指出一个可选的 **Variant** 参数是否已经传递给过程。

**语法**

**IsMissing(***argname***)**

必要的 *argname* 参数包含一个可选的 **Variant** 过程参数名。

**说明**

使用 **IsMissing** 函数来检测在调用一个程序时是否提供了可选 **Variant** 参数。如果对特定参数没有传递值过去，则 **IsMissing** 返回 **True**；否则返回 **False**。如果 **IsMissing** 对某个参数返回 **True**，则在其它代码中使用这个丢失的参数将产生一个用户自定义的错误。如果对 **ParamArray** 参数使用 **IsMissing**，则函数总是返回 **False**。为了检测空的 **ParamArray**，可试看一下数组的上界是否小于它的下界。

**注意**   **IsMissing** 对简单数据类型（例如 **Integer**或**Double）**不起作用，因为与**Variants**不同，它们没有“丢失”标志位的前提。正由于此，对于可选参数类型，可以指定缺省值。如果调用过程时，参数被忽略，则该参数将具有该缺省值，如下列示例中所示：

    Sub MySub(Optional MyVar As String = "specialvalue")
        If MyVar = "specialvalue" Then
           ' MyVar 被忽略。
        Else
        ...
    End Sub

在许多情况下，如果用户从函数调用中忽略，则可以通过使缺省值等于希望 `MyVar` 所包含的值来完全忽略 `If MyVar` 测试。这将使您的代码更简洁有效。
