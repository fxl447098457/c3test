# Files 集合

# Files 集合

                   

**描述**

在一个文件夹内的所有 **File** 对象的集合。

**说明**

下面的代码举例说明了如何获得一个 **Files** 集合，以及如何用 **For Each...Next** 语句来访问这个集合中的每个File：

    Sub ShowFolderList(folderspec)
        Dim fs, f, f1, fc, s
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set f = fs.GetFolder(folderspec)
        Set fc = f.Files
        For Each f1 in fc
            s = s & f1.name 
            s = s & vbCrLf
        Next
        MsgBox s
    End Sub
