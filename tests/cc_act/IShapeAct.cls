VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "IShapeAct"
Option Explicit

' ai/022 B11/C05: the contract the two CoClass blocks below list. Declared in its own
' .cls host (B03 form) on purpose -- a contract declared inside a .bas does not reach a
' call site yet (pre-existing gap, ai/022 D43), and C05 must not look like it depends on it.

Interface IShapeAct
    Sub Move(dx As Double)
    Function Area() As Double
End Interface
