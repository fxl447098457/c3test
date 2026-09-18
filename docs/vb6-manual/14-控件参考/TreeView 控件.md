# TreeView 控件

# TreeView 控件

               

**TreeView** 控件显示 **Node** 对象的分层列表，每个 **Node** 对象均由一个标签和一个可选的位图组成。**TreeView** 一般用于显示文档标题、索引入口、磁盘上的文件和目录、或能被有效地分层显示的其它种类信息。

**语法**

**Treeview**

**说明**

创建了 **TreeView** 控件之后，可以通过设置属性与调用方法对各 **Node** 对象进行操作，这些操作包括添加、删除、对齐和其它操作。可以编程展开与折回 **Node** 对象来显示或隐藏所有子节点。Collapse、Expand 和 NodeClick 三个事件也提供编程功能。

**Node** 对象使用 **Root**、**Parent**、**Child**、**FirstSibling**、**Next**、**Previous** 和 **LastSibling** 属性。在代码中可通过检索对 **Node** 对象的引用，从而在树上定位。也可以使用键盘定位。UP ARROW 键和 DOWN ARROW 键向下循环穿过所有展开的 **Node** 对象。从左到右、从上到下地选择 **Node** 对象。若在树的底部，选择便跳回树的顶部，必要时滚动窗口。RIGHT ARROW 键和 LEFT ARROW 键也穿过所有展开的 **Node** 对象，但是如果选择了未展开的 **Node**之后再按 RIGHT ARROW 键，该 **Node** 便展开；第二次按该键，选择将移向下一个 **Node**。相反，若扩展的 **Node** 有焦点，这时再按 LEFT ARROW 键，该 **Node** 便折回。如果按下 ANSI 字符集 中的键，焦点将跳转至以那个字母开头的最近的 **Node**。后续的按该键的动作将使选择向下循环，穿过以那个字母开头的所有展开节点。

控件的外观有八种可用的替换样式，它们是文本、位图、直线和 +/- 号的组合，**Node** 对象可以任一种组合出现。

**TreeView** 控件使用由 **ImageList** 属性指定的 **ImageList** 控件，来存储显示于 **Node** 对象的位图和图标。任何时刻，**TreeView** 控件只能使用一个 **ImageList**。这意味着，当 **TreeView** 控件的 **Style** 属性被设置成显示图像的样式时，**TreeView** 控件中每一项的旁边都有一个同样大小的图像。

**发行注意 TreeView** 控件是 MSCOMCTL.OCX 文件中的一组 ActiveX 控件的一部分。为了在应用程序中使用 **TreeView** 控件，必须将 MSCOMCTL.OCX 文件添加到工程中。在发行应用程序时，要在用户的 Microsoft Windows System 或 System32 目录中安装 MSCOMCTL.OCX 文件。
