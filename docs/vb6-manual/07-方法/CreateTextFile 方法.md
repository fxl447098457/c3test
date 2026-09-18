# CreateTextFile 方法

# CreateTextFile 方法

           

**描述**

创建一个指定的文件名并且返回一个用于该文件读写的 **TextStream** 对象。

**语法**

*object*.**CreateTextFile(***filename*\[**,** *overwrite*\[**,** *unicode*\]\]**)**

**CreateTextFile** 方法有如下几部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *object* | 必需的。始终是一个 **FileSystemObject** 或 **Folder** 对象的名字。 |
| *filename* | 必需的。字符串表达式，它标识创建的文件。 |
| *overwrite* | 可选的。**Boolean** 值，表示一个已存在文件是否可被覆盖。如果可被覆盖其值为 **True**，其值为 **False** 时不能覆盖。如果它被省略，则已存在文件不能覆盖。 |
| *unicode* | 可选的。**Boolean** 值，表示文件是作为一个 Unicode 文件创建的还是作为一个ASCII 文件创建的。如果作为一个 Unicode 文件创建，其值为 **True**，作为一个 ASCII 文件创建，其值为 **False**。如果省略的话，则认为是一个 ASCII 文件。 |

  

**说明**

下面的代码举例说明如何使用 **CreateTextFile** 方法创建和打开文本文件。

    Sub CreateAfile
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set a = fs.CreateTextFile("c:\testfile.txt", True)
        a.WriteLine("This is a test.")
        a.Close
    End Sub

对于一个已经存在的 *filename*，如果 *overwrite* 参数是 **False** 或者没有提供，则发生一个错误。
