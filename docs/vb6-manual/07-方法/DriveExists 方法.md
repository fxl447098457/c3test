# DriveExists 方法

# DriveExists 方法

           

**描述**

如果指定的驱动器存在，返回 **True**，如果不存在返回 **False**。

**语法**

*object*.**DriveExists(***drivespec***)**

**DriveExists** 方法语法有如下几部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *Object* | 必需的。始终是一个 **FileSystemObject** 的名字。 |
| *Drivespec* | 必需的。一个驱动器字符或一个完整的路径说明。 |

  

**说明**

对于可删除介质的驱动器，即使没有介质存在， **DriveExists** 方法也返回 **True**。使用 **Drive** 对象的 **IsReady** 属性确定驱动器是否准备好。
