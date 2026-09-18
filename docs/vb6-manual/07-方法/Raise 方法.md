# Raise 方法

产生运行时错误。

**语法**

*object***.Raise *number*, *source*, *description*, *helpfile*, *helpcontext***

**Raise** 方法具有下列对象限定符和命名参数：

|  |  |
|----|----|
| **参数** | **描述** |
| *object* | 必需的。总是 **Err** 对象。 |
| ***Number*** | 必需的。Long整数，识别错误性质。Visual Basic 错误（既有 Visual Basic 定义的错误也有用户定义的错误）的范围在 0–65535 之间。从 0–512 的范围保留为系统错误；从 513–65535 的范围可以用做用户定义的错误。当在类模块中将 **Number** 属性设置成自己的错误代码时，可将错误代码号添加到 **vbObjectError** 常数上。例如，为了产生错误号 513，可将 **vbObjectError** + 513 赋值到 **Number** 属性。 |
| ***source*** | 可选的。字符串表达式，为产生错误的对象或应用程序命名。当设置对象的这一属性时，要使用窗体 *project.class*。如果没有指定 *source*，则使用当前 Visual Basic 工程的程序设计 ID。 |
| ***description*** | 可选的。描述错误的字符串表达式。如果没有指定，则检查 **Number** 的值。如果可以将错误映射成 Visual Basic 运行时错误代码，则将 **Error** 函数返回的字符串作为 **Description** 使用。如果没有与 **Number** 对应的 Visual Basic 错误，则要用到消息“应用程序定义的错误或对象定义的错误”。 |
| ***helpfile*** | 可选的。帮助文件的完整限定的路径，在帮助文件中可以找到有关错误的帮助信息。如果没有指定，则 Visual Basic 会使用 Visual Basic 帮助文件的完整限定的驱动器、路径和文件名。 |
| ***helpcontext*** | 可选的。识别 ***helpfile*** 内的标题的上下文 ID，而 ***helpfile*** 提供有助于了解错误的描述。如果省略，则使用处理有关错误的 Visual Basic 帮助文件的上下文 ID，该 ID 与 **Number** 属性对应。 |

  

**说明**

除了 ***number*** 之外，所有参数都是可选的。如果使用 **Raise** 而不指定一些参数，并且 **Err** 对象的属性设置含有未清除的值，则视这些值为错误的值。

**Raise** 被用来生成运行时错误，并可用来代替 **Error** 语句。当书写类模块时要生成错误，**Raise** 是有用的，因为 **Err** 对象比 **Error** 语句可能提供更丰富的信息。例如，用 **Raise** 方法，可以在 **Source** 属性中说明生成错误的来源，可以引用该错误的联机帮助。
