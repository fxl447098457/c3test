' M039 - benchmark module 039 (generated, conservative VB6 syntax)
Option Explicit

Public Function M039_Sum(ByVal n As Long) As Long
    Dim i As Long, acc As Long
    acc = 0
    For i = 1 To n
        acc = acc + i
    Next i
    M039_Sum = acc
End Function

Public Function M039_Fib(ByVal n As Long) As Long
    Dim a As Long, b As Long, t As Long, i As Long
    a = 0: b = 1
    For i = 2 To n
        t = a + b
        a = b
        b = t
    Next i
    If n <= 0 Then M039_Fib = 0 Else M039_Fib = b
End Function

Public Function M039_Concat(ByVal s As String, ByVal k As Long) As String
    Dim out As String, i As Long
    out = s
    For i = 1 To k
        out = out & "-" & CStr(i)
    Next i
    M039_Concat = out
End Function

Public Function M039_Avg(ByVal n As Long) As Double
    Dim i As Long, acc As Double
    acc = 0
    For i = 1 To n
        acc = acc + Sqr(i)
    Next i
    If n > 0 Then M039_Avg = acc / n Else M039_Avg = 0
End Function

Public Function M039_Pick(ByVal a As Long, ByVal b As Long, ByVal c As Long) As Long
    Dim m As Long
    m = a
    If b > m Then m = b
    If c > m Then m = c
    M039_Pick = m
End Function

Public Function M039_Run(ByVal n As Long) As Long
    Dim total As Long, s As String, d As Double
    total = M039_Sum(n) + M039_Fib(n) + M039_Pick(n, n * 2, n * 3)
    s = M039_Concat("m039", 3)
    d = M039_Avg(n)
    If Len(s) > 0 Then total = total + Len(s)
    If d > 0 Then total = total + CLng(d)
    M039_Run = total
End Function

Public Function M039_Identity(ByVal x As Long) As Long
    M039_Identity = x
End Function