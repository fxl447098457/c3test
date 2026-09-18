# Delete 方法

# Delete 方法

           

**描述**

删除一个指定的文件或文件夹。

**语法**

*object*.**Delete** *force*

**Delete** 方法语法有如下几部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *object* | 必需的。始终是一个 **File** 或 **Folder** 对象的名字。 |
| *force* | 可选的。**Boolean** 值，如果要删除具有只读属性设置的文件或文件夹，其值为**True**。当其值为 **False** 时（缺省），不能删除具有只读属性设置的文件或文件夹。 |

  

**说明**

如果指定的文件或文件夹不存在，则发生一个错误。

对于一个 **File** 或 **Folder**，**Delete** 方法的结果和 执行 **FileSystemObject.DeleteFile** 或**FileSystemObject.DeleteFolder** 操作的结果是一样的。

**Delete** 方法对于文件夹内是否有内容不做区别。不管指定的文件夹是否有内容，它都被删除。
