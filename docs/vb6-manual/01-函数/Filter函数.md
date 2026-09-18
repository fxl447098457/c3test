# Filter函数

# Filter函数

       

**描述**

返回一个下标从零开始的数组，该数组包含基于指定筛选条件的一个字符串数组的子集。

**语法**

**Filter(***InputStrings***,** *Value*\[**,** *Include*\[**,** *Compare*\]\]**)**

**Filter**函数语法有如下几部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *InputStrings* | 必需的。要执行搜索的一维字符串数组。 |
| *Value* | 必需的。要搜索的字符串。 |
| *Include* | 可选的。**Boolean**值，表示返回子串包含还是不包含*Value*字符串。如果*Include*是**True**，**Filter**返回的是包含*Value*子字符串的数组子集。如果*Include*是**False**，**Filter**返回的是不包含*Value*子字符串的数组子集。 |
| *Compare* | 可选的。数字值，表示所使用的字符串比较类型。有关其设置，请参阅下面的“设置值”部分。 |

  

**设置值**

*Compare*参数的设置值如下：

|  |  |  |
|----|----|----|
| **常数** | **值** | **描述** |
| **vbUseCompareOption** | –1 | 使用**Option Compare**语句的设置值来执行比较。 |
| **vbBinaryCompare** |  0 | 执行二进制比较。 |
| **vbTextCompare** |  1 | 执行文字比较。 |
| **vbDatabaseCompare** |  2 | 只用于Microsoft Access。基于您的数据库信息来执行比较。 |

  

**说明**

如果在*InputStrings*中没有发现与*Value*相匹配的值，**Filter**返回一个空数组。如果*InputStrings*是**Null**或不是一个一维数组，则产生错误。

**Filter**函数所返回的数组，其元素数目刚好是所找到的匹配项目数。
