# OpenAsTextStream 方法

# OpenAsTextStream 方法

           

**描述**

打开一个指定的文件并返回一个 **TextStream** 对象，该对象可用来对文件进行读、写、追加操作。

**语法**

*object*.**OpenAsTextStream(**\[*iomode*, \[*format*\]\]**)**

**OpenAsTextStream** 方法语法有如下几部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *Object* | 必需的。始终是一个 **File** 对象的名字。 |
| *Iomode* | 可选的。表明输入/输出方式。可为三个常数之一：**ForReading**、**ForWriting** 或 **ForAppending**。 |
| *format* | 可选的。三个 **Tristate** 值之一，用于指示打开文件的格式。如果省略，则文件以 ASCII 格式打开。 |

  

**设置值**

*iomode* 参数可为下面设置值中的任何值：

|  |  |  |
|----|----|----|
| **常数** | **值** | **描述** |
| **ForReading** | 1 | 打开一个只读文件，不能对此文件进行写操作。 |
| **ForWriting** | 2 | 打开一个用于写操作的文件。如果和此文件同名的文件已存在，则覆盖以前内容。 |
| **ForAppending** | 8 | 打开一个文件并写到文件的尾部。 |

  

*Format* 参数可为下面设置值中的任何值：

|                        |        |                           |
|------------------------|--------|---------------------------|
| **常数**               | **值** | **描述**                  |
| **TristateUseDefault** | –2     | 使用系统缺省打开文件。    |
| **TristateTrue**       | –1     | 以 Unicode 格式打开文件。 |
| **TristateFalse**      |   0    | 以 ASCII 格式打开文件。.  |

  

**说明**

**OpenAsTextStream** 方法提供了和 **FileSystemObject**. 的 **OpenTextFile** 方法相同的功能。此外，**OpenAsTextStream** 方法还可以用于对一个文件进行写操作。

下面的代码举例说明了 **OpenAsTextStream** 方法的使用：

    Sub TextStreamTest
        Const ForReading = 1, ForWriting = 2, ForAppending = 3
        Const TristateUseDefault = -2, TristateTrue = -1, 
    TristateFalse = 0
        Dim fs, f, ts, s
        Set fs = CreateObject("Scripting.FileSystemObject")
        fs.CreateTextFile "test1.txt"            '创建一个文件
        Set f = fs.GetFile("test1.txt")
        Set ts = f.OpenAsTextStream(ForWriting, TristateUseDefault)
        ts.Write "Hello World"
        ts.Close
        Set ts = f.OpenAsTextStream(ForReading, TristateUseDefault)
        s = ts.ReadLine
        MsgBox s
        ts.Close
    End Sub
