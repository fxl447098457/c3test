' M6 verify: comprehensive project main module
' Coverage: cross-module calls, UDT, Enum, ByRef, recursion, strings, file I/O, error handling
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

    Dim cfg As Config
    cfg.MaxCount = 100
    cfg.Name = "TestApp"
    PrintConfig cfg

    Dim level As Long
    level = LogInfo
    Debug.Print "LogLevel="; level

    Dim sum As Long
    sum = Add(10, 20)
    Debug.Print "Add(10,20)="; sum

    Dim prod As Long
    prod = Multiply(5, 6)
    Debug.Print "Multiply(5,6)="; prod

    Dim fact As Long
    fact = Factorial(5)
    Debug.Print "Factorial(5)="; fact

    Dim x As Long
    Dim y As Long
    x = 100
    y = 200
    SwapValues x, y
    Debug.Print "AfterSwap x="; x
    Debug.Print "AfterSwap y="; y

    Dim greeting As String
    greeting = MakeGreeting("World")
    Debug.Print "Greeting="; greeting

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

    On Error Resume Next
    Open "nonexistent_m6.xyz" For Input As fnum
    Debug.Print "ErrorHandled"
    On Error GoTo 0

    Dim arr() As Long
    ReDim arr(5)
    Dim i As Long
    For i = 0 To 5
        arr(i) = i * i
    Next i
    Debug.Print "arr(5)="; arr(5)

    Dim flags As Long
    flags = LogInfo Or LogWarning
    Debug.Print "Flags="; flags

    ' Conditional assertions
    If sum = 30 Then
        Debug.Print "M6A:OK"
    Else
        Debug.Print "M6A:FAIL"
    End If

    If fact = 120 Then
        Debug.Print "M6B:OK"
    Else
        Debug.Print "M6B:FAIL"
    End If

    If x = 200 And y = 100 Then
        Debug.Print "M6C:OK"
    Else
        Debug.Print "M6C:FAIL"
    End If

    If flags = 3 Then
        Debug.Print "M6D:OK"
    Else
        Debug.Print "M6D:FAIL"
    End If

    Debug.Print "=== M6 PASSED ==="
End Sub