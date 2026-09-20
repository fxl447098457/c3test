' test_p24.bas - P24 COM Special Project regression tests
' P24-01: COM return value string context unwrapping
' P24-03: For Each COM collection enumeration (IEnumVARIANT)

Sub Main()
    Dim fso As Object
    Dim drives As Object
    Dim d As Object
    Dim i As Long
    Dim s As String
    Dim passCount As Long
    passCount = 0
    
    Set fso = CreateObject("Scripting.FileSystemObject")
    
    ' P24-01a: COM返回值在字符串拼接中正确解包 (VARIANT→BSTR)
    s = "Path:" & fso.GetAbsolutePathName(".")
    If Len(s) > 5 Then
        passCount = passCount + 1
        Debug.Print "P24-01a:OK"
    Else
        Debug.Print "P24-01a:FAIL len="; Len(s)
    End If
    
    ' P24-01b: COM返回值赋值给String变量后Len正确
    s = fso.GetAbsolutePathName(".")
    If Len(s) > 3 Then
        passCount = passCount + 1
        Debug.Print "P24-01b:OK"
    Else
        Debug.Print "P24-01b:FAIL len="; Len(s)
    End If
    
    ' P24-01c: 链式COM调用返回字符串, InStr验证包含期望内容
    s = fso.GetFolder("C:\Windows").Name
    If InStr(s, "Windows") > 0 Then
        passCount = passCount + 1
        Debug.Print "P24-01c:OK"
    Else
        Debug.Print "P24-01c:FAIL s="; s
    End If
    
    ' P24-03a: For Each COM集合枚举 (预赋值集合变量)
    Set drives = fso.Drives
    i = 0
    For Each d In drives
        i = i + 1
    Next
    If i > 0 Then
        passCount = passCount + 1
        Debug.Print "P24-03a:OK count="; i
    Else
        Debug.Print "P24-03a:FAIL"
    End If
    
    ' P24-03b: For Each 内联COM属性 + 循环体内COM属性访问
    i = 0
    Dim readyCount As Long
    readyCount = 0
    For Each d In fso.Drives
        i = i + 1
        If d.IsReady Then
            readyCount = readyCount + 1
        End If
    Next
    If i > 0 And readyCount > 0 Then
        passCount = passCount + 1
        Debug.Print "P24-03b:OK ready="; readyCount
    Else
        Debug.Print "P24-03b:FAIL"
    End If
    
    Debug.Print "P24:"; passCount; "/5"
End Sub
