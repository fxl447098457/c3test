# SerialNumber 属性

# SerialNumber 属性

           

**描述**

返回用于唯一标识磁盘卷标的十进制序列号。

**语法**

*object*.**SerialNumber**

*object* 总是一个 **Drive** 对象。

**说明**

可以使用 **SerialNumber** 属性确保正确的磁盘已插入到某个带有可删除媒体的驱动器中。

下面的代码举例说明了 **SerialNumber** 属性的用法：

    Sub ShowDriveInfo(drvpath)
        Dim fs, d, s, t
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set d = fs.GetDrive(fs.GetDriveName(fs.GetAbsolutePathName(drvpath)))
        Select Case d.DriveType
            Case 0: t = "Unknown"
            Case 1: t = "Removable"
            Case 2: t = "Fixed"
            Case 3: t = "Network"
            Case 4: t = "CD-ROM"
            Case 5: t = "RAM Disk"
        End Select
        s = "Drive " & d.DriveLetter & ": - " & t
        s = s & vbCrLf & "SN: " & d.SerialNumber
        MsgBox s
    End Sub
