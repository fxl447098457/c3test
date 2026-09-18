# AvailableSpace 属性

# AvailableSpace 属性

           

**描述**

返回在指定的驱动器或网络共享上的用户可用空间容量。

**语法**

*object*.**AvailableSpace**

*object* 总是一个 **Drive** 对象。

**说明**

一般来说，**AvailableSpace** 属性的返回值与 **FreeSpace** 属性的返回值是相同的。对于支持限额的计算机系统来说，两个值之间可能会有所不同。

下面的代码举例说明了 **AvailableSpace** 属性的用法：

    Sub ShowAvailableSpace(drvPath)
        Dim fs, d, s
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set d = fs.GetDrive(fs.GetDriveName(drvPath))
        s = "Drive " & UCase(drvPath) & " - " 
        s = s & d.VolumeName  & vbCrLf
        s = s & "Available Space: " & FormatNumber(d.AvailableSpace/1024, 0) 
        s = s & " Kbytes"
        MsgBox s
    End Sub
