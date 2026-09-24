VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "CImpl"
Option Explicit

Implements IProbe

Private m_got As Long

Private Sub Ping(ByVal n As Long)
    m_got = n * 2
End Sub

Private Property Get Got() As Long
    Got = m_got
End Property
