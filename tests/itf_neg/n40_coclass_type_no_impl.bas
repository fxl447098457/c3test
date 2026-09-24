Attribute VB_Name = "N40NoImpl"
Option Explicit

' ai/022 B11/C05 (D54-2): a block that lists contracts but names no [Implementation] class
' has nothing a variable could bind to. Before this batch `Dim a As N40Mint` was swallowed by
' the lenient type fallback and became a late-bound DISPID call on a null pointer. The block
' itself stays legal (its identity line still prints), so the refusal lands on the USE.

CoClass N40Mint
    [Default] Interface N40I
End CoClass

Interface N40I
    Sub Go()
End Interface

Sub UseIt()
    Dim a As N40Mint
    Set a = New N40Mint
    a.Go
End Sub
