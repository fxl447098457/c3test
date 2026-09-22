Option Explicit

' ============================================================
'  test_gosub.bas - GoSub / Return 族
'
'  覆盖:
'    1. GoSub <命名标签> + Return
'    2. GoSub <数字行号标签> + Return (VB6 行号即标签)
'    3. 同一子过程被多次 GoSub (返回地址各自独立, 不是单一 goto)
'    4. 子过程体放在 Exit Sub 之后 (VB6 惯用法, 不会被顺序执行到)
'    5. 在 For 循环体内 GoSub (调用点在被方向拆分的 body 里, 返回地址仍需正确)
'  约定: 不弹 MsgBox/InputBox, 只输出 PASS/FAIL 供用例比对。
' ============================================================

Public Sub TestGoSub()
    Dim x As Long
    Dim y As Long

    ' 1. 命名标签
    GoSub AddX
    ' 2. 数字行号标签
    GoSub 300
    ' 3. 再次 GoSub 同一目标 (x 累加到 3)
    GoSub AddX

    ' 4. 出口先 Exit Sub, 子过程体在其后 (顺序执行不到)
    If x = 3 Then
        Debug.Print "GOSUB-PASS1"
    Else
        Debug.Print "GOSUB-FAIL1:"; x
    End If

    y = 5
    GoSub DoubleY
    If y = 10 Then
        Debug.Print "GOSUB-PASS2"
    Else
        Debug.Print "GOSUB-FAIL2:"; y
    End If
    Exit Sub
AddX:
    x = x + 1
    Return
300
    x = x + 1
    Return
DoubleY:
    y = y * 2
    Return
End Sub

Public Sub TestGoSubInLoop()
    Dim n As Long
    Dim calls As Long

    ' 5. 循环体内 GoSub: 每次调用都要能 Return 回各自的下一条语句
    For n = 1 To 4
        GoSub Bump
    Next n
    If calls = 4 Then
        Debug.Print "GOSUB-PASS3"
    Else
        Debug.Print "GOSUB-FAIL3:"; calls
    End If
    Exit Sub
Bump:
    calls = calls + 1
    Return
End Sub

Sub Main()
    TestGoSub
    TestGoSubInLoop
    Debug.Print "GOSUB DONE"
End Sub
