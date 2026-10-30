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

' Knife 256: a ByRef event param is an OUT param - the handler's write must
' reach the raiser after RaiseEvent returns (ComCtlsDemo ListView
' GetEmptyMarkup / empty-list markup text is exactly this shape).
Public Event GetEmptyMarkup(ByRef Text As String, ByRef Center As Boolean)

Public Function RenderMarkup() As String
    Dim T As String
    Dim C As Boolean
    T = ""
    C = False
    RaiseEvent GetEmptyMarkup(T, C)
    If C Then
        RenderMarkup = T & "|CENTERED"
    Else
        RenderMarkup = T & "|LEFT"
    End If
End Function
