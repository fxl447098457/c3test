' test_variant.bas - P8.4 Variant type improvement test
Sub Main()
    ' Test 1: Variant can hold Long
    Dim v As Variant
    v = 42
    If CLng(v) = 42 Then
        Debug.Print "PASS1a"
    Else
        Debug.Print "FAIL1a"
    End If
    
    ' Test 2: Variant can hold Double
    v = 3.14
    If CDbl(v) > 3.0 And CDbl(v) < 4.0 Then
        Debug.Print "PASS1b"
    Else
        Debug.Print "FAIL1b"
    End If
    
    ' Test 3: Variant can hold String
    v = "hello"
    If Len(CStr(v)) = 5 Then
        Debug.Print "PASS1c"
    Else
        Debug.Print "FAIL1c"
    End If
    
    ' Test 4: IsEmpty / IsNull
    Dim v2 As Variant
    If IsEmpty(v2) Then
        Debug.Print "PASS2a"
    Else
        Debug.Print "FAIL2a"
    End If
    
    v2 = Null
    If IsNull(v2) Then
        Debug.Print "PASS2b"
    Else
        Debug.Print "FAIL2b"
    End If
    
    ' Test 5: VarType - c3 maps integers to Long (vt=3)
    v = 42
    If VarType(v) = 3 Then  ' vbLong = 3 (c3 maps int literals to Long)
        Debug.Print "PASS3"
    Else
        Debug.Print "FAIL3"
    End If
    
    ' Test 6: Reassignment works
    v = 100
    If CLng(v) = 100 Then
        Debug.Print "PASS4"
    Else
        Debug.Print "FAIL4"
    End If
    
    ' Test 7: Variant with double value
    v = 99.5
    If CDbl(v) > 99.0 And CDbl(v) < 100.0 Then
        Debug.Print "PASS5"
    Else
        Debug.Print "FAIL5"
    End If
    
    Debug.Print "Done"
End Sub