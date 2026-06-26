VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "CCircle"
Option Explicit

Public Radius As Double

Public Function Area() As Double
    Area = 3.14159265358979 * Radius * Radius
End Function

Public Function Perimeter() As Double
    Perimeter = 2# * 3.14159265358979 * Radius
End Function