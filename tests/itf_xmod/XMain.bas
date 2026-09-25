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

    ' ---- B06a: QueryInterface / cross-interface Set / TypeOf ... Is <interface> ----
    ' sw's object is reached through two different interfaces; ILog is found by QI
    ' from an IWriter pointer (sibling-interface branch), and INope is not there.
    Dim sw As CWriter
    Set sw = New CWriter
    Dim wr As IWriter
    Dim lg As ILog
    Set wr = sw
    wr.Emit("through-writer")
    Set lg = wr
    lg.Log("through-log")
    If sw.Total() = 1 Then
        Debug.Print "QI1:OK"
    Else
        Debug.Print "QI1:FAIL"
    End If
    If sw.Logged() = 1 Then
        Debug.Print "QI2:OK"
    Else
        Debug.Print "QI2:FAIL"
    End If
    If lg Is Nothing Then
        Debug.Print "QI3:FAIL"
    Else
        Debug.Print "QI3:OK"
    End If
    If TypeOf wr Is ILog Then
        Debug.Print "TOF1:OK"
    Else
        Debug.Print "TOF1:FAIL"
    End If
    If TypeOf lg Is IWriter Then
        Debug.Print "TOF2:OK"
    Else
        Debug.Print "TOF2:FAIL"
    End If
    If TypeOf wr Is INope Then
        Debug.Print "TOF3:FAIL"
    Else
        Debug.Print "TOF3:OK"
    End If
    Set lg = Nothing
    Set wr = Nothing
    If sw.Total() = 1 Then
        Debug.Print "QI4:OK"
    Else
        Debug.Print "QI4:FAIL"
    End If

    ' ---- B06b: Nothing-safe upcast / downcast / TypeOf on a class variable ----
    ' (a) nz was never Set -> upcasting Nothing must yield Nothing, not a wild
    '     pointer (NULL + offsetof is non-zero, and the AddRef would deref it).
    Dim nz As CWriter
    Dim snz As IWriter
    Set snz = nz
    If snz Is Nothing Then
        Debug.Print "DN0:OK"
    Else
        Debug.Print "DN0:FAIL"
    End If

    ' (b) upcast then downcast: same instance, and the interface side letting go
    '     must not pull the object out from under the class variable (AddRef count).
    Dim dw As CWriter
    Set dw = New CWriter
    Dim di As IWriter
    Set di = dw
    di.Emit("down-cast")
    Dim dw2 As CWriter
    Set dw2 = di
    If dw2 Is Nothing Then
        Debug.Print "DN1:FAIL"
    Else
        Debug.Print "DN1:OK"
    End If
    Set di = Nothing
    If dw2 Is Nothing Then
        Debug.Print "DN2:FAIL"
    Else
        Debug.Print "DN2:OK"
    End If
    If dw2.Total() = 1 Then
        Debug.Print "DN3:OK"
    Else
        Debug.Print "DN3:FAIL"
    End If

    ' (c) TypeOf on a project class variable (static IID membership, no cast needed)
    If TypeOf dw Is IWriter Then
        Debug.Print "TOC1:OK"
    Else
        Debug.Print "TOC1:FAIL"
    End If
    If TypeOf dw Is ILog Then
        Debug.Print "TOC2:OK"
    Else
        Debug.Print "TOC2:FAIL"
    End If
    If TypeOf dw Is INope Then
        Debug.Print "TOC3:FAIL"
    Else
        Debug.Print "TOC3:OK"
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
