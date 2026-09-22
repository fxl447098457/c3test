Option Explicit

' ============================================================
'  test_goto.bas - GoTo / 标签 / On Error GoTo / Resume 族
'
'  覆盖:
'    1. 命名标签 前向 GoTo (跳过语句)
'    2. VB6 数字行号标签 (`200:` 带冒号, `100` 省略冒号) —— 行号即标签
'    3. 后向 GoTo 组循环 (GoTo 是过程内跳转, 不产生新栈帧)
'    4. On Error GoTo <命名标签> + Resume <行号> (错误处理器里回到指定标签)
'    5. 单行 If 里的冒号是**语句分隔符**而非标签 (`Then a = 1: a = 2`)
'    6. GoTo 跳出 For 循环到过程尾出口标签
'  约定: 不弹 MsgBox/InputBox, 不打无意义日志, 只输出 PASS/FAIL 供用例比对。
' ============================================================

Public Sub TestGotoLabels()
    Dim a As Long
    Dim i As Long

    ' 1. 命名标签: 跳过 a = 99
    a = 1
    GoTo SkipNamed
    a = 99
SkipNamed:

    ' 2. 数字行号标签 (带冒号): 跳过 a = 99
    GoTo 200
    a = 99
200:

    ' 3. 行号后省略冒号 (`100` 独占一行) + 后向 GoTo 组循环
    i = 0
100
    i = i + 1
    If i < 3 Then GoTo 100

    If a = 1 Then
        Debug.Print "GOTO-PASS1"
    Else
        Debug.Print "GOTO-FAIL1:"; a
    End If
    If i = 3 Then
        Debug.Print "GOTO-PASS2"
    Else
        Debug.Print "GOTO-FAIL2:"; i
    End If
End Sub

Public Sub TestOnErrorLabel()
    ' 4a. On Error GoTo <命名标签> + Resume <行号>
    On Error GoTo ErrNamed
    Err.Raise 5
    Debug.Print "GOTO-FAIL3-order"
ErrNamed:
    If Err.Number = 5 Then
        Debug.Print "GOTO-PASS3"
    Else
        Debug.Print "GOTO-FAIL3-num:"; Err.Number
    End If
    Resume 400
400:
    On Error GoTo 0
End Sub

Public Sub TestOnErrorNumber()
    Dim reached As Long

    ' 4b. On Error GoTo <行号> (行号形式的处理器) + Resume Next
    On Error GoTo 500
    reached = 2
    Err.Raise 11
    ' Resume Next 的落点 = 出错语句的下一句, 即这行
    reached = reached + 100
    On Error GoTo 0
    If reached = 102 Then
        Debug.Print "GOTO-PASS4"
    Else
        Debug.Print "GOTO-FAIL4:"; reached
    End If
    Exit Sub
500
    If Err.Number = 11 Then
        Debug.Print "GOTO-PASS5"
    Else
        Debug.Print "GOTO-FAIL5-num:"; Err.Number
    End If
    Resume Next
End Sub

Public Sub TestGotoOutOfLoop()
    Dim n As Long
    Dim total As Long

    ' 5. 单行 If: 冒号是语句分隔符, 不是标签
    n = 1
    If n = 1 Then n = n + 1: n = n + 1
    If n = 3 Then
        Debug.Print "GOTO-PASS6"
    Else
        Debug.Print "GOTO-FAIL6:"; n
    End If

    ' 6. GoTo 跳出 For 循环 (循环方向拆分的 body 里也需能配到 body 外的出口标签)
    total = 0
    For n = 1 To 10
        total = total + n
        If n = 3 Then GoTo OutOfLoop
    Next n
    Debug.Print "GOTO-FAIL7"
    Exit Sub
OutOfLoop:
    If total = 6 Then
        Debug.Print "GOTO-PASS7"
    Else
        Debug.Print "GOTO-FAIL7:"; total
    End If
End Sub

Sub Main()
    TestGotoLabels
    TestOnErrorLabel
    TestOnErrorNumber
    TestGotoOutOfLoop
    Debug.Print "GOTO DONE"
End Sub
