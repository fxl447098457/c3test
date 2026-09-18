# OpenTextFile 方法

# OpenTextFile 方法

           

**描述**

打开一个指定的文件并返回一个 **TextStream** 对象，该对象可用于对文件进行读操作或追加操作。

**语法**

*object*.**OpenTextFile(***filename*\[**,** *iomode*\[**,** *create*\[**,** *format*\]\]\]**)**

**OpenTextFile** 方法有如下几部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *Object* | 必需的。始终是一个 **FileSystemObject** 的名字。 |
| *filename* | 必需的。字符串表达式，它标识了打开的文件。 |
| *iomode* | 可选的。表示输入/输出方式。可为两个常数之一：**ForReading**或 **ForAppending**。 |
| *create* | 可选的。**Boolean** 值，它表示如果指定的 *filename* 不存在是否可以创建一个新文件。如果创建新文件，其值为 **True**。若不创建文件其值为 **False**。缺省值为 **False**。 |
| *format* | 可选的。三种 **Tristate** 值之一，用于指示打开文件的格式。如果省略，则文件以 ASCII 格式打开。 |

  

**设置值**

*iomode* 参数可为下面设置值的任何一个：

|                  |        |                                            |
|------------------|--------|--------------------------------------------|
| **常数**         | **值** | **描述**                                   |
| **ForReading**   | 1      | 打开一个只读文件。不能对此文件进行写操作。 |
| **ForAppending** | 8      | 打开一个文件并写到文件的尾部。             |

  

*Format* 参数可为下面设置值的任何值：

|                        |        |                           |
|------------------------|--------|---------------------------|
| **常数**               | **值** | **描述**                  |
| **TristateUseDefault** | –2     | 使用系统缺省打开文件。    |
| **TristateTrue**       | –1     | 以 Unicode 格式打开文件。 |
| **TristateFalse**      |   0    | 以 ASCII 格式打开文件。   |

  

**说明**

下面的代码举例说明了使用 **OpenTextFile** 方法打开一个用于追加文本的文件：

    Sub OpenTextFileTest
        Const ForReading = 1, ForWriting = 2, ForAppending = 3
        Dim fs, f
        Set fs = CreateObject("Scripting.FileSystemObject")
        Set f = fs.OpenTextFile("c:\testfile.txt", ForAppending,TristateFalse)
        f.Write "Hello world!"
        f.Close
    End Sub
