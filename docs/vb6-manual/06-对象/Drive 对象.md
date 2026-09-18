# Drive 对象

# Drive 对象

                   

**描述**

对特定磁盘驱动器或网络共享的属性提供访问。

**说明**

下面的代码举例说明了用 **Drive** 对象来访问驱动器属性：

    Sub ShowFreeSpace(drvPath)
        Dim fs, d, s
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set d = fs.GetDrive(fs.GetDriveName(drvPath))
        s = "Drive " & UCase(drvPath) & " - " 
        s = s & d.VolumeName  & vbCrLf
        s = s & "Free Space: " & FormatNumber(d.FreeSpace/1024, 0) 
        s = s & " Kbytes"
        MsgBox s
    End Sub
