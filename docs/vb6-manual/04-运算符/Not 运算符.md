# Not 运算符

用来对表达式进行逻辑否定运算。

**语法**

*result* **=** **Not** *expression*

**Not** 运算符的语法具有以下几个部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *result* | 必需的；任何数值变量。 |
| *expression* | 必需的；任何表达式。 |

  

**说明**

下表说明如何确定 *result*：

|  |  |
|----|----|
| **如果** *expression* **为** | **则** *result* **为** |
| **True** | **False** |
| **False** | **True** |
| Null | **Null** |

  

此外，**Not** 运算符改变任何变量的位值，并根据下表设置 *result* 中相应的位：

<table data-border="1" data-cellpadding="5" cols="4" data-frame="below" data-rules="rows">
<tbody>
<tr data-valign="top">
<td colspan="2" class="label" width="23%"><strong>如果在</strong> <em>expression 的位为</em></td>
<td colspan="2" class="label" width="77%"><strong>则在</strong> <em>result</em> <strong>中的位为</strong></td>
</tr>
<tr data-valign="top">
<td width="18%">0</td>
<td colspan="2" width="61%">1</td>
<td></td>
</tr>
<tr data-valign="top">
<td colspan="2" width="23%">1</td>
<td colspan="2" width="77%">0</td>
</tr>
</tbody>
</table>
