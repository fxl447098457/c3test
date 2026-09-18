# Option Private 语句

**Option Private 语句**

       

在允许引用跨越多个工程的主机应用程序中使用 **Option Private Module**，可以防止在模块所属的工程外引用该模块的内容。在不允许这种引用的主机应用程序中，例如，Visual Basic 的独立方式版本，**Option Private** 就不起作用。

**语法**

**Option Private Module**

**说明**

如果使用 **Option** **Private** 语句，则必须是写在模块级别中的任何过程之前。

如果模块中使用了 **Option Private Module**，则其公用部分（例如，在模块级定义的变量，对象，以及用户定义类型）在该模块所属的工程内仍是可用的，但对其它应用程序或工程则是不可用的。

**注意** 只有在支持同时加载多个工程，且允许在加载的工程间引用的主应用程序中可使用 **Option Private**。例如，Microsoft Excel 允许加载多个工程，**Option Private Module** 就可以用来限制跨工程的可见性。尽管 Visual Basic 允许加载多个工程，但在 Visual Basic 中是从不允许跨工程引用的。
