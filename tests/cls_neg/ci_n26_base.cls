VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "Vt26Base"
Option Explicit

' ai/022 B08e-6 site 8 (Fix 083c shape): the receiver is a Property Get of this module, so
' its emitted text carries a call. This pair pins the contract that the call site reports
' VB3027 rather than binding statically -- the priority-2 dispatcher already did that
' before this batch, and the case exists so a later refactor cannot quietly lose it.
Private m_self As Vt26Base

Public Overridable Function Speak() As String
    Speak = "base"
End Function

Public Property Get Own() As Vt26Base
    Set Own = m_self
End Property

Public Function BadPropReceiver() As String
    BadPropReceiver = Own.Speak()
End Function
