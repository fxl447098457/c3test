# Type 语句

**Type 语句**

       

在模块级别中使用，用于定义包含一个或多个元素的用户自定义的数据类型。

**语法**

\[**Private** \| **Public**\] **Type** *varname*  
*elementname* \[**(**\[*subscripts*\]**)**\] **As** *type*  
\[*elementname* \[**(**\[*subscripts*\]**)**\] **As** *type*\]  
**. . .**

**End Type**

**Type** 语句的语法包含下面部分：

|  |  |
|----|----|
| **部分** | **描述** |
| **Public** | 可选的。用于声明可在所有工程的所有模块的任何过程中使用的用户定义类型。 |
| **Private** | 可选的。用于声明只能在包含该声明的模块中使用的用户自定义的类型。 |
| *varname* | 必需的。用户自定义类型的名称；遵循标准的变量命名约定 |
| *elementname* | 必需的。用户自定义类型的元素名称。除了可以使用的关键字，元素名称也应遵循标准变量命名约定。 |
| *subscripts* | 可选的。数组元素的维数。当定义大小可变的数组时，只须圆括号。*subscripts* 参数使用如下语法： |
|   | \[*lower* **To**\] *upper* \[**,**\[*lower* **To**\] *upper*\] **. . .** |
|   | 如果不显式指定 *lower*，则数组的下界由 **Option** **Base** 语句控制。如果没有 **Option** **Base** 语句则下界为 0。 |
| *type* | 必需的。元素的数据类型；可以是Byte、Boolean、Integer、Long、Currency、Single、Double、Decimal（目前尚不支持）、Date、String（对变长的字符串）、**String** \* *length*（对定长的字符串）、Object、Variant、其它的用户自定义的类型或对象类型。 |

  

**说明**

**Type** 语句只能在模块级使用。使用 **Type** 语句声明了一个用户自定义类型后，就可以在该声明范围内的任何位置声明该类型的变量。可以使用 **Dim**、**Private、Public、ReDim** 或 **Static** 来声明用户自定义类型的变量。

在标准模块中，用户自定义类型按缺省设置是公用的。可以使用 **Private** 关键字来改变其可见性。而在类模块中，用户自定义类型只能是私有的，且使用 **Public** 关键字也不能改变其可见性。

在 **Type...End Type** 块中不允许使用行号和行标签。

用户自定义类型经常用来表示数据记录，记录一般由多个不同数据类型的元素组成。

下面的示例演示了一个用户自定义类型的大小固定的数组的用法：

    Type StateData
       CityCode (1 To 100) As Integer   ' Declare a static array.
       County As String * 30
    End Type

    Dim Washington(1 To 100) As StateData

在上述示例中，`StateData` 中包括了一个 `CityCode` 静态数组，且记录`Washington` 的结构与 `StateData` 相同。

当在用户自定义类型中声明大小固定的数组时，必须用数字文字或常数而不能用变量来声明数组的维数。

数组的下界由 **Option Base** 语句的设置确定。
