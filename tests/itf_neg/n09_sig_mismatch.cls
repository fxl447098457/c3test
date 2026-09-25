VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "Cat"
Option Explicit

Interface IShape
    Function Area() As Double
End Interface

Implements IShape

Private Function Area() As Long
    Area = 3
End Function
