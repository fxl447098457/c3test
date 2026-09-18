# Dir 函数

返回一个 **String**，用以表示一个文件名、目录名或文件夹名称，它必须与指定的模式或文件属性、或磁盘卷标相匹配。

**语法**

**Dir**\[**(***pathname*\[**,** *attributes*\]**)**\]

**Dir** 函数的语法具有以下几个部分：

<table data-border="1" data-cellpadding="5" cols="4" data-frame="below" data-rules="rows">
<tbody>
<tr data-valign="top">
<td colspan="2" class="label" width="16%"><strong>部分</strong></td>
<td colspan="2" class="label" width="84%"><strong>描述</strong></td>
</tr>
<tr data-valign="top">
<td width="12%"><em>pathname</em></td>
<td colspan="2" width="84%">可选参数。用来指定文件名的字符串表达式，可能包含目录或文件夹、以及驱动器。如果没有找到 <em>pathname</em>，则会返回零长度字符串 ("")。</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="12%"><em>attributes</em></td>
<td colspan="2" width="84%">可选参数。常数或数值表达式，其总和用来指定文件属性。如果省略，则会返回匹配 <em>pathname</em> 但不包含属性的文件。</td>
<td></td>
</tr>
</tbody>
</table>

  

**设置值**

*attributes* 参数的设置可为：

|  |  |  |
|----|----|----|
| **常数** | **值** | **描述** |
| **vbNormal** | 0 | (缺省) 指定没有属性的文件。 |
| **vbReadOnly** | 1 | 指定无属性的只读文件 |
| **vbHidden** | 2 | 指定无属性的隐藏文件 |
| **VbSystem** | 4 | 指定无属性的系统文件 |
| **vbVolume** | 8 | 指定卷标文件；如果指定了其它属性，则忽略**vbVolume** |
| **vbDirectory** | 16 | 指定无属性文件及其路径和文件夹。 |

  

**注意** 这些常数是由 VBA 所指定的，在程序代码中的任何位置，可以使用这些常数来替换真正的数值。

**说明**

**Dir** 支持多字符 (**\*)** 和单字符 (**?)** 的通配符来指定多重文件。

 由于 Macintosh 不支持通配符，使用文件类型指定文件组。可以使用 **MacID** 函数指定文件类型而不用文件名。比如，下列语句返回当前文件夹中第一个TEXT文件的名称:

    Dir("SomePath", MacID("TEXT"))

 为选中文件夹中所有文件，指定一空串:

    Dir("")

在 Microsoft Windows 中，如果在**Dir**函数中使用**MacID**函数，将产生错误。

任何大于256的*attribute*值都被认为是**MacID** 函数的值。

在第一次调用 **Dir** 函数时，必须指定 *pathname*，否则会产生错误。如果也指定了文件属性，那么就必须包括 *pathname*。

**Dir** 会返回匹配 *pathname* 的第一个文件名。若想得到其它匹配 *pathname* 的文件名，再一次调用 **Dir**，且不要使用参数。如果已没有合乎条件的文件，则 **Dir** 会返回一个零长度字符串 ("")。一旦返回值为零长度字符串，并要再次调用 **Dir** 时，就必须指定 *pathname*，否则会产生错误。不必访问到所有匹配当前 *pathname* 的文件名，就可以改变到一个新的 *pathname* 上。但是，不能以递归方式来调用 **Dir** 函数。以 **vbDirectory** 属性来调用 **Dir** 不能连续地返回子目录。

**提示** 由于文件名并不会以特别的次序来返回，所以可以将文件名存储在一个数组中，然后再对这个数组排序。
