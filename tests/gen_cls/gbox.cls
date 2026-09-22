VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Class GBox(Of T)
Attribute VB_Name = "GBox"
Option Explicit

Private m_val As T

Public Sub SetVal(ByVal v As T)
    m_val = v
End Sub

Public Function GetVal() As T
    GetVal = m_val
End Function

Public Function Describe() As String
    Describe = "box:" & CStr(m_val)
End Function
