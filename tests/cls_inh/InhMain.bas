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

    ' INH2: inherited Sub/Function through the stub + inherited private field
    If d.BumpTwice() = 2 Then
        Debug.Print "INH2:OK"
    Else
        Debug.Print "INH2:FAIL n=" & d.BumpTwice()
    End If

    ' INH3: inherited UDT field keeps the same offset inside the derived struct
    d.SetPt 5
    If d.PtSum() = 15 Then Debug.Print "INH3:OK" Else Debug.Print "INH3:FAIL sum=" & d.PtSum()

    ' INH4/INH5: shadowing is child-wins, the base instance is unaffected
    If d.Tag() = "mid" Then Debug.Print "INH4:OK" Else Debug.Print "INH4:FAIL tag=" & d.Tag()
    If b.Tag() = "base" Then Debug.Print "INH5:OK" Else Debug.Print "INH5:FAIL tag=" & b.Tag()

    ' INH6..INH8: Optional params forwarded by the stub (IsMissing seen in the base)
    If d.Cat("p") = "p/solo" Then Debug.Print "INH6:OK" Else Debug.Print "INH6:FAIL " & d.Cat("p")
    If d.Sum3(1, 2) = 3 Then Debug.Print "INH7:OK" Else Debug.Print "INH7:FAIL " & d.Sum3(1, 2)
    If d.Sum3(1, 2, 10) = 13 Then Debug.Print "INH8:OK" Else Debug.Print "INH8:FAIL " & d.Sum3(1, 2, 10)

    ' INH9: Property Get/Let stubs in both directions
    d.Name = "zz"
    If d.Name = "zz" Then Debug.Print "INH9:OK" Else Debug.Print "INH9:FAIL name=" & d.Name

    ' INH10: mid-level own member and a grandparent member both reachable (3-level chain)
    d.SetMidTag "m"
    If d.MidTag() = "m" And d.Label = "" Then
        d.Label = "LB"
        If d.Label = "LB" Then Debug.Print "INH10:OK" Else Debug.Print "INH10:FAIL label"
    Else
        Debug.Print "INH10:FAIL mid=" & d.MidTag()
    End If

    ' INH11: derived and base instances keep separate storage (prefix copy, not shared)
    b.SetPt 1
    If b.PtSum() = 3 And d.PtSum() = 15 Then Debug.Print "INH11:OK" Else Debug.Print "INH11:FAIL"
    If d.RevealSecret(4) = 8 Then Debug.Print "INH12:OK" Else Debug.Print "INH12:FAIL v=" & d.RevealSecret(4)
    If d.TagRoundTrip("S") = "S" Then Debug.Print "INH13:OK" Else Debug.Print "INH13:FAIL"
    ' INH14..INH16 (ai/022 B08b): an override applies to calls on the derived object, while the
    ' base instance keeps its own implementation (through-base dispatch needs the class vtable,
    ' which is ai/022 B08d). FrozenSeen() proves NotOverridable parses and inherits as a stub.
    If d.Speak() = "derived" Then Debug.Print "INH14:OK" Else Debug.Print "INH14:FAIL " & d.Speak()
    If b.Speak() = "base" Then Debug.Print "INH15:OK" Else Debug.Print "INH15:FAIL " & b.Speak()
    If d.FrozenSeen() = "frozen" Then Debug.Print "INH16:OK" Else Debug.Print "INH16:FAIL " & d.FrozenSeen()
End Sub
