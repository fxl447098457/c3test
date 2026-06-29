' P14.3.2: 嵌套Exit For测试
Sub Main()
    ' 基本Exit For (内层)
    Dim i As Long
    Dim j As Long
    Dim count As Long
    count = 0
    For i = 1 To 3
        For j = 1 To 5
            If j = 3 Then Exit For
            count = count + 1
        Next j
    Next i
    ' 每次外层循环内层执行2次(1,2)后Exit For, 共3*2=6
    Debug.Print "Nested Exit For count="; count
    
    ' Exit Do inside For
    count = 0
    For i = 1 To 3
        Do While True
            count = count + 1
            Exit Do
        Loop
    Next i
    ' 每次For循环Do执行1次后Exit Do, 共3
    Debug.Print "Exit Do in For count="; count
    
    ' Exit For inside Do
    count = 0
    i = 0
    Do While i < 2
        i = i + 1
        For j = 1 To 10
            If j = 2 Then Exit For
            count = count + 1
        Next j
    Loop
    ' 每次Do循环For执行1次(1)后Exit For, 共2
    Debug.Print "Exit For in Do count="; count
    
    Debug.Print "Nested Exit test PASSED"
End Sub
