' test_fixes.bas - 验证5个已知限制的修复
' 1. Debug.Print浮点数 2. Static变量 3. Select Case字符串/Is/多值 4. GoSub/Return 5. Module.Method

Sub Main()
    ' === 1. Debug.Print 浮点数 ===
    Dim d As Double
    d = 3.14159
    Debug.Print d
    Dim x As Long
    x = 42
    Debug.Print x
    Debug.Print 2.71828

    ' === 2. Static 变量 ===
    TestStatic
    TestStatic
    TestStatic

    ' === 3a. Select Case 字符串 ===
    Dim s As String
    s = "hello"
    Select Case s
        Case "hello"
            Debug.Print "String match: hello"
        Case "world"
            Debug.Print "String match: world"
        Case Else
            Debug.Print "String no match"
    End Select

    ' === 3b. Select Case Is ===
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

    ' === 3c. Select Case 多值 ===
    Dim wd As Long
    wd = 3
    Select Case wd
        Case 1, 7
            Debug.Print "Weekend"
        Case 2, 3, 4, 5, 6
            Debug.Print "Weekday"
    End Select

    ' === 4. GoSub/Return ===
    TestGoSub

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
