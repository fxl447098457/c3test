# MsgBox 函数

在对话框中显示消息，等待用户单击按钮，并返回一个 **Integer** 告诉用户单击哪一个按钮。

**语法**

**MsgBox(*prompt***\[, ***buttons***\] \[, ***title***\] \[, ***helpfile***, ***context***\]**)**

**MsgBox** 函数的语法具有以下几个命名参数：

|  |  |
|----|----|
| **部分** | **描述** |
| ***Prompt*** | 必需的。字符串表达式，作为显示在对话框中的消息。***prompt*** 的最大长度大约为 1024 个字符，由所用字符的宽度决定。如果 ***prompt*** 的内容超过一行，则可以在每一行之间用回车符 (**Chr(**13**)**)、换行符 (**Chr(**10**)**) 或是回车与换行符的组合 (**Chr(**13**)** & **Chr(**10**)**) 将各行分隔开来。 |
| ***Buttons*** | 可选的。数值表达式是值的总和，指定显示按钮的数目及形式，使用的图标样式，缺省按钮是什么以及消息框的强制回应等。如果省略，则 ***buttons*** 的缺省值为 0。 |
| ***Title*** | 可选的。在对话框标题栏中显示的字符串表达式。如果省略 ***title***，则将应用程序名放在标题栏中。 |
| ***Helpfile*** | 可选的。字符串表达式，识别用来向对话框提供上下文相关帮助的帮助文件。如果提供了 ***helpfile***，则也必须提供 ***context***。 |
| ***Context*** | 可选的。数值表达式，由帮助文件的作者指定给适当的帮助主题的帮助上下文编号。如果提供了 ***context***，则也必须提供 ***helpfile***。 |

  

**设置值**

***buttons*** 参数有下列设置值：

|  |  |  |
|----|----|----|
| **常数** | **值** | **描述** |
| **vbOKOnly** | 0 | 只显示 **OK** 按钮。 |
| **VbOKCancel** | 1 | 显示 **OK** 及 **Cancel** 按钮。 |
| **VbAbortRetryIgnore** | 2 | 显示 **Abort**、**Retry** 及 **Ignore** 按钮。 |
| **VbYesNoCancel** | 3 | 显示 **Yes**、**No** 及 **Cancel** 按钮。 |
| **VbYesNo** | 4 | 显示 **Yes** 及 **No** 按钮。 |
| **VbRetryCancel** | 5 | 显示 **Retry** 及 **Cancel** 按钮。 |
| **VbCritical** | 16 | 显示 **Critical Message** 图标。 |
| **VbQuestion** | 32 | 显示 **Warning Query** 图标。 |
| **VbExclamation** | 48 | 显示 **Warning Message** 图标。 |
| **VbInformation** | 64 | 显示 **Information Message** 图标。 |
| **vbDefaultButton1** | 0 | 第一个按钮是缺省值。 |
| **vbDefaultButton2** | 256 | 第二个按钮是缺省值。 |
| **vbDefaultButton3** | 512 | 第三个按钮是缺省值。 |
| **vbDefaultButton4** | 768 | 第四个按钮是缺省值。 |

  

<table data-border="1" data-cellpadding="5" cols="6" data-frame="below" data-rules="rows">
<tbody>
<tr data-valign="top">
<td width="28%"><strong>vbApplicationModal</strong></td>
<td width="9%">0</td>
<td colspan="3" width="50%">应用程序强制返回；应用程序一直被挂起，直到用户对消息框作出响应才继续工作。</td>
<td></td>
</tr>
<tr data-valign="top">
<td colspan="3" width="40%"><strong>vbSystemModal</strong></td>
<td width="10%">4096</td>
<td colspan="2" width="50%">系统强制返回；全部应用程序都被挂起，直到用户对消息框作出响应才继续工作。</td>
</tr>
<tr data-valign="top">
<td colspan="3" width="40%"><strong>vbMsgBoxHelpButton</strong></td>
<td width="10%">16384</td>
<td colspan="2" width="50%">将Help按钮添加到消息框</td>
</tr>
<tr data-valign="top">
<td colspan="3" width="40%"><strong>VbMsgBoxSetForeground</strong></td>
<td width="10%">65536</td>
<td colspan="2" width="50%">指定消息框窗口作为前景窗口</td>
</tr>
<tr data-valign="top">
<td colspan="3" width="40%"><strong>vbMsgBoxRight</strong></td>
<td width="10%">524288</td>
<td colspan="2" width="50%">文本为右对齐</td>
</tr>
<tr data-valign="top">
<td colspan="3" width="40%"><strong>vbMsgBoxRtlReading</strong></td>
<td width="10%">1048576</td>
<td colspan="2" width="50%">指定文本应为在希伯来和阿拉伯语系统中的从右到左显示</td>
</tr>
</tbody>
</table>

  

