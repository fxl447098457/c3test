# TextStream 对象

# TextStream 对象

                   

**描述**

加快对文件的顺序访问。

**语法**

**TextStream.**{*property* \| *method*}

*property* 和 *method* 参数可以是和 **TextStream** 对象相关联的任何属性和方法。请注意，在实际应用中，**TextStream** 被一个变量占位符所替代，该变量占位符表示从 **FileSystemObject** 返回的 **TextStream** 对象。

**说明**

在下面的代码中，`a` 是由 **FileSystemObject** 的 **CreateTextFile** 方法返回的 **TextStream** 对象：

    Set fs = CreateObject("Scripting.FileSystemObject")
    Set a = fs.CreateTextFile("c:\testfile.txt", True)
    a.WriteLine("This is a test.")
    a.Close

**WriteLine** 和 **Close** 是 **TextStream** 对象的两个方法。
