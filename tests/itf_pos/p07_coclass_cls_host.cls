VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "P07CoclassClsHost"
Option Explicit

' ai/022 B11/C01 positive: the .cls host form. A CoClass block may sit in a class module
' next to the implementation it exposes, alongside interface blocks and ordinary members.

CoClass Widget
    [CoClassId("{11111111-2222-3333-4444-555555555555}")]
    [ProgId("Shapes.Widget")]
    [ComCreatable(True)]
    [Implementation("Widget")]
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

Public Property Get Value() As Long
    Value = m_v
End Property
