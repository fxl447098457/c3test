VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "InhMid"
Option Explicit

Inherits InhBase

Private m_midTag As String

Public Sub SetMidTag(ByVal v As String)
    m_midTag = v
End Sub

Public Function MidTag() As String
    MidTag = m_midTag
End Function

' shadowing: InhMid declares Tag itself, so the inherited slot is child-wins
Public Function Tag() As String
    Tag = "mid"
End Function
