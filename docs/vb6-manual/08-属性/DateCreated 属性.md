# DateCreated 属性

# DateCreated 属性

           

**描述**

返回指定文件或文件夹的创建日期和时间。只读。

**语法**

*object*.**DateCreated**

*object* 总是一个 **file** 或**Folder**对象。

**说明**

下面的代码用一个文件举例说明了 **DateCreated** 属性的用法：

    Sub ShowFileInfo(filespec)
        Dim fs, f, s
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set f = fs.GetFile(filespec)
        s = "Created: " & f.DateCreated
        MsgBox s
    End Sub
