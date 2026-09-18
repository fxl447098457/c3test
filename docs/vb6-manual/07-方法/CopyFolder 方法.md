# CopyFolder 方法

# CopyFolder 方法

           

**描述**

从一个地方递归地复制一个文件夹到另一个地方。

**语法**

*object*.**CopyFolder** *source*, *destination*\[, ove*r*write\]

**CopyFolder** 方法语法有如下几部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *Object* | 必需的。始终为一个 **FileSystemObject** 的名字。 |
| *source* | 必需的。指明一个或多个被复制文件夹的字符串文件夹说明，可以包括通配符。 |
| *destination* | 必需的。指明 *source* 中被复制文件夹和子文件夹的接受端的字符串，不允许有通配符。 |
| *overwrite* | 选项的。**Boolean** 值，它表示已存在的文件夹是否被覆盖。如果为 **True**，文件被覆盖。如果为 **False**，文件不被覆盖。缺省值为 **True**。 |

  

**说明**

通配符仅可用于 *source* 参数的最后一个路径部件。例如你可以在下面情况使用它：

    FileSystemObject.CopyFolder "c:\mydocuments\letters\*", "c:\tempfolder\"

但不能在下面情况使用它：

    FileSystemObject.CopyFolder "c:\mydocuments\*\*", "c:\tempfolder\"

如果 *source* 包含通配符或 *destination* 以路径分隔符（\\为结尾，则认为 *destination* 是一个已存在的文件夹，在其中复制相匹配的文件夹和子文件夹。否则认为 *destination* 是一个要创建的文件夹的名字。不论何种情况，当复制一个文件夹时，可能发生四种事件。

- 如果 *destination* 不存在，*source* 文件夹和它所有的内容得到复制。这是通常的情况。  
    
- 如果 *destination* 是一个已存在的文件，则发生一个错误。  
    
- 如果 *destination* 是一个目录，它将尝试复制文件夹和它所有的内容。如果一个包含在 *source* 的文件已在 *destination* 中存在，当 *overwrite* 为 **False** 时发生一个错误，否则它将尝试覆盖这个文件。  
    
- 如果 *destination* 是一个只读目录，当尝试去复制一个已存在的只读文件到此目录并且 *overwrite*为 **False** 时，则发生一个错误。

如果 *source* 使用的通配符不能和任何文件夹匹配，也发生一个错误。

**CopyFolder** 方法停止在它遇到的第一个错误上。不要尝试回卷错误发生前所做的任何改变。
