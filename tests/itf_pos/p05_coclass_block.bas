Attribute VB_Name = "P05CoClassBlock"
Option Explicit

' ai/022 B11/C01 positive: the whole CoClass block form must parse (attribute lines,
' contract references, the same-line `[Default] Interface X` form) and reach the AST.
' C03a note: no [Implementation] here on purpose -- this is a single .bas file, so any
' target name would be unknown, and [ComCreatable(True)] is refused outside a DLL project.

Interface IShape
    Sub Move(dx As Long, dy As Long)
End Interface

Interface ICircle
    Property Get Radius() As Long
End Interface

CoClass Circle
    [CoClassId("{2E1B5C40-8A5F-4C2A-9E2D-6D6A0F2B9C11}")]
    [ProgId("Shapes.Circle")]
    [ComCreatable(False)]
    Interface IShape
    [Default] Interface ICircle
End CoClass

Sub Main()
    Debug.Print "ok"
End Sub
