# VolumeName 属性

# VolumeName 属性

           

**描述**

设置或返回指定驱动器的卷标名。读/写属性。

**语法**

*object*.**VolumeName** \[= *newname*\]

VolumeName 属性有下列几部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *object* | 必需的。总是一个 **Drive** 对象的名字。 |
| *newname* | 可选的。如果提供的话，*newname* 是指定 *object* 的新名字。 |

  

**说明**

下面的代码举例说明了 **VolumeName** 属性的用法：

    Sub ShowVolumeInfo(drvpath)
        Dim fs, d, s
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set d = fs.GetDrive(fs.GetDriveName(fs.GetAbsolutePathName(drvpath)))
        s = "Drive " & d.DriveLetter & ": - " & d.VolumeName
        MsgBox s
    End Sub
