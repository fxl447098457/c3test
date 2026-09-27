VERSION 5.00
Begin VB.Form RtfForm 
   Caption         =   "RtfForm"
   ClientHeight    =   4800
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   9000
   LinkTopic       =   "RtfForm"
   ScaleHeight     =   4800
   ScaleWidth      =   9000
   Begin RichTextLib.RichTextBox rt1 
      BorderStyle     =   1 
      Height          =   1800
      Left            =   120
      ScrollBars      =   3
      TabIndex        =   0
      Text            =   "DesignText-Alpha"
      Top             =   120
      Width           =   4200
      WordWrap        =   0   'False
   End
   Begin RichTextLib.RichTextBox rt2 
      BorderStyle     =   0 
      Height          =   1200
      Left            =   4680
      TabIndex        =   1
      Top             =   120
      Width           =   4200
   End
   Begin RichTextLib.RichTextBox rt3 
      Height          =   1200
      Left            =   120
      ReadOnly        =   -1  'True
      ScrollBars      =   2
      TabIndex        =   2
      Top             =   2160
      Width           =   4200
   End
   Begin RichTextLib.RichTextBox rt4 
      Height          =   1200
      Left            =   4680
      ScrollBars      =   1
      TabIndex        =   3
      Top             =   2160
      Width           =   4200
   End
End
Attribute VB_Name = "RtfForm"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit

' ai/029 C29-RT-a：VB6 RichTextBox 走**原生** Msftedit.dll 的 RICHEDIT50W（不加载
' RICHTX32.OCX —— 它是 32 位 inproc，x64 进程里 CoCreateInstance 直接失败，见 029 §三 D6）。
'
' 本格范围 = 窗口 + 创建样式 + 文本/选区/只读/上限/滚动条/换行面。
' Sel* 的**格式**面（粗斜体下划线删除线、颜色、字体、字号、对齐、缩进）留 RT-b；
' TextRTF + LoadFile/SaveFile + Find 留 RT-c；Change / SelChange 两条事件留 RT-d。
'
' 四枚控件的分工：rt1 = 设计期四位齐全（两种滚动条 + 换行关 + 边框 + 初值文本）、
' rt2 = 全默认（"原生自己是什么"的对照，也是文本/选区那一段的试验台）、
' rt3 = 只垂直滚动条 + 只读、rt4 = 只水平滚动条（横向量程那两条的证人）。
'
' 四条量出来的口径决定了这里为什么是这些针（三轮 C 探针，读数记在 029 §九 本格）：
'  ① **滚动条归控件管，样式位会被它自己抹掉**：创建时给了 WS_VSCROLL，空文本下 GWL_STYLE
'     那两位就没了（不需要滚动 ⇒ 连 bar 带样式位一起拆），灌 60 行又自己回来。所以 cgen 一并
'     挂 ES_DISABLENOSCROLL 让 bar 常驻（RT1..RT4 才谈得上"读回设计期那个值"）；而**事后**
'     SetWindowLong 那两位只有外观、量程停在默认 0..100（内容真高 0..1281）—— 所以这四位
'     没有写口，且判据除了样式位还配一条控件自己的读数：VScrollRange / HScrollRange
'     （GetScrollInfo 的 nMax）。没挂 bar 的那枚读数不会随内容长起来（RT12/RT15 钉它）。
'  ② ReadOnly 走 EM_SETREADONLY **事后有效** —— 与 ① 正相反。同一枚控件里两种属性各有各的
'     "事后行不行"，不许互相外推。
'  ③ WordWrap 原生**没有** Get 对称项（EM_SETTARGETDEVICE 单向）⇒ RTL 自存窗口属性。
'     "没写过"与"写过 True"都读 -1（原生默认开着换行），写 False 读 0 —— 顺带钉住哨兵口径：
'     设计期"未写"不能是 -1（VB6 布尔的 True 就是 -1），本线用 -999。
'  ④ **MaxLength 只管用户键盘输入，管不住 Text = 赋值**：限 20 再塞 30 个字符，控件把上限
'     抬到了文本长度（读数 20 -> 30），EM_REPLACESEL 同样穿过去。所以 RT27 钉的是"赋值会突破
'     上限"这条原生行为（VB6 的 OCX 在这里是截断），不是假装它钳得住。默认读数折 0 = "不限"
'     （原生默认上限实测就是 32767，不是天文数字）。

