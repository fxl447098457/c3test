# GetBaseName 方法

# GetBaseName 方法

           

**描述**

返回一个包含路径中最后部件的基本名字（去掉任何文件扩展名）的字符串。

**语法**

*object*.**GetBaseName(***path***)**

**GetBaseName** 方法语法有如下几部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *object* | 必需的。始终是一个 **FileSystemObject** 的名字。 |
| *path* | 必需的。 要返回其基本名字的部件的路径说明。 |

  

**说明**

如果没有部件和 *path* 参数匹配，**GetBaseName** 方法返回一个长度为零的字符串（""）。

**注意**   **GetBaseName** 方法只对提供的 *path* 字符串起作用。它既不试图去辨认路径，也不检查指定路径是否存在。
