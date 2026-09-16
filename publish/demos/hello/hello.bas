' VB6测试文件 - Hello World
' 用于验证词法分析器

Option Explicit

Public Sub Main()
    Dim name As String
    name = "World " & time
    
    If Len(name) > 0 Then
        ' MsgBox "Hello, " & name & "!", vbOKOnly, "Greeting"  ' 注释掉: 弹窗会阻塞无人值守运行
        Debug.Print "Hello, " & name & "!"
    Else
        Debug.Print "Name is empty"
    End If
    
    Dim i As Integer
    For i = 1 To 10
        Debug.Print "Count: "; i
    Next i
    
    Dim result As Long
    result = CalculateSum(100)
    Debug.Print "Sum = "; result
End Sub

Private Function CalculateSum(ByVal n As Long) As Long
    Dim sum As Long
    Dim i As Long
    sum = 0
    For i = 1 To n
        sum = sum + i
    Next i
    CalculateSum = sum
End Function
