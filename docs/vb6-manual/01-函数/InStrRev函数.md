# InStrRev函数

# InStrRev函数

       

**描述**

返回一个字符串在另一个字符串中出现的位置，从字符串的末尾算起。

**语法**

**InstrRev(***string1***,** *string2*\[**,** *start*\[**,** *compare*\]\]**)**

**InstrRev**函数语法有如下几部分：

<table data-border="1" data-cellpadding="5" cols="2" data-frame="below" data-rules="rows">
<colgroup>
<col style="width: 50%" />
<col style="width: 50%" />
</colgroup>
<tbody>
<tr data-valign="top">
<td class="label" width="33%"><strong>部分</strong></td>
<td class="label" width="67%"><strong>描述</strong></td>
</tr>
<tr data-valign="top">
<td width="33%"><em>string1</em></td>
<td width="67%">必需的。要执行搜索的字符串表达式。</td>
</tr>
<tr data-valign="top">
<td width="33%"><em>string2</em></td>
<td width="67%">必需的。要搜索的字符串表达式。</td>
</tr>
<tr data-valign="top">
<td width="33%"><em>start</em></td>
<td width="67%">可选的。数值表达式，设置每次搜索的开始位置。如果忽略，则使用–1，它表示从上一个字符位置开始搜索。如果start包含
<p>Null，则产生一个错误。</p></td>
</tr>
<tr data-valign="top">
<td width="33%"><em>compare</em></td>
<td width="67%">可选的。数字值，指出在判断子字符串时所使用的比较方法。如果忽略，则执行二进制比较。关于其值，请参阅“设置值”部分。</td>
</tr>
</tbody>
</table>

  

**设置值**

*compare*参数值如下：

|  |  |  |
|----|----|----|
| **常数** | **值** | **描述** |
| **vbUseCompareOption** | –1 | 用**Option Compare**语句的设置值来执行比较。 |
| **vbBinaryCompare** |  0 | 执行二进制比较。 |
| **vbTextCompare** |  1 | 执行文字比较。 |
| **vbDatabaseCompare** |  2 | 只用于Microsoft Access。基于您的数据库信息执行比较。 |

  

**返回值**

**InStrRev**返回值如下：

|  |  |
|----|----|
| **如果** | **InStrRev返回** |
| *string1*长度为零。 | 0 |
| *string1*为**Null**。 | **Null** |
| *string2*长度为零 | *Start* |
| *string2*为**Null** | **Null** |
| *string2*没有找到。 | 0 |
| *string2*在*string1*中找到*。* | 找到匹配字符串的位置。 |
| *start* \> **Len(***string2***)** | 0 |

  

**说明**

请注意，**InstrRev**函数的语法和**Instr**函数的语法不相同。
