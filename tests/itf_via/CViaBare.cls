VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "CViaBare"
Option Explicit

' ai/022 B10: a pure delegator -- it declares no member of the contract at all, so every
' one of the six IViaNamed slots is served by a generated forwarding adapter over m_h.

Implements IViaNamed Via m_h

Private m_h As CViaHolder

Public Sub Wire(ByVal w As CViaHolder)
    Set m_h = w
End Sub
