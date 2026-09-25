VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "DemoBase"
Option Explicit

' ai/022 B18 端到端示例（继承那一支）: 基类 —— Protected 成员（家族内可用）+ Overridable 虚成员。
Protected m_shift As Double
Private m_hits As Long

Public Overridable Function Speak() As String
    Speak = "base"
End Function

Protected Sub SetShift(ByVal v As Double)
    m_shift = v
End Sub

Protected Function Shift() As Double
    Shift = m_shift
End Function

Public Sub Bump()
    m_hits = m_hits + 1
End Sub

Public Function Hits() As Long
    Hits = m_hits
End Function
