# GetExtensionName 方法

# GetExtensionName 方法

           

**描述**

返回一个包含路径中最后部件扩展名的字符串。

**语法**

*object*.**GetExtensionName(***path***)**

**GetExtensionName** 方法语法有如下几部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *object* | 必需的。始终是一个 **FileSystemObject** 的名字。 |
| *path* | 必需的。.要返回其扩展名的部件的路径说明。 |

  

**说明**

对于网络驱动器，根目录（**\\**）被认为是一个部件。

如果没有部件和 *path* 参数相匹配，**GetExtensionName** 方法返回一个长度为零的字符串 ("")。
