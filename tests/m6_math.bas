' M6验证: 数学工具模块 (跨模块)
Attribute VB_Name = "MathHelper"

Option Explicit

Public Function Add(ByVal a As Long, ByVal b As Long) As Long
    Add = a + b
End Function

Public Function Multiply(ByVal a As Long, ByVal b As Long) As Long
    Multiply = a * b
End Function

Public Function Factorial(ByVal n As Long) As Long
    If n <= 1 Then
        Factorial = 1
    Else
        Factorial = n * Factorial(n - 1)
    End If
End Function

Public Sub SwapValues(ByRef a As Long, ByRef b As Long)
    Dim temp As Long
    temp = a
    a = b
    b = temp
End Sub

Public Function MakeGreeting(ByVal name As String) As String
    MakeGreeting = "Hello, " & name & "!"
End Function
