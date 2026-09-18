# ShortName 属性

# ShortName 属性

           

**描述**

返回需要较早的 8.3 命名约定的程序所使用的短名字。

**语法**

*object*.**ShortName**

*object* 总是一个 **File** 或 **Folder** 对象。

**说明**

下面的代码用一个 **File** 对象举例说明了 **ShortName** 属性的用法：

    Sub ShowShortName(filespec)
        Dim fs, f, s
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set f = fs.GetFile(filespec)
        s = "The short name for " & "" & UCase(f.Name)
        s = s & "" & vbCrLf
        s = s & "is: " & "" & f.ShortName & ""
        MsgBox s, 0, "Short Name Info"
    End Sub
