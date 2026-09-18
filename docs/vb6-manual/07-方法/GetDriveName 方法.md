# GetDriveName 方法

# GetDriveName 方法

           

**描述**

返回一个包含指定路径的驱动器名字的字符串。

**语法**

*object*.**GetDriveName(***path***)**

**GetDriveName** 方法语法有如下几部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *object* | 必需的。始终是一个 **FileSystemObject** 的名字。 |
| *path* | 必需的。要返回其驱动器名字的部件的路径说明。 |

  

**说明**

如果驱动器不能确定，**GetDriveName** 方法返回一个长度为零的字符串（""）。

**注意**   **GetDriveName** 方法只对提供的路径字符串起作用。它没有尝试去辨认路径，也不对指定路径是否存在进行检查。
