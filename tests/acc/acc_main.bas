Attribute VB_Name = "AccMain"
Option Explicit

Sub Main()
    Dim t As AccThing
    Set t = New AccThing
    t.Setup 7
    Dim u As AccUser
    Set u = New AccUser
    u.Use t
    Debug.Print "ACC-OK"
End Sub
