# TextBox 控件

# TextBox 控件

               

**TextBox** 控件有时也称作编辑字段或者编辑控件，显示设计时输入的用户输入的、或运行时在代码中赋予控件的信息。

**语法**

**TextBox**

**说明**

为了在 **TextBox** 控件中显示多行文本，要将 **MultiLine** 属性设置为 **True**。如果多行 **TextBox** 没有水平滚动条，那么即使 **TextBox** 调整了大小，文本也会自动换行。为了在 **TextBox** 上定制滚动条组合，需要设置 **ScrollBars** 属性。

如果文本框的 **MultiLine** 属性设置为 **True** 而且它的 **ScrollBars** 没有设置为 **None** (0)，则滚动条总出现在文本框上。

如果将 **MultiLine** 属性设置为 **True**，则可以在 **TextBox** 内用 **Alignment** 属性设置文本的对齐。如果 **MultiLine** 属性是 **False**，则 **Alignment** 属性不起作用。

在 DDE 对话中，**TextBox** 控件还可以起接收端链接的作用。
