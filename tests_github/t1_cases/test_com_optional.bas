' P24-11: COM Optional参数测试
' 验证: 省略COM方法的Optional参数时, IDispatch::Invoke自动使用默认值

Option Explicit

Sub Main()
    Dim passCount As Long
    passCount = 0
    
    Dim fso As FileSystemObject
    Set fso = CreateObject("Scripting.FileSystemObject")
    
    ' 测试1: BuildPath 传2个必要参数 (无省略)
    Dim bp As String
    bp = fso.BuildPath("C:\Windows", "System32")
    If bp = "C:\Windows\System32" Then
        passCount = passCount + 1
        Debug.Print "OP-1:OK BuildPath = "; bp
    Else
        Debug.Print "OP-1:FAIL BuildPath expected C:\Windows\System32, got "; bp
    End If
    
    ' 测试2: GetSpecialFolder 只传1个参数
    Dim sf As Object
    Set sf = fso.GetSpecialFolder(0)
    If Not sf Is Nothing Then
        passCount = passCount + 1
        Debug.Print "OP-2:OK GetSpecialFolder(0)"
    Else
        Debug.Print "OP-2:FAIL GetSpecialFolder(0) returned Nothing"
    End If
    
    ' 测试3: OpenTextFile 只传1个必要参数 (省略3个Optional)
    ' IOMode默认=1(ForReading), Create默认=False, Format默认=0(TristateFalse)
    Dim ts As Object
    Set ts = fso.OpenTextFile("C:\Windows\System32\drivers\etc\hosts")
    If Not ts Is Nothing Then
        passCount = passCount + 1
        Debug.Print "OP-3:OK OpenTextFile with 1 arg (3 optional omitted)"
        ' 读取第一行验证
        Dim line As String
        line = ts.ReadLine
        If Len(line) > 0 Then
            passCount = passCount + 1
            Debug.Print "OP-4:OK ReadLine returned data"
        Else
            Debug.Print "OP-4:FAIL ReadLine returned empty"
        End If
    Else
        Debug.Print "OP-3:FAIL OpenTextFile returned Nothing"
    End If
    
    Debug.Print "P24-11: "; passCount; "/4 passed"
End Sub
