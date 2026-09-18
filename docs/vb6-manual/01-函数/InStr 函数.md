# InStr 函数

返回 **Variant** (**Long**)，指定一字符串在另一字符串中最先出现的位置。

**语法**

**InStr**(\[***start***, \]***string1***, ***string2***\[, ***compare***\])

**InStr** 函数的语法具有下面的参数：

|  |  |
|----|----|
| **部分** | **说明** |
| ***start*** | 可选参数。为数值表达式，设置每次搜索的起点。如果省略，将从第一个字符的位置开始。如果 ***start*** 包含 Null，将发生错误。如果指定了 ***compare*** 参数，则一定要有 ***start*** 参数。 |
| ***string1*** | 必要参数。接受搜索的字符串表达式。 |
| ***string2*** | 必要参数。被搜索的字符串表达式。 |
| ***Compare*** | 可选参数。指定字符串比较。如果 ***compare*** 是 Null，将发生错误。如果省略 ***compare***，**Option** **Compare** 的设置将决定比较的类型。 |

  

**设置**

 *compare* 参数设置为：

|  |  |  |
|----|----|----|
| **常数** | **值** | **描述** |
| **vbUseCompareOption** | -1 | 使用**Option Compare** 语句设置执行一个比较。 |
| **vbBinaryCompare** | 0 | 执行一个二进制比较。 |
| **vbTextCompare** | 1 | 执行一个按照原文的比较。 |
| **vbDatabaseCompare** | 2 | 仅适用于Microsoft Access，执行一个基于数据库中信息的比较。 |

  

**返回值**

|  |  |
|----|----|
| **如果** | **InStr返回** |
| ***string1*** 为零长度 | 0 |
| ***string1*** 为 **Null** | Null |
| ***string2*** 为零长度 | ***Start*** |
| ***string2*** 为 **Null** | Null |
| ***string2*** 找不到 | 0 |
| ***在 string1*** 中找到***string2*** | 找到的位置 |
| ***start*** \> ***string2*** | 0 |

  

**说明**

**InStrB** 函数作用于包含在字符串中的字节数据。所以 **InStrB** 返回的是字节位置，而不是字符位置。
