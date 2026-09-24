Attribute VB_Name = "AccFamMain"
Option Explicit

Sub Main()
    Dim d As AccDerived
    Set d = New AccDerived
    d.TouchInherited
    d.Bump
    Debug.Print "ACC-FAM:" & CStr(d.Count())
End Sub
