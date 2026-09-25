VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "IViaShape"
Option Explicit

' ai/022 B10: head-line host form (B03) -- one Interface per .cls, module name == interface
' name. (The same contracts declared inside a .bas do NOT reach a call site today; that
' pre-existing gap is registered separately in ai/022, it is not part of B10.)

Interface IViaShape
    Function Area() As Double
    Sub Describe(ByVal txt As String)
    Property Get Name() As String
    Property Let Name(ByVal v As String)
End Interface
