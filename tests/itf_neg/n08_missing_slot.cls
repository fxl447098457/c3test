VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "Dog"
Option Explicit

Interface IShape
    Function Area() As Double
    Sub Describe()
End Interface

Implements IShape

Private Function Area() As Double
    Area = 1#
End Function
