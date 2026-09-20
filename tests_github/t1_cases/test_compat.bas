' test_compat.bas - VB6兼容性综合测试
' 覆盖: 字符串操作、ByRef/ByVal、嵌套调用、递归、Select Case、
'        Do Loop变体、While Wend、GoSub/Return、枚举、用户定义类型、
'        逻辑运算符(位运算)、字符串比较、类型转换
Attribute VB_Name = "TestCompat"

Option Explicit

' ===== 枚举测试 =====
Private Enum Color
    Red = 1
    Green = 2
    Blue = 3
End Enum

' ===== 用户定义类型(UDT)测试 =====
Private Type Point
    X As Long
    Y As Long
End Type

Private Type PersonInfo
    FullName As String
    Age As Long
End Type

' ===== 递归函数 =====
Private Function Factorial(ByVal n As Long) As Long
    If n <= 1 Then
        Factorial = 1
    Else
        Factorial = n * Factorial(n - 1)
    End If
End Function

' ===== ByRef参数 =====
Private Sub Swap(ByRef a As Long, ByRef b As Long)
    Dim temp As Long
    temp = a
    a = b
    b = temp
End Sub

' ===== ByVal与ByRef混合 =====
Private Function AddAndDouble(ByVal x As Long, ByRef y As Long) As Long
    y = y * 2
    AddAndDouble = x + y
End Function

' ===== 字符串处理函数 =====
Private Sub TestStringFuncs()
    Dim s As String
    s = "Hello World"
    
    ' InStr
    Dim pos As Long
    pos = InStr(s, "World")
    Debug.Print "InStr:"; pos
    
    ' Replace
    Dim s2 As String
    s2 = Replace(s, "World", "VB6")
    Debug.Print "Replace:"; s2
    
    ' Left/Right/Mid
    Debug.Print "Left:"; Left(s, 5)
    Debug.Print "Right:"; Right(s, 5)
    Debug.Print "Mid:"; Mid(s, 7, 5)
    
    ' LCase/UCase
    Debug.Print "LCase:"; LCase(s)
    Debug.Print "UCase:"; UCase(s)
    
    ' Len
    Dim s3 As String
    s3 = "  hello  "
    Debug.Print "Len:"; Len(s3)
End Sub

' ===== 数组测试增强 =====
Private Sub TestArrays()
    ' 动态数组
    Dim dyn() As Long
    ReDim dyn(5)
    Dim i As Long
    For i = 0 To 5
        dyn(i) = i * 10
    Next i
    Debug.Print "dyn(5)="; dyn(5)
    Debug.Print "UBound="; UBound(dyn)
    
    ' ReDim Preserve
    ReDim Preserve dyn(10)
    Debug.Print "dyn(5)_preserve="; dyn(5)
    Debug.Print "UBound_preserve="; UBound(dyn)
    
    ' Erase
    Erase dyn
End Sub

' ===== 主测试入口 =====
Public Sub Main()
    Debug.Print "=== Compat Test Start ==="
    
    ' 1. 枚举
    Dim c As Long
    c = Green
    Debug.Print "Enum Green="; c
    
    ' 2. UDT
    Dim p As Point
    p.X = 100
    p.Y = 200
    Debug.Print "Point.X="; p.X
    Debug.Print "Point.Y="; p.Y
    
    Dim emp As PersonInfo
    emp.FullName = "Alice"
    emp.Age = 30
    Debug.Print "PersonInfo.Age="; emp.Age
    
    ' 3. 递归
    Dim f As Long
    f = Factorial(6)
    Debug.Print "Factorial(6)="; f
    
    ' 4. ByRef Swap
    Dim a As Long
    Dim b As Long
    a = 10
    b = 20
    Swap a, b
    Debug.Print "Swap a="; a
    Debug.Print "Swap b="; b
    
    ' 5. ByVal/ByRef混合
    Dim v As Long
    v = 5
    Dim result As Long
    result = AddAndDouble(3, v)
    Debug.Print "AddAndDouble="; result
    Debug.Print "v_after="; v
    
    ' 6. Select Case
    Dim dayNum As Long
    dayNum = 3
    Select Case dayNum
    Case 1
        Debug.Print "Monday"
    Case 2
        Debug.Print "Tuesday"
    Case 3
        Debug.Print "Wednesday"
    Case Else
        Debug.Print "Other"
    End Select
    
    ' 7. Do While ... Loop
    Dim count As Long
    count = 0
    Do While count < 3
        count = count + 1
    Loop
    Debug.Print "DoWhile="; count
    
    ' 8. Do ... Loop Until
    count = 0
    Do
        count = count + 1
    Loop Until count >= 5
    Debug.Print "DoUntil="; count
    
    ' 9. While ... Wend
    Dim w As Long
    w = 0
    While w < 4
        w = w + 1
    Wend
    Debug.Print "WhileWend="; w
    
    ' 10. 逻辑运算符 (位运算)
    Dim x As Long
    Dim y As Long
    x = 12
    y = 10
    Debug.Print "And="; (x And y)
    Debug.Print "Or="; (x Or y)
    Debug.Print "Xor="; (x Xor y)
    
    ' 11. 字符串函数
    TestStringFuncs
    
    ' 12. 数组增强
    TestArrays
    
    ' 13. GoSub/Return
    GoSub Label1
    Debug.Print "AfterGoSub"
    GoTo SkipLabel1
Label1:
    Debug.Print "InGoSub"
    Return
SkipLabel1:
    
    ' 14. 退出For循环
    Dim j As Long
    Dim sum As Long
    sum = 0
    For j = 1 To 100
        sum = sum + j
        If j >= 10 Then
            Exit For
        End If
    Next j
    Debug.Print "ExitFor_sum="; sum
    Debug.Print "ExitFor_j="; j
    
    ' 15. Exit Do
    Dim k As Long
    k = 0
    Do
        k = k + 1
        If k = 7 Then
            Exit Do
        End If
    Loop
    Debug.Print "ExitDo="; k
    
    ' 16. 嵌套If
    Dim val1 As Long
    val1 = 42
    If val1 > 10 Then
        If val1 > 50 Then
            Debug.Print "Big"
        Else
            Debug.Print "Medium"
        End If
    Else
        Debug.Print "Small"
    End If
    
    ' 17. 类型转换
    Debug.Print "CLng(3.7)="; CLng(3.7)
    Debug.Print "CInt(2.3)="; CInt(2.3)
    Debug.Print "CDbl(5)="; CDbl(5)
    
    ' 18. 字符串比较
    Dim s1 As String
    Dim s2 As String
    s1 = "apple"
    s2 = "banana"
    If s1 < s2 Then
        Debug.Print "StrCmp_OK"
    End If
    
    Debug.Print "=== Compat Test End ==="
End Sub
