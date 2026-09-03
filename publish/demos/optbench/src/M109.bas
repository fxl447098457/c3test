' M109 - benchmark module 109 (generated, conservative VB6 syntax)
Option Explicit

Public Function M109_Sum(ByVal n As Long) As Long
    Dim i As Long, acc As Long
    acc = 0
    For i = 1 To n
        acc = acc + i
    Next i
    M109_Sum = acc
End Function

Public Function M109_Fib(ByVal n As Long) As Long
    Dim a As Long, b As Long, t As Long, i As Long
    a = 0: b = 1
    For i = 2 To n
        t = a + b
        a = b
        b = t
    Next i
    If n <= 0 Then M109_Fib = 0 Else M109_Fib = b
End Function

Public Function M109_Concat(ByVal s As String, ByVal k As Long) As String
    Dim out As String, i As Long
    out = s
    For i = 1 To k
        out = out & "-" & CStr(i)
    Next i
    M109_Concat = out
End Function

Public Function M109_Avg(ByVal n As Long) As Double
    Dim i As Long, acc As Double
    acc = 0
    For i = 1 To n
        acc = acc + Sqr(i)
    Next i
    If n > 0 Then M109_Avg = acc / n Else M109_Avg = 0
End Function

Public Function M109_Pick(ByVal a As Long, ByVal b As Long, ByVal c As Long) As Long
    Dim m As Long
    m = a
    If b > m Then m = b
    If c > m Then m = c
    M109_Pick = m
End Function

Public Function M109_Run(ByVal n As Long) As Long
    Dim total As Long, s As String, d As Double
    total = M109_Sum(n) + M109_Fib(n) + M109_Pick(n, n * 2, n * 3)
    s = M109_Concat("m109", 3)
    d = M109_Avg(n)
    If Len(s) > 0 Then total = total + Len(s)
    If d > 0 Then total = total + CLng(d)
    M109_Run = total
End Function

Public Function M109_Identity(ByVal x As Long) As Long
    M109_Identity = x
End Function