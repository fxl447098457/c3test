# CopyFile 方法

# CopyFile 方法

           

**描述**

把一个或多个文件从一个地方复制到另一个地方。

**语法**

*object*.**CopyFile** *source*, *destination*\[, *overwrite*\]

**CopyFile** 方法语法有如下几部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *object* | 必需的。*object*始终是一个 **FileSystemObject** 的名字。 |
| *source* | 必需的。指明一个或多个要被复制文件的字符串文件说明，它可以包括通配符。 |
| *destination* | 必需的。指明 *source* 中的一个或多个文件要被复制到的接受端的字符串，不允许有通配符。 |
| *overwrite* | 选项的。**Boolean** 值，它表示存在的文件是否被覆盖。如果是 **True**，文件将被覆盖；如果是 **False，**它们不被覆盖。缺省值是 **True**。注意如果 *destination* 具有只读属性设置，不论 *overwrite* 值如何，**CopyFile** 都将失败。 |

  

**说明**

通配符只能用在 *source* 参数的最后一个路径部件。例如，你可以在下面请况使用通配符：

    FileSystemObject.CopyFile "c:\mydocuments\letters\*.doc", "c:\tempfolder\"

但下面情况不能使用：

    FileSystemObject.CopyFile "c:\mydocuments\*\R1???97.xls", "c:\tempfolder"

如果 *source* 包含通配符或 *destination* 以路径分隔符（**\\**）为结尾，则认为 *destination* 是一个已存在文件夹，在其中复制相匹配的文件。否则认为 *destination* 是一个要创建文件的名字。不论是那种情况，当复制一个文件时，可能发生三种事件。

- 如果 *destination* 不存在，*source* 得到复制。这是通常的情况。  
    
- 如果 *destination* 是一个已存在的文件，则当 *overwrite* 值为 **False** 时发生一个错误，否则，*source*的复制文件将试图覆盖已存在文件。  
    
- 如果 *destination* 是一个目录，发生一个错误。

如果使用通配符的 *source* 不能和任何文件匹配，同样产生一个错误。**CopyFile** 方法停止在它遇到的第一个错误上。不要试图回卷或撤消错误发生前所做的任何改变。
