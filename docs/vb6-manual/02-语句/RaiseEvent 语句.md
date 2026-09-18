# RaiseEvent 语句

# RaiseEvent 语句

       

引发在一个类、窗体、或者文档中的模块级中声明的一个事件。

**语法**

**RaiseEvent** *eventname* \[**(***argumentlis*t**)**\]

必需的 *eventname* 是在模块 中声明的，并且符合 Basic 变量命名约定的一个事件的名称。

**RaiseEvent** 语句的语法有如下部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *Eventname* | 必需的。所引发的事件的名称。 |
| *Argumentlist* | 可选的。用逗号分隔的变量、数组，或者表达式的列表。*argumentlist* 必须用圆括号括起来。如果没有参数，则圆括号必须被忽略。 |

  

**说明**

如果在事件被引发的模块内该事件没有被声明，就会发生一个错误。下面这个程序片段说明了一个事件的声明以及一个引发该事件的过程。

    ' 在类模块的模块级声明一个事件
    Event LogonCompleted (UserName as String)

    Sub
       ' 引发该事件。
       RaiseEvent LogonCompleted ("AntoineJan")
    End Sub

如果在 **RaiseEvent** 中该事件没有参数，包括空的圆括号，则对该事件的调用就会导致一个错误。不能使用 **RaiseEvent** 来引发在模块中没有明确定义的事件。例如，如果一个窗体有一个 Click 事件，则不能使用 **RaiseEvent** 来引发该窗体的 Click 事件。如果在窗体模块 中声明了一个 Click 事件，则它将覆盖窗体自己的 Click 事件。仍然可以使用调用该事件的正常语法来调用该窗体的 Click 事件，但是不能使用 **RaiseEvent** 语句。

事件的引发是根据连接建立的顺序来进行的。因为事件可以有 **ByRef** 参数，所以后来连接的进程可能接收已经被一个更早的事件处理程序更改的参数。
