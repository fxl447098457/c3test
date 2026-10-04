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
' 账 #196: 控件的绘图面。RTL 里「这枚控件的绘图 DC 从哪儿来」早就有一处口径 (_Paint 派发期用
' 外层 BeginPaint 挂上的 VB6_PaintDC，否则回落窗口 DC)，Print/Cls 走的就是它 —— 但那句注释
' 说过「绝不能 ReleaseDC」的另一半没人接：**句柄交不回 VB 代码**。真工程那一形
' `With Picture1 : TextOut .hDC, ...` (Charts 2020/ucTreeMaps 的 PropPagFMR.pag:258) 只能撞
' cgen_expr_with.cpp 那条 "hwnd.成员" 兜底 = C2039 "hDC 不是 HWND__ 的成员"。
' 这一刀把 `.hDC` 接上，并照 VB6 的语义做成**一个对象一张**：反复读回同一个句柄，窗口销毁时
' 归还 (不缓存就是每读一次漏一张 GDI 句柄)。Print/Cls 那条不改语义，只是与它共用同一处口径。
' 四头判据，缺一头就是假绿：
'   DS01 同一枚反复读 + With 那一形 —— 三个数彼此相等且非零 (没缓存会每次换个句柄；
'        压根没出口会全是 0)。
'   DS02 两枚控件的句柄互不相等 —— 出口写成全局一份的话当场红 (同 #192 那条双向钉)。
'   DS03 GetDeviceCaps(hDC, LOGPIXELSX) > 0 —— 问的是 GDI：交回来的是一张**活的 DC**，
'        不是一个数字或野句柄。
'   DS04 GetPixel(hDC, 6, 6) 各自等于**自己那枚**的设计期底色 (picA 红、picB 蓝) —— 这才证明
'        这张 DC 指的是这一枚控件的表面，而不是屏幕或别人的窗口。底色取设计期值，
'        不靠运行期赋值，也就不与"谁最后落色"抢次序。
' 窗口问题在窗口活着的时候问：探针在 Timer 第一拍 (那时 WM_PAINT 已经把底色刷上去了)。
Private Declare Function GetDeviceCaps Lib "gdi32" (ByVal hdc As LongPtr, ByVal nIndex As Long) As Long
Private Declare Function GetPixel Lib "gdi32" (ByVal hdc As LongPtr, ByVal X As Long, ByVal Y As Long) As Long

Private Sub tmr_Timer()
    Dim hA As LongPtr, hA2 As LongPtr, hW As LongPtr, hB As LongPtr
    Dim ok1 As Boolean, ok2 As Boolean, ok3 As Boolean, ok4 As Boolean
    Dim px1 As Long, px2 As Long, caps As Long

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
    ok4 = (px1 = 255) And (px2 = 16711680)      ' 红 / 蓝，各自自己的底色

    Debug.Print "DS01-SAME=" & TF(ok1)
    Debug.Print "DS02-SEP=" & TF(ok2)
    Debug.Print "DS03-LIVE=" & TF(ok3)
    Debug.Print "DS04-PIXEL=" & TF(ok4)
    Debug.Print "DS05-RAW dpi=" & CStr(caps) & " a=" & CStr(px1) & " b=" & CStr(px2)
    Debug.Print "DS-DONE"
    Unload Me
End Sub

Private Function TF(b As Boolean) As String
    If b Then TF = "True" Else TF = "False"
End Function
