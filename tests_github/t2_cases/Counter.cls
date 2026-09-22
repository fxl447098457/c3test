VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "Counter"
Option Explicit

Private m_Count As Long

Public Sub Increment()
    m_Count = m_Count + 1
End Sub

Public Function GetCount() As Long
    GetCount = m_Count
End Function

Public Sub Reset()
    m_Count = 0
End Sub
