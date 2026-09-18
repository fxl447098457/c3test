# IsObject 函数

返回 **Boolean** 值，指出标识符是否表示对象变量。

**语法**

**IsObject(***identifier***)**

必要的 *identifier* 参数是一个变量名。

**说明**

**IsObject** 只用于确定 Variant 是否属于 **VarType** **vbObject**。如果 **Variant** 实际引用（或曾经引用过）一个对象，或者如果 **Variant** 包含 **Nothing**，则可能出现这种情况。

如果 *identifier* 是 Object类型或任何有效的类类型，或者，如果 *identifier* 是 **VarType** **vbObject** 的 **Variant** 或用户自定义的对象，则 **IsObject** 返回 **True**；否则返回 **False**。即使变量已设置成 **Nothing**，**IsObject** 也仍返回 **True**。

使用错误捕获方法可以确认对象引用是否有效。
