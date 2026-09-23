VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "Vt19Base"
Option Explicit

Private m_mode As Long

Public Overridable Property Let Mode(ByVal v As Long)
    m_mode = v
End Property

Public Property Get Mode() As Long
    Mode = m_mode
End Property
