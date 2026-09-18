# Name 属性

# Name 属性

           

**描述**

设置或返回指定文件或文件夹名。读/写属性。

**语法**

*object*.**Name** \[= *newname*\]

**Name** 属性有下列几部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *object* | 必需的。总是某个 **File** 或 **Folder** 对象的名字。 |
| *newname* | 可选的。如果提供的话，*newname* 是指定的 *object* 的新名。 |

  

**说明**

下面的代码举例说明了 **Name** 属性的用法：

    Sub ShowFileAccessInfo(filespec)
        Dim fs, f, s
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set f = fs.GetFile(filespec)
        s = f.Name & " on Drive " & UCase(f.Drive) & vbCrLf
        s = s & "Created: " & f.DateCreated & vbCrLf
        s = s & "Last Accessed: " & f.DateLastAccessed & vbCrLf
        s = s & "Last Modified: " & f.DateLastModified  
        MsgBox s, 0, "File Access Info"
    End Sub
