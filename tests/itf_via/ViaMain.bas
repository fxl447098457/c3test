Attribute VB_Name = "ViaMain"
Option Explicit

' ai/022 B10 acceptance run for `Implements <Interface> Via <holder field>`.
' Every slot reached below is served by a generated forwarding adapter over the holder's
' own interface table, so these lines prove the contract was satisfied without the
' delegator writing a single forwarding member -- and that it reaches the delegatee.

Sub Main()
    Dim h As CViaHolder
    Set h = New CViaHolder
    Dim b As CViaBare
    Set b = New CViaBare
    b.Wire h

    ' hs looks at the delegatee directly, so it shows what the delegator pushed in.
    Dim hs As IViaNamed
    Set hs = h
    Dim s As IViaNamed
    Set s = b

    ' VIA1: inherited slot (IViaShape.Describe), Sub with one arg
    s.Describe "q"
    If hs.Name() = "q?" Then
        Debug.Print "VIA1:OK"
    Else
        Debug.Print "VIA1:FAIL name=" & hs.Name()
    End If

    ' VIA2: the interface's own slot (IViaNamed.Rename)
    s.Rename "x"
    If hs.Name() = "ren:x" Then
        Debug.Print "VIA2:OK"
    Else
        Debug.Print "VIA2:FAIL name=" & hs.Name()
    End If

    ' VIA3: Function returning Double on an inherited slot
    h.SetArea 3.5
    If s.Area() = 3.5# Then
        Debug.Print "VIA3:OK"
    Else
        Debug.Print "VIA3:FAIL area=" & s.Area()
    End If

    ' VIA4: Function with two String args (argument order survives the hop)
    If s.Join("a", "b") = "a+b" Then
        Debug.Print "VIA4:OK"
    Else
        Debug.Print "VIA4:FAIL got=" & s.Join("a", "b")
    End If

    ' VIA5: per-slot composition -- the delegator keeps its OWN Property Get for that one
    ' slot (read is local) while its siblings still delegate.
    Dim d As CViaDeleg
    Set d = New CViaDeleg
    d.Wire h
    d.SetOwn "Q"
    Dim t As IViaNamed
    Set t = d
    If (t.Name() = "own:Q") And (t.Join("p", "r") = "p+r") Then
        Debug.Print "VIA5:OK"
    Else
        Debug.Print "VIA5:FAIL own=" & t.Name() & " join=" & t.Join("p", "r")
    End If

    ' VIA6: holder is Nothing -- Sub slots no-op, value slots answer the zero value
    ' instead of dereferencing NULL.
    Dim b2 As CViaBare
    Set b2 = New CViaBare
    Dim z As IViaNamed
    Set z = b2
    z.Describe "ignored"
    z.Rename "ignored"
    If (z.Area() = 0#) And (z.Name() = "") Then
        Debug.Print "VIA6:OK"
    Else
        Debug.Print "VIA6:FAIL area=" & z.Area() & " name=" & z.Name()
    End If

    ' VIA7: two interface views on one delegator: releasing one must not pull the
    ' instance out from under the other.
    Dim a1 As IViaNamed
    Dim a2 As IViaNamed
    Set a1 = b
    Set a2 = b
    Set a1 = Nothing
    a2.Rename "alive"
    If hs.Name() = "ren:alive" Then
        Debug.Print "VIA7:OK"
    Else
        Debug.Print "VIA7:FAIL name=" & hs.Name()
    End If

    ' VIA8: the delegator answers QueryInterface for the interface it delegates
    If TypeOf t Is IViaNamed Then
        Debug.Print "VIA8:OK"
    Else
        Debug.Print "VIA8:FAIL"
    End If

    ' VIA9: two delegators wired to one delegatee share its state (and a contract member
    ' is NOT callable on the class variable -- only through an interface view).
    b2.Wire h
    z.Describe "three"
    If s.Name() = "three?" Then
        Debug.Print "VIA9:OK"
    Else
        Debug.Print "VIA9:FAIL name=" & s.Name()
    End If

    Set a2 = Nothing
    Set s = Nothing
    Set t = Nothing
    Set z = Nothing
    Debug.Print "VIA-DONE"
End Sub
