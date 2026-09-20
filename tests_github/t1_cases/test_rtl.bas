' P4 RTL 测试 - 字符串/数学/日期
Option Explicit

Public Sub Main()
    ' ---- 字符串函数 ----
    Dim s As String
    s = "Hello World Hello"
    
    Debug.Print "=== String Functions ==="
    Debug.Print "Len: "; Len(s)
    Debug.Print "Left: "; Left(s, 5)
    Debug.Print "Right: "; Right(s, 5)
    Debug.Print "Mid: "; Mid(s, 7, 5)
    Debug.Print "UCase: "; UCase(s)
    Debug.Print "LCase: "; LCase(s)
    
    ' ---- 数学函数 ----
    Debug.Print "=== Math Functions ==="
    Dim angle As Double
    angle = 3.14159265358979 / 4
    Debug.Print "Sin="; CInt(Sin(angle) * 1000)
    Debug.Print "Cos="; CInt(Cos(angle) * 1000)
    Debug.Print "Sqr2="; CInt(Sqr(2) * 1000)
    Debug.Print "Abs42="; Abs(-42)
    Debug.Print "Int37="; Int(3.7)
    Debug.Print "FixM37="; Fix(-3.7)
    Debug.Print "Exp1="; CInt(Exp(1) * 1000)
    Debug.Print "LogE="; CInt(Log(2.71828) * 1000)
    
    ' ---- 日期时间 ----
    Debug.Print "=== Date/Time ==="
    Debug.Print "Year="; Year(Now)
    Debug.Print "Month="; Month(Now)
    Debug.Print "Day="; Day(Now)
    Debug.Print "Hour="; Hour(Now)
    Debug.Print "Minute="; Minute(Now)
    Debug.Print "Second="; Second(Now)
    
    ' ---- 类型转换 ----
    Debug.Print "=== Conversion ==="
    Debug.Print "Hex255="; Hex(255)
    Debug.Print "Oct8="; Oct(8)
    
    ' ---- 数值转换 ----
    Dim v As Long
    v = 42
    Debug.Print "CLng="; CLng(v)
    Debug.Print "CInt="; CInt(v)
    
    Debug.Print "=== ALL TESTS PASSED ==="
End Sub
