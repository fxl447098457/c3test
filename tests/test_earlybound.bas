' test_earlybound.bas - P6.3 COM早期绑定端到端测试
Option Explicit

Sub Main()
    Dim fso As FileSystemObject
    Dim passCount As Long
    passCount = 0
    
    ' 创建FSO对象 (早期绑定)
    Set fso = CreateObject("Scripting.FileSystemObject")
    
    ' 测试: 早期绑定方法调用 (FolderExists)
    If fso.FolderExists("C:\Windows") Then
        passCount = passCount + 1
        Debug.Print "EB-1:OK"
    Else
        Debug.Print "EB-1:FAIL"
    End If
    
    ' 测试: 早期绑定方法调用 (FileExists)
    If fso.FileExists("C:\Windows\System32\drivers\etc\hosts") Then
        passCount = passCount + 1
        Debug.Print "EB-2:OK"
    Else
        Debug.Print "EB-2:FAIL"
    End If
    
    ' 清理
    Set fso = Nothing
    
    Debug.Print "EB:"; passCount; "/2"
End Sub
