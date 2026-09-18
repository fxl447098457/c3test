# DriveLetter 属性

# DriveLetter 属性

           

**描述**

返回某个物理本地驱动器或网络共享的驱动器字母。只读。

**语法**

*object*.**DriveLetter**

*object* 总是一个 **Drive** 对象。

**说明**

如果指定的驱动器没有同某个驱动器字母关联起来，例如，未被映射到一个驱动器字母的网络共享，则 **DriveLetter** 属性返回一个 0 字节长度的字符串 ("")。

下面的代码举例说明了 **DriveLetter** 属性的用法：

    Sub ShowDriveLetter(drvPath)
        Dim fs, d, s
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set d = fs.GetDrive(fs.GetDriveName(drvPath))
        s = "Drive " & d.DriveLetter & ": - " 
        s = s & d.VolumeName  & vbCrLf
        s = s & "Free Space: " & FormatNumber(d.FreeSpace/1024, 0) 
        s = s & " Kbytes"
        MsgBox s
    End Sub
