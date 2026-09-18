# UpDown 控件

# UpDown控件

               

**UpDown** 控件有一对上下箭头按钮，单击时使诸如滚动位置或者关联的伙伴控件中的值增减。

**语法**

**UpDown**

**说明**

对用户而言，**UpDown** 控件及其伙伴控件常常象单个控件。伙伴控件可以是通过 **BuddyControl** 属性与 **UpDown** 控件相关联的任何控件，通常它显示如 **TextBox** 控件或 **CommandButton** 控件的数据。

**注意** 象内在的标签控件这样，一些辅助的无窗口控件，都不能用作伙伴控件。

通过设置 **AutoBuddy** 属性，**UpDown** 控件就自动地将Tab 键次序中前面的控件作为它的伙伴控件。如果Tab 键次序中没有前面的控件，**UpDown** 控件则将Tab 键次序中的下一个控件作为它的伙伴控件。另一种设置伙伴控件的方法是使用 **BuddyControl** 属性。在设计时，一旦 **AutoBuddy** 属性或 **BuddyControl** 属性被设置，伙伴控件会自动地按其大小和位置与 **UpDown** 控件配对。**UpDown** 控件可用 **Alignment** 属性决定它被放在伙伴控件的右面或左面。

当单击控件上的按钮时，**Increment**、**Min**、**Max** 和 **Wrap** 属性规定 **UpDown** 控件的 **Value** 属性如何改变。例如，如果有一些值是 10 的倍数，且在 20 与 80 之间，可以设置 **Increment**、**Min** 和 **Max** 属性值分别为 10、20 和 80。**Wrap** 属性允许 **Value** 属性超过 **Max** 属性值从 **Min** 属性值重新开始增加，或相反。

没有伙伴控件的 **UpDown** 控件起一种简化滚动条的作用。

注意 **UpDown** 控件应当代替 Visual Basic 4.0 的 Spin Button 控件。

**发行注意**   **UpDown** 控件是在 MSCOMCT2.OCX 文件中可找到的 ActiveX 控件的一部分。为了在应用程序中使用 **UpDown** 控件，必须在工程中添加 MSCOMCT2.OCX 文件。在发行应用程序时，要将 MSCOMCT2.OCX 文件安装在用户的 Microsoft Windows 的 SYSTEM 目录中。关于如何在工程中添加自定义控件的详细信息，请参阅《程序员指南》。
