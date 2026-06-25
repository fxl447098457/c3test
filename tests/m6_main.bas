' M6验证: 综合工程主模块
' 覆盖: 跨模块调用、UDT、Enum、ByRef、递归、字符串、文件I/O、错误处理
Attribute VB_Name = "MainApp"

Option Explicit

Private Type Config
    Name As String
    MaxCount As Long
End Type

Private Enum LogLevel
    LogDebug = 0
    LogInfo = 1
    LogWarning = 2
    LogError = 3
End Enum

Private Sub PrintConfig(ByRef cfg As Config)
    Debug.Print "Config: MaxCount="; cfg.MaxCount
End Sub

Public Sub Main()
    Debug.Print "=== M6 Milestone Verification ==="
    
    ' 1. UDT + Enum
    Dim cfg As Config
    cfg.MaxCount = 100
    cfg.Name = "TestApp"
    PrintConfig cfg
    
    Dim level As Long
    level = LogInfo
    Debug.Print "LogLevel="; level
    
    ' 2. 跨模块调用 (直接使用Public函数名, 不需要模块前缀)
    Dim sum As Long
    sum = Add(10, 20)
    Debug.Print "Add(10,20)="; sum
    
    Dim prod As Long
    prod = Multiply(5, 6)
    Debug.Print "Multiply(5,6)="; prod
    
    ' 3. 递归
    Dim fact As Long
    fact = Factorial(5)
    Debug.Print "Factorial(5)="; fact
    
    ' 4. ByRef
    Dim x As Long
    Dim y As Long
    x = 100
    y = 200
    SwapValues x, y
    Debug.Print "AfterSwap x="; x
    Debug.Print "AfterSwap y="; y
    
    ' 5. 字符串操作
    Dim greeting As String
    greeting = MakeGreeting("World")
    Debug.Print "Greeting="; greeting
    
    ' 6. 文件I/O
    Dim fnum As Long
    fnum = FreeFile
    Open "m6_test.txt" For Output As fnum
    Print #fnum, "M6 Test Data"
    Print #fnum, "Sum="; sum
    Close #fnum
    
    Dim line As String
    fnum = FreeFile
    Open "m6_test.txt" For Input As fnum
    Line Input #fnum, line
    Debug.Print "Read:"; line
    Close #fnum
    
    ' 7. 错误处理
    On Error Resume Next
    Open "nonexistent_m6.xyz" For Input As fnum
    Debug.Print "ErrorHandled"
    On Error GoTo 0
    
    ' 8. 数组
    Dim arr() As Long
    ReDim arr(5)
    Dim i As Long
    For i = 0 To 5
        arr(i) = i * i
    Next i
    Debug.Print "arr(5)="; arr(5)
    
    ' 9. 逻辑运算
    Dim flags As Long
    flags = LogInfo Or LogWarning
    Debug.Print "Flags="; flags
    
    Debug.Print "=== M6 PASSED ==="
End Sub
