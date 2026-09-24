VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "VbpImpl"
Option Explicit

' ai/022 B11/C02: the class module the vbp three-part entry hangs its CLSID on.
' Id.vbp lists it as Class=VbpImpl; VbpImpl.cls; {33333333-4444-5555-6666-777777777777}.

Private m_mode As Long

Public Sub Paint(ByVal mode As Long)
    m_mode = mode
End Sub
