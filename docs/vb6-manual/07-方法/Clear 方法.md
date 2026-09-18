# Clear 方法

清除 **Err** 对象的所有属性设置。

**语法**

*object***.Clear**

*objec* 总是 **Err** 对象。

**说明**

在处理错误之后使用 **Clear** 来清除 **Err** 对象，例如，在对 **On Error Resume Next** 使用拖延错误处理时就可使用 **Clear**。每当执行下列语句时就会自动调用 **Clear** 方法：

- 任意类型的 **Resume** 语句。  
    
- **Exit Sub**, **Exit Function**, **Exit Property**  
    
- 任何 **On Error** 语句。

**注意** 当处理因访问其他对象产生的错误时，与其使用 **On Error GoTo**，不如使用 **On Error Resume Next**。每一次与对象打交道之后都检查 **Err**，则可消除代码访问对象时的含混之处。可以确认是哪个对象将错误引入 **Err.Number** 中，也可以确认最初是哪个对象产生了这个错误（**Err.Source** 中指定的对象）。
