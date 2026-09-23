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
    ' base instance keeps its own implementation. FrozenSeen() proves NotOverridable parses and
    ' inherits as a stub.
    If d.Speak() = "derived" Then Debug.Print "INH14:OK" Else Debug.Print "INH14:FAIL " & d.Speak()
    If b.Speak() = "base" Then Debug.Print "INH15:OK" Else Debug.Print "INH15:FAIL " & b.Speak()
    If d.FrozenSeen() = "frozen" Then Debug.Print "INH16:OK" Else Debug.Print "INH16:FAIL " & d.FrozenSeen()
    ' INH17..INH23 (ai/022 B08d): the class vtable. PickThru/SpeakThru/GreetThru all run
    ' Me.<slot>() **inside the base body**, so what they return is the dispatch result.
    Dim m As InhMid
    Set m = New InhMid
    If b.PickThru() = "base" Then Debug.Print "INH17:OK" Else Debug.Print "INH17:FAIL " & b.PickThru()
    If m.PickThru() = "mid" Then Debug.Print "INH18:OK" Else Debug.Print "INH18:FAIL " & m.PickThru()
    ' INH19 is the evidence for 'bind to the nearest override', not to the leaf
    If d.PickThru() = "mid" Then Debug.Print "INH19:OK" Else Debug.Print "INH19:FAIL " & d.PickThru()
    If d.SpeakThru() = "derived" Then Debug.Print "INH20:OK" Else Debug.Print "INH20:FAIL " & d.SpeakThru()
    If b.SpeakThru() = "base" Then Debug.Print "INH21:OK" Else Debug.Print "INH21:FAIL " & b.SpeakThru()
    If d.GreetThru("bob") = "hi bob (derived)" Then Debug.Print "INH22:OK" Else Debug.Print "INH22:FAIL " & d.GreetThru("bob")
    ' INH23: a base-typed variable holding a derived instance must not slice to the base impl
    Dim up As InhBase
    Set up = d
    If up.Speak() = "derived" Then Debug.Print "INH23:OK" Else Debug.Print "INH23:FAIL " & up.Speak()
    ' INH24/INH25 (ai/022 B08d): a second branch off the same base (fan-out). Each instance
    ' dispatches on its own table, so the sibling's Speak must not leak into InhDerived.
    Dim sib As InhSib
    Set sib = New InhSib
    If sib.Speak() = "sibling" Then Debug.Print "INH24:OK" Else Debug.Print "INH24:FAIL " & sib.Speak()
    ' INH25: inherited slot the sibling does NOT override still resolves through ITS table
    ' (base's own Pick) while the base-typed view keeps working on the same instance
    If sib.PickThru() = "base" Then Debug.Print "INH25:OK" Else Debug.Print "INH25:FAIL " & sib.PickThru()
    Dim up3 As InhBase
    Set up3 = sib
    If up3.SpeakThru() = "sibling" Then Debug.Print "INH26:OK" Else Debug.Print "INH26:FAIL " & up3.SpeakThru()
    ' INH27..INH31 (ai/022 B08e): the very same calls written INSIDE `With <var>` must bind
    ' through the instance's own table. INH27/28 are the discriminators: wu is declared
    ' InhBase but holds the InhDerived instance, so a statically bound call answers "base".
    Dim wu As InhBase
    Set wu = d
    With wu
        If .Speak() = "derived" Then Debug.Print "INH27:OK" Else Debug.Print "INH27:FAIL " & .Speak()
        If .Greet("bob") = "hi bob (derived)" Then Debug.Print "INH28:OK" Else Debug.Print "INH28:FAIL " & .Greet("bob")
    End With
    Dim wm As InhMid
    Set wm = New InhMid
    With wm
        ' INH29: mid instance picks mid's override; INH30/31 stay on the direct path
        ' (Property Let/Get and a plain Sub are not virtual slots -> must not be rewritten).
        If .Pick() = "mid" Then Debug.Print "INH29:OK" Else Debug.Print "INH29:FAIL " & .Pick()
        .Name = "wn"
        If .Name = "wn" Then Debug.Print "INH30:OK" Else Debug.Print "INH30:FAIL " & .Name
        .Bump
        .Bump
        If .Hits() = 2 Then Debug.Print "INH31:OK" Else Debug.Print "INH31:FAIL " & .Hits()
    End With
End Sub
