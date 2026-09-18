# ParentFolder 属性

# ParentFolder 属性

           

**描述**

返回指定文件或文件夹的父文件夹对象。只读。

**语法**

*object*.**ParentFolder**

*object* 总是一个 **File** 或 **Folder** 对象。

**说明**

下面的代码用一个文件举例说明了 **ParentFolder** 属性的用法：

    Sub ShowFileAccessInfo(filespec)
        Dim fs, f, s
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set f = fs.GetFile(filespec)
        s = UCase(f.Name) & " in " & UCase(f.ParentFolder) & vbCrLf
        s = s & "Created: " & f.DateCreated & vbCrLf
        s = s & "Last Accessed: " & f.DateLastAccessed & vbCrLf
        s = s & "Last Modified: " & f.DateLastModified  
        MsgBox s, 0, "File Access Info"
    End Sub
