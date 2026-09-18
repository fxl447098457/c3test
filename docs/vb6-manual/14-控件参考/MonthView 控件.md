# MonthView 控件

# MonthView 控件

               

**MonthView** 控件可以用来创建一个能够让用户通过日历风格的界面查看和设置日期信息的应用程序。

**语法**

**MonthView**

**说明**

**MonthView** 控件的 **Value** 属性返回当前被选定的日期。

可以允许最终用户通过将 **MultiSelect** 属性设置为 **True**，并使用 **MaxSelProperty** 指定可选择的天数来选择一个连续的日期范围。**SelStart** 和 **SelEnd** 属性返回所选择的日期范围的第一个日期和最后一个日期。

可以用许多方法自定义一个 **MonthView** 控件的外观。可以使用各种颜色属性，例如 **MonthBackColor**、**TitleBackColor**、**TitleForeColor** 和 **TrailingForeColor** 为控件创建一个唯一的配色方案。

通过设置 **MonthRows** 和 **MonthColumns** 属性，可以在一个 **MonthView** 控件中一次显示多个月份（多至 12）。**MonthRows** 和 **MonthColumns** 属性的总数必须小于或等于 12。

**注意**    **MonthView** 控件是 ActiveX 控件组的一部分，位于 Mscomct2.ocx 文件中。如果要在应用程序中使用 **MonthView** 控件，必须将 Mscomct2.ocx 文件添加到工程中。在发布该应用程序时，需要在用户的 Microsoft Windows 的 System 或 System32 目录中安装这个 Mscomct2.ocx 文件。有关如何将 ActiveX 控件添加到工程的详细信息，请参阅《程序员指南》中的“添加控件到工程”。
