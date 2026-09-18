# Drives 属性

# Drives 属性

           

**描述**

返回包含本地机器上所有可用 **Drive** 对象的 **Drives** 集合。

**语法**

*object*.**Drives**

*object* 总是一个 **FileSystemObject**。

**说明**

对于可删除媒体驱动器来说，不需要插入媒体，就可使其出现在 **Drives** 集合中。

可以用 **For Each...Next** 结构遍及 **Drives** 集合中的成员，如下面的代码所示：

    Sub ShowDriveList
        Dim fs, d, dc, s, n
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set dc = fs.Drives
        For Each d in dc
            s = s & d.DriveLetter & " - " 
            If d.DriveType = 3 Then
                n = d.ShareName
            Else
                n = d.VolumeName
            End If
            s = s & n & vbCrLf
        Next
        MsgBox s
    End Sub
