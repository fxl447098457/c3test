VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "AccBase"
Option Explicit

' 084a: Private **字段**经 B07b inhFields 合并进派生类, 派生类经 Me. 访问合法
Private m_n As Long

Public Sub Bump()
    m_n = m_n + 1
End Sub

Public Function Count() As Long
    Count = m_n
End Function
