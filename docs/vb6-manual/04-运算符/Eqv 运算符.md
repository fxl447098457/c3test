# Eqv 运算符

用来对两个表达式进行逻辑等价运算。

**语法**

*result* **=** *expression1* **Eqv** *expression2*

**Eqv** 运算符的语法具有以下几个部分：

|  |  |
|----|----|
| **部分** | **描述** |
| *result* | 必需的；任何数值变量。 |
| *expression1* | 必需的；任何表达式。 |
| *expression2* | 必需的；任何表达式。 |

  

**说明**

如果有一个表达式是 Null，则 *result* 也是 **Null**。如果表达式都不是 **Null**，则根据下表来确定 *result*：

<table data-border="1" data-cellpadding="5" cols="5" data-frame="below" data-rules="rows">
<tbody>
<tr data-valign="top">
<td class="label" width="24%"><strong>如果</strong> <em>expression1</em> <strong>为</strong></td>
<td colspan="2" class="label" width="27%"><strong>且</strong> <em>expression2</em> <strong>为</strong></td>
<td colspan="2" class="label" width="49%"><strong>则</strong> <em>result</em> <strong>为</strong></td>
</tr>
<tr data-valign="top">
<td width="24%"><strong>True</strong></td>
<td colspan="2" width="27%"><strong>True</strong></td>
<td colspan="2" width="49%"><strong>True</strong></td>
</tr>
<tr data-valign="top">
<td width="24%"><strong>True</strong></td>
<td colspan="2" width="27%"><strong>False</strong></td>
<td colspan="2" width="49%"><strong>False</strong></td>
</tr>
<tr data-valign="top">
<td width="24%"><strong>False</strong></td>
<td colspan="2" width="27%"><strong>True</strong></td>
<td colspan="2" width="49%"><strong>False</strong></td>
</tr>
<tr data-valign="top">
<td width="24%"><strong>False</strong></td>
<td width="21%"><strong>False</strong></td>
<td colspan="2" width="39%"><strong>True</strong></td>
<td></td>
</tr>
</tbody>
</table>

  

**Eqv** 运算符对两个数值表达式中位置相同的位进行逐位比较，并根据下表对 *result* 中相应的位进行设置：

|  |  |  |
|----|----|----|
| **如果在** *expression1 的位为* | **且在** *expression2* **中的位为** | *result* **为** |
| 0 | 0 | 1 |
| 0 | 1 | 0 |
| 1 | 0 | 0 |
| 1 | 1 | 1 |
