# IsRootFolder 属性

# IsRootFolder 属性

           

**描述**

如果指定的文件夹是根文件夹，则返回 **True**；否则返回 **False**。

**语法**

*object*.**IsRootFolder**

*object* 总是一个 **Folder** 对象。

**说明**

下面的代码举例说明了 **IsRootFolder** 属性的用法：

    Dim fs
    Set fs = CreateObject("Scripting.FileSystemObject")
    Sub DisplayLevelDepth(pathspec)
        Dim f, n
        Set f = fs.GetFolder(pathspec)
        If f.IsRootFolder Then
            MsgBox "The specified folder is the root folder."
        Else
            Do Until f.IsRootFolder
                Set f = f.ParentFolder
                n = n + 1
            Loop
            MsgBox "The specified folder is nested " & n & " levels deep."
        End If
    End Sub
