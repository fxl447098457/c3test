# File 对象

# File 对象

                   

**描述**

提供对文件所有属性的访问。

**说明**

下面的代码举例说明了如何获得一个 **File** 对象，以及如何查看它的一个属性。

    Sub ShowFileInfo(filespec)
        Dim fs, f, s
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set f = fs.GetFile(filespec)
        s = f.DateCreated
        MsgBox s
    End Sub
