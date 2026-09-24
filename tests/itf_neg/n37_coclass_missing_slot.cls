VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "N37CoclassMissingSlot"
Option Explicit

' ai/022 B11/C03b negative: the block binds a class that leaves one slot unimplemented.
' Before this batch the whole contract question was silent (D48 measured nine shapes, eight
' of them without a word); C03a covered block shape, C03b covers the contract itself.

Interface N37IShape
    Sub Move(dx As Long)
    Function Area() As Double
End Interface

CoClass N37Circle
    [Implementation("N37CoclassMissingSlot")]
    [Default] Interface N37IShape
End CoClass

Private m_a As Double

Public Sub Move(dx As Long)
End Sub
