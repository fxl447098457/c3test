# Copy 方法

# Copy 方法

           

**描述**

把一个指定的文件或文件夹从一个地方复制到另一个地方。

**语法**

*object*.**Copy** *destination*\[, *overwrite*\]

**Copy** 方法语法有如下几部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *object* | 必需的。始终是一个 **File** 或 **Folder** 对象的名字。 |
| *destination* | 必需的。文件或文件夹要复制到的接受端。不允许有通配符。 |
| *overwrite* | 可选的。**Boolean** 值，如果该值为 **True** （缺省），则已存在的文件或文件夹将被覆盖。如果为 **False**，则它们不被覆盖。 |

  

**说明**

对一个 **File** 或 **Folder**，**Copy** 方法的结果和执行 **FileSystemObject.CopyFile** 或**FileSystemObject.CopyFolder** 操作的结果是一样的，在后者中， *object*所引用的文件或文件夹是作为参数传递的。应当注意，后面的方法能够复制多个文件或文件夹。
