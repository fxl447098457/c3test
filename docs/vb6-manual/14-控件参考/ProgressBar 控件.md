# ProgressBar 控件

# ProgressBar 控件

               

**ProgressBar** 控件通过从左到右用一些方块填充矩形来表示一个较长操作的进度。

**语法**

**ProgressBar**

**说明**

- **ProgressBar** 控件监视操作完成的进度。

**ProgressBar** 控件有一个行程和一个当前位置。行程代表该操作的整个持续时间。当前位置则代表应用程序在完成该操作过程时的进度。**Max** 和 **Min** 属性设置了行程的界限。**Value** 属性则指明了在行程范围内的当前位置。由于使用方块来填充控件，因此所填充的数量只能是接近于 **Value** 属性的当前设置值。基于控件的大小，**Value** 属性决定何时显示下一个方块。

**ProgressBar** 控件的 **Height** 属性和 **Width** 属性决定所填充控件的方块的数量和大小。方块数量越多，控件就越能精确地描述操作进度。为了增加显示方块的数量，需要减少控件的 **Height** 或者增加其 **Width。BorderStyle** 属性的设置值同样影响方块的数量和大小。为了适应边框要求，方块的大小要更小一点。

可以用 **ProgressBar** 控件的 **Align** 属性把它自动定位在窗体的顶部或底部。

**提示** 缩小方块的大小直到其所表示的进度增加量与实际进度值达到最接近的匹配程度，应使 **ProgressBar** 控件的宽度至少是其长度的 13 倍。

下面的示例说明如何用一个名为 ProgressBar1 的 **ProgressBar** 控件，来表示对一个大数组冗长的操作进度。把一个 **CommandButton** 控件和一个 **ProgressBar** 控件放在同一窗体里。示例代码中的 **Align** 属性把 **ProgressBar** 控件定位在沿着窗体的底部。该 **ProgressBar** 不显示任何文本。

    Private Sub Command1_Click()
       Dim Counter As Integer
       Dim Workarea(250) As String
       ProgressBar1.Min = LBound(Workarea)
       ProgressBar1.Max = UBound(Workarea)
       ProgressBar1.Visible = True

    '设置进度的值为 Min。
       ProgressBar1.Value = ProgressBar1.Min

    '在整个数组中循环。
       For Counter = LBound(Workarea) To UBound(Workarea)
          '设置数组中每项的初始值。
          Workarea(Counter) = "Initial value" & Counter
          ProgressBar1.Value = Counter
       Next Counter
       ProgressBar1.Visible = False
       ProgressBar1.Value = ProgressBar1.Min
    End Sub

    Private Sub Form_Load()
       ProgressBar1.Align = vbAlignBottom
       ProgressBar1.Visible = False
       Command1.Caption = "Initialize array"
    End Sub

**发行注意** **ProgressBar** 控件是 ActiveX 控件组的组成，该控件组可以在文件MSCOMCTL.OCX 中找到。要在应用程序中使用 **ProgressBar** 控件，必须把MSCOMCTL.OCX 文件加到该工程中。在发行应用程序时，应把文件MSCOMCTL.OCX 安装到 Microsoft Windows 的 System 目录或者 System32 目录下。关于如何把一个 ActiveX 控件加到工程中去的详细信息，请参阅《程序员指南》。
