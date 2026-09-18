# Attributes 属性

# Attributes 属性

           

**描述**

设置或者返回文件或文件夹的属性。读/写或只读，取决于属性。

**语法**

*object*.**Attributes** \[= *newattributes*\]

**Attributes** 属性有下列几个部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *object* | 必需的。总是某个 **File** 或者 **Folder** 对象的名字。 |
| *newattributes* | 可选的。如果提供的话，*newattributes* 就是所指定 *object* 的新属性值。 |

  

**设置**

*newattributes* 参数可以是具有下列值中的任意一个或任意的逻辑组合：

|                |        |                                           |
|----------------|--------|-------------------------------------------|
| **常数**       | **值** | **描述**                                  |
| **Normal**     | 0      | 一般文件。未设置属性。                    |
| **ReadOnly**   | 1      | 只读文件。属性为读/写。                   |
| **Hidden**     | 2      | 隐藏文件。属性为读/写。                   |
| **System**     | 4      | 系统文件。属性为读/写。                   |
| **Volume**     | 8      | 磁盘驱动器卷标。属性为只读。              |
| **Directory**  | 16     | 文件夹或目录。属性为只读。                |
| **Archive**    | 32     | 自上次备份后已经改变的文件。属性为读/写。 |
| **Alias**      | 64     | 链接或快捷方式。属性为只读。              |
| **Compressed** | 128    | 压缩文件。属性为只读。                    |

  

**说明**

下面的代码用一个文件举例说明了 **Attributes** 属性的用法：

    Sub SetClearArchiveBit(filespec)
        Dim fs, f, r
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set f = fs.GetFile(fs.GetFileName(filespec))
        If f.attributes and 32 Then
            r = MsgBox("The Archive bit is set, do you want to clear it?", vbYesNo, "Set/Clear Archive Bit")
            If r = vbYes Then 
                f.attributes = f.attributes - 32
                MsgBox "Archive bit is cleared."
            Else
                MsgBox "Archive bit remains set."
            End If
        Else
            r = MsgBox("The Archive bit is not set. Do you want to set it?", vbYesNo, "Set/Clear Archive Bit")
            If r = vbYes Then 
                f.attributes = f.attributes + 32
                MsgBox "Archive bit is set."
            Else
                MsgBox "Archive bit remains clear."
            End If
        End If
    End Sub
