VERSION 5.00
Begin VB.Form frmUnits 
   ClientHeight    =   3000
   ClientLeft      =   0
   ClientTop       =   0
   ClientWidth     =   6000
   ScaleHeight     =   3000
   ScaleMode       =   1  'Twips
   ScaleWidth      =   6000
   Begin VeUnits.ucUnitTwip uTw 
      Height          =   1200
      Left            =   120
      TabIndex        =   0
      Top             =   120
      Width           =   2400
   End
   Begin VeUnits.ucUnitPix uPix 
      Height          =   1200
      Left            =   120
      TabIndex        =   1
      Top             =   1440
      Width           =   2400
   End
End
Attribute VB_Name = "frmUnits"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit
' 两枚控件同尺寸 (2400 缇 = 160 像素 @96dpi), 只差 .ctl 声明的 ScaleMode。
' 判据写成**同一枚字体量出来的两个数之比**, 于是与 DPI 无关:
'   缇型控件的 TextWidth / ScaleWidth 必须 = 像素型的 x Screen.TwipsPerPixelX
Private Sub Form_Load()
    Dim tpp As Long
    Dim twT As Long, twP As Long
    Dim thT As Long, thP As Long
    Dim swT As Long, swP As Long
    tpp = Screen.TwipsPerPixelX
    twT = uTw.TW("MMMM")
    twP = uPix.TW("MMMM")
    thT = uTw.TH("MMMM")
    thP = uPix.TH("MMMM")
    swT = uTw.SW()
    swP = uPix.SW()
    Debug.Print "U-RAW tw=" & twT & " pix=" & twP & " swT=" & swT & " swP=" & swP
    Debug.Print "U-SW=" & CStr(Abs(swT - swP * tpp) <= 2)
    Debug.Print "U-TW=" & CStr(Abs(twT - twP * tpp) <= 2)
    Debug.Print "U-TH=" & CStr(Abs(thT - thP * tpp) <= 2)
    Debug.Print "U-DONE"
    Unload Me
End Sub
