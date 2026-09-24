Attribute VB_Name = "N25CoClassNoName"
Option Explicit

' ai/022 B11/C01 negative: a CoClass block opener must name the CoClass.

CoClass
End CoClass

Sub Main()
    Debug.Print "x"
End Sub
