# RGB 函数

返回一个 Long整数，用来表示一个 RGB 颜色值。

**语法**

**RGB(*red*, *green*, *blue*)**

**RGB** 函数的语法含有以下这些命名参数：

<table data-border="1" data-cellpadding="5" cols="4" data-frame="below" data-rules="rows">
<tbody>
<tr data-valign="top">
<td colspan="2" class="label" width="12%"><strong>部分</strong></td>
<td colspan="2" class="label" width="88%"><strong>描述</strong></td>
</tr>
<tr data-valign="top">
<td width="9%"><strong><em>red</em></strong></td>
<td colspan="2" width="70%">必要参数；<strong>Variant</strong> (<strong>Integer</strong>)。数值范围从 0 到 255，表示颜色的红色成份。</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="9%"><strong><em>green</em></strong></td>
<td colspan="2" width="70%">必要参数；<strong>Variant</strong> (<strong>Integer</strong>)。数值范围从 0 到 255，表示颜色的绿色成份。</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="9%"><strong><em>blue</em></strong></td>
<td colspan="2" width="70%">必要参数；<strong>Variant</strong> (<strong>Integer</strong>)。数值范围从 0 到 255，表示颜色的兰色成份。</td>
<td></td>
</tr>
</tbody>
</table>

  

**说明**

可以接受颜色说明的应用程序的方法和属性期望这个说明是一个代表 RGB 颜色值的数值。一个 RGB 颜色值指定红、绿、蓝三原色的相对亮度，生成一个用于显示的特定颜色。

传给 **RGB** 的任何参数的值，如果超过 255，会被当作 255。

下面的表格显示一些常见的标准颜色，以及这些颜色的红、绿、蓝三原色的成份：

<table data-border="1" data-cellpadding="5" cols="8" data-frame="below" data-rules="rows">
<tbody>
<tr data-valign="top">
<td colspan="2" class="label" width="16%"><strong>颜色</strong></td>
<td colspan="2" class="label" width="19%"><strong>红色值</strong></td>
<td colspan="2" class="label" width="22%"><strong>绿色值</strong></td>
<td colspan="2" class="label" width="43%"><strong>兰色值</strong></td>
</tr>
<tr data-valign="top">
<td width="12%">黑色</td>
<td colspan="2" width="16%">0</td>
<td colspan="2" width="17%">0</td>
<td colspan="2" width="34%">0</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="12%">兰色</td>
<td colspan="2" width="16%">0</td>
<td colspan="2" width="17%">0</td>
<td colspan="2" width="34%">255</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="12%">绿色</td>
<td colspan="2" width="16%">0</td>
<td colspan="2" width="17%">255</td>
<td colspan="2" width="34%">0</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="12%">青色</td>
<td colspan="2" width="16%">0</td>
<td colspan="2" width="17%">255</td>
<td colspan="2" width="34%">255</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="12%">红色</td>
<td colspan="2" width="16%">255</td>
<td colspan="2" width="17%">0</td>
<td colspan="2" width="34%">0</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="12%">洋红色</td>
<td colspan="2" width="16%">255</td>
<td colspan="2" width="17%">0</td>
<td colspan="2" width="34%">255</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="12%">黄色</td>
<td colspan="2" width="16%">255</td>
<td colspan="2" width="17%">255</td>
<td colspan="2" width="34%">0</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="12%">白色</td>
<td colspan="2" width="16%">255</td>
<td colspan="2" width="17%">255</td>
<td colspan="2" width="34%">255</td>
<td></td>
</tr>
</tbody>
</table>
