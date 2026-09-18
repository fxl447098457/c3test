# GetDrive 方法

# GetDrive 方法

           

**描述**

返回一个与指定路径中的驱动器相对应的 **Drive** 对象。

**语法**

*object*.**GetDrive** *drivespec*

**GetDrive** 方法语法有如下几部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *Object* | 必需的。始终是一个 **FileSystemObject** 的名字。 |
| *Drivespec* | 必需的。*drivespec*参数可以是一个驱动器字符（c）、一个驱动器字符加一个冒号（c:）、一个驱动器字符加冒号和路径分隔符（c:\\或任何网络共享的说明（\\computer2\share1）。 |

  

**说明**

对于网络共享，要进行检查以确保共享存在。

如果 *drivespec* 不符合任何一种可以接受的形式或者不存在，则发生一个错误。

对一个普通路径字符串调用 **GetDrive** 方法，使用下面步骤得到一个适合作为 *drivespec* 使用的字符串：

    DriveSpec = GetDriveName(GetAbsolutePathName(Path))
