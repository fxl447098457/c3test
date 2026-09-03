' M090 - benchmark module 090 (generated, conservative VB6 syntax)
Option Explicit

Public Function M090_Sum(ByVal n As Long) As Long
    Dim i As Long, acc As Long
    acc = 0
    For i = 1 To n
        acc = acc + i
    Next i
    M090_Sum = acc
End Function

Public Function M090_Fib(ByVal n As Long) As Long
    Dim a As Long, b As Long, t As Long, i As Long
    a = 0: b = 1
    For i = 2 To n
        t = a + b
        a = b
        b = t
    Next i
    If n <= 0 Then M090_Fib = 0 Else M090_Fib = b
End Function

Public Function M090_Concat(ByVal s As String, ByVal k As Long) As String
    Dim out As String, i As Long
    out = s
    For i = 1 To k
        out = out & "-" & CStr(i)
    Next i
    M090_Concat = out
End Function

Public Function M090_Avg(ByVal n As Long) As Double
    Dim i As Long, acc As Double
    acc = 0
    For i = 1 To n
        acc = acc + Sqr(i)
    Next i
    If n > 0 Then M090_Avg = acc / n Else M090_Avg = 0
End Function

Public Function M090_Pick(ByVal a As Long, ByVal b As Long, ByVal c As Long) As Long
    Dim m As Long
    m = a
    If b > m Then m = b
    If c > m Then m = c
    M090_Pick = m
End Function

Public Function M090_Run(ByVal n As Long) As Long
    Dim total As Long, s As String, d As Double
    total = M090_Sum(n) + M090_Fib(n) + M090_Pick(n, n * 2, n * 3)
    s = M090_Concat("m090", 3)
    d = M090_Avg(n)
    If Len(s) > 0 Then total = total + Len(s)
    If d > 0 Then total = total + CLng(d)
    M090_Run = total
End Function

Public Function M090_Identity(ByVal x As Long) As Long
    M090_Identity = x
End Function