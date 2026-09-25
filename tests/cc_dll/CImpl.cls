VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "CImpl"
Option Explicit

Implements IProbe

Private m_got As Long

Private Sub Ping(ByVal n As Long)
    m_got = n * 2
End Sub

Private Property Get Got() As Long
    Got = m_got
End Property

' ai/022 B17: 一个**公有成员** —— 接口槽之外的那一半对外面。契约成员是 Private（VB6 语义里
' 接口实现不进类的公有面），所以纯接口宿主在 IDispatch 晚绑定下按名字一个成员也点不到；
' 外部 `CreateObject` + 按名调用的验收（test_p613 那一族）要有东西可点，就得有公有成员。
' 同一个类同时有两个面这件事本身也值得钉住：晚绑定走 desc->methods（这条），
' 早绑定走 IProbe 的契约槽（Ping/Got）。
Public Function Twice(ByVal n As Long) As Long
    Twice = n * 2
End Function
