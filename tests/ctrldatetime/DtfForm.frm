VERSION 5.00
Begin VB.Form DtfForm 
   Caption         =   "DtfForm"
   ClientHeight    =   3600
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   6000
   LinkTopic       =   "DtfForm"
   ScaleHeight     =   3600
   ScaleWidth      =   6000
   Begin MSComCtl2.DTPicker dt1 
      CheckBox        =   -1  'True
      Format          =   1
      Height          =   375
      Left            =   240
      TabIndex        =   0
      Top             =   240
      Width           =   2400
   End
   Begin MSComCtl2.DTPicker dt2 
      Height          =   375
      Left            =   240
      TabIndex        =   1
      Top             =   720
      Width           =   2400
   End
   Begin MSComCtl2.DTPicker dt3 
      Format          =   2
      Height          =   375
      Left            =   240
      TabIndex        =   2
      Top             =   1200
      UpDown          =   -1  'True
      Width           =   2400
   End
   Begin MSComCtl2.DTPicker dt4 
      CustomFormat    =   "yyyy-MM-dd HH:mm"
      Format          =   3
      Height          =   375
      Left            =   240
      TabIndex        =   3
      Top             =   1680
      Width           =   2400
   End
End
Attribute VB_Name = "DtfForm"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit

' ai/029 C29-DT-a：VB6 DTPicker 走**原生** SysDateTimePick32（不加载 MSCOMCT2.OCX —— 它是
' 32 位 inproc，x64 进程里 CoCreateInstance 直接失败，见 029 §三 D6）。
'
' 本格只管"窗口 + 样式 + 标量属性面"，Date 型那三格（Value / MinDate / MaxDate）留 DT-b。
'
' 两条量出来的硬事实，判据的形状就是被它们决定的：
'  ① **CheckBox / UpDown 只能在创建时给**（它们是复选框/微调按钮那两枚子窗口的创建参数）。
'     事后 SetWindowLong 写 DTS_SHOWNONE / DTS_UPDOWN，控件立刻把这两位抹回去 —— DT11 就是
'     钉这条边界的针（写完读回 0、宽度不动）。谁做出真运行期切换，这条必须翻红。
'     Format 不一样：切档运行期**有效**（DT9/DT10 用控件自己算的宽度证它真的重算了）。
'     三条仍然一律从 .frm 立进创建样式，因为设计期就该在创建时就对。
'  ② 样式位的 getter 只证明"写进去了"，不证明"控件按这档在画" —— 因为 SDK 常数
'     DTS_TIMEFORMAT = 0x0009 自带 bit0（就是 DTS_UPDOWN 那位），只对自己的掩码读数会
'     自洽地假绿。所以 DT6-DT10 全部配**控件侧**读数：DTM_GETIDEALSIZE（问控件自己算的
'     "装得下当前格式"的宽度），长日期明显比时间宽，宽度对上了才叫格式真选中。
'
' Date 型那三格（Value / MinDate / MaxDate）与 DTN_* 事件面都不在这一格里。

Private Function TF(ByVal ok As Boolean) As String
    If ok Then TF = "Y" Else TF = "N"
End Function

