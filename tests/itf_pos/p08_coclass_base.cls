VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "P08Base"
Option Explicit

' The ancestor carries both slots. Note it does NOT write `Implements P08IShape` -- a base
' that implements a new-style Interface cannot be inherited at all today (VB3022), so the
' only reachable ancestor-supplied shape is "declares the members, claims no interface".

Public Sub Move(dx As Long)
End Sub

Public Function Area() As Double
    Area = 3#
End Function
