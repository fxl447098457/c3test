# StrConv 函数

返回按指定类型转换的 **Variant** (**String**)。

**语法**

**StrConv**(***string, conversion, LCID***)

**StrConv** 函数的语法有下面的命名参数：

|  |  |
|----|----|
| **部分** | **说明** |
| ***string*** | 必要参数。要转换的字符串表达式。 |
| ***conversion*** | 必要参数。Integer。其值的和决定转换的类型。 |
| ***LCID*** | 可选的。如果与系统LocaleID不同，则为LocaleID（系统LocaleID为缺省值。） |

  

**设置值**

***conversion*** 参数的设置值为：

|                    |        |                                      |
|--------------------|--------|--------------------------------------|
| **常数**           | **值** | **说明**                             |
| **vbUpperCase**    | 1      | 将字符串文字转成大写。               |
| **vbLowerCase**    | 2      | 将字符串文字转成小写。               |
| **vbProperCase**   | 3      | 将字符串中每个字的开头字母转成大写。 |
| **vbWide\***       | 4\*    | 将字符串中单字节字符转成双字节字符。 |
| **vbNarrow\***     | 8\*    | 将字符串中双字节字符转成单字节字符。 |
| **vbKatakana\*\*** | 16\*\* | 将字符串中平假名字符转成片假名字符。 |

  

<table data-border="1" data-cellpadding="5" cols="6" data-frame="below" data-rules="rows">
<tbody>
<tr data-valign="top">
<td width="25%"><strong>vbHiragana**</strong></td>
<td colspan="2" width="11%">32**</td>
<td colspan="2" width="62%">将字符串中片假名字符转成平假名字符。</td>
<td></td>
</tr>
<tr data-valign="top">
<td colspan="2" width="26%"><strong>vbUnicode</strong></td>
<td colspan="2" width="11%">64</td>
<td colspan="2" width="63%">根据系统的缺省码页将字符串转成 Unicode。</td>
</tr>
<tr data-valign="top">
<td colspan="2" width="26%"><strong>vbFromUnicode</strong></td>
<td colspan="2" width="11%">128</td>
<td colspan="2" width="63%">将字符串由 Unicode 转成系统的缺省码页。</td>
</tr>
</tbody>
</table>

  

\*应用到远东国别。

\*\*仅应用到日本。

**注意** 这些常数是由 VBA 指定的。可以在程序中使用它们来替换真正的值。其中大部分是可以组合的，例如 **vbUpperCase + vbWide**，互斥的常数不能组合，例如 **vbUnicode + vbFromUnicode**。当在不适用的国别使用常数 **vbWide**、**vbNarrow**、**vbKatakana**，和 **vbHiragana** 时，就会导致运行时错误。

下面是一些一般情况下的有效分界符：Null (**Chr\$(**0**)**)，水平制表符 (**Chr\$(**9**)**)，换行 (**Chr\$(**10**)**)，垂直制表符 (**Chr\$(**11**)**)，换页 (**Chr\$(**12**)**) ，回车 (**Chr\$(**13**)**)，空白 (SBCS) (**Chr\$(**32**)**)。在 DBCS中，空白的实际值会随国家/地区而不同。

**说明**

在把 ANSI 格式的 **Byte** 数组转换为字符串时，您应该使用 **StrConv** 函数。当您转换 Unicode 格式的这种数组时，使用赋值语句。
