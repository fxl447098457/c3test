# Command 函数

返回命令行的参数部分，该命令行用于装入 Microsoft Visual Basic 或 Visual Basic 开发的可执行程序。

**语法**

**Command**

**说明**

当从命令行装入 Visual Basic 时，`/cmd` 之后的命令行的任何部分作为命令行的参数传递给程序。下面的示例中，`cmdlineargs` 代表 **Command** 函数返回的参数信息。

    VB /cmd cmdlineargs

对于使用 Visual Basic 开发并编译为 .exe 文件的应用程序，**Command** 返回出现在命令行中应用程序名之后的任何参数。例如：

    MyApp cmdlineargs

想知道如何在正在使用的应用程序的用户界面中改变命令行参数，请搜寻关于“命令行参数”的帮助。
