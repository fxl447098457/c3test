' 语义分析测试 - 无内置函数依赖
Option Explicit

Public x As Long
Private Const MAX_VALUE As Long = 100

Public Sub TestSem()
    Dim a As Long
    Dim b As Integer
    a = 42
    b = 10
    
    Dim c As Long
    c = a + b
    
    If c > 50 Then
        Dim d As String
        d = "big"
    Else
        Dim e As Double
        e = 3.14
    End If
    
    Dim result As Long
    result = AddNumbers(a, b)
End Sub

Private Function AddNumbers(ByVal x As Long, ByVal y As Long) As Long
    AddNumbers = x + y
End Function
