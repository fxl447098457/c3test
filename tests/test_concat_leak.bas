' P6.12.1 - BSTR Concat leak test
' Tests: a & b & c should use ConcatFree for intermediate result

Option Explicit

Public Sub Main()
    Dim s1 As String
    Dim s2 As String
    Dim s3 As String

    s1 = "Hello"
    s2 = " "
    s3 = "World"

    ' Simple 2-part concat
    Dim r1 As String
    r1 = s1 & s3
    Print r1

    ' 3-part concat (tests ConcatFree leak fix)
    Dim r2 As String
    r2 = s1 & s2 & s3
    Print r2

    ' 4-part concat
    Dim r3 As String
    r3 = "A" & "B" & "C" & "D"
    Print r3

    ' 5-part concat with variables
    Dim a As String
    Dim b As String
    Dim c As String
    Dim d As String
    Dim e As String
    a = "1"
    b = "2"
    c = "3"
    d = "4"
    e = "5"
    Dim r4 As String
    r4 = a & b & c & d & e
    Print r4

    ' Concat in expression
    Dim r5 As String
    r5 = "Result: " & s1 & s2 & s3 & "!"
    Print r5
End Sub
