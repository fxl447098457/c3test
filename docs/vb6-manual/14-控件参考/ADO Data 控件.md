# ADO Data  控件

# ADO Data 控件

               

**ADO Data 控件**与内部 **Data** 控件以及 **Remote Data控件** (RDC) 相似。**ADO Data 控件**使您能使用 **Microsoft ActiveX Data Objects** (ADO) 快速地创建一个到数据库的连接。

**说明**

在设计时，您可以通过首先将 **ConnectionString** 属性设置为一个有效的连接字符串，然后将 **RecordSource** 属性设置为一个适合于数据库管理者的语句来创建一个连接。您也可以将 **ConnectionString** 属性设置为定义连接的文件名。该文件是由“**数据链接**”对话框产生的，当您单击“属性”窗口中的 **ConnectionString**，然后单击“**生成**”或“**选择**”时，该对话框出现。

您可以通过将 **DataSource** 属性设置为 **ADO Data 控件**，把 **ADO Data 控件**连接到一个数据绑定的控件，例如 **DataGrid**、**DataCombo**、或 **DataList** 控件。

在运行时，您可以动态地设置 **ConnectionString** 和 **RecordSource** 属性来更改数据库。或者，您可以将 **Recordset** 属性直接设置为一个原先已经打开的记录集。