Private Function TF(ByVal ok As Boolean) As String
    If ok Then TF = "Y" Else TF = "N"
End Function

Private Function Rep(ByVal s As String, ByVal n As Long) As String
    Dim r As String, i As Long
    r = ""
    For i = 1 To n
        r = r & s
    Next i
    Rep = r
End Function

Private Sub Form_Load()
    Dim big As String
    Dim oneLong As String
    Dim i As Long
    Dim s1 As Long, s2 As Long, s3 As Long, s4 As Long
    Dim v2 As Long, v3 As Long, h2 As Long, h4 As Long
    Dim L1 As Long, L2 As Long

    ' --- 1..4 创建样式：四枚各一种设计期组合，读回各自那一位（枚举照 VB6 文档：0 无/1 水平/2 垂直/3 两者）---
    s1 = rt1.ScrollBars: s2 = rt2.ScrollBars: s3 = rt3.ScrollBars: s4 = rt4.ScrollBars
    Debug.Print "RT1=" & TF(rt1.ScrollBars = 3)
    Debug.Print "RT2=" & TF(rt2.ScrollBars = 0)
    Debug.Print "RT3=" & TF(rt3.ScrollBars = 2 And rt4.ScrollBars = 1)
    ' 方向针：1=水平、2=垂直 不许反过来（TextBox 那一格正是反的 —— 同一条枚举、两套口径）
    Debug.Print "RT4=" & TF(rt4.ScrollBars <> 2 And rt3.ScrollBars <> 1)

    ' --- 5..6 边框（通用那条 WS_EX_CLIENTEDGE 路），两枚各自算自己的 ---
    Debug.Print "RT5=" & TF(rt1.BorderStyle = 1)
    Debug.Print "RT6=" & TF(rt2.BorderStyle = 0 And rt3.BorderStyle = 0)

    ' --- 7..8 自动换行：写 False 的那枚读 0，没写过的那枚读 -1（原生默认开着）---
    Debug.Print "RT7=" & TF(rt1.WordWrap = 0)
    Debug.Print "RT8=" & TF(rt2.WordWrap = -1 And rt3.WordWrap = -1)

    ' --- 9 设计期 ReadOnly 到了控件（走 Init 那条），没写的两枚仍可编辑 ---
    Debug.Print "RT9=" & TF(rt3.ReadOnly = -1 And rt2.ReadOnly = 0)

    ' --- 10 设计期初值文本（.frm 的 Text = "..."，句柄赋值之后才发）---
    Debug.Print "RT10=" & TF(rt1.Text = "DesignText-Alpha")

    ' --- 11..13 垂直量程：挂 bar 的两枚随内容长起来，没挂的那枚停在默认那一档 ---
    big = ""
    For i = 1 To 60
        big = big & "line " & i & " xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" & vbCrLf
    Next i
    rt1.Text = big
    rt2.Text = big
    rt3.Text = big
    v3 = rt3.VScrollRange
    v2 = rt2.VScrollRange
    Debug.Print "RT11=" & TF(v3 > 100)
    Debug.Print "RT12=" & TF(v2 <= 100)
    Debug.Print "RT13=" & TF(rt1.VScrollRange > 100)

    ' --- 14..15 水平量程：关掉换行、一行长文本才撑得出横向量程；没挂横向 bar 的那枚不动 ---
    oneLong = "A" & Rep("bcd", 400)
    rt4.WordWrap = False
    rt4.Text = oneLong
    rt2.Text = oneLong
    h4 = rt4.HScrollRange
    h2 = rt2.HScrollRange
    Debug.Print "RT14=" & TF(h4 > 100)
    Debug.Print "RT15=" & TF(h2 <= 100)

    ' --- 16 WordWrap 运行期可逆（写下去读得回）---
    rt3.WordWrap = False
    Debug.Print "RT16=" & TF(rt3.WordWrap = 0)
    rt3.WordWrap = True
    Debug.Print "RT17=" & TF(rt3.WordWrap = -1 And rt1.WordWrap = 0)

    ' --- 18..20 选区：起点、长度、读回的文本；空选区读 SelText 是空串 ---
    rt2.Text = "0123456789ABCDEFGHIJ"
    rt2.SelStart = 4
    rt2.SelLength = 6
    Debug.Print "RT18=" & TF(rt2.SelStart = 4 And rt2.SelLength = 6)
    Debug.Print "RT19=" & TF(rt2.SelText = "456789")
    rt2.SelLength = 0
    Debug.Print "RT20=" & TF(rt2.SelText = "")

    ' --- 21..22 越界夹取：起点夹到文末、长度夹到剩余（原生 EM_SETSEL 自己夹，RTL 再夹一次）---
    L1 = Len(rt2.Text)
    rt2.SelStart = 99999
    Debug.Print "RT21=" & TF(rt2.SelStart = L1 And rt2.SelLength = 0)
    rt2.SelStart = L1 - 3
    rt2.SelLength = 99999
    Debug.Print "RT22=" & TF(rt2.SelLength = 3)

    ' --- 23..24 写 SelText：有选区是替换，空选区是插入 ---
    rt2.Text = "0123456789ABCDEFGHIJ"
    rt2.SelStart = 2
    rt2.SelLength = 4
    rt2.SelText = "XY"
    Debug.Print "RT23=" & TF(rt2.Text = "01XY6789ABCDEFGHIJ")
    rt2.Text = "0123456789ABCDEFGHIJ"
    rt2.SelStart = 5
    rt2.SelLength = 0
    rt2.SelText = "Q"
    Debug.Print "RT24=" & TF(rt2.Text = "01234Q56789ABCDEFGHIJ")

    ' --- 25..26 ReadOnly 运行期可逆，且不改别人的状态 ---
    rt2.ReadOnly = True
    Debug.Print "RT25=" & TF(rt2.ReadOnly = -1 And rt1.ReadOnly = 0)
    rt2.ReadOnly = False
    Debug.Print "RT26=" & TF(rt2.ReadOnly = 0)

    ' --- 27..30 MaxLength：默认 0 = 不限、写短文本时读得回、赋值会突破上限、改回 0 ---
    Debug.Print "RT27=" & TF(rt1.MaxLength = 0)
    rt2.Text = "short"
    rt2.MaxLength = 20
    Debug.Print "RT28=" & TF(rt2.MaxLength = 20)
    rt2.Text = Rep("z", 30)
    L2 = rt2.MaxLength
    Debug.Print "RT29=" & TF(L2 = 30 And Len(rt2.Text) = 30)
    rt2.MaxLength = 0
    Debug.Print "RT30=" & TF(rt2.MaxLength = 0 And Len(rt2.Text) = 30)

    ' --- 31 上限是每枚控件各自的：rt2 改过，rt1 不许跟着变 ---
    Debug.Print "RT31=" & TF(rt1.MaxLength = 0 And rt1.VScrollRange > 100)

    ' --- 32 通用属性面 + 两枚不串台（无头环境不问 Visible）---
    rt4.Enabled = False
    Debug.Print "RT32=" & TF(rt4.Enabled = False And rt3.Enabled = -1)

    ' 原始读数打在针之外：四位样式 / 边框 / 四个量程 / 上限被抬后的读数 / 长度
    Debug.Print "W=" & s1 & "/" & s2 & "/" & s3 & "/" & s4
    Debug.Print "S=" & v3 & "/" & v2 & "/" & rt1.VScrollRange & "/" & h4 & "/" & h2
    Debug.Print "L=" & L1 & "/" & L2 & "/" & Len(rt2.Text) & "/" & rt1.MaxLength
    Debug.Print "B=" & rt1.BorderStyle & "/" & rt2.BorderStyle & "/" & rt3.BorderStyle
    Debug.Print "CTRLRICHTEXT-DONE"
    Unload Me
End Sub
