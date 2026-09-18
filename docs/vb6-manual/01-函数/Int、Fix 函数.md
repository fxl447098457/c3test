# Int、Fix 函数

返回参数的整数部分。

**语法**

**Int(***number***)**

**Fix(***number***)**

必要的 *number* 参数是 Double 或任何有效的数值表达式。如果 *number* 包含 Null，则返回 **Null**。

**说明**

**Int** 和 **Fix** 都会删除 *number* 的小数部份而返回剩下的整数。

**Int** 和 **Fix** 的不同之处在于，如果 *number* 为负数，则 **Int** 返回小于或等于 *number* 的第一个负整数，而 **Fix** 则会返回大于或等于 *number* 的第一个负整数。例如，**Int** 将 -8.4 转换成 -9，而 **Fix** 将 -8.4 转换成 -8。

**Fix(***number***)** μèóú￡o

    Sgn(number) * Int(Abs(number))
