VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "P09Holder"
Option Explicit

' ai/022 B11/C03b positive (p09, together with p09_impl.cls): the contract itself. P09Holder
' really does implement it, which is what lets P09Impl delegate the whole contract to it.

Interface P09IShape
    Sub Move(dx As Long)
    Function Area() As Double
End Interface

Implements P09IShape

Private m_a As Double

Public Sub Move(dx As Long)
End Sub

Public Function Area() As Double
    Area = m_a
End Function
