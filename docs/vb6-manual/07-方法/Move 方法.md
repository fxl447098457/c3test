# Move 方法

# Move 方法

           

**描述**

将一个指定的文件或文件夹从一个地方移动到另一个地方。

**语法**

*object*.**Move** *destination*

**Move** 方法语法有如下几部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *object* | 必需的。始终是一个 **File** 或 **Folder** 对象的名字。 |
| *destination* | 必需的。文件或文件夹要移动到的目标。不允许有通配符。 |

  

**说明**

**Move** 方法对一个 **File** 或 **Folder** 的结果和执行 **FileSystemObject.MoveFile** 或 **FileSystemObject.MoveFolder** 操作的结果是一样的。但应当注意，后面的方法能够移动多个文件或文件夹。
