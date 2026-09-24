Attribute VB_Name = "N32CoClassDupEntry"
Option Explicit

' ai/022 B11/C03a negative: the contract set is a set.

Interface IOne
    Sub A()
End Interface

CoClass CCA
    Interface IOne
    Interface IOne
End CoClass

Sub Main()
    Debug.Print "x"
End Sub
