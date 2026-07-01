Attribute VB_Name = "Module1"
Option Explicit

Sub TestDefaults()
    Dim n As Long
    n = TestNeg()
    n = TestNeg(-5)
    Dim s As String
    s = TestVbCr()
    s = TestVbCrLf()
    s = TestVbTab()
    s = TestVbNullString()
    s = TestVbCr("override")
End Sub

Function TestNeg(Optional ByVal n As Long = -1) As Long
    TestNeg = n
End Function

Function TestVbCr(Optional ByVal s As String = vbCr) As String
    TestVbCr = s
End Function

Function TestVbCrLf(Optional ByVal s As String = vbCrLf) As String
    TestVbCrLf = s
End Function

Function TestVbTab(Optional ByVal s As String = vbTab) As String
    TestVbTab = s
End Function

Function TestVbNullString(Optional ByVal s As String = vbNullString) As String
    TestVbNullString = s
End Function