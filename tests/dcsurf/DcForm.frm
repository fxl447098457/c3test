VERSION 5.00
Begin VB.Form DcForm 
   BackColor       =   &H00C0C0C0&
   Caption         =   "DcSurf"
   ClientHeight    =   2600
   ClientLeft      =   0
   ClientTop       =   0
   ClientWidth     =   5200
   ScaleMode       =   3  'Pixel
   Begin VB.PictureBox picA 
      BackColor       =   &H000000FF&
      FontName        =   "MS Sans Serif"
      FontSize        =   18
      Height          =   1200
      Left            =   120
      ScaleMode       =   3  'Pixel
      Top             =   240
      Width           =   1500
   End
   Begin VB.PictureBox picB 
      BackColor       =   &H00FF0000&
      Height          =   1200
      Left            =   1800
      ScaleMode       =   3  'Pixel
      Top             =   240
      Width           =   1500
   End
   Begin VB.PictureBox picTwip 
      BackColor       =   &H0000FF00&
      Height          =   1200
      Left            =   3400
      ScaleMode       =   1  'Twips
      Top             =   240
      Width           =   1500
   End
   Begin VB.Timer tmr 
      Interval        =   150
      Left            =   120
      Top             =   1600
   End
End
Attribute VB_Name = "DcForm"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit
' 账 #196: 控件的绘图面。DS 那一组钉 `.hDC`（RTL 里「这枚控件的绘图 DC 从哪儿来」早就有一处口径，
' Print/Cls 走它，缺的是「句柄交得回 VB 代码」那一半 —— 真工程那一形 `With Picture1 : TextOut .hDC`
' 只能撞 cgen_expr_with.cpp 的 "hwnd.成员" 兜底 = C2039，物证 Charts 2020/ucTreeMaps 的 PropPagFMR.pag:258）；
' TH 那一组钉按 HWND 的 TextHeight / TextWidth（同账欠的第二条，同一枚 Picture1 的 pag:265
' `.CurrentY + .TextHeight(Text)`，物证是 ucTreeMaps 现在只剩这一条 C2039）。
' DS01 同一枚反复读 + With 那一形三个数彼此相等且非零（没缓存会每次换个句柄；没出口全是 0）
' DS02 两枚互不相等（出口写成全局一份当场红，同 #192 那条双向钉）
' DS03 GetDeviceCaps(hDC, LOGPIXELSX) > 0 —— 问的是 GDI：交回来是一张活的 DC，不是数字或野句柄
' DS04 GetPixel 各自等于**自己那枚**的设计期底色 —— 证明这张 DC 指的是这一枚控件的表面
' TH01 反复读 + With 那一形逐数相等且 > 0（两形同归一处出口；只接一头的话这里一头是 0 一头是数）
' TH02 单位：同尺寸同字体的缇框与像素框量同一串字，缇框那个数必须**明显大于**像素框那个
'      （1 像素 = 1440/DPI 缇，96dpi 是 15 倍、240dpi 还有 6 倍 ⇒ 只钉「>4 倍」这一个下界，
'      不钉绝对数也不钉比值本身，DPI 变了不假红；没折算的话这里恒为 1 倍，账 #177 记的正是这一味）
' TH03 字体：运行期把 picB 的字号换大 ⇒ 同一枚再量一次必须跟着变大 —— 这一问只钉「量的是不是
'      这枚窗口现在在用的字体」（同一条 setter / WM_SETFONT，Fix 129 在 UserControl 那族栽的
'      正是用屏幕默认字体量）。设计期那枚 18pt 只作为读数打出来（TH06/TH07），它归 #154 那条路。
' TH04 宽度随文字变：长串 > 短串 > 0（拦住"什么都不量、恒答一个常数"那种假绿）
' 窗口问题在窗口活着的时候问：探针在 Timer 第一拍（那时 WM_PAINT 已经把底色刷上去了）。
Private Declare Function GetDeviceCaps Lib "gdi32" (ByVal hdc As LongPtr, ByVal nIndex As Long) As Long
Private Declare Function GetPixel Lib "gdi32" (ByVal hdc As LongPtr, ByVal X As Long, ByVal Y As Long) As Long

