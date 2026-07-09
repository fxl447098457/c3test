' test_earlybound2.bas - P24-07: step-by-step COM test
Option Explicit

Sub Main()
    Dim fso As FileSystemObject
    Dim passCount As Long
    passCount = 0
    
    Set fso = CreateObject("Scripting.FileSystemObject")
    
    ' Test 1: FolderExists
    If fso.FolderExists("C:\Windows") Then
        passCount = passCount + 1
        Debug.Print "EB2-1:OK FolderExists"
    End If
    
    ' Test 2: FileExists
    If fso.FileExists("C:\Windows\System32\drivers\etc\hosts") Then
        passCount = passCount + 1
        Debug.Print "EB2-2:OK FileExists"
    End If
    
    ' Test 3: BuildPath (2-param method, String return)
    Dim bp As String
    bp = fso.BuildPath("C:\Windows", "System32")
    Debug.Print "EB2-3:BuildPath=["; bp; "]"
    If bp = "C:\Windows\System32" Then
        passCount = passCount + 1
    End If
    
    ' Test 4: GetExtensionName
    Dim ext As String
    ext = fso.GetExtensionName("C:\test\file.txt")
    Debug.Print "EB2-4:Ext=["; ext; "]"
    If ext = "txt" Then
        passCount = passCount + 1
    End If
    
    ' Test 5: GetFileName
    Dim fn As String
    fn = fso.GetFileName("C:\Windows\System32\cmd.exe")
    Debug.Print "EB2-5:FileName=["; fn; "]"
    If fn = "cmd.exe" Then
        passCount = passCount + 1
    End If
    
    ' Test 6: GetAbsolutePathName
    Dim absPath As String
    absPath = fso.GetAbsolutePathName(".")
    Debug.Print "EB2-6:AbsPath=["; absPath; "]"
    If Len(absPath) > 0 Then
        passCount = passCount + 1
    End If
    
    ' Test 7: GetDrive + DriveType (Long prop)
    Dim drv As Drive
    Set drv = fso.GetDrive("C")
    Dim dt As Long
    dt = drv.DriveType
    Debug.Print "EB2-7:DriveType="; dt
    If dt >= 0 And dt <= 5 Then
        passCount = passCount + 1
    End If
    
    ' Test 8: Drive.IsReady (Boolean prop)
    If drv.IsReady Then
        passCount = passCount + 1
        Debug.Print "EB2-8:OK IsReady"
    End If
    
    ' Test 9: CreateTextFile + WriteLine + Close
    Dim ts As TextStream
    Set ts = fso.CreateTextFile("C:\Users\Public\c3_eb2_test.txt")
    ts.WriteLine "Hello C3"
    ts.Close
    passCount = passCount + 1
    Debug.Print "EB2-9:OK CreateTextFile"
    
    ' Test 10: Read back
    Set ts = fso.OpenTextFile("C:\Users\Public\c3_eb2_test.txt", 1)
    Dim content As String
    content = ts.ReadAll
    ts.Close
    If Len(content) > 0 Then
        passCount = passCount + 1
        Debug.Print "EB2-10:OK ReadAll"
    End If
    
    ' Cleanup
    fso.DeleteFile "C:\Users\Public\c3_eb2_test.txt"
    
    Set ts = Nothing
    Set drv = Nothing
    Set fso = Nothing
    
    Debug.Print "EB2:"; passCount; "/10"
End Sub
