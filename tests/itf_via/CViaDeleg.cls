VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "CViaDeleg"
Option Explicit

' ai/022 B10: the delegator. It writes no Describe/Rename/Area/Name-let member at all --
' the whole IViaNamed contract is handed to m_h. It does keep its OWN Property Get Name,
' which wins for that one slot (per-slot composition: read is local, write is delegated).

Implements IViaNamed Via m_h

Private m_h As CViaHolder
Private m_own As String

Public Sub Wire(ByVal w As CViaHolder)
    Set m_h = w
End Sub

Public Sub SetOwn(ByVal v As String)
    m_own = v
End Sub

Private Property Get Name() As String
    Name = "own:" & m_own
End Property
