Attribute VB_Name = "TestMain"
Option Explicit

Sub Main()
    Dim c As Counter
    Set c = New Counter
    c.Increment
    c.Increment
    c.Increment
    Debug.Print c.GetCount()
    c.Reset
    Debug.Print c.GetCount()
End Sub
