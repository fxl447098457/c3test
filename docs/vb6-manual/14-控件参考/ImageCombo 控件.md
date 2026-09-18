# ImageCombo  控件

# ImageCombo 控件

               

**ImageCombo** 控件是标准 Windows 组合框的允许绘图版本。控件列表部分中的每一项都可以有一幅图片指定给它。

除了支持图片之外， **ImageCombo** 还提供了一个对象和基于集合的列表控件。控件列表部分的每一项是一个不同的 **ComboItem** 对象，而且列表中的所有项组合起来构成 **ComboItems** 集合。这就使它容易一项一项地指定诸如标记文本、**ToolTip**文本、关键字值以及缩进等级等属性。

**语法**

**ImageCombo**

**说明**

使用 **ImageCombo** 控件可以显示一个包含图片的项目列表。每一项可以有自己的图片，也可以对多个列表项使用相同的图片。

 **ImageCombo** 控件包括一个 **ComboItem** 对象的集合。一个 **ComboItem** 对象定义了出现在控件列表部分中的项目的各种特性。

除了用列表项目来显示图片外， **ImageCombo** 控件还使用集合和对象管理控件的列表部分。这使它很容易使用相似的对象和集合概念来对列表中的输入项进行操作，例如 **Add、** **Remove** 和 **Clear** 方法，以及 **For Each** 和 **With... End With** 结构。

**注意**    **ImageCombo** 控件是一组 ActiveX 控件的一部分，这组 ActiveX 控件能够在 Mscomctl.ocx 文件中找到。要在您的应用程序中使用 **ImageCombo** 控件，必须先将 Mscomctl.ocx 文件添加到工程中。当发布您的应用程序时，要把 Mscomctl.ocx 文件安装到用户的 Microsoft Windows System 或 System32 目录中。有关如何向一个工程中添加 ActiveX 控件的更多信息，请参阅《程序员指南*》*中的“添加控件到工程”。
