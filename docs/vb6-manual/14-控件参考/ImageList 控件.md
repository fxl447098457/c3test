# ImageList 控件

# ImageList 控件

               

**ImageList** 控件包含 **ListImage** 对象的集合，该集合中的每个对象都可以通过其索引或关键字被引用。**ImageList** 控件不能独立使用，只是作为一个便于向其它控件提供图象的资料中心。

**语法**

**ImageList**

**说明**

**ImageList** 控件的作用象图像的储藏室，同时，它需要第二个控件显示所储存的图像。第二个控件可以是任何能显示图像 **Picture** 对象的控件，也可以是特别设计的、用于绑定 **ImageList** 控件的 Windows 通用控件之一。这些控件包括**ListView**、**ToolBar**、**TabStrip**、**Header**、**ImageCombo**、和 **TreeView** 控件。为了与这些控件一同使用 **ImageList**，必须通过一个适当的属性将特定的 **ImageList**控件绑定到第二个控件。对于 **ListView** 控件，必须设置其 **Icons** 和 **SmallIcons** 属性为 **ImageList** 控件。对于 **TreeView**、**TabStrip**、**ImageCombo**、和 **Toolbar** 控件，必须设置 **ImageList** 属性为 **ImageList** 控件。

在设计时，可以用“ImageList 控件属性”对话框的“图像”选项卡来添加图象。在运行时，可以用 **Add** 方法给 **ListImages** 集合添加图象。

对于 Windows 通用控件来说，设计时可以用“自定义属性”对话框来指定一个 **ImageList**。运行时也可以用 **ImageList** 属性指定一个 **ImageList** 控件，就象下面的例子所述的那样，它可以设置 **TreeView** 控件的：

    TreeView1.ImageList = ImageList1  '指定 ImageList 属性

**重点**   当与 Windows 通用控件一起使用 **ImageList** 控件时，在将它绑定到第二个控件之前，按照您希望的顺序将全部需要的图像插入到 **ImageList**。一旦 **ImageList** 被绑定到第二个控件 ，您就不能再删除图像了，并且也不能将图像插入到 **ListImages** 集合中间。但是您可以在集合的末尾添加图像。

一旦 **ImageList** 与某个 Windows 通用控件相关联，就可以在过程中用 **Index** 属性或 **Key** 属性的值来引用 **ListImage** 对象了。下面的示例设置 **TreeView** 控件的第三个 **Node** 对象的 **Image** 属性为 **ImageList** 控件中的第一个 **ListImage** 对象：

    '使用 ImageList1 的 Index 属性值。
    TreeView1.Nodes(3).Image = 1
    '或者使用 Key 属性值。
    TreeView1.Nodes(3).Image = "image 1"   '假定 Key 值为 "image 1。"

要与其它控件（不能绑定到 **ImageList** 控件的控件）一起使用 **ImageList** 控件，将第二个控件的 **Picture**属性赋值给 **ImageList** 控件中的任何一个图像的 **Picture**对象。例如，下面的代码把 **ListImages** 集合中的第一个 **ListImage** 对象的 **Picture** 对象赋值给一个新创建的 **StatusBar** 面板的 **Picture** 属性：

    Dim pnlX As Panel
    Set pnlX = StatusBar1.Panels.Add() '添加一个 Panel 对象。
    Set pnlX.Picture = ImageList1.ListImages(1).Picture '设置图片。

**注意**   将图像赋值给 **Picture** 对象时，必须使用 **Set** 语句。

可以插入任何大小的图像到 **ImageList** 控件中。然而，由第二个控件显示的图像大小是由一个因素决定的：第二个控件是否也是一个绑定到 **ImageList** 控件的 Windows 通用控件。

当 **ImageList** 控件被绑定到另一个 Windows 通用控件时，不同大小的图片可以被添加到控件中，但是在关联的 Windows 通用控件中显示的图象大小将受到添加到 **ImageList** 图像中的第一个图象大小的约束。例如，如果您添加一个 16 × 16像素的图像到 **ImageList** 控件中，然后将 **ImageList** 绑定到 **TreeView** 控件（用 **Node** 对象显示），所有存储于 **ImageList** 控件中的图像将以 16 × 16像素显示，即使它们的尺寸是更大或更小。

    此外，如果您用 Picture 对象显示图像，则存储在 ImageList 控件中的任何图像都将以图像最初的大小显示，无论它的尺寸是更大还是更小。

**发行注意**   **ImageList** 控件是 ActiveX 控件组的一部分，该控件组可以在文件 MSCOMCTL.OCX 中找到。若是在应用程序中使用 **ImageList** 控件，必须把文件 MSCOMCTL.OCX 添加到工程中。在分行应用程序时，应把文件 MSCOMCTL.OCX 安装到 Microsoft Windows 的 System 目录或者 System32 目录下。关于如何把一个 ActiveX 控件添加到工程中去的详细内容，请参阅《程序员指南》。
