# GetSpecialFolder 方法

# GetSpecialFolder 方法

           

**描述**

返回指定的特殊文件夹。

**语法**

*object***.GetSpecialFolder(***folderspec***)**

**GetSpecialFolder** 方法语法有如下几部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *object* | 必需的。始终是一个 **FileSystemObject** 的名字。 |
| *folderspec* | 必需的。要返回的特殊文件夹的名字。可以是在设置值部分中列出的任何常数。 |

  

**设置值**

*folderspec* 参数可为任何的下列值：

|  |  |  |
|----|----|----|
| **常数** | **值** | **描述** |
| **WindowsFolder** | 0 | Windows 文件夹，包含由 Windows 操作系统安装的文件。 |
| **SystemFolder** | 1 | 系统文件夹，包含库、字体、设备驱动程序。 |
| **TemporaryFolder** | 2 | Temp 文件夹，用于存储临时文件。它的路径在 TMP 环境变量中。 |
