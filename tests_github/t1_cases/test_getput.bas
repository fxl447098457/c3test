' test_getput.bas - P8.2 Get/Put statement tests
' Tests: Random mode + Binary mode Get/Put

Sub Main()
    ' === Test 1: Random mode Put/Get with Long ===
    Open "test_rnd.dat" For Random As #1 Len = 4
    
    Dim v1 As Long
    Dim v2 As Long
    Dim v3 As Long
    v1 = 100
    v2 = 200
    v3 = 300
    Put #1, 1, v1
    Put #1, 2, v2
    Put #1, 3, v3
    
    Dim r1 As Long
    Dim r2 As Long
    Dim r3 As Long
    Get #1, 1, r1
    Get #1, 2, r2
    Get #1, 3, r3
    
    Close #1
    
    If r1 = 100 Then
        Debug.Print "PASS1a"
    Else
        Debug.Print "FAIL1a"
    End If
    If r2 = 200 Then
        Debug.Print "PASS1b"
    Else
        Debug.Print "FAIL1b"
    End If
    If r3 = 300 Then
        Debug.Print "PASS1c"
    Else
        Debug.Print "FAIL1c"
    End If
    
    ' === Test 2: Random mode overwrite ===
    Open "test_rnd.dat" For Random As #2 Len = 4
    Dim vNew As Long
    vNew = 999
    Put #2, 2, vNew
    
    Dim rNew As Long
    Get #2, 2, rNew
    Close #2
    
    If rNew = 999 Then
        Debug.Print "PASS2"
    Else
        Debug.Print "FAIL2"
    End If
    
    ' === Test 3: Binary mode Put/Get ===
    Dim b1 As Long
    b1 = 42
    Open "test_bin.dat" For Binary As #3
    Put #3, 1, b1
    
    Dim br1 As Long
    Get #3, 1, br1
    Close #3
    
    If br1 = 42 Then
        Debug.Print "PASS3"
    Else
        Debug.Print "FAIL3"
    End If
    
    ' === Test 4: Random mode Double ===
    Dim d1 As Double
    d1 = 3.14
    Open "test_dbl.dat" For Random As #4 Len = 8
    Put #4, 1, d1
    
    Dim dr1 As Double
    Get #4, 1, dr1
    Close #4
    
    Dim diff As Double
    diff = dr1 - 3.14
    If diff = 0 Then
        Debug.Print "PASS4"
    Else
        Debug.Print "FAIL4"
    End If
    
    Debug.Print "Done"
End Sub
