VERSION 5.00
Begin VB.Form MvfForm 
   Caption         =   "MvfForm"
   ClientHeight    =   4800
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   9000
   LinkTopic       =   "MvfForm"
   ScaleHeight     =   4800
   ScaleWidth      =   9000
   Begin MSComCtl2.MonthView mv1 
      Height          =   2400
      Left            =   240
      MaxSelCount     =   7
      MonthColumns    =   2
      MonthRows       =   1
      MultiSelect     =   -1  'True
      TabIndex        =   0
      Top             =   120
      Width           =   6000
   End
   Begin MSComCtl2.MonthView mv2 
      Height          =   1560
      Left            =   240
      TabIndex        =   1
      Top             =   2760
      Width           =   2520
   End
   Begin MSComCtl2.MonthView mv3 
      Height          =   1560
      Left            =   3000
      ShowWeekNumbers =   -1  'True
      TabIndex        =   2
      Top             =   2760
      Width           =   2520
   End
   Begin MSComCtl2.MonthView mv4 
      Height          =   1560
      Left            =   5760
      ShowToday       =   0   'False
      TabIndex        =   3
      Top             =   2760
      Width           =   2520
   End
End
Attribute VB_Name = "MvfForm"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit

' ai/029 C29-MV-a：VB6 MonthView 走**原生** SysMonthCal32（不加载 MSCOMCT2.OCX —— 它是
' 32 位 inproc，x64 进程里 CoCreateInstance 直接失败，见 029 §三 D6）。
'
' 本格只管"窗口 + 样式 + 标量属性面"。Date 型那三格（Value / SelStart / SelEnd）留 MV-b，
' MCN_SELCHANGE(-749) 那条事件（VB6 的 DateClick）留 MV-c。
'
' 三条与 DTPicker 不同的量出来的事实，判据的形状就是被它们决定的：
'  ① **多月平铺不是一个开关，而是"窗口多大"**。原生没有"给几行几列"这条消息 —— 头 6361 行
'     那段注释教的做法是"把 MCM_GETMINREQRECT 的矩形按想要的月数乘大，控件就自己排几个"。
'     所以 MonthRows/MonthColumns 由 RTL 撑矩形落进窗口尺寸，判据一律问
'     MCM_GETCALENDARCOUNT（眼下真画了几个月）—— 这是控件自己的答案，不是我们那张表。
'  ② **ShowToday 与原生 MCS_NOTODAY 是反的**，而且它管的正是 MV-b 要用的那条"today 高亮"。
'     所以这一格不能像 CheckBox 那样按"写下去读回来"了结 —— 读回来对、观感是另一件事，
'     于是每条样式判据都配一条 MCM_GETMINREQRECT 的控件侧尺寸（要不要今天那一行会改高、
'     装不装周号会改宽），尺寸对不上就翻红。
'  ③ MaxSelCount 是**真往返过控件**的一条（MCM_SET/GETMAXSELCOUNT），不像 CustomFormat 那样
'     得自存。而它只在挂了 MCS_MULTISELECT 的控件上写得住：mv2 没那一位，写 9 之后读回还是默认
'     的 1（MV20 钉这条）。于是这格顺手成了"样式位真不真"的第二个控件侧证人 —— 问的是控件有
'     没有按那位办事，不是"我们的掩码读数说我们写进去了"。
'  ④ 与 DTPicker 相反的一条：这三位样式**运行期写是有效的**（MV21 用 MCM_GETMINREQRECT 的高
'     度跟着变来证）。DTS_SHOWNONE 那两位会被控件抹回去，MCS_ 这三位不会 —— 所以 MonthView
'     没有"只能创建时给"这条边界，设计期照旧立进创建参数只是为了让观感从第一帧就对。

Private Function TF(ByVal ok As Boolean) As String
    If ok Then TF = "Y" Else TF = "N"
End Function