Private Sub Form_Load()
    Dim bkg As Long, txt As Long, trl As Long, tbk As Long, ttx As Long
    Dim wLong As Long, wTime As Long, w0 As Long, w1 As Long
    Dim dReq As Date
    ' 五个色值刻意取"三字节都不同且都很小"的组合：系统默认色是 0xFFFFFF / 0 / 0x808080
    ' 那一族，任何一个恰好撞上都会让 DT20 那条"控件之间不共享"变成假绿或假红。
    bkg = 66051: txt = 263430: trl = 460809: tbk = 658188: ttx = 855567

    ' --- 1..5 创建样式进了窗口（三枚各一种设计期组合） ---
    Debug.Print "DT1=" & TF(dt1.Format = 1)
    Debug.Print "DT2=" & TF(dt1.CheckBox = -1)
    Debug.Print "DT3=" & TF(dt2.Format = 0 And dt2.CheckBox = 0 And dt2.UpDown = 0)
    Debug.Print "DT4=" & TF(dt3.UpDown = -1)
    Debug.Print "DT5=" & TF(dt3.Format = 2 And dt3.CheckBox = 0)

    ' --- 6..8 控件侧读数：长日期比时间宽，且三枚各自算各自的 ---
    wLong = dt1.IdealWidth
    wTime = dt3.IdealWidth
    Debug.Print "DT6=" & TF(wLong > 0 And wTime > 0)
    Debug.Print "DT7=" & TF(wLong > wTime)
    Debug.Print "DT8=" & TF(dt2.IdealWidth > 0 And dt2.IdealWidth <> wLong And dt2.IdealWidth <> wTime)

    ' --- 9..11 运行期切档 vs 运行期改 CheckBox：一个真生效、一个不落地（都量过） ---
    ' 原始读数（本机，W 那条）：dt1 长日期+复选框 = 143，dt3 时间 = 64，
    ' dt2 创建时短日期 = 95，运行期切成长日期 = 121。
    ' 121 落在"长日期但不带复选框"那一档 ⇒ Format 这一族**运行期切得动**（样式位 + 控件
    ' 自己算的宽度一起动），而下面 DT11 的 CheckBox 写完读回 0、宽度纹丝不动 ——
    ' 那两位是子窗口（复选框/微调按钮）的创建参数，控件只在 WM_CREATE 时读一次。
    w0 = dt2.IdealWidth
    dt2.Format = 1
    w1 = dt2.IdealWidth
    Debug.Print "DT9=" & TF(dt2.Format = 1 And w1 > w0)
    Debug.Print "DT10=" & TF(w1 > wTime And w1 < wLong)
    dt2.CheckBox = True
    Debug.Print "DT11=" & TF(dt2.CheckBox = 0 And dt2.IdealWidth = w1)
    ' 原始宽度打在针之外：长/时间/切档前/切档后，四条数字一起进日志，判据红了好对
    Debug.Print "W=" & wLong & "/" & wTime & "/" & w0 & "/" & w1

    ' --- 12..14 自定义格式（DTM_SETFORMATW 是真运行期消息；串自存，原生没有 Get 对称项） ---
    dt2.CustomFormat = "yyyy-MM-dd"
    Debug.Print "DT12=" & TF(dt2.CustomFormat = "yyyy-MM-dd")
    Debug.Print "DT13=" & TF(dt2.Format = 3)
    dt2.Format = 0
    Debug.Print "DT14=" & TF(dt2.CustomFormat = "" And dt2.Format = 0)

    ' --- 15..19 下拉月历五色：逐格写、逐格读回自己那格 ---
    dt2.CalendarBackColor = bkg
    dt2.CalendarForeColor = txt
    dt2.CalendarTrailingForeColor = trl
    dt2.CalendarTitleBackColor = tbk
    dt2.CalendarTitleForeColor = ttx
    Debug.Print "DT15=" & TF(dt2.CalendarBackColor = bkg)
    Debug.Print "DT16=" & TF(dt2.CalendarForeColor = txt)
    Debug.Print "DT17=" & TF(dt2.CalendarTrailingForeColor = trl)
    Debug.Print "DT18=" & TF(dt2.CalendarTitleBackColor = tbk)
    Debug.Print "DT19=" & TF(dt2.CalendarTitleForeColor = ttx)

    ' --- 20..21 序号与窗口都不串台：改"标题底色"那一格，另外四格原样；dt1 从没涂过色 ---
    dt2.CalendarTitleBackColor = 1234567
    Debug.Print "DT20=" & TF(dt2.CalendarBackColor = bkg And dt2.CalendarForeColor = txt _
                              And dt2.CalendarTrailingForeColor = trl _
                              And dt2.CalendarTitleForeColor = ttx)
    Debug.Print "DT21=" & TF(dt1.CalendarBackColor <> bkg And dt1.CalendarForeColor <> txt)

    ' --- 22..24 设计期 CustomFormat 那条字符串路（.frm 里带引号 + 当 C 字面量发，两头都得对） ---
    Debug.Print "DT22=" & TF(dt4.Format = 3)
    Debug.Print "DT23=" & TF(dt4.CustomFormat = "yyyy-MM-dd HH:mm")
    Debug.Print "DT24=" & TF(dt4.IdealWidth > w0)

    ' --- 25..36 C29-DT-b：Date 值面（Value / MinDate / MaxDate + "无日期"那一态）---
    ' 换算是 oleaut32 那一对现成函数（VariantTimeToSystemTime / SystemTimeToVariantTime）。
    ' VB 侧 Date 就是 double 序列号，所以 43894 这种整数序列直接当日期用（= 2020-03-04）。
    ' DT30 / DT31 两条按**实测真值**写（原先各猜错过一次，见 029 §九 本格）：
    '   · 设一个低于 MinDate 的日期，控件**直接拒绝、值保持原样**，不是钳到 MinDate；
    '   · DTS_SHOWNONE 那枚**创建时是勾上的**（HasDate=-1、Value=今天），不是未勾。
    Debug.Print "DT25=" & TF(Int(dt2.Value) = Int(Now))
    dReq = 43894
    dt2.Value = dReq
    Debug.Print "DT26=" & TF(CLng(dt2.Value) = 43894)
    dt2.MinDate = 43831
    Debug.Print "DT27=" & TF(CLng(dt2.MinDate) = 43831)
    dt2.MaxDate = 44999
    Debug.Print "DT28=" & TF(CLng(dt2.MaxDate) = 44999)
    ' 改一端必须不动另一端（原生是一张 (min,max) 表 + 有效位标志；只发 GDTR_MIN 会清掉 max）
    Debug.Print "DT29=" & TF(CLng(dt2.MinDate) = 43831)
    dt2.Value = 40000
    Debug.Print "DT30=" & TF(CLng(dt2.Value) = 43894)
    Debug.Print "DT31=" & TF(dt1.HasDate = -1 And Int(dt1.Value) = Int(Now))
    dt1.Value = 43894
    Debug.Print "DT32=" & TF(dt1.HasDate = -1 And CLng(dt1.Value) = 43894)
    dt1.HasDate = False
    ' 未勾这一态原生仍回填一个内部日期（本机 36494）⇒ GetValue 必须认返回标志才回 0
    Debug.Print "DT33=" & TF(dt1.HasDate = 0 And dt1.Value = 0)
    dt1.HasDate = True
    Debug.Print "DT34=" & TF(dt1.HasDate = -1 And CLng(dt1.Value) = 43894)
    Debug.Print "DT35=" & TF(dt3.MinDate = 0 And dt3.MaxDate = 0)
    Debug.Print "DT36=" & TF(CLng(dt3.Value) <> CLng(dt2.Value))

    Debug.Print "CTRLDATETIME-DONE"
    Unload Me
End Sub
