# TabStrip 控件

# TabStrip 控件

               

**TabStrip** 控件就象笔记本的书签或者一组文件夹的标签一样。通过使用 **TabStrip** 控件，可以在应用程序中为某个窗口或者对话框的相同区域定义多个页面。

**语法**

**TabStrip**

**说明**

该控件由 **Tabs** 集合中的一个或者更多个 **Tab** 对象组成。在设计时和运行时，都可以通过设置该控件的属性影响 **Tab** 对象外观。也可以在设计时用 TabStrip 控件的属性页来添加或删除选项卡，要是在运行时用方法来添加或删除 **Tab** 对象。

**Style** 属性决定了 **TabStrip** 控件看起来是象下压按钮还是象笔记本标签 。在设计时将一个 **TabStrip** 控件放在某个窗体上时，它就有了一个笔记本标签。如果 **Style** 属性被设置为 **tabTabs**，那么 **TabStrip** 控件的内部区域周围将有一个边框。当 **Style** 属性被设置为 **tabButtons** 时，控件的内部区域周围不显示边框，不过，那个区域仍然存在。

要设置 **TabStrip** 控件的整体大小，用其拖动句柄或者设置其 **Top**、**Left**、**Height** 和 **Width** 属性。运行时根据该控件的整体大小，Visual Basic 自动决定内部区域的大小和位置并返回 Client-coordinate 属性— **ClientLeft**、**ClientTop**、**ClientHeight** 和 **ClientWidth**。**MultiRow** 属性决定了该控件能否具有多于一行的选项卡，**TabWidthStyle** 属性决定了每一行的外观，还有，如果 **TabWidthStyle** 属性被设置为 **tabFixed**，则可以用 **TabFixedHeight** 和 **TabFixedWidth** 属性来给 **TabStrip** 控件中的所有选项卡设置相同的高度和宽度。

**TabStrip** 控件不是容器。要想包含实际页面和它们的对象，必须用 **Frame** 控件或者其它容器，它们的大小必须与控件中所有 **Tab** 对象共享的内部区域匹配。如果针对该容器使用了一个控件数组，则可以使特定的 **Tab** 对象与数组中的每一项相关联，请看下例：

    Option Explicit
    Private mintCurFrame As Integer' Current Frame visible

    Private Sub Tabstrip1_Click()
       If Tabstrip1.SelectedItem.Index = mintCurFrame _
          Then Exit Sub ' No need to change frame.
       ' Otherwise, hide old frame, show new.
       Frame1(Tabstrip1.SelectedItem.Index).Visible = True
       Frame1(mintCurFrame).Visible = False
       ' Set mintCurFrame to new value.
       mintCurFrame = Tabstrip1.SelectedItem.Index
    End Sub

**注意** 在容器上将控件分组时,必须使用上述显示/隐藏策略,而不使用 **Zorder Method** 将框架带到全面来。否则，实现访问键(ALT + 访问键)的控件将一直响应键盘命令，甚至当容器不是最顶端的控件时也如此。还要注意，必须将每个组放在它自己的容器中，以此将 **OptionButton** 控件的组分离开，否则，窗体上的所有 **OptionButtons** 将象一大组 **OptionButtons**。

**提示** 使用 **BorderStyle** 属性为 None 的 **Frame** 控件作为容器来取代 **PictureBox** 控件。**Frame** 控件比起 **PictureBox** 控件来说所花费的开销少。

**TabStrip** 控件的 **Tabs** 属性是所有 **Tab** 对象的集合。每个 **Tab** 对象都具有与其当前状态和外观相关联的属性。例如，可以使 **ImageList** 控件与 **TabStrip** 控件相关联，然后就可以在单个选项卡上使用图象了。也可以使工具提示与每个 **Tab** 对象相关联。

**发行注意**   **TabStrip** 控件是一组自定义控件的一部分，这组自定义控件可以在文件 MSCOMCTL.OCX 中找到。为了在应用程序中使用 **TabStrip** 控件，必须将文件 MSCOMCTL.OCX 添加到工程中去。当发布应用程序时，要把文件 MSCOMCTL.OCX 安装到用户的 Microsoft Windows SYSTEM 目录下。关于如何在工程中添加自定义控件的详细信息，请参阅《程序员指南》。
