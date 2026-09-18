# DateLastModified 属性

# DateLastModified 属性

           

**描述**

返回最后一次修改指定文件或文件夹的日期和时间。只读。

**语法**

*object*.**DateLastModified**

*object* 总是一个 **file** 或 **Folder** 对象。

**说明**

下面的代码用一个文件举例说明了 **DateLastModified** 属性的用法：

    Sub ShowFileAccessInfo(filespec)
        Dim fs, f, s
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set f = fs.GetFile(filespec)
        s = UCase(filespec) & vbCrLf
        s = s & "Created: " & f.DateCreated & vbCrLf
        s = s & "Last Accessed: " & f.DateLastAccessed & vbCrLf
        s = s & "Last Modified: " & f.DateLastModified  
        MsgBox s, 0, "File Access Info"
    End Sub
