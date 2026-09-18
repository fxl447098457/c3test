# TotalSize 属性

# TotalSize 属性

           

**描述**

以字节为单位，返回驱动器或网络共享的总空间大小。

**语法**

*object*.**TotalSize**

*object* 总是一个 **Drive** 对象。

**说明**

下面的代码举例说明了 **TotalSize** 属性的用法：

    Sub ShowSpaceInfo(drvpath)
        Dim fs, d, s
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set d = fs.GetDrive(fs.GetDriveName(fs.GetAbsolutePathName(drvpath)))
        s = "Drive " & d.DriveLetter & ":"
        s = s & vbCrLf
        s = s & "Total Size: " & FormatNumber(d.TotalSize/1024, 0) & " Kbytes"
        s = s & vbCrLf
        s = s & "Available: " & FormatNumber(d.AvailableSpace/1024, 0) & " Kbytes"
        MsgBox s
    End Sub
