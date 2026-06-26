' test_earlybound.bas - P6.3 COM早期绑定端到端测试
Option Explicit

Sub Main()
    Dim fso As FileSystemObject
    
    ' 创建FSO对象 (早期绑定)
    Set fso = CreateObject("Scripting.FileSystemObject")
    
    ' 测试: 早期绑定方法调用 (FolderExists)
    If fso.FolderExists("C:\Windows") Then
        Debug.Print "C:\Windows exists"
    End If
    
    ' 测试: 早期绑定方法调用 (FileExists)
    If fso.FileExists("C:\Windows\System32\drivers\etc\hosts") Then
        Debug.Print "hosts file exists"
    End If
    
    ' 清理
    Set fso = Nothing
    
    Debug.Print "Early binding test OK"
End Sub
