VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "VbpImpl"
Option Explicit

' ai/022 B11/C02: the class module the vbp three-part entry hangs its CLSID on.
' Id.vbp lists it as Class=VbpImpl; VbpImpl.cls; {33333333-4444-5555-6666-777777777777}.
' C03b (ai/022 D51): CCVbp lists IShape + INamed, so this class has to meet Move and
' get_Label. Paint stays as a member that is *not* part of any contract -- a class may
' carry more than its coclass exposes.

Private m_mode As Long

Public Sub Move(dx As Long, dy As Long)
    m_mode = dx + dy
End Sub

Public Property Get Label() As String
    Label = "VbpImpl"
End Property

Public Sub Paint(mode As Long)
    m_mode = mode
End Sub
