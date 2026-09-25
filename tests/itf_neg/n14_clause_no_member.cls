VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "Gap"
Option Explicit

Interface IShape
    Function Area() As Double
End Interface

Implements IShape

' clause names a member the interface does not declare
Private Function Cover(ByVal p As String) As Double Implements IShape.Perimeter
    Cover = 1#
End Function