Private Sub Form_Load()
    Dim bkg As Long, txt As Long, trl As Long, tbk As Long, ttx As Long, mbk As Long
    Dim w2 As Long, w3 As Long, h2 As Long, h4 As Long
    Dim n2 As Long, n4 As Long

    ' 六个色值刻意取"三字节都不同且都很小"的组合，撞上任一默认色（0xFFFFFF / 0 / 0x808080
    ' 那一族）就会让 MV14 那条"控件之间不共享"变成假绿或假红。
    bkg = 66051: txt = 263430: trl = 460809: tbk = 658188: ttx = 855567: mbk = 250131

    ' --- 1..4 创建样式进了窗口（四枚各一种设计期组合；mv2 是全默认的对照） ---
    Debug.Print "MV1=" & TF(mv1.MultiSelect = -1)
    Debug.Print "MV2=" & TF(mv2.MultiSelect = 0 And mv2.ShowWeekNumbers = 0 And mv2.ShowToday = -1)
    Debug.Print "MV3=" & TF(mv3.ShowWeekNumbers = -1 And mv3.MultiSelect = 0)
    Debug.Print "MV4=" & TF(mv4.ShowToday = 0)

    ' --- 5..6 多月平铺：控件自己报"眼下画了几个月" ---
    Debug.Print "MV5=" & TF(mv1.MonthCount = 2)
    Debug.Print "MV6=" & TF(mv2.MonthCount = 1)

    ' --- 7..10 控件侧尺寸：周号加宽、今天那一行加高（问的是 MCM_GETMINREQRECT） ---
    w2 = mv2.MinReqWidth: w3 = mv3.MinReqWidth
    h2 = mv2.MinReqHeight: h4 = mv4.MinReqHeight
    Debug.Print "MV7=" & TF(w2 > 0 And w3 > w2)
    Debug.Print "MV8=" & TF(h2 > 0 And h2 > h4)
    Debug.Print "MV9=" & TF(mv1.MinReqWidth = w2 And mv1.MinReqHeight = h2)
    Debug.Print "MV10=" & TF(mv4.MinReqWidth > 0)

    ' --- 11..17 六格配色（VB6 真名，MonthBackColor 在官方那页的配色段里就有）：逐格写、逐格读回自己那格 ---
    mv2.BackColor = bkg
    mv2.ForeColor = txt
    mv2.TitleBackColor = tbk
    mv2.TitleForeColor = ttx
    mv2.TrailingForeColor = trl
    mv2.MonthBackColor = mbk
    Debug.Print "MV11=" & TF(mv2.BackColor = bkg)
    Debug.Print "MV12=" & TF(mv2.ForeColor = txt)
    Debug.Print "MV13=" & TF(mv2.TitleBackColor = tbk)
    Debug.Print "MV14=" & TF(mv2.TitleForeColor = ttx And mv2.TrailingForeColor = trl)
    Debug.Print "MV15=" & TF(mv2.MonthBackColor = mbk)
    ' 改一格必须不动另外几格（MCSC_ 那六个序号撞车的话就是这条红）
    mv2.TitleBackColor = 1234567
    Debug.Print "MV16=" & TF(mv2.BackColor = bkg And mv2.ForeColor = txt _
                              And mv2.TitleForeColor = ttx And mv2.TrailingForeColor = trl _
                              And mv2.MonthBackColor = mbk)
    ' 底色是每枚控件各自的，mv1 从没涂过色 ⇒ 五条都不该是上面那些值
    Debug.Print "MV17=" & TF(mv1.BackColor <> bkg And mv1.TitleForeColor <> ttx)

    ' --- 18..20 设计期 MaxSelCount 真到了控件；没那枚样式位的控件**问得出默认值、写不进去** ---
    Debug.Print "MV18=" & TF(mv1.MaxSelCount = 7)
    mv1.MaxSelCount = 3
    Debug.Print "MV19=" & TF(mv1.MaxSelCount = 3)
    n2 = mv2.MaxSelCount
    mv2.MaxSelCount = 9
    n4 = mv2.MaxSelCount
    ' 量出来的口径：原生默认上限是 1，而 MCM_SETMAXSELCOUNT 在**没挂 MCS_MULTISELECT** 的控件上
    ' 直接被拒（写完读回还是 1）。这就是"样式位真不真"的控件侧证据 —— 比 GWL_STYLE 那一圈
    ' 读数硬，因为它问的是控件按没按那位在办事。mv1 有那一位，所以上面 MV19 写 3 就读得出 3。
    Debug.Print "MV20=" & TF(n2 = 1 And n4 = 1)
    ' 原始读数打在针之外：mv2 默认上限 / 事后写 9 之后的读数 / 四个尺寸
    Debug.Print "R=" & n2 & "/" & n4 & " " & w2 & "/" & w3 & "/" & h2 & "/" & h4

    ' --- 21 运行期改样式位：读回来是一回事，控件的尺寸读数跟没跟是另一回事 ---
    mv4.ShowToday = True
    Debug.Print "MV21=" & TF(mv4.ShowToday = -1 And mv4.MinReqHeight > h4)

    ' --- 22 通用属性面 + 两枚不串台 ---
    mv3.Enabled = False
    Debug.Print "MV22=" & TF(mv3.Enabled = False And mv2.Enabled = -1)

    Debug.Print "CTRLMONTHVIEW-DONE"
    Unload Me
End Sub
