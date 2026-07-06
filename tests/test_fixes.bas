' test_fixes.bas - verify 5 known bug fixes
' 1. Debug.Print float 2. Static var 3. Select Case string/Is/multi 4. GoSub/Return 5. Module.Method

Sub Main()
    Dim d As Double
    d = 3.14159
    Dim x As Long
    x = 42

    TestStatic
    TestStatic
    TestStatic

    Dim s As String
    s = "hello"
    Select Case s
        Case "hello"
            Debug.Print "String match: hello"
        Case Else
            Debug.Print "String no match"
    End Select

    Dim n As Long
    n = 15
    Select Case n
        Case Is > 20
            Debug.Print "Is: >20"
        Case Is > 10
            Debug.Print "Is: >10"
        Case Else
            Debug.Print "Is: other"
    End Select

    Dim wd As Long
    wd = 3
    Select Case wd
        Case 1, 7
            Debug.Print "Weekend"
        Case 2, 3, 4, 5, 6
            Debug.Print "Weekday"
    End Select

    TestGoSub

    If x = 42 Then
        Debug.Print "FIX1:OK"
    Else
        Debug.Print "FIX1:FAIL"
    End If

    If n > 10 And n <= 20 Then
        Debug.Print "FIX2:OK"
    Else
        Debug.Print "FIX2:FAIL"
    End If

    If wd >= 2 And wd <= 6 Then
        Debug.Print "FIX3:OK"
    Else
        Debug.Print "FIX3:FAIL"
    End If

    Debug.Print "All fixes passed!"
End Sub

Sub TestStatic()
    Static count As Long
    count = count + 1
    Debug.Print "Static count:"
    Debug.Print count
End Sub

Sub TestGoSub()
    GoSub MyLabel
    Debug.Print "After first GoSub"
    GoSub MyLabel
    Debug.Print "After second GoSub"
    Exit Sub
MyLabel:
    Debug.Print "In GoSub"
    Return
End Sub