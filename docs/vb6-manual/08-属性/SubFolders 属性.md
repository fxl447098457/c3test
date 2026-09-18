# SubFolders 属性

# SubFolders 属性

           

**描述**

返回包含所有文件夹的一个 **Folders** 集合，这些文件夹包含在某个特定的文件夹中，包括设置了隐藏和系统文件属性的那些文件夹。

**语法**

*object*.**SubFolders**

*object* 总是一个 **Folder** 对象。

**说明**

下面的代码举例说明了 **SubFolders** 属性的用法：

    Sub ShowFolderList(folderspec)
        Dim fs, f, f1, s, sf
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set f = fs.GetFolder(folderspec)
        Set sf = f.SubFolders
        For Each f1 in sf
            s = s & f1.name 
            s = s &  vbCrLf
        Next
        MsgBox s
    End Sub
