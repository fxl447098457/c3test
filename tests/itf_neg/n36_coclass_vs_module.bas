Attribute VB_Name = "N36CoClassVsModule"
Option Explicit

' ai/022 B11/C03a negative (pair with n36_other.bas): the block name collides with a
' *different* module, which is a real namespace clash (naming the host module is legal --
' see p07).

Interface IOne
    Sub A()
End Interface

CoClass N36Other
    Interface IOne
End CoClass

Sub Main()
    Debug.Print "x"
End Sub
