VERSION 5.00
Begin VB.UserControl ucUnitPix 
   Appearance      =   0  'Flat
   BackColor       =   &H80000005&
   BorderStyle     =   0  'None
   ClientHeight    =   1200
   ClientLeft      =   0
   ClientTop       =   0
   ClientWidth     =   2400
   ScaleHeight     =   1200
   ScaleMode       =   3  'Pixel
   ScaleWidth      =   2400
End
Attribute VB_Name = "ucUnitPix"
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

Public Function HwOk() As String
    ' 账 #159: 这条比较改前发成 vb6_VarCmpLongNe(&vb6_UserControl_hWnd, 0) —— 拿
    ' 8 字节 void* 全局的**地址**当 vb6_VARIANT* 传 (RTL 签名第一形参是
    ' vb6_VARIANT*) ⇒ 读到的是越界垃圾, 恒不勾。现在类型由 kHostPseudoRows 答
    ' LongPtr ⇒ 走 C 直比 (-(vb6_UserControl_hWnd != 0))。
    HwOk = CStr(UserControl.hWnd <> 0)
End Function
Public Function HwVs() As String
    ' 第二头证人: 同一条读法再问一次「hWnd 与 hDC 是不是两个不同的值」。
    ' 只钉 HwOk 一条时, 「两个成员都被读成同一个垃圾」也能蒙过去。
    HwVs = CStr(UserControl.hWnd <> UserControl.hDC)
End Function
