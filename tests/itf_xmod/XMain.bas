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

    ' B04: interface value = thin pointer into the object's slot field;
    ' every call below goes through vb6_ivtbl_IWriter, not a direct class call.
    Dim s As IWriter
    Set s = w
    s.Emit("delta")
    If s.Total() = 4 Then
        Debug.Print "IFV1:OK"
    Else
        Debug.Print "IFV1:FAIL"
    End If
    If s.Last() = "delta" Then
        Debug.Print "IFV2:OK"
    Else
        Debug.Print "IFV2:FAIL"
    End If
    Set s = Nothing
    If s Is Nothing Then
        Debug.Print "IFV3:OK"
    Else
        Debug.Print "IFV3:FAIL"
    End If

End Sub
