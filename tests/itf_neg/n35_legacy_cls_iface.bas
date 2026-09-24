Attribute VB_Name = "N35LegacyClsIface"
Option Explicit

' ai/022 B11/C03a negative (pair with n35_cls.cls): VB6's habit of using a class module as an
' interface is not a contract, so it cannot be listed as one.

Interface IOne
    Sub A()
End Interface

CoClass CCA
    Interface LegacyIface
    [Default] Interface IOne
End CoClass

Sub Main()
    Debug.Print "x"
End Sub
