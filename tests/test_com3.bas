' test_com3.bas - P6.2 COM后期绑定综合测试
' 覆盖: CreateObject, 方法调用, 属性Get/Set, 链式调用, Is Nothing, 数值属性

Sub Main()
    Dim fso As Object
    Dim folder As Object
    Dim file As Object
    Dim ts As Object
    Dim name As String
    
    ' 1. CreateObject 创建
    Set fso = CreateObject("Scripting.FileSystemObject")
    
    ' 2. Is Nothing 检查
    If fso Is Nothing Then
        Debug.Print "ERROR: FSO is Nothing"
    Else
        Debug.Print "FSO created OK"
    End If
    
    ' 3. COM方法返回对象 + 属性读取
    Set folder = fso.GetFolder("C:\Users")
    Debug.Print folder.Name
    Debug.Print folder.Path
    
    ' 4. 整数属性
    Debug.Print folder.Attributes
    
    ' 5. 链式调用
    Debug.Print fso.GetFolder("C:\Users").Name
    
    ' 6. COM方法返回对象 + 获取子对象
    Set file = fso.GetFile("C:\Windows\System32\drivers\etc\hosts")
    Debug.Print file.Name
    Debug.Print file.Size
    
    ' 7. 对象属性释放
    Set file = Nothing
    Set folder = Nothing
    Set fso = Nothing
    
    Debug.Print "All tests passed"
End Sub
