Attribute VB_Name = "N30CoClassUnknownIface"
Option Explicit

' ai/022 B11/C03a negative: a contract entry must name an Interface block.

Interface IOne
    Sub A()
End Interface

CoClass CCA
    Interface INotThere
    [Default] Interface IOne
End CoClass

Sub Main()
    Debug.Print "x"
End Sub
