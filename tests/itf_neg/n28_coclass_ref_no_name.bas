Attribute VB_Name = "N28CoClassRefNoName"
Option Explicit

' ai/022 B11/C01 negative: `Interface` inside a CoClass block is a reference and must
' carry the interface name (it is not the start of an inline definition).

Interface IAny
    Sub Go()
End Interface

CoClass Thing
    Interface
End CoClass

Sub Main()
    Debug.Print "x"
End Sub
