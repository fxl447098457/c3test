VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "CRectangle"
Option Explicit

Implements IShape

Public Width As Double
Public Height As Double

Private Function IShape_Area() As Double
    IShape_Area = Width * Height
End Function

Private Function IShape_Perimeter() As Double
    IShape_Perimeter = 2# * (Width + Height)
End Function

Private Sub IShape_Describe()
    Debug.Print "Rectangle"
End Sub
