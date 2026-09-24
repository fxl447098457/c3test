VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "RingAct"
Option Explicit

' ai/022 B11/C05: derived implementation bound by CoClass Ring. It writes neither Move
' (ancestor-supplied -- the exact shape the 3.4c contract check had to learn to accept)
' nor a new Area; Area is overridden, so a call through the group name must land on this
' one, which is what proves the group view keeps the virtual table rather than a static call.

Inherits ShapeAct

Public Overrides Function Area() As Double
    Area = -1#
End Function
