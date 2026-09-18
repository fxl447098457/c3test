# ShortPath 属性

# ShortPath 属性

           

**描述**

返回需要较早的 8.3 文件命名约定的程序所使用的短路径。

**语法**

*object*.**ShortPath**

*object* 总是一个 **File** 或 **Folder** 对象。

**说明**

下面的代码用一个 **File** 对象举例说明了 **ShortName** 属性的用法：

    Sub ShowShortPath(filespec)
        Dim fs, f, s
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set f = fs.GetFile(filespec)
        s = "The short path for " & "" & UCase(f.Name)
        s = s & "" & vbCrLf
        s = s & "is: " & "" & f.ShortPath & ""
        MsgBox s, 0, "Short Path Info"
    End Sub
