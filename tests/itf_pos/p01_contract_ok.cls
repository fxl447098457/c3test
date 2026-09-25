VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "Square"
Option Explicit

Interface IShape
    Function Area() As Double
    Sub Describe(ByVal txt As String)
    Property Get Name() As String
    Property Let Name(ByVal v As String)
End Interface

Interface INamed Extends IShape
    Sub Rename(ByVal s As String)
End Interface

Implements INamed

Private m_name As String

Private Function Area() As Double
    Area = 4#
End Function

Private Sub Describe(ByVal txt As String)
End Sub

Private Property Get Name() As String
    Name = m_name
End Property

Private Property Let Name(ByVal v As String)
    m_name = v
End Property

Private Sub Rename(ByVal s As String)
End Sub
