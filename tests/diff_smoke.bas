' Differential smoke test: VB6 native vs C3
' Writes results to App.Path\result.txt
' (Debug.Print has no output in a native VB6 exe)

Option Explicit

Sub Main()
    Dim i As Integer
    Dim s As Long
    For i = 1 To 100
        s = s + i
    Next i

    Dim t As String
    t = ""
    For i = 1 To 5
        t = t & Chr(65 + i)
    Next i

    Dim arr(1 To 4) As Double
    arr(1) = 1.5
    arr(2) = 2.5
    arr(3) = 3.5
    arr(4) = 4.5
    Dim d As Double
    d = 0
    For i = 1 To 4
        d = d + arr(i)
    Next i

    Dim h As Integer
    h = FreeFile
    Open App.Path & "\result.txt" For Output As #h
    Print #h, "SUM=" & s
    Print #h, "STR=" & t
    Print #h, "FLT=" & Format(d, "0.00")
    If s = 5050 Then
        Print #h, "BRANCH=OK"
    Else
        Print #h, "BRANCH=NG"
    End If
    Close #h
End Sub
