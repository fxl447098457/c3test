' M050 - benchmark module 050 (generated, conservative VB6 syntax)
Option Explicit

Public Function M050_Sum(ByVal n As Long) As Long
    Dim i As Long, acc As Long
    acc = 0
    For i = 1 To n
        acc = acc + i
    Next i
    M050_Sum = acc
End Function

Public Function M050_Fib(ByVal n As Long) As Long
    Dim a As Long, b As Long, t As Long, i As Long
    a = 0: b = 1
    For i = 2 To n
        t = a + b
        a = b
        b = t
    Next i
    If n <= 0 Then M050_Fib = 0 Else M050_Fib = b
End Function

Public Function M050_Concat(ByVal s As String, ByVal k As Long) As String
    Dim out As String, i As Long
    out = s
    For i = 1 To k
        out = out & "-" & CStr(i)
    Next i
    M050_Concat = out
End Function

Public Function M050_Avg(ByVal n As Long) As Double
    Dim i As Long, acc As Double
    acc = 0
    For i = 1 To n
        acc = acc + Sqr(i)
    Next i
    If n > 0 Then M050_Avg = acc / n Else M050_Avg = 0
End Function

Public Function M050_Pick(ByVal a As Long, ByVal b As Long, ByVal c As Long) As Long
    Dim m As Long
    m = a
    If b > m Then m = b
    If c > m Then m = c
    M050_Pick = m
End Function

Public Function M050_Run(ByVal n As Long) As Long
    Dim total As Long, s As String, d As Double
    total = M050_Sum(n) + M050_Fib(n) + M050_Pick(n, n * 2, n * 3)
    s = M050_Concat("m050", 3)
    d = M050_Avg(n)
    If Len(s) > 0 Then total = total + Len(s)
    If d > 0 Then total = total + CLng(d)
    M050_Run = total
End Function

Public Function M050_Identity(ByVal x As Long) As Long
    M050_Identity = x
End Function