Attribute VB_Name = "ClsHost"
Option Explicit

Sub Main()
    Dim t As PkgThing
    Set t = New PkgThing
    t.Setup 21
    Debug.Print "PKG-C1:" & CStr(t.Value)
End Sub
