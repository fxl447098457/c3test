# SetAttr 语句

为一个文件设置属性信息。

**语法**

**SetAttr** ***pathname*, *attributes***

**SetAttr** 语句的语法含有以下这些命名参数：

<table data-border="1" data-cellpadding="5" cols="4" data-frame="below" data-rules="rows">
<tbody>
<tr data-valign="top">
<td colspan="2" class="label" width="17%"><strong>部分</strong></td>
<td colspan="2" class="label" width="83%"><strong>描述</strong></td>
</tr>
<tr data-valign="top">
<td width="13%"><strong><em>pathname</em></strong></td>
<td colspan="2" width="66%">必要参数。用来指定一个文件名的字符串表达式，可能包含目录或文件夹、以及驱动器。</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="13%"><strong><em>Attributes</em></strong></td>
<td colspan="2" width="66%">必要参数。常数或数值表达式，其总和用来表示文件的属性。</td>
<td></td>
</tr>
</tbody>
</table>

  

**设置值**

***attributes*** 参数设置可为：

<table data-border="1" data-cellpadding="5" cols="6" data-frame="below" data-rules="rows">
<tbody>
<tr data-valign="top">
<td colspan="2" class="label" width="21%"><strong>常数</strong></td>
<td colspan="2" class="label" width="12%"><strong>值</strong></td>
<td colspan="2" class="label" width="67%"><strong>描述</strong></td>
</tr>
<tr data-valign="top">
<td width="17%"><strong>vbNormal</strong></td>
<td colspan="2" width="9%">0</td>
<td colspan="2" width="53%">常规（缺省值）</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="17%"><strong>VbReadOnly</strong></td>
<td colspan="2" width="9%">1</td>
<td colspan="2" width="53%">只读。</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="17%"><strong>vbHidden</strong></td>
<td colspan="2" width="9%">2</td>
<td colspan="2" width="53%">隐藏。</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="17%"><strong>vbSystem</strong></td>
<td colspan="2" width="9%">4</td>
<td colspan="2" width="53%">系统文件</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="17%"><strong>vbArchive</strong></td>
<td colspan="2" width="9%">32</td>
<td colspan="2" width="53%">上次备份以后，文件已经改变</td>
<td></td>
</tr>
</tbody>
</table>

  

**注意** 这些常数是由 VBA 所指定的，在程序代码中的任何位置，可以使用这些常数来替换真正的数值。

**说明**

如果想要给一个已打开的文件设置属性，则会产生运行时错误。
