' test_com2.bas - P6.2 COM后期绑定完整测试
' 测试方法调用 + 属性读取 + Debug.Print + 链式调用

Sub Main()
    Dim fso As Object
    Dim folder As Object
    Dim name As String
    Dim path As String
    
    ' 创建FileSystemObject
    Set fso = CreateObject("Scripting.FileSystemObject")
    
    ' 测试COM方法调用 (返回Object) + 属性读取 (返回String)
    Set folder = fso.GetFolder("C:\Users")
    name = folder.Name
    path = folder.Path
    Debug.Print name
    Debug.Print path
    
    ' 测试COM方法返回整数属性
    Debug.Print folder.Attributes
    
    ' 测试链式COM调用: fso.GetFolder("C:\").Name
    Debug.Print fso.GetFolder("C:\Users").Name
    
    ' 释放对象
    Set folder = Nothing
    Set fso = Nothing
End Sub
