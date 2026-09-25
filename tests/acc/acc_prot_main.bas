Attribute VB_Name = "ProtMain"
Option Explicit

Sub Main()
    Dim d As ProtDerived
    Set d = New ProtDerived
    d.Touch
    Debug.Print "PROT:" & d.TagValue()
End Sub
