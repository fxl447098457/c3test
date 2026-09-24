Attribute VB_Name = "ActBlocks"
Option Explicit

' ai/022 B11/C05 acceptance: two groups, both with [Implementation] and a [Default]
' contract. No [ProgId] is written -- the ProgID the CreateObject rewrite must recognise
' is the derived one, "<vbp Name>.<block>" = ActApp.Circle / ActApp.Ring.

CoClass Circle
    [Implementation("ShapeAct")]
    [Default] Interface IShapeAct
End CoClass

CoClass Ring
    [Implementation("RingAct")]
    [Default] Interface IShapeAct
End CoClass
