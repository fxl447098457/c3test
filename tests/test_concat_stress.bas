' BSTR Concat stress test - loop many times to expose leaks
Option Explicit

Public Sub Main()
    Dim i As Long
    Dim s As String
    Dim r As String
    
    For i = 1 To 1000
        s = "A" & "B" & "C" & "D" & "E"
    Next i
    
    r = s & "F" & "G" & "H"
    Print r
End Sub
