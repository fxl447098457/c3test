# Randomize 语句

初始化随机数生成器。

**语法**

**Randomize** \[*number*\]

可选的 *number* 参数是 Variant 或任何有效的数值表达式。

**说明**

**Randomize** 用 *number* 将 **Rnd** 函数的随机数生成器初始化，该随机数生成器给 *number* 一个新的种子值。如果省略 *number*，则用系统计时器返回的值作为新的种子值。

如果没有使用 **Randomize**，则（无参数的）**Rnd** 函数使用第一次调用 **Rnd** 函数的种子值。

**注意** 若想得到重复的随机数序列，在使用具有数值参数的 **Randomize** 之前直接调用具有负参数值的 **Rnd**。使用具有同样 *number* 值的 **Randomize** 是不会得到重复的随机数序列的。
