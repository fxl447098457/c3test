# GetAbsolutePathName 方法

# GetAbsolutePathName 方法

           

**描述**

从提供的路径说明中返回一个完整、明确的路径。

**语法**

*object*.**GetAbsolutePathName(***pathspec***)**

**GetAbsolutePathName** 方法语法有如下几部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *object* | 必需的。始终是一个 **FileSystemObject** 的名字。 |
| *pathspec* | 必需的。要改变到一个完整、明确路径的路径说明。 |

  

**说明**

一个路径如果提供了从指定驱动器根目录的一个完整引用，则该路径是完整、明确的。一个完整的路径如果指定一个被映射驱动器的根文件夹，它只能以路径分隔符（**\\**为结尾。

假设当前目录是 c:\mydocuments\reports，下面的表说明了 **GetAbsolutePathName** 方法的行为。

|  |  |
|----|----|
| ***Pathspec*** | **返回的路径** |
| "c:" | "c:\mydocuments\reports" |
| "c:.." | "c:\mydocuments" |
| "c:\\\\ | "c:\\ |
| "c:\*.\*\may97" | "c:\mydocuments\reports\\.\*\may97" |
| "region1" | "c:\mydocuments\reports\region1" |
| "c:\\.\\.\mydocuments" | "c:\mydocuments" |
