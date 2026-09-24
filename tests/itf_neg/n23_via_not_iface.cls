VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "N23ViaNotIface"
Option Explicit

' ai/022 B10 negative: Via is only defined for a checked Interface contract. The name here
' is not an Interface block, so there is no slot table to hand over -> VB3029.

Implements IN23Ghost Via m_x

Private m_x As Long
