VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
END
Attribute VB_Name = "EventSource"
Option Explicit

Public Event OnData(ByVal Id As Long, ByVal Value As Double)
Public Event OnError(ByVal ErrorCode As Long)

Public Name As String

Public Sub FireData(ByVal Id As Long, ByVal Value As Double)
    RaiseEvent OnData(Id, Value)
End Sub

Public Sub FireError(ByVal ErrorCode As Long)
    RaiseEvent OnError(ErrorCode)
End Sub