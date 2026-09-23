VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "InhBase"
Option Explicit

Private m_hits As Long

Public Sub Bump()
    m_hits = m_hits + 1
End Sub

Public Function Hits() As Long
    Hits = m_hits
End Function
