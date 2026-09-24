Attribute VB_Name = "N29CoClassDupName"
Option Explicit

' ai/022 B11/C03a negative: CoClass names are project-wide unique.

Interface IOne
    Sub A()
End Interface

CoClass CCA
    Interface IOne
End CoClass

CoClass CCA
    Interface IOne
End CoClass

Sub Main()
    Debug.Print "x"
End Sub
