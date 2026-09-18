# DeleteFile 方法

# DeleteFile 方法

           

**描述**

删除一个指定的文件。

**语法**

*object*.**DeleteFile** *filespec*\[, *force*\]

**DeleteFile** 方法语法有如下几部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *object* | 必需的。始终是一个 **FileSystemObject** 的名字。 |
| *filespec* | 必需的。要删除文件的名字。*Filespec* 可以在最后的路径部件中包含通配符。 |
| *force* | 可选的。**Boolean** 值，如果要删除具有只读属性设置的文件，其值为 **True**。如果其值为 **False** （缺省），则不能删除具有只读属性设置的文件。 |

  

**说明**

如果没有发现相匹配的文件，则产生一个错误。**DeleteFile** 方法停在它遇到的第一个错误上。不要尝试回卷或撤消错误发生前所做的任何改变。
