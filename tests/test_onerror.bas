' test_onerror.bas - P8.3 On Error GoTo end-to-end test

Sub Main()
    ' === Test 1: On Error GoTo with file not found ===
    On Error GoTo ErrorHandler1
    
    ' This should fail - file doesn't exist for Input mode
    Open "nonexistent_file_xyz.dat" For Input As #1
    
    ' Should NOT reach here
    Debug.Print "FAIL1a"
    GoTo Done1

ErrorHandler1:
    Debug.Print "PASS1"
Done1:
    On Error GoTo 0
    
    ' === Test 2: On Error Resume Next ===
    On Error Resume Next
    
    ' This should fail silently
    Open "nonexistent_file_xyz.dat" For Input As #2
    
    Debug.Print "PASS2"
    On Error GoTo 0
    
    ' === Test 3: On Error GoTo with multiple errors ===
    On Error GoTo ErrorHandler3
    
    Dim x As Long
    x = 42
    ' Normal code works
    If x = 42 Then Debug.Print "PASS3a"
    
    ' Trigger error
    Open "nonexistent_file_xyz.dat" For Input As #3
    
    Debug.Print "FAIL3b"
    GoTo Done3

ErrorHandler3:
    Debug.Print "PASS3b"
Done3:
    On Error GoTo 0
    
    Debug.Print "Done"
End Sub
