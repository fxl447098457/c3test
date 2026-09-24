VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "ProtBase"
Option Explicit

Protected m_tag As String

Protected Sub Tag(ByVal s As String)
    m_tag = s
End Sub

Public Property Get TagValue() As String
    TagValue = m_tag
End Property
