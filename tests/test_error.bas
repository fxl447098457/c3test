' test_error.bas - Simple error handling test
Sub Main()
    ' Test: On Error Resume Next only
    On Error Resume Next
    Open "nonexistent_file_xyz.txt" For Input As #1
    Debug.Print "Resumed after file error"
    On Error GoTo 0

    Debug.Print "Error Test PASSED"
End Sub
