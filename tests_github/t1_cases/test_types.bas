' test_types.bas - VB6类型系统专项测试
' 覆盖: 所有基本数据类型、隐式转换、运算符优先级、
'        Boolean运算、字符串拼接、常量、静态变量
Attribute VB_Name = "TestTypes"

Option Explicit

' ===== 常量声明 =====
Const PI As Double = 3.14159265358979
Const MAX_SIZE As Long = 100
Const GREETING As String = "Hello"

' ===== 静态变量测试 =====
Private Function Counter() As Long
    Static cnt As Long
    cnt = cnt + 1
    Counter = cnt
End Function

' ===== 主测试入口 =====
Public Sub Main()
    Debug.Print "=== Types Test Start ==="

    ' 1. 基本数据类型
    Dim iVal As Integer
    Dim lVal As Long
    Dim sVal As Single
    Dim dVal As Double
    Dim bVal As Boolean
    Dim strVal As String
    
    iVal = 32767
    lVal = 2147483647
    sVal = 3.14
    dVal = 2.718281828
    bVal = True       ' VB6 True = -1
    strVal = "test"
    
    Debug.Print "Integer="; iVal
    Debug.Print "Long="; lVal
    Debug.Print "Single="; sVal
    Debug.Print "Double="; dVal
    Debug.Print "Boolean="; bVal
    Debug.Print "String="; strVal

    ' 2. Boolean运算
    Dim t As Boolean
    Dim f As Boolean
    t = True
    f = False
    Debug.Print "TrueVal="; t
    Debug.Print "FalseVal="; f
    Debug.Print "NotTrue="; (Not t)
    
    If t And Not f Then
        Debug.Print "BoolLogic_OK"
    End If

    ' 3. 运算符优先级
    Dim result As Long
    result = 2 + 3 * 4       ' 14, 乘法优先
    Debug.Print "Priority1="; result
    result = (2 + 3) * 4     ' 20, 括号优先
    Debug.Print "Priority2="; result
    result = 10 \ 3          ' 3, 整除
    Debug.Print "IntDiv="; result
    result = 10 Mod 3        ' 1, 取模
    Debug.Print "ModOp="; result
    result = 2 ^ 10          ' 1024, 幂运算
    Debug.Print "Power="; result

    ' 4. 字符串拼接
    Dim s As String
    s = "Hello" & " " & "World"
    Debug.Print "Concat="; s
    s = s & "!"
    Debug.Print "Concat2="; s

    ' 5. 隐式类型转换
    Dim mixed As Long
    mixed = 3.7              ' Double -> Long, 截断为3
    Debug.Print "Implicit1="; mixed
    mixed = CLng(3.7)        '四舍五入为4
    Debug.Print "Implicit2="; mixed

    ' 6. 常量使用
    Debug.Print "PI="; PI
    Debug.Print "MAX_SIZE="; MAX_SIZE
    Debug.Print "GREETING="; GREETING

    ' 7. 静态变量(多次调用同一函数)
    Debug.Print "Static1="; Counter()
    Debug.Print "Static2="; Counter()
    Debug.Print "Static3="; Counter()

    ' 8. 比较运算符
    If 5 > 3 Then
        Debug.Print "GT_OK"
    End If
    If 3 >= 3 Then
        Debug.Print "GE_OK"
    End If
    If 2 < 5 Then
        Debug.Print "LT_OK"
    End If
    If 5 <= 5 Then
        Debug.Print "LE_OK"
    End If
    If 5 = 5 Then
        Debug.Print "EQ_OK"
    End If
    If 5 <> 3 Then
        Debug.Print "NE_OK"
    End If

    ' 9. 负数运算
    Dim neg As Long
    neg = -42
    Debug.Print "Neg="; neg
    neg = -neg
    Debug.Print "NegNeg="; neg

    ' 10. 嵌套函数调用
    Dim absVal As Long
    absVal = Abs(-99)
    Debug.Print "Abs="; absVal
    Debug.Print "Sqr="; Sqr(144)

    ' 11. 字符串长度和索引
    Dim msg As String
    msg = "ABCDEF"
    Debug.Print "Len="; Len(msg)
    Debug.Print "Asc="; Asc(msg)

    ' 12. Hex/Oct
    Debug.Print "Hex255="; Hex(255)
    Debug.Print "Oct8="; Oct(8)

    Debug.Print "=== Types Test End ==="
End Sub
