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

Sub Main()
    Debug.Print "K271-DONE"
End Sub
