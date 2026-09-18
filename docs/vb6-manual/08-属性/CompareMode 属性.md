# CompareMode 属性

# CompareMode 属性

           

**描述**

设置或返回某个 **Dictionary** 对象中的比较字符串关键字的比较模式。

**语法**

*object*.**CompareMode**\[ = *compare*\]

**CompareMode** 属性具有下列部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *object* | 必需的。总是一个 **Dictionary** 对象的名称。 |
| *compare* | 可选的。如果提供的话，*compare* 是一个代表比较模式的值，该比较模式用于象 **StrComp** 这样的函数。 |

  

**设置**

*compare* 参数可以具有下列值：

|  |  |  |
|----|----|----|
| **常数** | **值** | **描述** |
| **VbUseCompareOption** | –1 | 使用 **Option Compare** 语句的设置值进行比较。 |
| **vbBinaryCompare** |  0 | 进行二进制比较。 |
| **vbTextCompare** |  1 | 进行文字比较。 |
| **vbDatabaseCompare** |  2 | 仅用于 Microsoft Access。进行基于您自己数据库中信息的比较。 |

  

**说明**

如果试图对已经包含数据的 **Dictionary** 对象的比较模式进行更改的话，就会出错。

**CompareMode** 属性所用的参数值与 **StrComp** 函数所用的 *compare* 参数相同。可以用大于 2 的值表示使用特定 Locale IDs (LCID) 的比较。
