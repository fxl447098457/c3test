# Replace函数

# Replace函数

       

**描述**

返回一个字符串，该字符串中指定的子字符串已被替换成另一子字符串，并且替换发生的次数也是指定的。

**语法**

**Replace(***expression***,** *find***,** *replacewith*\[**,** *start*\[**,** *count*\[**,** *compare*\]\]\]**)**

**Replace**函数语法有如下几部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *expression* | 必需的。字符串表达式，包含要替换的子字符串。 |
| *find* | 必需的。要搜索到的子字符串。 |
| *replacewith* | 必需的。用来替换的子字符串。 |
| *start* | 可选的。在表达式中子字符串搜索的开始位置。如果忽略，假定从1开始。 |
| *count* | 可选的。子字符串进行替换的次数。如果忽略，缺省值是 –1，它表明进行所有可能的替换。 |
| *compare* | 可选的。数字值，表示判别子字符串时所用的比较方式。关于其值，请参阅“设置值”部分。 |

  

**设置值**

*compare*参数的设置值如下：

|  |  |  |
|----|----|----|
| **常数** | **值** | **描述** |
| **vbUseCompareOption** | –1 | 使用**Option Compare**语句的设置值来执行比较。 |
| **vbBinaryCompare** | 0 | 执行二进制比较。 |
| **vbTextCompare** | 1 | 执行文字比较。 |
| **vbDatabaseCompare** | 2 | 仅用于Microsoft Access。基于您的数据库的信息执行比较。 |

  

**返回值**

**Replace**的返回值如下：

|  |  |
|----|----|
| **如果** | **Replace返回值** |
| *expression*长度为零 | 零长度字符串("")。 |
| *expression*为**Null** | 一个错误。 |
| *find*长度为零 | *expression*的复本。 |
| *replacewith*长度为零 | *expression*的复本，其中删除了所有出现的*find* 的字符串。 |
| *start* \> **Len(***expression***)** | 长度为零的字符串。 |
| *count* is 0 | *expression*.的复本。 |

  

**说明**

**Replace**函数的返回值是一个字符串，但是，其中从*start*所指定的位置开始，到*expression*字符串的结尾处的一段子字符串已经发生过替换动作。并不是原字符串从头到尾的一个复制。
