# RichTextBox 控件

# RichTextBox 控件

               

**RichTextBox** 控件不仅允许输入和编辑文本，同时还提供了标准 **TextBox** 控件未具有的、更高级的指定格式的许多功能。

**语法**

**RichTextBox**

**说明**

**RichTextBox** 提供了一些属性，对于本控件文本的任何部分，用这些属性都可以指定格式。为了改变文本的格式，首先要选定它。只有选定的文本才能赋予字符和段落格式。使用这些属性，可把文本改为粗体或斜体，或改变其颜色，以及创建上标和下标。通过设置左右缩进和悬挂式缩进，可调整段落的格式。

**RichTextBox** 控件能以 rtf 格式和普通 ASCII 文本格式这两种形式打开和保存文件。可以使用控件的方法（**LoadFile** 和 **SaveFile**）直接读写文件，或使用与 Visual Basic 文件输入/输出语句联结的、诸如 **SelRTF** 和 **TextRTF** 之类的控件属性打开和保存文件。

通过使用 **OLEObjects** 集合，**RichTextBox** 控件支持对象的嵌入。插入到控件中的每个对象，都代表 **OLEObject** 对象。用这样的控件，就可以创建包含其它文档或对象的文档。例如，可创建这样的文档，它有一个嵌入的 Microsoft Excel 电子数据表格、或 Microsoft Word 文档、或其它已在系统中注册的 OLE 对象。为了把一个对象插入到 **RichTextBox** 控件中，只需简单地拖动一个文件（例如 在Windows 95“资源管理器”中的拖动），或拖动的是另一应用程序（如 Microsoft Word）所用文件的一个突出显示的区域，然后将所拖内容直接放入控件。

**RichTextBox** 控件支持 OLE 对象的剪贴板和 OLE 拖/放操作。从剪贴板中粘贴进一个对象时，它被插在当前插入点处。一个对象被拖放到控件时，插入点将跟踪着鼠标光标的移动，直至鼠标按钮释放时该对象即被插入。这种行为和 Microsoft Word 的一样。

使用 **SelPrint** 方法，可以打印 **RichTextBox** 控件的全部或部分文本。

因为 **RichTextBox** 是一个数据绑定控件，通过 **Data** 控件可以把它绑定到 Microsoft Access 数据库的 Binary 或 Memo 字段上，也可把它绑定到具有相同容量的其它数据库字段上（例如 SQL 服务器中的 TEXT 数据类型的字段）。

标准 **TextBox** 控件用到的所有属性、事件和方法，**RichTextBox** 控件几乎都能支持，例如 **MaxLength、** **MultiLine**、 **ScrollBars、** **SelLength、** **SelStart** 和 **SelText**。对于那些可以使用 **TextBox** 控件的应用程序，也可以很容易地使用 **RichTextBox** 控件。而且，**RichTextBox** 控件并没有和标准 **TextBox** 控件一样具有 64K 字符容量的限制。

**发行注意** 为了能在应用程序中使用 **RichTextBox** 控件，必须把Richtx32.ocx 文件添加到工程中。因此，在应用程序发行时，Richtx32.ocx 文件就应安装在 Microsoft Windows 的 SYSTEM 目录内。有关怎样把自定义控件添加到工程中的详细内容，请参阅《程序员指南》。
