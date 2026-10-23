Attribute VB_Name = "mdl2947"
Option Explicit

Sub Main()
    Dim x As IK2947
    Set x = New H2947
    Set x.Owner = New O2947
    Debug.Print "K2947-a=" & x.Has()
    Set x.Owner = Nothing
    Debug.Print "K2947-b=" & x.Has()
    Debug.Print "K2947-DONE"
End Sub
