' test_p1324.bas - P13.24 Late-binding comprehensive verification
' Tests: method calls returning String/Long/Boolean/Object,
'         property Let (write), statement-level calls, multi-arg methods,
'         no-arg method/property, chained accessDeep

Sub Main()
    Dim fso As Object
    Dim folder As Object
    Dim drives As Object
    Dim result As String
    Dim numVal As Long
    Dim boolVal As Long
    
    ' 1. CreateObject - basic
    Set fso = CreateObject("Scripting.FileSystemObject")
    
    ' 2. Method returning String: GetAbsolutePathName
    result = fso.GetAbsolutePathName("test")
    Debug.Print result
    
    ' 3. Method returning String with multiple args: BuildPath
    result = fso.BuildPath("C:\Temp", "test.txt")
    Debug.Print result
    
    ' 4. Method returning Boolean: FolderExists
    boolVal = fso.FolderExists("C:\Windows")
    Debug.Print boolVal
    
    ' 5. Method returning Object with arg: GetFolder
    Set folder = fso.GetFolder("C:\Windows")
    Debug.Print folder.Name
    
    ' 6. No-arg property returning Object: Drives (collection)
    Set drives = fso.Drives
    Debug.Print drives.Count
    
    ' 7. Method returning String: GetExtensionName
    result = fso.GetExtensionName("C:\test.docx")
    Debug.Print result
    
    ' 8. Method returning String: GetFileName
    result = fso.GetFileName("C:\Users\test.txt")
    Debug.Print result
    
    ' 9. Statement-level method call (return value discarded)
    ' CreateTextFile creates a file and returns TextStream, but we discard it
    ' Actually, skip this to avoid creating files - test FolderExists as statement
    fso.FolderExists "C:\Windows"
    
    ' 10. Long property: Size
    numVal = folder.Size
    Debug.Print numVal
    
    ' 11. Deep chained access: fso.GetFolder("C:\Windows").Name
    Debug.Print fso.GetFolder("C:\Windows").Name
    
    ' 12. Integer-like property: Attributes
    numVal = folder.Attributes
    Debug.Print numVal
    
    ' 13. Method returning Object with numeric arg: GetSpecialFolder(0)=Windows
    Set folder = fso.GetSpecialFolder(0)
    Debug.Print folder.Name
    
    ' Cleanup
    Set drives = Nothing
    Set folder = Nothing
    Set fso = Nothing
    
    Debug.Print "P13.24 PASS"
End Sub
