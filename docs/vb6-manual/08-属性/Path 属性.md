# Path 属性

# Path 属性

           

**描述**

返回指定文件、文件夹、或驱动器的路径。

**语法**

*object*.**Path**

*object* 总是一个 **File**、**Folder、**或 **Drive** 对象。

**说明**

对于驱动器字母来说，不包括根驱动器。例如，C 驱动器的路径是 C:，而不是 C:\\

下面的代码用一个 **File** 对象举例说明了 **Path** 属性的用法：

    Sub ShowFileAccessInfo(filespec)
        Dim fs, d, f, s
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set f = fs.GetFile(filespec)
        s = UCase(f.Path) & vbCrLf
        s = s & "Created: " & f.DateCreated & vbCrLf
        s = s & "Last Accessed: " & f.DateLastAccessed & vbCrLf
        s = s & "Last Modified: " & f.DateLastModified  
        MsgBox s, 0, "File Access Info"
    End Sub