第一组值 (0–5) 描述了对话框中显示的按钮的类型与数目；第二组值 (16, 32, 48, 64) 描述了图标的样式；第三组值 (0, 256, 512) 说明哪一个按钮是缺省值；而第四组值 (0, 4096) 则决定消息框的强制返回性。将这些数字相加以生成 ***buttons*** 参数值的时候，只能由每组值取用一个数字。

**注意** 这些常数都是 Visual Basic for Applications (VBA) 指定的。结果，可以在程序代码中到处使用这些常数名称，而不必使用实际数值。

**返回值**

<table data-border="1" data-cellpadding="5" cols="5" data-frame="below" data-rules="rows">
<tbody>
<tr data-valign="top">
<td colspan="2" class="label" width="16%"><strong>常数</strong></td>
<td colspan="2" class="label" width="12%"><strong>值</strong></td>
<td class="label" width="72%"><strong>描述</strong></td>
</tr>
<tr data-valign="top">
<td width="15%"><strong>vbOK</strong></td>
<td colspan="2" width="11%">1</td>
<td colspan="2" width="74%"><strong>OK</strong></td>
</tr>
<tr data-valign="top">
<td colspan="2" width="16%"><strong>vbCancel</strong></td>
<td colspan="2" width="12%">2</td>
<td width="72%"><strong>Cancel</strong></td>
</tr>
<tr data-valign="top">
<td colspan="2" width="16%"><strong>vbAbort</strong></td>
<td colspan="2" width="12%">3</td>
<td width="72%"><strong>Abort</strong></td>
</tr>
<tr data-valign="top">
<td colspan="2" width="16%"><strong>vbRetry</strong></td>
<td colspan="2" width="12%">4</td>
<td width="72%"><strong>Retry</strong></td>
</tr>
<tr data-valign="top">
<td colspan="2" width="16%"><strong>vbIgnore</strong></td>
<td colspan="2" width="12%">5</td>
<td width="72%"><strong>Ignore</strong></td>
</tr>
<tr data-valign="top">
<td colspan="2" width="16%"><strong>vbYes</strong></td>
<td colspan="2" width="12%">6</td>
<td width="72%"><strong>Yes</strong></td>
</tr>
<tr data-valign="top">
<td colspan="2" width="16%"><strong>vbNo</strong></td>
<td colspan="2" width="12%">7</td>
<td width="72%"><strong>No</strong></td>
</tr>
</tbody>
</table>

  

**说明**

在提供了 ***helpfile*** 与 ***context*** 的时候，用户可以按 F1 来查看与 **context** 相应的帮助主题。像 Microsoft Excel 这样一些主机应用程序也会在对话框中自动添加一个 **Help** 按钮。

如果对话框显示 **Cancel** 按钮，则按下 ESC 键与单击 **Cancel** 按钮的效果相同。如果对话框中有 **Help** 按钮，则对话框中提供有上下文相关的帮助。但是，直到其它按钮中有一个被单击之前，都不会返回任何值。

**注意** 如果还要指定第一个命名参数以外的参数，则必须在表达式中使用 **MsgBox**。为了省略某些位置参数，必须加入相应的逗号分界符。
