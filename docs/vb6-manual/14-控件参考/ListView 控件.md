# ListView 控件

# ListView 控件

               

**ListView** 控件可使用四种不同视图显示项目。通过此控件，可将项目组成带有或不带有列标头的列，并显示伴随的图标和文本。

**语法**

**ListView**

**说明**

可使用 **ListView** 控件将称作 **ListItem** 对象的列表条目组织成下列四种不同的视图之一：

- 大（标准）图标  
    
- 小图标  
    
- 列表  
    
- 报表

**View** 属性决定在列表中控件使用何种视图显示项目。还可用 **LabelWrap** 属性控制列表中与项目关联的标签是否可换行显示。另外，还可管理列表中项目的排序方法和选定项目的外观。

**ListView** 控件包括 **ListItem** 和 **ColumnHeader** 对象。**ListItem** 对象定义 **ListView** 控件中项目的各种特性，诸如：

- 项目的简要描述。  
    
- 由 **ImageList** 控件提供的与项目一起出现的图标。  
    
- 附加的文本片段，称作子项目，它们与显示在报表视图中的 **ListItem** 对象关联。

可以使用 **HideColumnHeaders** 属性决定是否在 **ListView** 控件中显示列标头。列标头可以在设计时添加，也可以在运行时添加。设计时，使用 **ListView**“控件属性”对话框的“列首”选项卡添加列标头。运行时，使用 **Add** 方法添加 **ColumnHeader** 对象到 **ColumnHeaders** 集合中。

**发行注意**   **ListView** 控件是 Mscomctl.ocx 文件中一组 ActiveX 控件的一部分。若要在应用程序中使用 **ListView** 控件，则必须将 Mscomctl.ocx 文件添加到工程中。当发行应用程序时，请将 Mscomctl.ocx 文件安装到用户的 Microsoft Windows System 或 System32 目录下。关于如何将 ActiveX 控件添加到 Visual Basic 工程的详细信息，请参阅 Visual Basic《程序员指南》。
