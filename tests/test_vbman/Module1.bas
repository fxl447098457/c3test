Attribute VB_Name = "Module1"
' P24-04: VB_GlobalNameSpace regression test
' Tests: VBMAN.Version() using GlobalNameSpace feature from VBMANLIB

Sub Main()
    Dim v As String
    
    ' VBMAN is a GlobalNameSpace class from VBMANLIB
    ' VBMAN() method on sGlobal creates cVBMAN, .Version() returns version string
    v = VBMAN.Version()
    Debug.Print "VBMAN: "; v
    
    ' Also test creating cVBMAN directly via CreateObject
    Dim vm As Object
    Set vm = CreateObject("VBMANLIB.cVBMAN")
    Debug.Print "cVBMAN: "; vm.Version()
    
    Debug.Print "P24-04 PASS"
End Sub
