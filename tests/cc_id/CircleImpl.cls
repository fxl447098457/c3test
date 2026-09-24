VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "CircleImpl"
Option Explicit

' ai/022 B11/C02: the class CCCircle binds to. Its body is irrelevant to identity resolution
' (the contract-aggregation check that would look inside it is C03b, not C03a).

Private m_mode As Long

Public Sub Move(ByVal dx As Long, ByVal dy As Long)
    m_mode = dx + dy
End Sub

Public Sub Paint(ByVal mode As Long)
    m_mode = mode
End Sub
