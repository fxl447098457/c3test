VERSION 5.00
Begin VB.UserControl ucUnitTwip 
   Appearance      =   0  'Flat
   BackColor       =   &H80000005&
   BorderStyle     =   0  'None
   ClientHeight    =   1200
   ClientLeft      =   0
   ClientTop       =   0
   ClientWidth     =   2400
   ScaleHeight     =   1200
   ScaleMode       =   1  'Twips
   ScaleWidth      =   2400
End
Attribute VB_Name = "ucUnitTwip"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = True
Attribute VB_PredeclaredId = False
Attribute VB_Exposed = False
Option Explicit
' <vbeclipse> 回归夹子 (ve_units): 控件自己的坐标系读数。
' TextWidth/ScaleWidth 必须按 .ctl 声明的 ScaleMode 交出 (账 #175/#177)。

Private Sub UserControl_Initialize()
    Debug.Print "I-MODE=" & UserControl.ScaleMode & " I-TW=" & UserControl.TextWidth("MMMM") & " I-SW=" & UserControl.ScaleWidth
End Sub

Public Function Ctx() As String
    ' 账 #179: 这两个值必须**不随调用方变** —— 容器里调也要读到这一枚控件自己的
    ' ScaleMode/ScaleWidth。改前实测容器里调 uTw.Ctx() 得到 mode=3 (另一枚控件留下的
    ' 残值), 控件自己调得到 mode=1。hWnd 那一族不在这里钉 (那是台账 #159)。
    Ctx = UserControl.ScaleMode & "/" & UserControl.ScaleWidth
End Function
Public Function TW(ByVal s As String) As Long
    TW = UserControl.TextWidth(s)
End Function

Public Function TH(ByVal s As String) As Long
    TH = UserControl.TextHeight(s)
End Function

Public Function SW() As Long
    SW = UserControl.ScaleWidth
End Function
