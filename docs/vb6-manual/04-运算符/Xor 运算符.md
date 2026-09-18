# Xor 运算符

用来对两个表达式进行逻辑互斥或运算。

**语法**

\[*result* **=**\] *expression1* **Xor** *expression2*

**Xor** 运算符的语法具有以下几个部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *result* | 可选；任何数值变量。 |
| *expression1* | 必需的；任何表达式。 |
| *expression2* | 必需的；任何表达式。 |

  

**说明**

如果表达式中有一个而且只有一个值为 **True**，则 *result* 为 **True**。但是，如果表达式中有一个为 Null，则 *result* 也为 **Null**。当两个表达式都不为 **Null**，则根据下表来确定 *result*：

|  |  |  |
|----|----|----|
| **如果** *expression1* **为** | **且** *expression2* **为** | **则** *result* **为** |
| **True** | **True** | **False** |
| **True** | **False** | **True** |
| **False** | **True** | **True** |
| **False** | **False** | **False** |

  

**Xor** 运算符既可作为逻辑运算符，也可作为位运算符。使用互斥或的逻辑进行的两个表达式的逐位比较，其结果通过下表说明：

<table data-border="1" data-cellpadding="5" cols="5" data-frame="below" data-rules="rows">
<tbody>
<tr data-valign="top">
<td class="label" width="24%"><strong>如果</strong> <em>expression1</em> <strong>为</strong></td>
<td colspan="2" class="label" width="24%"><strong>且</strong> <em>expression2</em> <strong>为</strong></td>
<td colspan="2" class="label" width="52%"><strong>则</strong> <em>result</em> <strong>为</strong></td>
</tr>
<tr data-valign="top">
<td width="24%">0</td>
<td width="19%">0</td>
<td colspan="2" width="41%">0</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="24%">0</td>
<td colspan="2" width="24%">1</td>
<td colspan="2" width="52%">1</td>
</tr>
<tr data-valign="top">
<td width="24%">1</td>
<td colspan="2" width="24%">0</td>
<td colspan="2" width="52%">1</td>
</tr>
<tr data-valign="top">
<td width="24%">1</td>
<td colspan="2" width="24%">1</td>
<td colspan="2" width="52%">0</td>
</tr>
</tbody>
</table>
