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

Public Function TW(ByVal s As String) As Long
    TW = UserControl.TextWidth(s)
End Function

Public Function TH(ByVal s As String) As Long
    TH = UserControl.TextHeight(s)
End Function

Public Function SW() As Long
    SW = UserControl.ScaleWidth
End Function
