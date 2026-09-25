VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "OnlyOne"
Option Explicit

Interface IShape
    Function Area() As Double
End Interface

Interface IHidden
    Sub Hide()
End Interface

Implements IShape

Private Function Area() As Double
    Area = 1#
End Function

' clause names an interface this class does not implement
Private Sub Vanish() Implements IHidden.Hide
End Sub
