VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "CircleImpl"
Option Explicit

' ai/022 B11/C02: the class CCCircle binds to. ai/022 D51 (C03b): since the contract check
' went live, its body is the evidence -- Move and Paint sit exactly in the two slots that
' CCCircle lists (IShape.Move / ITagged.Paint), written the same way the interfaces do.
' C03a note: no [ComCreatable(True)] anywhere -- this is a Type=Exe project.

Private m_mode As Long

Public Sub Move(dx As Long, dy As Long)
    m_mode = dx + dy
End Sub

Public Sub Paint(mode As Long)
    m_mode = mode
End Sub
