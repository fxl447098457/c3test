# Me

**Me** 关键字像是隐含声明的变量。这个关键字适用于类模块中的每个过程。当类有多个实例时，**Me** 在代码正在执行的地方提供引用具体实例的方法。要把当前执行类实例的有关信息传递到另一个模块的过程，**Me** 非常有用。例如，假定模块中有以下过程：

    Sub ChangeFormColor(FormName As Form)
       FormName.BackColor = RGB(Rnd * 256, Rnd * 256, Rnd * 256)
    End Sub

可以调用这个过程并使用下列语句将窗体类的当前实例作为参数传递。

    ChangeFormColor Me
