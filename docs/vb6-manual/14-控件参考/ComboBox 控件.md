# ComboBox 控件

# ComboBox 控件

               

**ComboBox** 控件将 **TextBox** 控件和 **ListBox** 控件的特性结合在一起－既可以在控件的文本框部分输入信息，也可以在控件的列表框部分选择一项。

**语法**

**ComboBox**

**说明**

为了添加或删除 **ComboBox** 控件中的项目，需要使用 **AddItem** 或 **RemoveItem** 方法。设置 **List**、**ListCount**、和 **ListIndex** 属性，使访问 **ComboBox** 中的项目成为可能。也可以在设计时使用 **List** 属性将项目添加到列表中。

**注意** 只有当 **ComboBox** 的下拉部分的内容被滚动时，Scroll 事件才在 **ComboBox** 中发生，而不是每次 **ComboBox** 的内容改变时。例如，如果 **ComboBox** 的下拉部分包含五行，并且最顶上的项为突出显示，则在您按完向下箭头键六下（或按一次 PgUp 键）之前 Scroll 事件不发生。再往后，每按一次向上箭头键引发一次 Scroll 事件。
