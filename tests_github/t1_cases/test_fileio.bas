' test_fileio.bas - File I/O test
Sub Main()
    Dim fnum As Integer
    Dim line As String

    ' Write to file
    fnum = FreeFile
    Open "test_output.txt" For Output As #fnum
    Print #fnum, "Hello from VB6!"
    Print #fnum, "Line 2"
    Close #fnum

    ' Read back
    fnum = FreeFile
    Open "test_output.txt" For Input As #fnum
    Line Input #fnum, line
    Debug.Print line
    Line Input #fnum, line
    Debug.Print line
    Close #fnum

    Debug.Print "File I/O Test PASSED"
End Sub
