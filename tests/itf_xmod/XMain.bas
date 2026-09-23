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

    ' ---- B05: reference counting through the IUnknown prefix slots ----
    ' (a) instance owned only by an interface variable: Set Nothing releases the
    '     last reference, so Class_Terminate fires ("TERM last=bye").
    Dim z As IWriter
    Set z = New CWriter
    z.Emit("bye")
    Set z = Nothing
    If z Is Nothing Then
        Debug.Print "LIFE1:OK"
    Else
        Debug.Print "LIFE1:FAIL"
    End If

    ' (b) a class variable keeps its own reference: releasing every interface
    '     variable that aliases it must NOT destroy the instance.
    Dim w2 As CWriter
    Set w2 = New CWriter
    Dim p1 As IWriter
    Dim p2 As IWriter
    Set p1 = w2
    Set p2 = w2
    Set p1 = Nothing
    p2.Emit("kept")
    If p2.Total() = 1 Then
        Debug.Print "LIFE2:OK"
    Else
        Debug.Print "LIFE2:FAIL"
    End If
    Set p2 = Nothing
    If w2.Total() = 1 Then
        Debug.Print "LIFE3:OK"
    Else
        Debug.Print "LIFE3:FAIL"
    End If

    ' (c) end-of-procedure release: only the procedure epilogue frees this one
    ScopeExit
    Debug.Print "LIFE9:OK"
End Sub

Sub ScopeExit()
    Dim s2 As IWriter
    Set s2 = New CWriter
    s2.Emit("scoped")
End Sub
