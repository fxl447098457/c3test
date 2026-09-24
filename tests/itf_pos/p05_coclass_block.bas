Attribute VB_Name = "P05CoClassBlock"
Option Explicit

' ai/022 B11/C01 positive: the whole CoClass block form must parse (attribute lines,
' contract references, the same-line `[Default] Interface X` form) and reach the AST.
' Nothing consumes Module::coclasses yet -- C01 is syntax + AST only, so this case is
' graded by --syntax-only staying silent.

Interface IShape
    Sub Move(dx As Long, dy As Long)
End Interface

Interface ICircle
    Property Get Radius() As Long
End Interface

CoClass Circle
    [CoClassId("{2E1B5C40-8A5F-4C2A-9E2D-6D6A0F2B9C11}")]
    [ProgId("Shapes.Circle")]
    [ComCreatable(True)]
    [Implementation("CircleImpl")]
    Interface IShape
    [Default] Interface ICircle
End CoClass

Sub Main()
    Debug.Print "ok"
End Sub
