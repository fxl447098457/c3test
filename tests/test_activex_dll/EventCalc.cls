VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "EventCalc"
Option Explicit

Private m_Value As Long

Public Event Changed()

Public Sub SetValue(ByVal v As Long)
    m_Value = v
    RaiseEvent Changed()
End Sub

Public Function GetValue() As Long
    GetValue = m_Value
End Function

Public Sub Increment()
    m_Value = m_Value + 1
    RaiseEvent Changed()
End Sub