Private Sub tmr_Timer()
    Dim hA As LongPtr, hA2 As LongPtr, hW As LongPtr, hB As LongPtr
    Dim ok1 As Boolean, ok2 As Boolean, ok3 As Boolean, ok4 As Boolean
    Dim px1 As Long, px2 As Long, caps As Long
    Dim tA As Single, tA2 As Single, tW As Single, tB As Single, tT As Single, tB2 As Single
    Dim wLong As Single, wShort As Single
    Dim pfA As Long, pfB As Long
    Dim ok5 As Boolean, ok6 As Boolean, ok7 As Boolean, ok8 As Boolean

    hA = picA.hDC
    hA2 = picA.hDC
    With picA
        hW = .hDC
    End With
    hB = picB.hDC

    ok1 = (hA <> 0) And (hA = hA2) And (hA = hW)
    ok2 = (hB <> 0) And (hA <> hB)
    caps = GetDeviceCaps(hA, 88)                 ' LOGPIXELSX
    ok3 = (caps > 0) And (GetDeviceCaps(hB, 88) > 0)
    px1 = GetPixel(hA, 6, 6)
    px2 = GetPixel(hB, 6, 6)
    ok4 = (px1 = 255) And (px2 = 16711680)       ' 红 / 蓝，各自自己的底色

    tA = picA.TextHeight("Xg")
    tA2 = picA.TextHeight("Xg")
    With picA
        tW = .TextHeight("Xg")
    End With
    tB = picB.TextHeight("Xg")
    tT = picTwip.TextHeight("Xg")
    wLong = picB.TextWidth("WWWWWW")
    wShort = picB.TextWidth("W")
    ' TH03 钉的是「量的到底是不是这枚窗口现在在用的字体」那一头。账 #200 之前这一问两头都哑：
    ' 设计期 18pt 的 picA 与运行期改成 20pt 的 picB 都量 16（TH06 的 a / b / b2 三格就是那组读数），
    ' 那时它只留读数不当判据。#200 抓到的是 STATIC 这一类窗口压根不答 WM_GETFONT，修法是字体只从
    ' 一处出口问 + 自己存一份；于是这条升回判据：tA > tB（设计期大字号的那个量得更大）、tB2 > tB
    ' （运行期改字号真的跟着走）、pfA >= 18（问窗口的那位证人 FontPixelHeight 也答得出数 —— 改前
    ' 它拿到 NULL 就 return 0，整条 Debug.Print 一行都不打）。
    picB.FontSize = 20
    tB2 = picB.TextHeight("Xg")
    pfA = picA.FontPixelHeight
    pfB = picB.FontPixelHeight

    ok5 = (tA > 0) And (tA = tA2) And (tA = tW)              ' 两形同归一处
    ok6 = (tT > 4 * tB)                                      ' 同字体: 缇框那个数明显大于像素框那个
    ok7 = (tA > tB) And (tB2 > tB) And (pfA >= 18)
    ok8 = (wLong > wShort) And (wShort > 0) And (tB > 0)

    Debug.Print "DS01-SAME=" & TF(ok1)
    Debug.Print "DS02-SEP=" & TF(ok2)
    Debug.Print "DS03-LIVE=" & TF(ok3)
    Debug.Print "DS04-PIXEL=" & TF(ok4)
    Debug.Print "DS05-RAW dpi=" & CStr(caps) & " a=" & CStr(px1) & " b=" & CStr(px2)
    Debug.Print "TH01-TWOFORMS=" & TF(ok5)
    Debug.Print "TH02-UNITS=" & TF(ok6)
    Debug.Print "TH03-FONT=" & TF(ok7)
    Debug.Print "TH04-WIDTH=" & TF(ok8)
    Debug.Print "TH05-RAW px=" & CStr(tB) & " twip=" & CStr(tT) & " wS=" & CStr(wShort) & " wL=" & CStr(wLong)
    Debug.Print "TH06-FONTRAW a=" & CStr(tA) & " b=" & CStr(tB) & " b2=" & CStr(tB2) & " fsA=" & CStr(picA.FontSize) & " pfA=" & CStr(pfA) & " pfB=" & CStr(pfB)
    Debug.Print "DS-DONE"
    Unload Me
End Sub

Private Function TF(b As Boolean) As String
    If b Then TF = "True" Else TF = "False"
End Function
