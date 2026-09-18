# FileSystem 属性

# FileSystem 属性

           

**描述**

返回指定驱动器所使用的文件系统类型。

**语法**

*object*.**FileSystem**

*object* 总是一个 **Drive** 对象。

**说明**

可以得到的返回类型包括 FAT、NTFS、以及 CDFS。

下面的代码举例说明了 **FileSystem** 属性的用法：

    Sub ShowFileSystemType
        Dim fs,d, s
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set d = fs.GetDrive("e:")
        s = d.FileSystem
        MsgBox s
    End Sub
