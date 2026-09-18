# StrComp 函数

返回 **Variant** (**Integer**)，为字符串比较的结果。

**语法**

**StrComp**(***string1***, ***string2***\[, ***compare***\])

**StrComp** 函数的语法有下面的命名参数：

|  |  |
|----|----|
| **部分** | **说明** |
| ***string1*** | 必要参数。任何有效的字符串表达式。 |
| ***string2*** | 必要参数。任何有效的字符串表达式。 |
| ***Compare*** | 可选参数。指定字符串比较的类型。如果 ***compare*** 参数是 Null，将发生错误。如果省略 ***compare***，**Option** **Compare** 的设置将决定比较的类型。 |

  

**设置**

**compare** 参数设置为：

|  |  |  |
|----|----|----|
| **常数** | **值** | **描述** |
| **vbUseCompareOption** | -1 | 使用**Option Compare**语句设置执行一个比较。 |
| **vbBinaryCompare** | 0 | 执行一个二进制比较。 |
| **vbTextCompare** | 1 | 执行一个按照原文的比较。 |
| **vbDatabaseCompare** | 2 | 仅适用于Microsoft Access，执行一个基于数据库信息的比较。 |

  

**返回值**

**StrComp** 函数有下列返回值：

|  |  |
|----|----|
| **如果** | **StrComp 返回** |
| ***string1*** 小于 ***string2*** | -1 |
| ***string1*** 等于 ***string2*** | 0 |
| ***string1*** 大于 ***string2*** | 1 |
| ***string1*** 或 ***string 2***为 **Null** | **Null** |
