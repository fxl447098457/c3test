' P4 Array Test - SAFEARRAY support
Option Explicit

Public Sub Main()
    Debug.Print "=== Array Tests ==="
    
    ' 静态数组 Dim arr(5) As Long
    Dim arr(5) As Long
    
    ' 赋值
    Dim i As Long
    For i = 0 To 5
        arr(i) = i * 10
    Next i
    
    ' 读取
    Debug.Print "arr(0)="; arr(0)
    Debug.Print "arr(3)="; arr(3)
    Debug.Print "arr(5)="; arr(5)
    Debug.Print "UBound="; UBound(arr)
    Debug.Print "LBound="; LBound(arr)
    
    ' 指定下界的数组
    Dim scores(1 To 3) As Long
    scores(1) = 85
    scores(2) = 92
    scores(3) = 78
    Debug.Print "scores(2)="; scores(2)
    Debug.Print "scoresUB="; UBound(scores)
    Debug.Print "scoresLB="; LBound(scores)
    
    ' 字符串数组
    Dim names(2) As String
    names(0) = "Alice"
    names(1) = "Bob"
    names(2) = "Charlie"
    Debug.Print "names(1)="; names(1)
    
    Debug.Print "=== Array Tests PASSED ==="
End Sub
