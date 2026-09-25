VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "Ov14Derived"
Option Explicit

Inherits Ov14Base

Public Overrides Function Calc(ByVal a As String) As Long
    Calc = Len(a)
End Function
