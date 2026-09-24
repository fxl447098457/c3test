VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "Vt24Base"
Option Explicit

' ai/022 B08e-6 site 12 (Fix 015 method chain): the receiver of the overridable call is the
' emitted text of an inner call, so evaluating it twice would run the inner call twice.
' Before this batch `k.Chain(2).Speak()` compiled and bound statically to this base body.
Public Overridable Function Speak() As String
    Speak = "base"
End Function

Public Function Chain(ByVal n As Long) As Vt24Base
    Set Chain = Me
End Function

Public Function BadChain(ByVal k As Vt24Base) As String
    BadChain = k.Chain(2).Speak()
End Function
