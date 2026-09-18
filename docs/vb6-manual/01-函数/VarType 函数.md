# VarType 函数

返回一个 **Integer，**指出变量的子类型。

**语法**

**VarType(***varname***)**

必要的 *varname* 参数是一个 Variant，包含用户定义类型变量之外的任何变量。

**返回值**

<table data-border="1" data-cellpadding="5" cols="6" data-frame="below" data-rules="rows">
<tbody>
<tr data-valign="top">
<td colspan="2" class="label" width="22%"><strong>常数</strong></td>
<td colspan="2" class="label" width="12%"><strong>值</strong></td>
<td colspan="2" class="label" width="66%"><strong>描述</strong></td>
</tr>
<tr data-valign="top">
<td width="18%"><strong>vbEmpty</strong></td>
<td colspan="2" width="9%">0</td>
<td colspan="2" width="52%">Empty（未初始化）</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="18%"><strong>vbNull</strong></td>
<td colspan="2" width="9%">1</td>
<td colspan="2" width="52%">Null（无有效数据）</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="18%"><strong>vbInteger</strong></td>
<td colspan="2" width="9%">2</td>
<td colspan="2" width="52%">整数</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="18%"><strong>vbLong</strong></td>
<td colspan="2" width="9%">3</td>
<td colspan="2" width="52%">长整数</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="18%"><strong>vbSingle</strong></td>
<td colspan="2" width="9%">4</td>
<td colspan="2" width="52%">单精度浮点数</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="18%"><strong>vbDouble</strong></td>
<td colspan="2" width="9%">5</td>
<td colspan="2" width="52%">双精度浮点数</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="18%"><strong>vbCurrency</strong></td>
<td colspan="2" width="9%">6</td>
<td colspan="2" width="52%">货币值</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="18%"><strong>vbDate</strong></td>
<td colspan="2" width="9%">7</td>
<td colspan="2" width="52%">日期</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="18%"><strong>vbString</strong></td>
<td colspan="2" width="9%">8</td>
<td colspan="2" width="52%">字符串</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="18%"><strong>vbObject</strong></td>
<td colspan="2" width="9%">9</td>
<td colspan="2" width="52%">对象</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="18%"><strong>vbError</strong></td>
<td colspan="2" width="9%">10</td>
<td colspan="2" width="52%">错误值</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="18%"><strong>vbBoolean</strong></td>
<td colspan="2" width="9%">11</td>
<td colspan="2" width="52%">布尔值</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="18%"><strong>vbVariant</strong></td>
<td colspan="2" width="9%">12</td>
<td colspan="2" width="52%"><strong>Variant</strong>（只与变体中的数组一起使用）</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="18%"><strong>vbDataObject</strong></td>
<td colspan="2" width="9%">13</td>
<td colspan="2" width="52%">数据访问对象</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="18%"><strong>vbDecimal</strong></td>
<td colspan="2" width="9%">14</td>
<td colspan="2" width="52%">十进制值</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="18%"><strong>vbByte</strong></td>
<td colspan="2" width="9%">17</td>
<td colspan="2" width="52%">位值</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="18%"><strong>vbUserDefinedType</strong></td>
<td colspan="2" width="9%">36</td>
<td colspan="2" width="52%">包含用户定义类型的变量</td>
<td></td>
</tr>
<tr data-valign="top">
<td width="18%"><strong>vbArray</strong></td>
<td colspan="2" width="9%">8192</td>
<td colspan="2" width="52%">数组</td>
<td></td>
</tr>
</tbody>
</table>

  

**注意** 这些常数是由 Visual Basic 为应用程序指定的。这些名称可以在程序代码中到处使用，以代替实际值。

**说明**

**VarType** 函数自身从不对 **vbArray** 返回值。**VarType** 总是要加上一些其他值来指出一个具体类型的数组。常数 **vbVariant** 只与 **vbArray** 一起返回，以表明 **VarType** 函数的参数是一个 **Variant** 类型的数组。例如，对一个整数数组的返回值是 **vbInteger** + **vbArray**，或 8194。如果一个对象有缺省属性，则 **VarType** **(***object***)** 返回对象缺省属性的类型。
