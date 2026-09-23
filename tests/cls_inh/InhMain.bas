Option Explicit

Sub Main()
    Dim d As InhDerived
    Set d = New InhDerived
    Debug.Print "INH0:" & d.Name2()

    Dim b As InhBase
    Set b = New InhBase
    b.Bump
    b.Bump
    If b.Hits() = 2 Then
        Debug.Print "INH1:OK"
    Else
        Debug.Print "INH1:FAIL hits=" & b.Hits()
    End If
End Sub
