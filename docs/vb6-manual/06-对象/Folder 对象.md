# Folder 对象

# Folder 对象

                   

**描述**

提供对一个文件夹所有属性的访问。

**说明**

下面的代码举例说明了如何获得一个 **Folder** 对象，以及如何返回它的一个属性：

    Sub ShowFolderInfo(folderspec)
        Dim fs, f, s,
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set f = fs.GetFolder(folderspec)
        s = f.DateCreated
        MsgBox s
    End Sub
