# DeleteFolder 方法

# DeleteFolder 方法

           

**描述**

删除一个指定的文件夹和它的内容。

**语法**

*object*.**DeleteFolder** *folderspec*\[, *force*\]

**DeleteFolder** 方法语法有如下几部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *object* | 必需的。始终是一个 **FileSystemObject** 的名字。 |
| *folderspec* | 必需的。要删除的文件夹的名字。 *Folderspec* 可以在最后的路径部件中包含通配符。 |
| *force* | 可选的。**Boolean** 值，如果要删除具有只读属性设置的文件夹，其值为 **True**，如果值为 **False** （缺省），则不能删除具有只读属性设置的文件夹。 |

  

**说明**

**DeleteFolder**方法对文件夹中有无内容不做区别。不管指定的文件夹中是否有内容，它都被删除。

如果没有发现相匹配的文件夹，则发生一个错误。**DeleteFolder** 方法停止在它遇到的第一个错误上，不要尝试回卷或撤消错误发生前所做的任何改变。
