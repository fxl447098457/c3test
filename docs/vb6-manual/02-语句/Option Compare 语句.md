# Option Compare 语句

**Option Compare 语句**

       

在模块级别中使用，用于声明字符串比较时所用的缺省比较方法。

**语法**

**Option Compare** {**Binary** \| **Text** \| **Database**}

**说明**

如果使用，则 **Option** **Compare** 语句必须写在模块的所有过程之前。

**Option Compare** 语句为模块指定字符串比较的方法（**Binary、Text** 或 **Database**）。如果模块中没有 **Option** **Compare** 语句，则缺省的文本比较方法是 **Binary**。

**Option Compare Binary** 是根据字符的内部二进制表示而导出的一种排序顺序来进行字符串比较。在 Microsoft Windows 中，排序顺序由代码页确定。典型的二进制排序顺序如下例所示：

    A < B < E < Z < a < b < e < z < _ < _ < _ < _ < _ < ?

**Option Compare Text** 根据由系统国别确定的一种不区分大小写的文本排序级别来进行字符串比较。当使用 **Option Compare Text** 对相同字符排序时，会产生下述文本排序级别：

    (A=a) < ( _=_) < (B=b) < (E=e) < (_=_) < (Z=z) < (_=_) 

**Option** **Compare** **Database** 只能在 Microsoft Access 中使用。当需要字符串比较时，将根据数据库的国别 ID 确定的排序级别进行比较。
