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

' C29-SL-c: 三条计数针（两条 Change + 一条 Scroll）。事件断言一律**只数增量**，
' 不拿绝对值比 —— 原生自发的那几条什么时候到、到几条，不由我们定。
Private gChg1 As Long
Private gScr1 As Long
Private gChg2 As Long

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
    ' ---- C29-SL-c: 事件面（Change / Scroll 走 WM_HSCROLL / WM_VSCROLL 那一条通道）----
    ' SimNotify 是**判据专用**助手（与 DT-c 的 SimChange / MV-c 的 SimDateClick / RT-d 的
    ' SimNotify 同先例）：无头环境点不了鼠标，而直接调 handler 会绕开整条派发链 ——
    ' 它先把值推到 pos（真手势都是控件先动、再发通知），再按原生那一档发一条**真**消息进父窗。
    ' 码值/通道/wParam 布局全部实测（探针 .build/slprobe/slmeasure7.c 与 8.c）：
    '   横杆发 WM_HSCROLL、竖杆发 WM_VSCROLL；LOWORD = TB_* 码，5/4 那两档的值在 HIWORD；
    '   一次真拖 = 5×N → 4 → 8；方向键 = 0 → 8；程序化 SETPOS/SETRANGE 一条都不发。
    Dim b1 As Long, b2 As Long, b3 As Long
    sld1.Value = 20                 ' 先把基准摆明（程序化赋值不发通知，SC10 钉这条）
    b1 = gChg1: b2 = gScr1: b3 = gChg2
    Debug.Print "SC0=" & b1 & "/" & b2 & "/" & b3
    sld1.SimNotify(5, 40)           ' TB_THUMBTRACK：值 20→40 ⇒ Change 一次；滑块那两档 ⇒ Scroll 一次
    Debug.Print "SC1-track=" & TF(gChg1 - b1 = 1 And gScr1 - b2 = 1)
    sld1.SimNotify(5, 40)           ' 同值再来一条（拖动不足一像素的那种）：Scroll 涨、Change 不涨
    Debug.Print "SC2-same=" & TF(gChg1 - b1 = 1 And gScr1 - b2 = 2)
    sld1.SimNotify(5, 55)           ' 拖拽中连续触发就是这一串 5 ⇒ Change 跟着涨到 2
    Debug.Print "SC3-next=" & TF(gChg1 - b1 = 2 And gScr1 - b2 = 3)
    sld1.SimNotify(4, 55)           ' TB_THUMBPOSITION（落点与当前同值）⇒ 只算 Scroll
    Debug.Print "SC4-posit=" & TF(gChg1 - b1 = 2 And gScr1 - b2 = 4)
    sld1.SimNotify(8, 55)           ' TB_ENDTRACK 是"收尾"那条，不带新值 ⇒ 两条都不许点
    Debug.Print "SC5-endtrk=" & TF(gChg1 - b1 = 2 And gScr1 - b2 = 4)
    sld1.SimNotify(0, 54)           ' TB_LINEUP（方向键那一档）：值动了 ⇒ Change；不是滑块档 ⇒ Scroll 不涨
    Debug.Print "SC6-line=" & TF(gChg1 - b1 = 3 And gScr1 - b2 = 4)
    Debug.Print "SC7-value=" & CStr(sld1.Value)
    sld2.SimNotify(5, 30)           ' 竖杆：同一条 case、发的是 WM_VSCROLL（sld2 只挂 Change）
    Debug.Print "SC8-vert=" & TF(gChg2 - b3 = 1)
    Debug.Print "SC9-isolate=" & TF(gChg1 - b1 = 3 And gScr1 - b2 = 4)
    sld1.Value = 66                 ' 程序化赋值**不发**通知（实测；DT41 同型的哨兵，哪天齐平就得翻红）
    Debug.Print "SC10-setval=" & TF(gChg1 - b1 = 3 And gScr1 - b2 = 4)
    sld1.SimNotify(5, 66)           ' SetValue 已把基准推到 66 ⇒ 这条同值通知不该被当成"变了"
    Debug.Print "SC11-baseline=" & TF(gChg1 - b1 = 3 And gScr1 - b2 = 5)
    Debug.Print "SLIDER-DONE"
    Unload Me
End Sub

' VB6 这两条处理器都没有参数（与 DT-c 那三条同形），所以生成的回调形参表是空的。
Private Sub sld1_Change()
    gChg1 = gChg1 + 1
End Sub

Private Sub sld1_Scroll()
    gScr1 = gScr1 + 1
End Sub

Private Sub sld2_Change()
    gChg2 = gChg2 + 1
End Sub
