Attribute VB_Name = "P06CoClassSoftIdent"
Option Explicit

' ai/022 B11/C01 positive guard: CoClass is registered as a SOFT keyword, so a program
' that already uses CoClass as an identifier (variable / UDT member) still parses.

Private Type TShape
    CoClass As Long
End Type

Sub Main()
    Dim CoClass As Long
    Dim s As TShape
    CoClass = 5
    s.CoClass = 6
    If CoClass = 5 And s.CoClass = 6 Then Debug.Print "SOFT:OK"
End Sub
