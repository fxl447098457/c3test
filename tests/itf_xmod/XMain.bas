Option Explicit

Sub Main()
    Dim w As CWriter
    Set w = New CWriter
    w.Emit("alpha")
    w.Emit("beta")
    w.Emit("gamma")

    If w.Total() = 3 Then
        Debug.Print "XMOD1:OK"
    Else
        Debug.Print "XMOD1:FAIL total=" & w.Total()
    End If

    If w.Last() = "gamma" Then
        Debug.Print "XMOD2:OK"
    Else
        Debug.Print "XMOD2:FAIL last=" & w.Last()
    End If
End Sub
