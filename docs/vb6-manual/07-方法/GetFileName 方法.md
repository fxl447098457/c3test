# GetFileName 方法

# GetFileName 方法

           

**描述**

返回指定路径中的最后部件，该路径不是驱动器说明的一部分。

**语法**

*object*.**GetFileName(***pathspec***)**

**GetFileName** 方法语法有如下几部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *object* | 必需的。始终是一个 **FileSystemObject** 的名字。 |
| *pathspec* | 必需的。到一个指定文件的路径（绝对的或相对的）。 |

  

**说明**

如果 *pathspec* 不是以已命名部件结尾，**GetFileName** 方法返回一个零长度字符串（""）。

**注意**   **GetFileName** 方法仅在提供的路径字符串上起作用。它没有尝试去辨认路径，也不对指定路径是否存在进行检查。
