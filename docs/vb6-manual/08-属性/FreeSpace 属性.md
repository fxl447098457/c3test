# FreeSpace 属性

# FreeSpace 属性

           

**描述**

返回指定驱动器上或网络共享的用户可用磁盘剩余空间容量。只读。

**语法**

*object*.**FreeSpace**

*object* 总是一个 **Drive** 对象。

**说明**

一般来说，**FreeSpace** 属性的返回值和 **AvailableSpace** 属性的返回值是相同的。对于支持限额的计算机系统来说，二者之间可能有所不同。

下面的代码举例说明了 **FreeSpace** 属性的用法：

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
