# GetParentFolderName 方法

# GetParentFolderName 方法

           

**描述**

返回一个包含指定路径最后部件父文件夹名字的字符串。

**语法**

*object*.**GetParentFolderName(***path***)**

**GetParentFolderName** 方法语法有如下几部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *object* | 必需的。始终是 一个 **FileSystemObject** 的名字。 |
| *path* | 必需的。要返回其父文件夹名字的部件的路径说明。 |

  

**说明**

如果 *path* 参数指定的部件没有父文件夹，则 **GetParentFolderName** 方法返回一个零长度字符串（""）。

**注意**   **GetParentFolderName** 方法仅对提供的 *path* 字符串起作用。它没有尝试去辨认路径，也不对指定路径是否存在进行检查。
