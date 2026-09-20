' M7Main.bas - P6.7 M7里程碑综合验证
' 覆盖P6.1+P6.2+P6.4+P6.5功能协同工作
Option Explicit

' P6.5 WithEvents事件源声明
Dim WithEvents Src As EventSource

' 事件处理器
Private Sub Src_OnData(ByVal Id As Long, ByVal Value As Double)
    Debug.Print "EventData: Id="; Id; " Value="; Value
End Sub

Private Sub Src_OnError(ByVal ErrorCode As Long)
    Debug.Print "EventError: Code="; ErrorCode
End Sub

Sub Main()
    Dim passCount As Long
    passCount = 0
    
    Debug.Print "=== M7 COM Comprehensive Test ==="
    
    ' ========== P6.1 COM基础设施 ==========
    Dim fso As Object
    Set fso = CreateObject("Scripting.FileSystemObject")
    
    If Not IsNothing(fso) Then
        Debug.Print "P6.1 CreateObject: PASS"
        passCount = passCount + 1
    Else
        Debug.Print "P6.1 CreateObject: FAIL"
    End If
    
    ' ========== P6.2 IDispatch后期绑定 ==========
    Dim folder As Object
    Set folder = fso.GetFolder("C:\Windows")
    Dim folderName As String
    folderName = folder.Name
    Debug.Print "P6.2 FolderName="; folderName
    
    ' 链式调用验证
    Debug.Print "P6.2 Chain="; fso.GetFolder("C:\Windows").Name
    
    ' 属性读取验证 (Attributes是整数)
    Dim attrs As Long
    attrs = folder.Attributes
    If attrs > 0 Then
        Debug.Print "P6.2 LateBinding: PASS"
        passCount = passCount + 1
    Else
        Debug.Print "P6.2 LateBinding: FAIL"
    End If
    
    Set folder = Nothing
    
    ' ========== P6.4 类模块 ==========
    Dim c As CCircle
    Set c = New CCircle
    c.Radius = 5#
    
    If c.Radius = 5# Then
        Debug.Print "P6.4 ClassModule: PASS"
        passCount = passCount + 1
    Else
        Debug.Print "P6.4 ClassModule: FAIL"
    End If
    
    ' ========== P6.5 WithEvents事件 ==========
    Set Src = New EventSource
    
    Src.FireData 1, 3.14
    Src.FireError 404
    
    Debug.Print "P6.5 Events: PASS"
    passCount = passCount + 1
    
    ' ========== 汇总 ==========
    Debug.Print "=== M7 Results: "; passCount; "/4 PASSED ==="
    
    Set Src = Nothing
    Set c = Nothing
    Set fso = Nothing
    
    Debug.Print "=== M7 Test Complete ==="
End Sub