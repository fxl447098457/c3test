# FormatPercent函数

# FormatPercent函数

       

**描述**

返回一个百分比格式（乘以100）的表达式，后面有%符号。

**语法**

**FormatPercent(***Expression*\[**,***NumDigitsAfterDecimal* \[**,***IncludeLeadingDigit* \[**,***UseParensForNegativeNumbers* \[**,***GroupDigits*\]\]\]\]**)**

**FormatPercent**函数语法有如下几部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *Expression* | 必需的。要格式化的表达式。 |
| *NumDigitsAfterDecimal* | 可选的。表示小数点右边的显示位数。缺省值为–1，表示使用计算机的区域设置值。 |
| *IncludeLeadingDigit* | 可选的。三态常数，表示小数点前是否显示零。关于其值，请参阅“设置值”部分。 |
| *UseParensForNegativeNumbers* | 可选的。三态常数，表示是否把负数放在圆括号内。关于其值，请参阅“设置值”部分。 |
| *GroupDigits* | 可选的。三态常数，表示是否用组分隔符对数字进行分组，组分隔符在计算机的区域设置值中指定。关于其值，请参阅“设置值”部分。 |

  

**设置值**

*IncludeLeadingDigit、UseParensForNegativeNumbers*和*GroupDigits*参数的设置值如下：

|                        |        |                                  |
|------------------------|--------|----------------------------------|
| **常数**               | **值** | **描述**                         |
| **TristateTrue**       | –1     | True                             |
| **TristateFalse**      | 0      | False                            |
| **TristateUseDefault** | –2     | 使用计算机区域设置值中的设置值。 |

  

**说明**

当忽略一个或多个选项参数时，被忽略的参数值由计算机的区域设置值提供。

**注意**   所有的设置值信息都来自“**区域设置**”的“**数字**”选项卡。
