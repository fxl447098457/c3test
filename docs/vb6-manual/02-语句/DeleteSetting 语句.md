# DeleteSetting 语句

# DeleteSetting 语句

       

在 Windows 注册表中，从应用程序项目里删除区域或注册表项设置。

**语法**

**DeleteSetting *appname*,** ***section***\[**,** ***key***\]

**DeleteSetting** 语句的语法具有下列命名参数：

<table data-border="1" data-cellpadding="5" cols="4" data-frame="below" data-rules="rows">
<tbody>
<tr data-valign="top">
<td colspan="2" class="label" width="16%"><strong>部分</strong></td>
<td colspan="2" class="label" width="84%"><strong>描述</strong></td>
</tr>
<tr data-valign="top">
<td width="12%"><strong><em>appname</em></strong></td>
<td colspan="2" width="67%">必需的。字符串表达式，包含应用程序或工程的名称，区域或注册表项用于这些应用程序或工程。</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="12%"><strong><em>section</em></strong></td>
<td colspan="2" width="67%">必要。字符串表达式，包含要删除注册表项设置的区域名称。如果只有 <strong><em>appname</em></strong> 和 <strong><em>section</em></strong>，则将指定的区域连同所有有关的注册表项设置都删除。</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="12%"><strong><em>key</em></strong></td>
<td colspan="2" width="67%">可选。字符串表达式，包含要删除的注册表项设置。</td>
<td></td>
</tr>
</tbody>
</table>

  

**说明**

如果提供了所有参数，则删除指定的注册表项设置。如果试图使用不存在的区域或注册表项设置上的 **DeleteSetting** 语句，则发生一个运行时错误。
