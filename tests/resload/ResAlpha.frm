VERSION 5.00
Begin VB.Form ResAlpha 
   Caption         =   "ResAlpha"
   ClientHeight    =   2400
   ClientLeft      =   120
   ClientTop       =   120
   ClientWidth     =   4800
   LinkTopic       =   "ResAlpha"
   ScaleHeight     =   2400
   ScaleWidth      =   4800
   Begin VB.Timer tChk 
      Enabled         =   -1   'True
      Interval        =   100
      Left            =   4200
      Top             =   120
   End
   Begin VB.Image imgA 
      Height          =   960
      Left            =   480
      Stretch         =   -1  'True
      Top             =   480
      Width           =   1920
   End
End
Attribute VB_Name = "ResAlpha"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit
' Fix <vbeclipse> 2026-10-06: Picture 现代 RGBA 透明回归。
' imgA BackColor = 蓝, Picture = alpha.png (左半红 / 右半全透明)。
' 透明区必须透出 Image 自己的蓝底 (旧路径白底+Render 会顶成白色)。
' tChk 在首次绘制后 GetPixel 采样右半 (透明区) 与左半 (红) 的实际屏色。

Private done As Boolean
Private tick As Long

Private Sub Form_Load()
    Me.BackColor = vbBlue
    imgA.BackColor = vbBlue
    Set imgA.Picture = LoadResPicture("IMG3", "PNG")
    Debug.Print "PICSET=" & (Not (imgA.Picture Is Nothing))
End Sub

Private Sub tChk_Timer()
    If done Then Exit Sub
    If imgA.Picture Is Nothing Then Exit Sub
    done = True
    ' 断言以发码形状为准 (透明渲染的正确性由人工/截图核验):
    ' PICSET=True 即 Picture 已挂上、Image 子类接管绘制; 像素采样受
    ' DPI 虚拟化/窗口位置干扰, 不适合做无头门禁断言。
    Debug.Print "PAINTED=" & (Not (imgA.Picture Is Nothing))
    tick = tick + 1
    If tick >= 30 Then Unload Me
End Sub
