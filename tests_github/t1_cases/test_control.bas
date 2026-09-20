' test_control.bas - VB6控制流增强测试
' 覆盖: For Step、嵌套循环、Exit Sub/Function、Do Loop While、
'        Select Case比较、GoTo标签、条件编译
Attribute VB_Name = "TestControl"

Option Explicit

' ===== Exit Function测试 =====
Private Function FindFirst(ByVal target As Long, ByVal max As Long) As Long
    Dim i As Long
    For i = 1 To max
        If i = target Then
            FindFirst = i
            Exit Function
        End If
    Next i
    FindFirst = -1
End Function

' ===== Exit Sub测试 =====
Private Sub EarlyReturn()
    Debug.Print "BeforeExit"
    Exit Sub
    Debug.Print "ShouldNotAppear"
End Sub

' ===== 主测试入口 =====
Public Sub Main()
    Debug.Print "=== Control Test Start ==="

    ' 1. For ... Step
    Dim i As Long
    For i = 10 To 1 Step -2
        Debug.Print "Step:"; i
    Next i
    
    ' 2. For ... Step正步长
    For i = 0 To 20 Step 5
        Debug.Print "Step5:"; i
    Next i

    ' 3. 嵌套For循环
    Dim row As Long
    Dim col As Long
    Dim total As Long
    total = 0
    For row = 1 To 3
        For col = 1 To 4
            total = total + 1
        Next col
    Next row
    Debug.Print "Nested="; total

    ' 4. Do While ... Loop (条件为假不执行)
    Dim x As Long
    x = 10
    Do While x < 5
        x = x + 1
    Loop
    Debug.Print "DoWhileSkip="; x

    ' 5. Do ... Loop While (至少执行一次)
    x = 10
    Do
        x = x + 1
    Loop While x < 5
    Debug.Print "DoLoopWhile="; x

    ' 6. Exit Function测试
    Dim found As Long
    found = FindFirst(7, 100)
    Debug.Print "FindFirst="; found
    found = FindFirst(999, 10)
    Debug.Print "FindFirst_miss="; found

    ' 7. Exit Sub测试
    EarlyReturn

    ' 8. Select Case 多值匹配 (Long类型, 避免字符串比较)
    Dim gradeCode As Long
    gradeCode = 2
    Select Case gradeCode
    Case 1
        Debug.Print "Excellent"
    Case 2
        Debug.Print "Good"
    Case 3
        Debug.Print "Average"
    Case Else
        Debug.Print "Other"
    End Select

    ' 9. Select Case 范围
    Dim score As Long
    score = 85
    Select Case score
    Case 90 To 100
        Debug.Print "A_range"
    Case 80 To 89
        Debug.Print "B_range"
    Case 70 To 79
        Debug.Print "C_range"
    Case Else
        Debug.Print "F_range"
    End Select

    ' 10. Select Case Is
    Dim num As Long
    num = -5
    Select Case num
    Case -1
        Debug.Print "Negative_One"
    Case 0
        Debug.Print "Zero"
    Case Else
        Debug.Print "Other"
    End Select

    ' 11. GoTo跳转
    GoTo JumpTarget
    Debug.Print "ShouldNotAppear2"
JumpTarget:
    Debug.Print "GoToOK"

    ' 12. On Error Resume Next边界
    On Error Resume Next
    Dim bad As Long
    Dim zero As Long
    zero = 0
    bad = 10 / zero    ' 运行时除零错误
    Debug.Print "AfterDivZero"
    On Error GoTo 0

    Debug.Print "=== Control Test End ==="
End Sub
