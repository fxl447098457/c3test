VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "Vt25Base"
Option Explicit

' ai/022 B08e-6 site 10 (Fix 088c AST fallback chain): `k.Own` emits a call, so the outer
' overridable call could not load a vtable without running the property twice. Before this
' batch `k.Own.Speak()` compiled and answered "base" even for a derived instance.
Private m_self As Vt25Base

Public Overridable Function Speak() As String
    Speak = "base"
End Function

Public Property Get Own() As Vt25Base
    Set Own = m_self
End Property

Public Function BadPropChain(ByVal k As Vt25Base) As String
    BadPropChain = k.Own.Speak()
End Function
