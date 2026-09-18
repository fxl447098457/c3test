# Property Get 语句

**Property Get 语句**

       

声明 **Property** 过程的名称，参数以及构成其主体的代码，该过程获取一个属性的值。

**语法**

\[**Public** \| **Private** \| **Friend**\] \[**Static**\] **Property** **Get** *name* \[**(***arglist***)**\] \[**As** *type*\]  
\[*statements*\]  
\[*name* **=** *expression*\]  
\[**Exit Property**\]  
\[*statements*\]  
\[*name* **=** *expression*\]

**End Property**

**Property Get** 语句的语法包含下面部分：

<table data-border="1" data-cellpadding="5" cols="2" data-frame="below" data-rules="rows">
<colgroup>
<col style="width: 50%" />
<col style="width: 50%" />
</colgroup>
<tbody>
<tr data-valign="top">
<td class="label" width="19%"><strong>部分</strong></td>
<td class="label" width="81%"><strong>描述</strong></td>
</tr>
<tr data-valign="top">
<td width="19%"><strong>Public</strong></td>
<td width="81%">可选的。表示所有模块的所有其它过程都可访问 <strong>Property Get</strong> 过程。如果在包含 <strong>Option Private</strong> 的模块中使用，则该过程在该工程外是不可使用的。</td>
</tr>
<tr data-valign="top">
<td width="19%"><strong>Private</strong></td>
<td width="81%">可选的。表示只有包含其声明的模块的其它过程可以访问该 <strong>Property</strong> <strong>Get</strong> 过程。</td>
</tr>
<tr data-valign="top">
<td width="19%"><strong>Friend</strong></td>
<td width="81%">可选的。只能在类模块中使用。表示该 <strong>Property Get</strong> 过程在整个工程中都是可见的，但对对象实例的控制者是不可见的。</td>
</tr>
<tr data-valign="top">
<td width="19%"><strong>Static</strong></td>
<td width="81%">可选的。表示在调用之间保留该 <strong>Property Get</strong> 过程的局部变量的值。<strong>Static</strong> 属性对在该 <strong>Property Get</strong> 过程外声明变量不会产生影响，即使过程中也使用了这些变量。</td>
</tr>
<tr data-valign="top">
<td width="19%"><em>name</em></td>
<td width="81%">必需的。<strong>Property Get</strong> 过程的名称；遵循标准的变量命名约定，但不能与同一模块中的 <strong>Property</strong> <strong>Let</strong> 或 <strong>Property Set</strong> 过程同名。</td>
</tr>
<tr data-valign="top">
<td width="19%"><em>arglist</em></td>
<td width="81%">可选的。代表在调用时要传递给 <strong>Property Get</strong> 过程的参数的变量列表。多个变量则用逗号隔开。<strong>Property</strong> <strong>Get</strong> 过程中的每个参数的名称和数据类型必须与相应 <strong>Property</strong> <strong>Let</strong> 过程（如果存在）中的参数一致。</td>
</tr>
<tr data-valign="top">
<td width="19%"><em>type</em></td>
<td width="81%">可选的。该 <strong>Property Get</strong> 过程的返回值的数据类型；可以是 Byte、Boolean、Integer、Long、Currency、Single、Double、Decimal（目前尚不支持）、Date、String（除定长）、Object、Variant或任何用户定义类型。任何类型的数组都不能作为返回值，但包含数组的 <strong>Variant</strong> 可以作为返回值。
<p><strong>Property</strong> <strong>Get</strong> 过程的返回值类型必须与相应的 <strong>Property</strong> <strong>Let</strong> 过程（如果有）的最后一个（有时是仅有的）参数的数据类型相同，该 <strong>Property</strong> <strong>Let</strong> 过程将其右边表达式的值赋给属性。</p></td>
</tr>
<tr data-valign="top">
<td width="19%"><em>statements</em></td>
<td width="81%">可选的。<strong>Property Get</strong> 过程体中所执行的任何语句组。</td>
</tr>
<tr data-valign="top">
<td width="19%"><em>expression</em></td>
<td width="81%">可选的。<strong>Property Get</strong> 语句所定义的过程返回的属性值。</td>
</tr>
</tbody>
</table>

  

其中的 *arglist* 参数的语法及语法的各个部分如下：

\[**Optional**\] \[**ByVal** \| **ByRef**\] \[**ParamArray**\] *varname*\[**( )**\] \[**As** *type*\] \[**=** *defaultvalue*\]

|  |  |
|----|----|
| **部分** | **描述** |
| **Optional** | 可选的。表示参数不是必需的。如果使用了该选项，则 *arglist* 中的后续参数都是可选的，而且必须都使用 **Optional** 关键字声明。 |
| **ByVal** | 可选的。表示该参数按值传递。 |
| **ByRef** | 可选的。表示该参数按地址传递。**ByRef** 是 Visual Basic 的缺省选项。 |
| **ParamArray** | 可选的。只用于 *arglist* 的最后一个参数，指明最后这个参数是一个 **Variant** 元素的 **Optional** 数组。使用 **ParamArray** 关键字可以提供任意数目的参数。**ParamArray** 关键字不能与 **ByVal、ByRef** 或 **Optional** 一起使用。 |
| *varname* | 必需的。代表参数的变量名称；遵循标准的变量命名约定。 |
| *type* | 可选的。传递给该过程的参数的数据类型；可以是 **Byte、Boolean**、**Integer、Long、Currency、Single、Double、Decimal**（目前尚不支持）、**Date、String**（只支持变长）、**Object** 或 **Variant**。如果参数不是 **Optional**，则也可以是用户自定义的类型，或对象类型。 |
| *defaultvalue* | 可选的。任何常数或常量表达式。只在 **Optional** 参数时是合法的。如果类型为 **Object**，则显式的缺省值只能是 **Nothing**。 |

  

**说明**

如果没有使用 **Public，Private** 或 **Friend** 显式指定，则 **Property** 过程缺省为公用。如果没有使用 **Static**，则在调用之后不会保留局部变量的值。**Friend** 关键字只能在类模块中使用。**Friend** 过程可以被工程中的任何模块的过程访问。**Friend** 过程不会在其父类的类型库中出现，且 **Friend** 过程不能被后期绑定。

所有的可执行代码都必须属于某个过程。不能在别的 **Property、Sub** 或 **Function** 过程中定义 **Property Get** 过程。

**Exit Property** 语句使执行立即从一个 **Property Get** 过程中退出。程序接着从调用该 **Property Get** 过程的语句下一条语句开始执行。在 **Property Get** 过程中的任何位置都可以有 **Exit Property** 语句。

**Property Get** 过程与 **Sub** 和 **Property Let** 过程的相似之处是：**Property Get** 过程是一个可以获取参数，执行一系列语句，以及改变其参数的值的独立过程，而与 **Sub** 和 **Property Let** 过程不同的是：当要返回属性的值时，可以在表达式的右边使用 **Property Get** 过程，这与使用 **Function** 或属性名的方式一样。
