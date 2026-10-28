Option Explicit

' Fixture for the ByRef-object out-parameter station (knife 271). VB6 writes a
' method's result back through an out argument, and the idiom is to hand over the
' enclosing Function's OWN return variable -- ComCtlsDemo Common/Common.bas:1017
' does exactly that with Font.Clone. The two procedures below are the same sentence
' written against the two slot kinds (the return slot and a plain local): both must
' reach the same packer, and the degraded by-value form must not come back.
' ASCII on purpose: sources are read as ANSI by the driver.

Public Function CloneByReturnSlot(ByVal Font As IFont) As StdFont
    If Not Font Is Nothing Then Font.Clone CloneByReturnSlot
End Function

Public Sub CloneByLocalVar(ByVal Font As IFont)
    Dim g As StdFont
    Font.Clone g
End Sub

' Knife 273 adds the runtime leg to the same station: on a font the caller really
' holds (C3 builds New StdFont out of its own host font object), Clone is a method
' with ONE [out] argument in stdole2.tlb, so the new font has to come back through
' the argument. Before this knife no-slot read True (the caller slot stayed empty);
' aliased would mean Clone returned the SAME object -- VB6 gives an independent copy,
' and the caller then writes Bold/Underline on that copy.
Public Function CloneViaLocal(ByVal Font As IFont) As StdFont
    Dim h As StdFont
    Font.Clone h
    Set CloneViaLocal = h
End Function

Sub Main()
    Dim f As New StdFont
    Dim g As StdFont
    Dim h As StdFont
    Set g = CloneByReturnSlot(f)
    Set h = CloneViaLocal(f)
    Debug.Print "K271:no-slot="; (g Is Nothing)
    Debug.Print "K271:local-slot="; (h Is Nothing)
    Debug.Print "K271:aliased="; (g Is f)
    Debug.Print "K271-DONE"
End Sub
