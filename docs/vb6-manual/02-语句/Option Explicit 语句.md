# Option Explicit 语句

**Option Explicit 语句**

       

在模块级别中使用，强制显式声明模块中的所有变量。

**语法**

**Option Explicit**

**说明**

如果使用，**Option** **Explicit** 语句必须写在模块的所有过程之前。

如果模块中使用了 **Option Explicit**，则必须使用 **Dim、Private、Public、ReDim** 或 **Static** 语句来显式声明所有的变量。如果使用了未声明的变量名在编译时间会出现错误。

如果没有使用 **Option Explicit** 语句，除非使用 **Def***type* 语句指定了缺省类型，否则所有未声明的变量都是 **Variant** 类型的。

**注意** 使用 **Option Explicit** 可以避免在键入已有变量时出错，在变量的范围不是很清楚的代码中使用该语句可以避免混乱。
