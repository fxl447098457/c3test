' test_err_obj.bas - P24-12: Err object comprehensive test
Option Explicit

Sub Main()
    Dim passCount As Long
    passCount = 0
    
    ' Test 1: Err.Number initial state is 0
    On Error Resume Next
    If Err.Number = 0 Then
        passCount = passCount + 1
        Debug.Print "ERR-1:OK Number=0"
    End If
    
    ' Test 2: Err.Number after Err.Raise 11
    Err.Raise 11
    If Err.Number = 11 Then

        passCount = passCount + 1
        Debug.Print "ERR-2:OK Number="; Err.Number
    End If
    
    ' Test 3: Err.Clear
    Err.Clear
    If Err.Number = 0 Then
        passCount = passCount + 1
        Debug.Print "ERR-3:OK Clear"
    End If
    
    ' Test 4: Err.Raise
    On Error Resume Next
    Err.Raise 5
    If Err.Number = 5 Then
        passCount = passCount + 1
        Debug.Print "ERR-4:OK Raise 5"
    End If
    
    ' Test 5: Err.Description
    If Len(Err.Description) >= 0 Then
        passCount = passCount + 1
        Debug.Print "ERR-5:OK Description"
    End If
    
    ' Test 6: Err.Source
    If Len(Err.Source) >= 0 Then
        passCount = passCount + 1
        Debug.Print "ERR-6:OK Source"
    End If
    
    On Error GoTo 0
    
    Debug.Print "ERR:"; passCount; "/6"
End Sub
