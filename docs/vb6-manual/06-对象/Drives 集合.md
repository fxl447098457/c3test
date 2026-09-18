# Drives 集合

# Drives 集合

                   

**描述**

所有可用驱动器的只读集合。

**说明**

对于可删除的驱动器，不需要将媒体插入其中，它就可以在Drives集合中显示出来。

下面的代码举例说明了如何获得 **Drives** 集合，以及如何用 **For Each...Next** 语句来访问该集合中的每个Drive：

    Sub ShowDriveList
        Dim fs, d, dc, s, n
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set dc = fs.Drives
        For Each d in dc
            s = s & d.DriveLetter & " - " 
            If d.DriveType = Remote Then
                n = d.ShareName
            Else
                n = d.VolumeName
            End If
            s = s & n & vbCrLf
        Next
        MsgBox s
    End Sub
