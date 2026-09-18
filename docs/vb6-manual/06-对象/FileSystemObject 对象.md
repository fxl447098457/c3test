# FileSystemObject 对象

# FileSystemObject 对象

                   

**描述**

提供对计算机文件系统的访问。

**语法**

**Scripting.FileSystemObject**

**说明**

下面的代码举例说明了如何使用 **FileSystemObject** 返回一个 **TextStream** 对象，该对象是可读并可写的：

    Set fs = CreateObject("Scripting.FileSystemObject")
    Set a = fs.CreateTextFile("c:\testfile.txt", True)
    a.WriteLine("This is a test.")
    a.Close

在上面列出的代码中，**CreateObject** 函数返回 **FileSystemObject** (`fs`)。**CreateTextFile** 方法接着创建文件作为一个 **TextStream** 对象(`a`)，而 **WriteLine** 方法则向创建的文本文件中写入一行文本。**Close** 方法刷新缓冲区并关闭文件。
