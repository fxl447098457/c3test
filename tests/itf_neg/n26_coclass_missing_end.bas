Attribute VB_Name = "N26CoClassMissingEnd"
Option Explicit

' ai/022 B11/C01 negative: the block must be closed with `End CoClass`; a bare `End`
' or falling out of the block is a block-structure error, not a silent absorb.

Interface IAny
    Sub Go()
End Interface

CoClass Broken
    Interface IAny

Sub Main()
    Debug.Print "x"
End Sub
