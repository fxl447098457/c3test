# DriveType 属性

# DriveType 属性

           

**描述**

返回一个值，表示指定驱动器的类型。

**语法**

*object*.**DriveType**

*object* 总是一个 **Drive** 对象。

**说明**

下面的代码举例说明了 **DriveType** 属性的用法：

    Sub ShowDriveType(drvpath)
        Dim fs, d, s, t
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set d = fs.GetDrive(drvpath)
        Select Case d.DriveType
            Case 0: t = "Unknown"
            Case 1: t = "Removable"
            Case 2: t = "Fixed"
            Case 3: t = "Network"
            Case 4: t = "CD-ROM"
            Case 5: t = "RAM Disk"
        End Select
        s = "Drive " & d.DriveLetter & ": - " & t
        MsgBox s
    End Sub
