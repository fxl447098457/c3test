# Seek 函数

返回一个 Long，在 **Open** 语句打开的文件中指定当前的读/写位置。

**语法**

**Seek(***filenumber***)**

必要的 *filenumber* 参数是一个包含有效文件号的 Integer。

**说明**

**Seek** 函数返回介于 1 和 2，147，483，647（相当于 2^31 – 1）之间的值。

对各种文件访问方式的返回值如下：

<table data-border="1" data-cellpadding="5" cols="2" data-frame="below" data-rules="rows">
<colgroup>
<col style="width: 50%" />
<col style="width: 50%" />
</colgroup>
<tbody>
<tr data-valign="top">
<td class="label" width="15%"><strong>方式</strong></td>
<td class="label" width="85%"><strong>返回值</strong></td>
</tr>
<tr data-valign="top">
<td width="15%"><strong>Random</strong></td>
<td width="85%">下一个读出或写入的记录号。</td>
</tr>
<tr data-valign="top">
<td width="15%"><strong>Binary</strong>,<br />
<strong>Output</strong>,<br />
<strong>Append</strong>,<br />
<strong>Input</strong></td>
<td width="85%">下一个操作将要发生时所在的字节位置。文件中的第一个字节位于位置 1，第二个字节位于位置 2，依此类推。</td>
</tr>
</tbody>
</table>
