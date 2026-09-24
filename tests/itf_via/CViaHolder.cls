VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "CViaHolder"
Option Explicit

' ai/022 B10: the delegatee. It implements IViaNamed for real, and every contract member is
' Private on purpose -- that is the VB6 idiom for an interface implementation, and it is
' exactly why the generated Via adapter must go through the interface table (a Private proc
' compiles to `static` C, unreachable from another translation unit).

Implements IViaNamed

Private m_name As String
Private m_area As Double

Private Function Area() As Double
    Area = m_area
End Function

Private Sub Describe(ByVal txt As String)
    m_name = txt & "?"
End Sub

Private Property Get Name() As String
    Name = m_name
End Property

Private Property Let Name(ByVal v As String)
    m_name = v
End Property

Private Sub Rename(ByVal s As String)
    m_name = "ren:" & s
End Sub

Private Function Join(ByVal a As String, ByVal b As String) As String
    Join = a & "+" & b
End Function

Public Sub SetArea(ByVal v As Double)
    m_area = v
End Sub
