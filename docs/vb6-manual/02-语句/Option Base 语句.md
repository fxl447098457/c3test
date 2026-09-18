# Option Base 语句

**Option Base 语句**

       

在模块级别中使用，用来声明数组下标的缺省下界。

**语法**

**Option Base** {**0** \| **1**}

**说明**

由于下界的缺省设置是 **0**，因此无需使用 **Option Base** 语句。如果使用该语句，则必须写在模块的所有过程之前。一个模块中只能出现一次 **Option** **Base**，且必须位于带维数的数组声明之前。

**注意**   **Dim、Private、Public、ReDim** 以及 **Static** 语句中的 **To** 子句提供了一种更灵活的方式来控制数组的下标。不过，如果没有使用 **To** 子句显式地指定下界，则可以使用 **Option Base** 将缺省下界设为 1。使用 **Array** 函数或 **ParamArray** 关键字创建的数组的下界为 0；**Option Base** 对 **Array** 或 **ParamArray** 不起作用。

**Option Base** 语句只影响位于包含该语句的模块中的数组下界。
