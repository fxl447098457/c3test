' test_p1324.bas - P13.24 Late-binding comprehensive verification
' Tests: method calls returning String/Long/Boolean/Object,
'         property Get, statement-level calls, multi-arg methods,
'         no-arg property, chained access

Sub Main()
    Dim fso As Object
    Dim folder As Object
    Dim drives As Object
    Dim result As String
    Dim numVal As Long
    Dim passCount As Long
    passCount = 0
    
    Set fso = CreateObject("Scripting.FileSystemObject")
    
    ' 1. Method returning String: GetAbsolutePathName
    result = fso.GetAbsolutePathName("test")
    If Len(result) > 0 Then
        passCount = passCount + 1
        Debug.Print "P13-1:OK"
    Else
        Debug.Print "P13-1:FAIL"
    End If
    
    ' 2. Method returning String with multiple args: BuildPath
    result = fso.BuildPath("C:\Temp", "test.txt")
    If InStr(result, "test.txt") > 0 Then
        passCount = passCount + 1
        Debug.Print "P13-2:OK"
    Else
        Debug.Print "P13-2:FAIL"
    End If
    
    ' 3. Method returning Boolean: FolderExists
    numVal = fso.FolderExists("C:\Windows")
    If numVal <> 0 Then
        passCount = passCount + 1
        Debug.Print "P13-3:OK"
    Else
        Debug.Print "P13-3:FAIL"
    End If
    
    ' 4. Method returning Object with arg: GetFolder
    Set folder = fso.GetFolder("C:\Windows")
    result = folder.Name
    If InStr(result, "Windows") > 0 Then
        passCount = passCount + 1
        Debug.Print "P13-4:OK"
    Else
        Debug.Print "P13-4:FAIL"
    End If
    
    ' 5. No-arg property returning Object: Drives (collection)
    Set drives = fso.Drives
    numVal = drives.Count
    If numVal > 0 Then
        passCount = passCount + 1
        Debug.Print "P13-5:OK"
    Else
        Debug.Print "P13-5:FAIL"
    End If
    
    ' 6. Statement-level method call (return value discarded)
    fso.FolderExists "C:\Windows"
    passCount = passCount + 1
    Debug.Print "P13-6:OK"
    
    ' 7. Deep chained access: fso.GetFolder("C:\Windows").Name
    result = fso.GetFolder("C:\Windows").Name
    If InStr(result, "Windows") > 0 Then
        passCount = passCount + 1
        Debug.Print "P13-7:OK"
    Else
        Debug.Print "P13-7:FAIL"
    End If
    
    ' 8. Method returning Object with numeric arg: GetSpecialFolder(0)=Windows
    Set folder = fso.GetSpecialFolder(0)
    result = folder.Name
    If Len(result) > 0 Then
        passCount = passCount + 1
        Debug.Print "P13-8:OK"
    Else
        Debug.Print "P13-8:FAIL"
    End If
    
    ' Cleanup
    Set drives = Nothing
    Set folder = Nothing
    Set fso = Nothing
    
    Debug.Print "P13:"; passCount; "/8"
End Sub
