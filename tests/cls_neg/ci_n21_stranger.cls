VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "Prot21Stranger"
Option Explicit

Private m_b As Prot21Base

Public Sub Poke(ByVal v As Long)
    m_b.m_hidden = v
End Sub
