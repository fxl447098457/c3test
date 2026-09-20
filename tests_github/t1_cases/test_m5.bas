' test_m5.bas - M5 Milestone Test: Arrays + File I/O + Error Handling
Sub Main()
    Dim scores(1 To 5) As Integer
    Dim i As Integer
    Dim fnum As Integer
    Dim line As String
    Dim total As Integer

    ' 1. Array: Fill scores
    For i = 1 To 5
        scores(i) = i * 10
    Next i

    ' 2. File I/O: Write scores to file
    fnum = FreeFile
    Open "scores.txt" For Output As #fnum
    For i = 1 To 5
        Print #fnum, Str(scores(i))
    Next i
    Close #fnum

    ' 3. File I/O: Read back and sum
    fnum = FreeFile
    Open "scores.txt" For Input As #fnum
    total = 0
    For i = 1 To 5
        Line Input #fnum, line
        total = total + Val(line)
    Next i
    Close #fnum

    Debug.Print "Total="; total

    ' 4. Error Handling: Resume Next on file not found
    On Error Resume Next
    Open "nonexistent.txt" For Input As #fnum
    Debug.Print "Error handled"
    On Error GoTo 0

    ' 5. Array verification
    If UBound(scores) = 5 And LBound(scores) = 1 Then
        Debug.Print "M5 PASSED"
    Else
        Debug.Print "M5 FAILED"
    End If
End Sub
