VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "Button"
Option Explicit

Public Event Click(ByVal X As Long, ByVal Y As Long)
Public Event Resize(ByVal Width As Long, ByVal Height As Long)

Private m_Caption As String

Public Sub SetCaption(ByVal Value As String)
    m_Caption = Value
End Sub

Public Sub DoClick(ByVal X As Long, ByVal Y As Long)
    RaiseEvent Click(X, Y)
End Sub

Public Sub DoResize(ByVal W As Long, ByVal H As Long)
    RaiseEvent Resize(W, H)
End Sub
