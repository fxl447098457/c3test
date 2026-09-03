' M022 - benchmark module 022 (generated, conservative VB6 syntax)
Option Explicit

Public Function M022_Sum(ByVal n As Long) As Long
    Dim i As Long, acc As Long
    acc = 0
    For i = 1 To n
        acc = acc + i
    Next i
    M022_Sum = acc
End Function

Public Function M022_Fib(ByVal n As Long) As Long
    Dim a As Long, b As Long, t As Long, i As Long
    a = 0: b = 1
    For i = 2 To n
        t = a + b
        a = b
        b = t
    Next i
    If n <= 0 Then M022_Fib = 0 Else M022_Fib = b
End Function

Public Function M022_Concat(ByVal s As String, ByVal k As Long) As String
    Dim out As String, i As Long
    out = s
    For i = 1 To k
        out = out & "-" & CStr(i)
    Next i
    M022_Concat = out
End Function

Public Function M022_Avg(ByVal n As Long) As Double
    Dim i As Long, acc As Double
    acc = 0
    For i = 1 To n
        acc = acc + Sqr(i)
    Next i
    If n > 0 Then M022_Avg = acc / n Else M022_Avg = 0
End Function

Public Function M022_Pick(ByVal a As Long, ByVal b As Long, ByVal c As Long) As Long
    Dim m As Long
    m = a
    If b > m Then m = b
    If c > m Then m = c
    M022_Pick = m
End Function

Public Function M022_Run(ByVal n As Long) As Long
    Dim total As Long, s As String, d As Double
    total = M022_Sum(n) + M022_Fib(n) + M022_Pick(n, n * 2, n * 3)
    s = M022_Concat("m022", 3)
    d = M022_Avg(n)
    If Len(s) > 0 Then total = total + Len(s)
    If d > 0 Then total = total + CLng(d)
    M022_Run = total
End Function

Public Function M022_Identity(ByVal x As Long) As Long
    M022_Identity = x
End Function