# IsReady 属性

# IsReady 属性

           

**描述**

如果指定的驱动器已准备好，返回 **True**；否则返回 **False**。

**语法**

object.**IsReady**

object 总是一个 **Drive** 对象。

**说明**

对于可删除媒体驱动器和 CD-ROM 驱动器来说，仅当插入了适当的媒体，并已准备好供访问时，**IsReady** 才返回 **True**。

下面的代码举例说明了 **IsReady** 属性的用法：

    Sub ShowDriveInfo(drvpath)
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
        If d.IsReady Then 
            s = s & vbCrLf & "Drive is Ready."
        Else
            s = s & vbCrLf & "Drive is not Ready."
        End If
        MsgBox s
    End Sub
