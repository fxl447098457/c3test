' M056 - benchmark module 056 (generated, conservative VB6 syntax)
Option Explicit

Public Function M056_Sum(ByVal n As Long) As Long
    Dim i As Long, acc As Long
    acc = 0
    For i = 1 To n
        acc = acc + i
    Next i
    M056_Sum = acc
End Function

Public Function M056_Fib(ByVal n As Long) As Long
    Dim a As Long, b As Long, t As Long, i As Long
    a = 0: b = 1
    For i = 2 To n
        t = a + b
        a = b
        b = t
    Next i
    If n <= 0 Then M056_Fib = 0 Else M056_Fib = b
End Function

Public Function M056_Concat(ByVal s As String, ByVal k As Long) As String
    Dim out As String, i As Long
    out = s
    For i = 1 To k
        out = out & "-" & CStr(i)
    Next i
    M056_Concat = out
End Function

Public Function M056_Avg(ByVal n As Long) As Double
    Dim i As Long, acc As Double
    acc = 0
    For i = 1 To n
        acc = acc + Sqr(i)
    Next i
    If n > 0 Then M056_Avg = acc / n Else M056_Avg = 0
End Function

Public Function M056_Pick(ByVal a As Long, ByVal b As Long, ByVal c As Long) As Long
    Dim m As Long
    m = a
    If b > m Then m = b
    If c > m Then m = c
    M056_Pick = m
End Function

Public Function M056_Run(ByVal n As Long) As Long
    Dim total As Long, s As String, d As Double
    total = M056_Sum(n) + M056_Fib(n) + M056_Pick(n, n * 2, n * 3)
    s = M056_Concat("m056", 3)
    d = M056_Avg(n)
    If Len(s) > 0 Then total = total + Len(s)
    If d > 0 Then total = total + CLng(d)
    M056_Run = total
End Function

Public Function M056_Identity(ByVal x As Long) As Long
    M056_Identity = x
End Function