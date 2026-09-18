# Name 语句

重新命名一个文件、目录、或文件夹。

**语法**

**Name** *oldpathname* **As** *newpathname*

**Name** 语句的语法具有以下几个部分：

<table data-border="1" data-cellpadding="5" cols="3" data-frame="below" data-rules="rows">
<tbody>
<tr data-valign="top">
<td class="label" width="22%"><strong>部分</strong></td>
<td colspan="2" class="label" width="78%"><strong>描述</strong></td>
</tr>
<tr data-valign="top">
<td width="22%"><em>oldpathname</em></td>
<td width="61%">必要参数。字符串表达式，指定已存在的文件名和位置，可以包含目录或文件夹、以及驱动器。</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="22%"><em>newpathname</em></td>
<td width="61%">必要参数。字符串表达式，指定新的文件名和位置，可以包含目录或文件夹、以及驱动器。而由 <em>newpathname</em> 所指定的文件名不能存在。</td>
<td></td>
</tr>
</tbody>
</table>

  

**说明**

 Name 语句重新命名文件并将其移动到一个不同的目录或文件夹中。如有必要，Name 可跨驱动器移动文件。 但当 newpathname 和 oldpathname 都在相同的驱动器中时，只能重新命名已经存在的目录或文件夹。 Name 不能创建新文件、目录或文件夹。

在一个已打开的文件上使用 **Name**，将会产生错误。必须在改变名称之前，先关闭打开的文件。**Name** 参数不能包括多字符 (**\***) 和单字符 (**?**) 的统配符。
