Attribute VB_Name = "N27CoClassBadMember"
Option Explicit

' ai/022 B11/C01 negative: a CoClass block holds only attribute lines and interface
' references -- no fields, no implementation.

Interface IAny
    Sub Go()
End Interface

CoClass Thing
    Dim x As Long
    Interface IAny
    Sub Work()
    End Sub
End CoClass

Sub Main()
    Debug.Print "x"
End Sub
