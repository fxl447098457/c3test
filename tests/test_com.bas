' test_com.bas - P6.1 COM基础设施测试
' 测试 CreateObject, Set Nothing, IsNothing

Sub Main()
    ' Test 1: CreateObject - 创建FileSystemObject
    Dim fso As Object
    Set fso = CreateObject("Scripting.FileSystemObject")
    
    If fso Is Nothing Then
        Debug.Print "FAIL: CreateObject returned Nothing"
    Else
        Debug.Print "PASS: CreateObject succeeded"
    End If
    
    ' Test 2: IsNothing (通过Is运算符)
    Dim obj As Object
    If obj Is Nothing Then
        Debug.Print "PASS: Uninitialized object Is Nothing"
    Else
        Debug.Print "FAIL: Uninitialized object should be Nothing"
    End If
    
    ' Test 3: Set Nothing (释放COM对象)
    Set fso = Nothing
    
    If fso Is Nothing Then
        Debug.Print "PASS: Set Nothing released object"
    Else
        Debug.Print "FAIL: Object should be Nothing after Set Nothing"
    End If
    
    Debug.Print "COM basic tests completed"
End Sub
