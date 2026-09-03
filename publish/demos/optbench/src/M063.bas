' M063 - benchmark module 063 (generated, conservative VB6 syntax)
Option Explicit

Public Function M063_Sum(ByVal n As Long) As Long
    Dim i As Long, acc As Long
    acc = 0
    For i = 1 To n
        acc = acc + i
    Next i
    M063_Sum = acc
End Function

Public Function M063_Fib(ByVal n As Long) As Long
    Dim a As Long, b As Long, t As Long, i As Long
    a = 0: b = 1
    For i = 2 To n
        t = a + b
        a = b
        b = t
    Next i
    If n <= 0 Then M063_Fib = 0 Else M063_Fib = b
End Function

Public Function M063_Concat(ByVal s As String, ByVal k As Long) As String
    Dim out As String, i As Long
    out = s
    For i = 1 To k
        out = out & "-" & CStr(i)
    Next i
    M063_Concat = out
End Function

Public Function M063_Avg(ByVal n As Long) As Double
    Dim i As Long, acc As Double
    acc = 0
    For i = 1 To n
        acc = acc + Sqr(i)
    Next i
    If n > 0 Then M063_Avg = acc / n Else M063_Avg = 0
End Function

Public Function M063_Pick(ByVal a As Long, ByVal b As Long, ByVal c As Long) As Long
    Dim m As Long
    m = a
    If b > m Then m = b
    If c > m Then m = c
    M063_Pick = m
End Function

Public Function M063_Run(ByVal n As Long) As Long
    Dim total As Long, s As String, d As Double
    total = M063_Sum(n) + M063_Fib(n) + M063_Pick(n, n * 2, n * 3)
    s = M063_Concat("m063", 3)
    d = M063_Avg(n)
    If Len(s) > 0 Then total = total + Len(s)
    If d > 0 Then total = total + CLng(d)
    M063_Run = total
End Function

Public Function M063_Identity(ByVal x As Long) As Long
    M063_Identity = x
End Function