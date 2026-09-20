' test_com.bas - P6.1 COM基础设施测试
' 测试 CreateObject, Is Nothing, Set Nothing

Sub Main()
    Dim fso As Object
    Dim passCount As Long
    passCount = 0
    
    ' Test 1: CreateObject - 创建FileSystemObject
    Set fso = CreateObject("Scripting.FileSystemObject")
    If fso Is Nothing Then
        Debug.Print "COM-1:FAIL"
    Else
        passCount = passCount + 1
        Debug.Print "COM-1:OK"
    End If
    
    ' Test 2: Uninitialized object Is Nothing
    Dim obj As Object
    If obj Is Nothing Then
        passCount = passCount + 1
        Debug.Print "COM-2:OK"
    Else
        Debug.Print "COM-2:FAIL"
    End If
    
    ' Test 3: Set Nothing 释放COM对象
    Set fso = Nothing
    If fso Is Nothing Then
        passCount = passCount + 1
        Debug.Print "COM-3:OK"
    Else
        Debug.Print "COM-3:FAIL"
    End If
    
    Debug.Print "COM:"; passCount; "/3"
End Sub
