VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "DemoShape"
Option Explicit

' ai/022 B18 端到端示例（接口那一支）: 实现新式接口的类 = CoClass 块的 [Implementation]。
' 契约成员按 VB6 惯例是 Private（不进类的默认面），另有一个**公有成员**给外部晚绑定客户点。
Implements IDemoShape

Private m_x As Double

Private Sub Move(ByVal dx As Double)
    m_x = m_x + dx
End Sub

Private Function Area() As Double
    Area = m_x * 2#
End Function

Private Property Get Label() As String
    Label = "shape"
End Property

Public Function Twice(ByVal n As Long) As Long
    Twice = n * 2
End Function
