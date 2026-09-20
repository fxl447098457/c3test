' test_not_com.bas - P24-07: Bug 4 test - Not operator on COM result
Option Explicit

Sub Main()
    Dim fso As FileSystemObject
    Set fso = CreateObject("Scripting.FileSystemObject")
    
    ' Test: Not on COM boolean result
    If Not fso.FileExists("C:\__nonexistent_file__") Then
        Debug.Print "NOT-COM:OK Not FileExists"
    Else
        Debug.Print "NOT-COM:FAIL"
    End If
    
    ' Test: Not on actual existing file
    If Not fso.FolderExists("C:\__nonexistent_folder__") Then
        Debug.Print "NOT-COM2:OK Not FolderExists"
    Else
        Debug.Print "NOT-COM2:FAIL"
    End If
    
    Set fso = Nothing
    Debug.Print "NOT-COM:PASS"
End Sub