' P24-10: COM默认属性测试
' 验证: obj(args) 等价于 obj.Item(args) (前期绑定, 只读)
' DISPID_VALUE=0标识默认成员

Option Explicit

Sub Main()
    Dim passCount As Long
    passCount = 0
    
    ' 测试1: 前期绑定 Dictionary — Dim d As Dictionary
    ' dict(key) 应等价于 dict.Item(key)
    Dim d As Dictionary
    Set d = CreateObject("Scripting.Dictionary")
    d.Add "name", "Alice"
    d.Add "age", "30"
    
    ' 使用默认属性语法: d("name") 而非 d.Item("name")
    Dim v1 As String
    v1 = d("name")
    If v1 = "Alice" Then
        passCount = passCount + 1
        Debug.Print "DP-1:OK dict(""name"") = Alice"
    Else
        Debug.Print "DP-1:FAIL dict(""name"") expected Alice, got "; v1
    End If
    
    ' 使用默认属性语法: d("age")
    Dim v2 As String
    v2 = d("age")
    If v2 = "30" Then
        passCount = passCount + 1
        Debug.Print "DP-2:OK dict(""age"") = 30"
    Else
        Debug.Print "DP-2:FAIL dict(""age"") expected 30, got "; v2
    End If
    
    ' 对比: 显式 .Item 语法应产生相同结果
    Dim v3 As String
    v3 = d.Item("name")
    If v3 = v1 Then
        passCount = passCount + 1
        Debug.Print "DP-3:OK dict.Item(""name"") = dict(""name"")"
    Else
        Debug.Print "DP-3:FAIL dict.Item != dict()"
    End If
    
    ' 测试2: 不同类型的值
    d.Add "count", "42"
    Dim v4 As String
    v4 = d("count")
    If v4 = "42" Then
        passCount = passCount + 1
        Debug.Print "DP-4:OK dict(""count"") = 42"
    Else
        Debug.Print "DP-4:FAIL dict(""count"") expected 42, got "; v4
    End If
    
    Debug.Print "P24-10: "; passCount; "/4 passed"
End Sub
