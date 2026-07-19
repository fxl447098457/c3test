VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "Calc"
Option Explicit

Private m_Value As Long

Public Sub SetValue(ByVal v As Long)
    m_Value = v
End Sub

Public Function Add(ByVal x As Long, ByVal y As Long) As Long
    Add = x + y
End Function

Public Function Multiply(ByVal x As Long, ByVal y As Long) As Long
    Multiply = x * y
End Function

Public Function GetValue() As Long
    GetValue = m_Value
End Function
