VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "WidgetHost"
Option Explicit

' ai/022 B11/C01 positive: the .cls host form. ai/022 D48 (C03a): the block may carry the
' same name as its host module -- that pairing is the point of the form, not a name clash.

CoClass WidgetHost
    [CoClassId("{11111111-2222-3333-4444-555555555555}")]
    [ProgId("Shapes.Widget")]
    [ComCreatable(False)]
    [Implementation("WidgetHost")]
    Interface IWidget
    [Default] Interface INamed
End CoClass

Interface IWidget
    Sub Spin(rpm As Long)
End Interface

Interface INamed
    Property Get Name() As String
End Interface

Private m_v As Long

Public Sub Spin(ByVal rpm As Long)
End Sub

Public Property Get Name() As String
    Name = m_v
End Property
