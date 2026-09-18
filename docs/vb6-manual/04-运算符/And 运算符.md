# And 运算符

用来对两个表达式进行逻辑连接。

**语法**

*result* **=** *expression1* **And** *expression2*

**And** 运算符的语法具有以下几个部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *result* | 必需的；任何数值变量。 |
| *expression1* | 必需的；任何表达式。 |
| *expression2* | 必需的；任何表达式。 |

  

**说明**

如果两个表达式的值都是 **True**，则 *result* 是 **True**。如果其中一个表达式的值是 **False**，则 *result* 是 **False**。下列表格说明如何确定 *result*：

|  |  |  |
|----|----|----|
| **如果** *expression1* **为** | **且** *expression2* **为** | **则** *result* **为** |
| **True** | **True** | **True** |
| **True** | **False** | **False** |
| **True** | Null | **Null** |
| **False** | **True** | **False** |
| **False** | **False** | **False** |
| **False** | **Null** | **False** |
| **Null** | **True** | **Null** |
| **Null** | **False** | **False** |
| **Null** | **Null** | **Null** |

  

**And** 运算符还对两个数值表达式中位置相同的位进行逐位比较，并根据下表对 *result* 中相应的位进行设置：

<table data-border="1" data-cellpadding="5" cols="5" data-frame="below" data-rules="rows">
<tbody>
<tr data-valign="top">
<td class="label" width="24%"><strong>如果在</strong> <em>expression1 的位为</em></td>
<td colspan="2" class="label" width="27%"><strong>且在</strong> <em>expression2</em> <strong>中的位为</strong></td>
<td colspan="2" class="label" width="49%"><em>result</em> <strong>为</strong></td>
</tr>
<tr data-valign="top">
<td width="24%">0</td>
<td width="21%">0</td>
<td colspan="2" width="38%">0</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="24%">0</td>
<td colspan="2" width="27%">1</td>
<td colspan="2" width="49%">0</td>
</tr>
<tr data-valign="top">
<td width="24%">1</td>
<td colspan="2" width="27%">0</td>
<td colspan="2" width="49%">0</td>
</tr>
<tr data-valign="top">
<td width="24%">1</td>
<td colspan="2" width="27%">1</td>
<td colspan="2" width="49%">1</td>
</tr>
</tbody>
</table>
