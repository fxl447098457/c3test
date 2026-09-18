# Type 属性

# Type 属性

           

**描述**

返回关于某个文件或文件夹类型的信息。例如，对于以.TXT 结尾的文件来说，返回 "Text Document"。

**语法**

*object*.**Type**

*object* 总是一个 **File** 或 **Folder** 对象。

**说明**

下面的代码举例说明了返回某个文件夹类型的 **Type** 属性的用法。在这个示例中，试图将 Recycle Bin 的路径或其他唯一的文件夹提供给过程。

    Sub ShowFileSize(filespec)
        Dim fs, f, s
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set f = fs.GetFolder(filespec)
        s = UCase(f.Name) & " is a " & f.Type 
        MsgBox s, 0, "File Size Info"
    End Sub
