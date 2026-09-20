' test_com3.bas - P6.2 COM后期绑定综合测试
' 覆盖: CreateObject, 方法调用, 属性Get, 链式调用, 整数属性

Sub Main()
    Dim fso As Object
    Dim folder As Object
    Dim s As String
    Dim passCount As Long
    passCount = 0
    
    Set fso = CreateObject("Scripting.FileSystemObject")
    
    ' Test 1: COM方法返回对象 + 属性读取
    Set folder = fso.GetFolder("C:\Users")
    s = folder.Name
    If Len(s) > 0 Then
        passCount = passCount + 1
        Debug.Print "COM3-1:OK"
    Else
        Debug.Print "COM3-1:FAIL"
    End If
    
    ' Test 2: 链式调用
    s = fso.GetFolder("C:\Windows").Name
    If InStr(s, "Windows") > 0 Then
        passCount = passCount + 1
        Debug.Print "COM3-2:OK"
    Else
        Debug.Print "COM3-2:FAIL"
    End If
    
    ' Test 3: 整数属性
    Dim attr As Long
    attr = folder.Attributes
    If attr <> 0 Then
        passCount = passCount + 1
        Debug.Print "COM3-3:OK"
    Else
        Debug.Print "COM3-3:FAIL"
    End If
    
    ' Test 4: 对象释放
    Set folder = Nothing
    Set fso = Nothing
    If fso Is Nothing Then
        passCount = passCount + 1
        Debug.Print "COM3-4:OK"
    Else
        Debug.Print "COM3-4:FAIL"
    End If
    
    Debug.Print "COM3:"; passCount; "/4"
End Sub
