VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "N38CoclassSigMismatch"
Option Explicit

' ai/022 B11/C03b negative: the member is there but the written signature differs (ByVal vs
' the interface's ByRef). Same vocabulary the Implements checker uses -- interface_sig.hpp is
' the one definition of "what a slot is", so the two checkers cannot disagree (D51-2).

Interface N38IShape
    Sub Move(dx As Long)
End Interface

CoClass N38Circle
    [Implementation("N38CoclassSigMismatch")]
    [Default] Interface N38IShape
End CoClass

Public Sub Move(ByVal dx As Long)
End Sub
