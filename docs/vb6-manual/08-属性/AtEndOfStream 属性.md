# AtEndOfStream 属性

# AtEndOfStream 属性

           

**描述**

只读属性，如果文件指针在 **TextStream** 文件末尾，则该属性值返回 **True；**否则返回 **False**。

**语法**

*object*.**AtEndOfStream**

*object* 总是一个 **TextStream** 对象的名称。

**说明**

**AtEndOfStream** 属性仅应用于已打开供读取的 **TextStream** 文件；否则就会出错。

下面的代码举例说明了 **AtEndOfStream** 属性的用法：

    Dim fs, a, retstring
    Set fs = CreateObject("Scripting.FileSystemObject")
    Set a = fs.OpenTextFile("c:\testfile.txt", ForReading, False)
    Do While a.AtEndOfStream <> True
        retstring = a.ReadLine
        ...
    Loop
    a.Close
