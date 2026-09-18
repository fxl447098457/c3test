# ShareName 属性

# ShareName 属性

           

**描述**

返回指定驱动器的网络共享名。

**语法**

*object*.**ShareName**

*object* 总是一个 **Drive** 对象。

**说明**

如果 *object* 不是一个网络驱动器，则 **ShareName** 属性返回一个 0 字节长度的字符串 ("")。

下面的代码举例说明了 **ShareName** 属性的用法：

    Sub ShowDriveInfo(drvpath)
        Dim fs, d, s 
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set d = fs.GetDrive(fs.GetDriveName(fs.GetAbsolutePathName(drvpath)))
        s = "Drive " & d.DriveLetter & ": - " & d.ShareName
        MsgBox s
    End Sub
