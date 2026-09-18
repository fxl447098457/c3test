# Folders 集合

# Folders 集合

                   

**描述**

包含在一个 **Folder** 对象内的所有 **Folder** 对象的集合。

**说明**

下面的代码举例说明了如何获得一个 **Folders** 集合，以及如何用 **For Each...Next** 语句来访问该集合中的每个Folder：

    Sub ShowFolderList(folderspec)
        Dim fs, f, f1, fc, s
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set f = fs.GetFolder(folderspec)
        Set fc = f.SubFolders
        For Each f1 in fc
            s = s & f1.name 
            s = s &  vbCrLf
        Next
        MsgBox s
    End Sub
