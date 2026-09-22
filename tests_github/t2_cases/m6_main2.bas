' m6_main2.bas - 测试 Module.Method 跨模块调用语法
Sub Main()
    Dim result As Long
    result = MathUtils.Add(10, 20)
    Debug.Print result
    result = MathUtils.Multiply(3, 7)
    Debug.Print result
End Sub
