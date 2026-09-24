Attribute VB_Name = "N31CoClassTwoDefaults"
Option Explicit

' ai/022 B11/C03a negative: a coclass has exactly one default interface.

Interface IOne
    Sub A()
End Interface

Interface ITwo
    Sub B()
End Interface

CoClass CCA
    [Default] Interface IOne
    [Default] Interface ITwo
End CoClass

Sub Main()
    Debug.Print "x"
End Sub
