# Drive 属性

# Drive 属性

           

**描述**

返回指定文件或文件夹所在的驱动器符号。只读。

**语法**

*object*.**Drive**

*object* 总是一个 **File** 或 **Folder** 对象。

**说明**

下面的代码举例说明了 **Drive** 属性的用法：

    Sub ShowFileAccessInfo(filespec)
        Dim fs, f, s
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set f = fs.GetFile(filespec)
        s = f.Name & " on Drive " & UCase(f.Drive) & vbCrLf
        s = s & "Created: " & f.DateCreated & vbCrLf
        s = s & "Last Accessed: " & f.DateLastAccessed & vbCrLf
        s = s & "Last Modified: " & f.DateLastModified  
        MsgBox s, 0, "File Access Info"
    End Sub
