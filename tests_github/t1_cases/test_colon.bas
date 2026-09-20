' test_colon.bas - P8.5 Colon line separator test (a=1:b=2)
Sub Main()
    ' Test 1: Multiple statements on one line with colon
    Dim a As Long
    Dim b As Long
    a = 1: b = 2
    
    If a + b = 3 Then
        Debug.Print "PASS1"
    Else
        Debug.Print "FAIL1"
    End If
    
    ' Test 2: Three statements on one line
    Dim x As Long
    Dim y As Long
    Dim z As Long
    x = 10: y = 20: z = 30
    
    If x + y + z = 60 Then
        Debug.Print "PASS2"
    Else
        Debug.Print "FAIL2"
    End If
    
    ' Test 3: Mixing colon and newline
    Dim c As Long
    c = 100: c = c + 50
    c = c + 25
    
    If c = 175 Then
        Debug.Print "PASS3"
    Else
        Debug.Print "FAIL3"
    End If
    
    Debug.Print "Done"
End Sub