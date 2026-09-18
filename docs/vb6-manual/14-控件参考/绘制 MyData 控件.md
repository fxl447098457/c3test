# 绘制 MyData 控件

数据控件需要一个用户界面，允许用户在数据中向前或向后移动。在这种情况下，我们将使用四个 CommandButtons、一个 Label 控件以及一个 PictureBox 作为容器来复制熟悉的 Data 控件的功能和外观。

**注意** 该主题是帮助您创建示例数据源控件系列主题的一部分。创建数据源是第一部分。

**要添加组成控件到 MyData 控件，请按照以下步骤执行：**

1.  在“工程资源管理器”窗口中，双击“**MyData**”打开它的设计器。  
      

2.  在“工具箱”中，双击 **PictureBox** 控件在 MyData 设计器中放置一个 PictureBox 控件。并将它移到设计器的左上角附近。  
      

3.  在“属性”窗口中，设置该 PictureBox 控件的下列属性值：
    |           |                                |
    |-----------|--------------------------------|
    | **属性**  | **值**                         |
    | BackColor | &H80000005 (Window Background) |
    | Width     | 4500                           |

      

4.  在 PictureBox 控件被选择的情况下，在“工具箱”中选择 **CommandButton** 控件并且在 Picture1 的顶部绘制一个 CommandButton 控件。重复该过程总共添加四个 CommandButton 控件（但不要创建一个控件数组）。  
      

5.  在“属性”窗口中，设置 CommandButton 控件的下列属性值。显示多个值的地方表示从 Command1 到 Command4 的设置值；否则该值适用于所有的四个 CommandButton 控件：

    |          |                                         |
    |----------|-----------------------------------------|
    | **属性** | **值**                                  |
    | (Name)   | cmdFirst; cmdPrev; cmdNext; cmdLast     |
    | Caption  | (Empty)                                 |
    | Picture  | First.bmp; Prev.bmp; Next.bmp; Last.bmp |
    | Style    | 1 - Graphical                           |
    | Width    | 300                                     |

      

    **注意** Picture 属性的位图可以在与 AXData 示例应用程序相同的目录中找到。

6.  在“工具箱”中，选择 **Label** 控件，在 Picture1 上面绘制一个标签。在“属性”窗口中，设置 Label 控件的下列属性值：
    |           |                 |
    |-----------|-----------------|
    | **属性**  | **值**          |
    | (Name)    | lblCaption      |
    | BackStyle | 0 - Transparent |
    | Caption   | MyData          |

      

7.  重新排列 Shape 和 Label 控件，使它们外观和下面显示的示例相似。不要担心精确的位置，因为 Resize 事件中的代码将管理它们。选择 MyData 控件，并使用抓取柄调整它的大小，使它比包含控件的区域稍微大一点。

    <img src="vb52ro1.gif" data-border="0" />

8.  双击 **MyData** 设计器，将代码窗口放到前面，添加下面的代码到 UserControl_Resize 事件过程：

        Private Sub UserControl_Resize()
        Picture1.Move 0, 0, Width, Height
        cmdFirst.Move 0, 0, cmdFirst.Width, Height - 60
        cmdPrev.Move cmdFirst.Left + cmdFirst.Width, 0, _
        cmdPrev.Width, Height - 60
        cmdLast.Move (Width - cmdLast.Width) - 60, 0, _
        cmdLast.Width, Height - 60
        cmdNext.Move cmdLast.Left - cmdNext.Width, 0, _
        cmdNext.Width, Height - 60

        lblCaption.Height = TextHeight("A")
        lblCaption.Move cmdPrev.Left + _
        cmdPrev.Width, ((Height - 60) _
        / 2) - (lblCaption.Height / 2), _
        cmdNext.Left - (cmdPrev.Left _
        + cmdPrev.Width)
        End Sub

    当 MyData 被初始化时，或每当它调整大小重新排列组成控件并提供一个一致的外观时，将执行这段代码。

9.  添加一对 Property Let / Property Get 过程显露 lblCaption 的 Caption 属性：

        Public Property Get Caption() As String
        Caption = lblCaption.Caption
        End Property

        Public Property Let Caption(ByVal NewCaption As String)
        lblCaption.Caption = NewCaption
        PropertyChanged "Caption"
        End Property

10. 在“对象窗口”框中，选择（“通用”）。在“过程”框中，选择（“**声明**”），定位到代码模块的上面。添加下面的代码：

        Option Explicit

        '枚举 BOFAction 属性。
        Public Enum BOFActionType
        adDoMoveFirst = 0
        adStayBOF = 1
        End Enum

        '枚举 EOFAction 属性。
        Public Enum EOFActionType
        adDoMoveLast = 0
        adStayEOF = 1
        adDoAddNew = 2
        End Enum

        ' 为 ADO 连接和 Recordset 对象声明对象变量。
        Private cn As ADODB.Connection
        Private WithEvents rs As ADODB.Recordset

        '缺省属性值：
        Const m_def_RecordSource = ""
        Const m_def_BOFAction = BOFActionType.adDoMoveFirst
        Const m_def_EOFAction = EOFActionType.adDoMoveLast
        Const m_def_ConnectionString = ""

        '属性变量：
        Private m_RecordSource As String
        Private m_BOFAction As BOFActionType
        Private m_EOFAction As EOFActionType
        Private m_ConnectionString As String

11. 在“对象窗口”框中，选择“UserControl”。在“过程”框中，选择 **InitProperties** 事件。添加下面的代码到 UserControl_InitProperties 事件过程：

        Private Sub UserControl_InitProperties()
        m_RecordSource = m_def_RecordSource
        m_BOFAction = m_def_BOFAction
        m_EOFAction = m_def_EOFAction
        lblCaption.Caption = Ambient.DisplayName
        m_ConnectionString = m_def_ConnectionString
        Set UserControl.Font = Ambient.Font
        End Sub

12. 在继续进行之前，现在是保存您更改的好时机。从“文件”菜单中选择“保存工程”保存您的工程。

**详细信息** 请参阅“创建 ActiveX 控件”中的“绘制 ShapeLabel 控件”和“添加事件到 ShapeLabel 控件”。

### 步骤

该主题是帮助您创建示例 ActiveX 数据源系列主题的一部分。

|            |                                                              |
|------------|--------------------------------------------------------------|
| **要**     | **请参阅**                                                   |
| 到下一步骤 | 添加 AXDataSource 工程 |
| 从头开始   | 创建数据源                   |
