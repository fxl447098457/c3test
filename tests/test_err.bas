' test_err.bas - Err object test
Option Explicit

Sub Main()
    On Error Resume Next
    Dim n As Long
    n = Err.Number
    Debug.Print CStr(n)
    Err.Clear
End Sub