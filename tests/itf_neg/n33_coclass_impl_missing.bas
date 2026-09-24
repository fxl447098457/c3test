Attribute VB_Name = "N33CoClassImplMissing"
Option Explicit

' ai/022 B11/C03a negative: [Implementation] must name a project class module.

Interface IOne
    Sub A()
End Interface

CoClass CCA
    [Implementation("NoSuchClass")]
    Interface IOne
End CoClass

Sub Main()
    Debug.Print "x"
End Sub
