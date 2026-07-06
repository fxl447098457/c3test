' test_p24.bas - P24 COM Special Project regression tests
' P24-01: COM return value string context unwrapping
' P24-03: For Each COM collection enumeration (IEnumVARIANT)

Sub Main()
    Dim fso As Object
    Dim drives As Object
    Dim d As Object
    Dim i As Long
    Dim s As String
    
    ' P24-01: COM return value used in string concatenation context
    ' Before fix: COM call returning VARIANT(BSTR) was not unwrapped in string concat
    Set fso = CreateObject("Scripting.FileSystemObject")
    s = "Path: " & fso.GetAbsolutePathName(".")
    Debug.Print s
    
    ' P24-01: COM return value assigned to String variable
    s = fso.GetAbsolutePathName("C:")
    Debug.Print "AbsPath: " & s
    
    ' P24-03: For Each on COM collection (IEnumVARIANT)
    ' Before fix: For Each on COM collection didn't enumerate via IEnumVARIANT
    Set drives = fso.Drives
    i = 0
    For Each d In drives
        i = i + 1
    Next
    Debug.Print "Drives:"; i
    
    ' P24-01: Chained COM call in string concat context
    s = "Folder: " & fso.GetFolder("C:\Windows").Name
    Debug.Print s
    
    ' P24-03: For Each with COM object property access inside loop
    i = 0
    For Each d In fso.Drives
        If d.IsReady Then
            i = i + 1
        End If
    Next
    Debug.Print "Ready:"; i
    
    Debug.Print "P24 PASS"
End Sub
