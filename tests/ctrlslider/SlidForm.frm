VERSION 5.00
Begin VB.Form SlidForm 
   Caption         =   "SlidForm"
   ClientHeight    =   2400
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   4200
   LinkTopic       =   "SlidForm"
   ScaleHeight     =   2400
   ScaleWidth      =   4200
   Begin VB.Timer tGo 
      Enabled         =   -1   'True
      Interval        =   60
      Left            =   3720
      Top             =   2040
   End
   Begin MSComctlLib.Slider sld1 
      Height          =   400
      Left            =   120
      TabIndex        =   0
      TickFrequency   =   10
      Top             =   120
      Width           =   2000
      _ExtentX        =   3528
      _ExtentY        =   706
   End
   Begin MSComctlLib.Slider sld2 
      Height          =   1200
      Left            =   2400
      Orientation     =   1
      TabIndex        =   1
      Top             =   120
      Width           =   400
      _ExtentX        =   706
      _ExtentY        =   2117
   End
   Begin MSComctlLib.Slider sld3 
      Height          =   400
      Left            =   120
      TabIndex        =   2
      Top             =   720
      Width           =   2000
      _ExtentX        =   3528
      _ExtentY        =   706
   End
   Begin MSComctlLib.Slider sld4 
      Height          =   400
      LargeChange     =   8
      Left            =   120
      Max             =   100
      Min             =   10
      SelStart        =   20
      SelEnd          =   60
      SelectRange     =   -1   'True
      SmallChange     =   2
      TabIndex        =   3
      TickFrequency   =   5
      Top             =   1200
      Value           =   42
      Width           =   2000
      _ExtentX        =   3528
      _ExtentY        =   706
   End
   Begin MSComctlLib.Slider sld5 
      Height          =   400
      Left            =   120
      TabIndex        =   4
      Top             =   1680
      Width           =   2000
      _ExtentX        =   3528
      _ExtentY        =   706
   End
End
Attribute VB_Name = "SlidForm"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit

' ai/029 C29-SL-a: Slider 的窗口 + 创建样式 + 标量属性面（原生 msctls_trackbar32）。
' 登记之前这枚控件**连窗口都没建**（探针 .build/slprobe 实测：CreateControls 里压根没有它，
' vb6_hwnd_sld1 恒 NULL，属性读回全空、写进去静默丢，退出码照旧 0）。
'
' 判据纪律（本格两条实测口径，见 029 §九 C29-SL-0）：
'   1) 方向**不能拿 channel 矩形当证人** —— 实测 TBM_GETCHANNELRECT 的 rect 永远把行程长度
'      放在 x 分量，水平杆与竖直杆答同一组数（第一发探针因此误判过"运行期改不动"）。
'      正解是 TravelIsVert：把滑块推到量程两端各读一次 TBM_GETTHUMBRECT，看位移落在哪根轴。
'      所以每条 Orientation 断言都配一条 TravelIsVert（SL2+SL4、SL6、SL9 就是这几对）。
'   2) TickFrequency 原生**问不出**（没有 GETTICFREQ；GETTIC 答不出、GETTICPOS 与频率无关），
'      所以它只能自存读回；"控件当前有没有画刻度"另用 TickPresent 证（GETTICPOS(0) != -1）。
'   3) 值面（Min/Max/Value/Small·LargeChange/Sel*）刻意**不在本批** —— 归 SL-b，
'      所以这件夹具一次都不碰那些名字（它们还没登记，碰了就落到"拿 HWND 当 IDispatch"那条兜底）。

Private Function TF(ByVal ok As Boolean) As String
    If ok Then TF = "Y" Else TF = "N"
End Function

Private Sub tGo_Timer()
    tGo.Enabled = False
    Debug.Print "SL1-vis=" & CStr(sld1.Visible)
    Debug.Print "SL2-ori=" & CStr(sld1.Orientation)
    Debug.Print "SL3-freq=" & CStr(sld1.TickFrequency)
    Debug.Print "SL4-travel=" & CStr(sld1.TravelIsVert)
    Debug.Print "SL5-tick=" & CStr(sld1.TickPresent)
    Debug.Print "SL6-vertori=" & CStr(sld2.Orientation) & CStr(sld2.TravelIsVert)
    Debug.Print "SL7-nofreq=" & CStr(sld3.TickFrequency) & CStr(sld3.TickPresent)
    sld3.TickFrequency = 5
    Debug.Print "SL8-freqset=" & CStr(sld3.TickFrequency)
    ' 运行期翻方向: 实测**有效**（写 TBS_VERT + 换帧之后滑块位移轴跟着换）
    sld1.Orientation = 1
    Debug.Print "SL9-flip=" & CStr(sld1.Orientation) & CStr(sld1.TravelIsVert)
    sld1.Orientation = 0
    Debug.Print "SL10-flipback=" & CStr(sld1.Orientation) & CStr(sld1.TravelIsVert)
    sld1.Enabled = False
    Debug.Print "SL11-en=" & CStr(sld1.Enabled) & CStr(sld3.Enabled)
    ' ---- C29-SL-b 值面：sld4 设计期写满、sld5 什么都没写（默认档照原生答，不猜 VB6 文档）----
    Debug.Print "SB1-range=" & CStr(sld4.Min) & "/" & CStr(sld4.Max)
    Debug.Print "SB2-value=" & CStr(sld4.Value)
    ' SB3 是 **Init 参数顺序的证人**：先立 range(10..100) 再立 page=8。顺序反过来就会被
    ' range 那次重算把 page 顶成 18（探针实测），这条读数就会变成 2/18。
    Debug.Print "SB3-changes=" & CStr(sld4.SmallChange) & "/" & CStr(sld4.LargeChange)
    Debug.Print "SB4-sel=" & CStr(sld4.SelStart) & "/" & CStr(sld4.SelEnd) _
        & "/" & CStr(sld4.SelectRange)
    sld4.Value = 500
    Debug.Print "SB5-clampmax=" & CStr(sld4.Value)
    sld4.Value = 5
    Debug.Print "SB6-clampmin=" & CStr(sld4.Value)
    sld4.Max = 40
    Debug.Print "SB7-shrink=" & CStr(sld4.Min) & "/" & CStr(sld4.Max) & "/" & CStr(sld4.Value)
    sld4.SelectRange = False
    Debug.Print "SB8-selrange-off=" & CStr(sld4.SelectRange)
    Debug.Print "SB9-defaults=" & CStr(sld5.Min) & "/" & CStr(sld5.Max) & "/" _
        & CStr(sld5.SmallChange) & "/" & CStr(sld5.LargeChange) & "/" & CStr(sld5.Value) & "/" _
        & CStr(sld5.TickFrequency)
    ' 起点越过终点时两端互相顶（原生不接受反向区段）
    sld5.SelectRange = True
    sld5.SelStart = 30
    sld5.SelEnd = 70
    sld5.SelStart = 80
    Debug.Print "SB10-push=" & CStr(sld5.SelStart) & "/" & CStr(sld5.SelEnd)
    Debug.Print "SLIDER-DONE"
    Unload Me
End Sub
