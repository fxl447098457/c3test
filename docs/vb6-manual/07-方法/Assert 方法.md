# Assert 方法

# Assert 方法

           

有条件地在该方法出现的行上挂起执行。

**语法**

*object***.Assert** *booleanexpression*

 **Assert** 方法的语法有如下的对象限定符和参数：

|  |  |
|----|----|
| **部分** | **描述** |
| *object* | 必需的。总是 **Debug** 对象。 |
| *booleanexpression* | 必需的。一个值为 **True** 或者 **False** 的表达式。 |

  

**说明**

**Assert** 调用只在开发环境中工作。当模块被编译成为一个可执行的文件时，调用 **Debug** 对象的方法就会被忽略。

全部 *booleanexpression* 常常被计算。例如，即使一个 **And** 表达式的第一部分被计算为 **False**，整个表达式还要被计算。
