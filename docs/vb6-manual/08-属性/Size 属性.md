# Size 属性

# Size 属性

           

**描述**

对于文件来说，返回以字节为单位的指定文件大小。对于文件夹来说，返回以字节为单位的包含在文件夹中所有文件和子文件夹的大小。

**语法**

*object*.**Size**

*object* 总是一个 **File** 或 **Folder** 对象。

**说明**

下面的代码用一个 **Folder** 对象举例说明了 **Size** 属性的用法：

    Sub ShowFolderSize(filespec)
        Dim fs, f, s
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set f = fs.GetFolder(filespec)
        s = UCase(f.Name) & " uses " & f.size & " bytes."
        MsgBox s, 0, "Folder Size Info"
    End Sub
