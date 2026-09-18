# Files 属性

# Files 属性

           

**描述**

返回由所有 **File** 对象组成的 **Files** 集合，这些 **File** 对象包含在指定的文件夹中──包括设置了隐藏和系统文件属性的那些文件。

**语法**

*object*.**Files**

*object* 总是一个 **Folder** 对象。

**说明**

下面的代码举例说明了 **Files** 属性的用法：

    Sub ShowFileList(folderspec)
        Dim fs, f, f1, fc, s
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set f = fs.GetFolder(folderspec)
        Set fc = f.Files
        For Each f1 in fc
            s = s & f1.name 
            s = s &  vbCrLf
        Next
        MsgBox s
    End Sub
