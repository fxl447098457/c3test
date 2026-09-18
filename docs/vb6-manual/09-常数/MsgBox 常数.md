# MsgBox 常数

**MsgBox 常数**

   

可在代码中的任何地方使用下列常数代替实际值：

### MsgBox 参数

<table data-border="1" data-cellpadding="5" cols="4" data-frame="below" data-rules="rows">
<tbody>
<tr data-valign="top">
<td class="label" width="31%"><strong>常数</strong></td>
<td class="label" width="12%"><strong>值</strong></td>
<td colspan="2" class="label" width="57%"><strong>描述</strong></td>
</tr>
<tr data-valign="top">
<td width="31%"><strong>vbOKOnly</strong></td>
<td width="12%">0</td>
<td colspan="2" width="57%">只有 <strong>OK</strong> 按钮（缺省值）</td>
</tr>
<tr data-valign="top">
<td width="31%"><strong>vbOKCancel</strong></td>
<td width="12%">1</td>
<td colspan="2" width="57%"><strong>OK</strong> 和 <strong>Cancel</strong> 按钮</td>
</tr>
<tr data-valign="top">
<td width="31%"><strong>vbAbortRetryIgnore</strong></td>
<td width="12%">2</td>
<td colspan="2" width="57%"><strong>Abort</strong>、<strong>Retry</strong>，和 <strong>Ignore</strong> 按钮</td>
</tr>
<tr data-valign="top">
<td width="31%"><strong>vbYesNoCancel</strong></td>
<td width="12%">3</td>
<td colspan="2" width="57%"><strong>Yes</strong>、<strong>No</strong>，和 <strong>Cancel</strong> 按钮</td>
</tr>
<tr data-valign="top">
<td width="31%"><strong>vbYesNo</strong></td>
<td width="12%">4</td>
<td colspan="2" width="57%"><strong>Yes</strong> 和 <strong>No</strong> 按钮</td>
</tr>
<tr data-valign="top">
<td width="31%"><strong>vbRetryCancel</strong></td>
<td width="12%">5</td>
<td colspan="2" width="57%"><strong>Retry</strong> 和 <strong>Cancel</strong> 按钮</td>
</tr>
<tr data-valign="top">
<td width="31%"><strong>vbCritical</strong></td>
<td width="12%">16</td>
<td colspan="2" width="57%">关键消息</td>
</tr>
<tr data-valign="top">
<td width="31%"><strong>vbQuestion</strong></td>
<td width="12%">32</td>
<td colspan="2" width="57%">警告询问</td>
</tr>
<tr data-valign="top">
<td width="31%"><strong>vbExclamation</strong></td>
<td width="12%">48</td>
<td colspan="2" width="57%">警告消息</td>
</tr>
<tr data-valign="top">
<td width="31%"><strong>vbInformation</strong></td>
<td width="12%">64</td>
<td colspan="2" width="57%">通知消息</td>
</tr>
<tr data-valign="top">
<td width="31%"><strong>vbDefaultButton1</strong></td>
<td width="12%">0</td>
<td colspan="2" width="57%">第一个按钮是缺省的（缺省值）</td>
</tr>
<tr data-valign="top">
<td width="31%"><strong>vbDefaultButton2</strong></td>
<td width="12%">256</td>
<td colspan="2" width="57%">第二个按钮是缺省的</td>
</tr>
<tr data-valign="top">
<td width="31%"><strong>vbDefaultButton3</strong></td>
<td width="12%">512</td>
<td colspan="2" width="57%">第三个按钮是缺省的</td>
</tr>
<tr data-valign="top">
<td width="31%"><strong>vbDefaultButton4</strong></td>
<td width="12%">768</td>
<td colspan="2" width="57%">第四个按钮是缺省的</td>
</tr>
<tr data-valign="top">
<td width="31%"><strong>vbApplicationModal</strong></td>
<td width="12%">0</td>
<td width="45%">应用程序形态的消息框（缺省值）</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="31%"><strong>vbSystemModal</strong></td>
<td width="12%">4096</td>
<td colspan="2" width="57%">系统强制返回的消息框</td>
</tr>
<tr data-valign="top">
<td width="31%"><strong>vbMsgBoxHelpButton</strong></td>
<td width="12%">16384</td>
<td colspan="2" width="57%">添加Help按钮到消息框</td>
</tr>
<tr data-valign="top">
<td width="31%"><strong>VbMsgBoxSetForeground</strong></td>
<td width="12%">65536</td>
<td colspan="2" width="57%">指定消息框窗口作为前景窗口</td>
</tr>
<tr data-valign="top">
<td width="31%"><strong>vbMsgBoxRight</strong></td>
<td width="12%">524288</td>
<td colspan="2" width="57%">文本是右对齐的</td>
</tr>
<tr data-valign="top">
<td width="31%"><strong>vbMsgBoxRtlReading</strong></td>
<td width="12%">1048576</td>
<td colspan="2" width="57%">指定在希伯来语和阿拉伯语系统中，文本应当显示为从右到左读</td>
</tr>
</tbody>
</table>

  

### MsgBox 返回值

|          |        |          |
|----------|--------|----------|
| **常数** | **值** | **描述** |

  

<table data-border="1" data-cellpadding="5" cols="6" data-frame="below" data-rules="rows">
<tbody>
<tr data-valign="top">
<td width="16%"><strong>vbOK</strong></td>
<td width="11%">1</td>
<td colspan="3" width="60%">按下 <strong>OK</strong> 按钮</td>
<td></td>
</tr>
<tr data-valign="top">
<td colspan="3" width="29%"><strong>vbCancel</strong></td>
<td width="10%">2</td>
<td colspan="2" width="61%">按下 <strong>Cancel</strong> 按钮</td>
</tr>
<tr data-valign="top">
<td colspan="3" width="29%"><strong>vbAbort</strong></td>
<td width="10%">3</td>
<td colspan="2" width="61%">按下 <strong>Abort</strong> 按钮</td>
</tr>
<tr data-valign="top">
<td colspan="3" width="29%"><strong>vbRetry</strong></td>
<td width="10%">4</td>
<td colspan="2" width="61%">按下 <strong>Retry</strong> 按钮</td>
</tr>
<tr data-valign="top">
<td colspan="3" width="29%"><strong>vbIgnore</strong></td>
<td width="10%">5</td>
<td colspan="2" width="61%">按下 <strong>Ignore</strong> 按钮</td>
</tr>
<tr data-valign="top">
<td colspan="3" width="29%"><strong>vbYes</strong></td>
<td width="10%">6</td>
<td colspan="2" width="61%">按下 <strong>Yes</strong> 按钮</td>
</tr>
<tr data-valign="top">
<td colspan="3" width="29%"><strong>vbNo</strong></td>
<td width="10%">7</td>
<td colspan="2" width="61%">按下 <strong>No</strong> 按钮</td>
</tr>
</tbody>
</table>
