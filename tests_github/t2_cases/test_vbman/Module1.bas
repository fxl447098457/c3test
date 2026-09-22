Attribute VB_Name = "Module1"
' P24-04: VB_GlobalNameSpace regression test
' Tests: VBMAN.Version() using GlobalNameSpace feature from VBMANLIB

Sub Main()
    Dim v As String
    Dim passCount As Long
    passCount = 0
    
    ' VBMAN is a GlobalNameSpace class from VBMANLIB
    ' VBMAN() method on sGlobal creates cVBMAN, .Version() returns version string
    v = VBMAN.Version()
    If Len(v) > 0 Then
        passCount = passCount + 1
        Debug.Print "P24-04a:OK"
    Else
        Debug.Print "P24-04a:FAIL"
    End If
    
    ' Also test creating cVBMAN directly via CreateObject
    Dim vm As Object
    Set vm = CreateObject("VBMANLIB.cVBMAN")
    Dim v2 As String
    v2 = vm.Version()
    If Len(v2) > 0 Then
        passCount = passCount + 1
        Debug.Print "P24-04b:OK"
    Else
        Debug.Print "P24-04b:FAIL"
    End If
    
    Debug.Print "P24-04:"; passCount; "/2"
End Sub
