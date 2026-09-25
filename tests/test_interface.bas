' B01 (ai/022 design record D1/D2): tB-style Interface contract blocks, syntax layer.
' Scope of this case: the blocks parse (Extends chain, all member kinds, bracket
' attribute lines), the new soft keywords keep working as ordinary identifiers,
' and the untouched pipeline still builds and runs. Interface-typed variables and
' contract checking arrive in B02/B04, so no member is called through a block here.

[InterfaceId("{11111111-2222-3333-4444-555555555555}")]
[Description("shape contract")]
Interface IShape
    Function Area() As Double
    Sub Draw(h As Long)
    Property Get Title() As String
    Property Let Caption(v As String)
    Property Set Target(o As Object)
End Interface

Interface IDrawnShape Extends IShape
    Sub MarkDrawn()
End Interface

[InterfaceId("{AAAAAAAABBBBCCCCDDDDEEEEEEEEEEEE}")]
[OleAutomation]
Interface IRaw
    Function Handle(msg As String) As Long
End Interface

Interface IAttr
    [DispId(1)]
    [PreserveSig]
    Function Slot() As Long
End Interface

' soft-keyword insurance: Interface / Extends must stay usable as names
Sub SoftNames()
    Dim Interface As Long
    Dim Extends As Long
    Interface = 7
    Extends = 5
    Debug.Print "ITF-SOFT:" & (Interface + Extends)
End Sub

Sub Main()
    SoftNames
    Debug.Print "ITF-1:OK"
    Debug.Print "ITF-2:OK"
    Debug.Print "INTERFACE-DONE"
End Sub
