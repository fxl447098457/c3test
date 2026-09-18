# End 语句

结束一个过程或块。

**语法**

**End**

**End** **Function**

**End** **If**

**End Property**

**End Select**

**End Sub**

**End Type**

**End With**

**End** 语句的语法有以下几种形式：

|  |  |
|----|----|
| **语句** | **描述** |
| **End** | 停止执行。不是必要的，可以放在过程中的任何位置关闭代码执行、关闭以 **Open** 语句打开的文件并清除变量。 |
| **End Function** | 必要的，用于结束一个 **Function** 语句。 |
| **End If** | 必要的，用于结束一个**If…Then…Else** 语句块。 |
| **End Property** | 必要的，用于结束一个**Property Let**、**Property Get**、或 **Property Set** 过程。 |
| **End Select** | 必要的，用于结束一个 **Select Case** 语句。 |
| **End Sub** | 必要的，用于结束一个 **Sub** 语句。 |
| **End Type** | 必要的，用于结束一个用户定义类型的定义（**Type** 语句）。 |
| **End With** | 必要的，用于结束一个 **With** 语句。 |

  

**说明**

在执行时，**End** 语句会重置所有模块级别变量和所有模块的静态局部变量。若要保留这些变量的值，改为使用 **Stop** 语句，则可以在保留这些变量值的基础上恢复执行。

**注意**   **End** 语句不调用 Unload、QueryUnload、或 Terminate 事件或任何其它 Visual Basic 代码，只是生硬地终止代码执行。窗体和类模块中的 Unload、QueryUnload、和 Terminate 事件代码未被执行。类模块创建的对象被破坏，由 **Open** 语句打开的文件被关闭，并且释放程序所占用的内存。其它程序的对象引用无效。

**End** 语句提供了一种强迫中止程序的方法。Visual Basic 程序正常结束应该卸载所有的窗体。只要没有其它程序引用该程序公共类模块创建的对象并无代码执行，程序将立即关闭。
